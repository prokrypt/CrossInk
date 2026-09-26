#include "LibraryPrewarm.h"

#ifndef SIMULATOR
#include <Arduino.h>
#include <HalStorage.h>
#include <LibraryBuilder.h>
#include <Logging.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <atomic>

#include "CrossPointSettings.h"
#endif

namespace LibraryPrewarm {

#ifdef SIMULATOR

// The simulator's FreeRTOS shim is not relied on for background builds; the
// Library keeps scanning in the foreground there.
void tick(bool) {}
void pause() {}
void stop(bool) {}
bool active() { return false; }
bool working() { return false; }
void finishForLibrary() {}

#else

namespace {
// The builder normally runs on the 8 KB Arduino loop task; give it the same.
// Transient: the stack is freed as soon as the build has been collected.
constexpr uint32_t kStackBytes = 8192;
// Same internal-heap floor as other opportunistic background work: a paused
// build keeps its buffers, and Home must still be able to decode covers.
constexpr uint32_t kMinFreeHeap = 96 * 1024;
constexpr uint32_t kMinMaxAlloc = 48 * 1024;

// Everything below is touched by the main loop only, except the atomics and
// the result fields, which the build task writes before giving doneSem.
TaskHandle_t task = nullptr;
SemaphoreHandle_t doneSem = nullptr;
std::atomic<bool> paused{false};
std::atomic<bool> cancelRequested{false};
uint32_t startGeneration = 0;
bool useMetadata = false;
bool buildOk = false;
bool buildCancelled = false;
// A build that failed on its own (not stopped) is not retried until the next
// boot; the Library still scans in the foreground and reports the problem.
bool gaveUp = false;

bool serviceBuild(void*) {
  // Blocks instead of polling so a paused build costs no wakeups and the chip
  // can stay in light sleep. resume/cancel send a notification.
  while (paused.load(std::memory_order_acquire) && !cancelRequested.load(std::memory_order_acquire)) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  }
  return !cancelRequested.load(std::memory_order_acquire);
}

void buildTask(void*) {
  library::BuildStats stats;
  library::BuildControl control;
  control.service = &serviceBuild;
  buildOk = library::buildLibraryIndex("/", stats, useMetadata, &control);
  buildCancelled = stats.cancelled;
  xSemaphoreGive(doneSem);
  // The owner deletes this task after collecting the result. Parking here
  // rather than self-deleting keeps a late notification from the owner safe.
  for (;;) ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
}

void wake() {
  if (task) xTaskNotifyGive(task);
}

void finalize() {
  vTaskDelete(task);
  task = nullptr;
  if (buildOk) {
    // The generation read before the walk: a change made during it leaves the
    // counter ahead, so the Library rescans.
    Storage.noteLibraryScanned(startGeneration);
    LOG_INF("LIBPW", "Library index ready in the background");
  } else if (buildCancelled) {
    LOG_DBG("LIBPW", "Background Library build stopped; will retry when Home is idle");
  } else {
    gaveUp = true;
    LOG_ERR("LIBPW", "Background Library build failed; leaving it to the Library");
  }
}

void collectIfDone() {
  if (task && xSemaphoreTake(doneSem, 0) == pdTRUE) finalize();
}

void waitForDone() {
  if (!task) return;
  xSemaphoreTake(doneSem, portMAX_DELAY);
  finalize();
}

void start() {
  if (!doneSem) {
    doneSem = xSemaphoreCreateBinary();
    if (!doneSem) {
      LOG_ERR("LIBPW", "Cannot create background Library build semaphore");
      gaveUp = true;
      return;
    }
  }
  startGeneration = Storage.libraryContentGeneration();
  useMetadata = SETTINGS.libraryUseMetadata != 0;
  buildOk = false;
  buildCancelled = false;
  paused.store(false, std::memory_order_release);
  cancelRequested.store(false, std::memory_order_release);
  // Idle priority: the walk only gets the CPU when the loop and render tasks
  // have nothing to do. Mutex priority inheritance covers the card lock. On
  // dual-core chips it stays off the loop's core.
  const BaseType_t core = portNUM_PROCESSORS > 1 ? 0 : tskNO_AFFINITY;
  if (xTaskCreatePinnedToCore(&buildTask, "LibPrewarm", kStackBytes, nullptr, tskIDLE_PRIORITY, &task, core) !=
      pdPASS) {
    task = nullptr;
    gaveUp = true;
    LOG_ERR("LIBPW", "Cannot start background Library build (%u free, %u max alloc)", ESP.getFreeHeap(),
            ESP.getMaxAllocHeap());
    return;
  }
  LOG_DBG("LIBPW", "Background Library build started");
}
}  // namespace

void tick(const bool idle) {
  collectIfDone();
  if (task) {
    if (idle && paused.load(std::memory_order_relaxed)) {
      paused.store(false, std::memory_order_release);
      wake();
    } else if (!idle) {
      paused.store(true, std::memory_order_release);
    }
    return;
  }
  if (!idle || gaveUp || Storage.libraryScanCurrent()) return;
  if (ESP.getFreeHeap() < kMinFreeHeap || ESP.getMaxAllocHeap() < kMinMaxAlloc) return;
  start();
}

void pause() {
  if (task) paused.store(true, std::memory_order_release);
}

void stop(const bool handOffToLibrary) {
  collectIfDone();
  if (!task) return;
  if (handOffToLibrary) {
    paused.store(false, std::memory_order_release);
    wake();
    return;
  }
  cancelRequested.store(true, std::memory_order_release);
  wake();
  waitForDone();
}

bool active() { return task != nullptr; }

bool working() { return task != nullptr && !paused.load(std::memory_order_relaxed); }

void finishForLibrary() {
  collectIfDone();
  if (!task) return;
  paused.store(false, std::memory_order_release);
  wake();
  waitForDone();
}

#endif

}  // namespace LibraryPrewarm
