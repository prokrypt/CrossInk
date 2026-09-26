#include "InputWake.h"

#include <Arduino.h>

#if defined(ARDUINO_ARCH_ESP32) && !defined(SIMULATOR)

#include <BoardConfig.h>
#include <Logging.h>
#include <driver/gpio.h>
#include <esp_sleep.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <hal/gpio_ll.h>
#include <soc/gpio_struct.h>

#include <array>

namespace {
std::array<gpio_num_t, 8> wakePins{};
size_t wakePinCount = 0;
SemaphoreHandle_t wakeSignal = nullptr;

// Wake lines use level interrupts, because only those can also end a light
// sleep. A level keeps firing while it holds, so each line disarms itself here
// until the next wait() re-arms it against the level it reads then.
void IRAM_ATTR onWakeLine(void* arg) {
  gpio_ll_intr_disable(&GPIO, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(arg)));
  BaseType_t higherPriorityTaskWoken = pdFALSE;
  xSemaphoreGiveFromISR(wakeSignal, &higherPriorityTaskWoken);
  if (higherPriorityTaskWoken == pdTRUE) portYIELD_FROM_ISR();
}

void addWakePin(const int8_t pin) {
  if (pin < 0 || wakePinCount >= wakePins.size()) return;
  const auto gpioPin = static_cast<gpio_num_t>(pin);
  for (size_t i = 0; i < wakePinCount; ++i) {
    if (wakePins[i] == gpioPin) return;
  }
  gpio_intr_disable(gpioPin);
  if (gpio_isr_handler_add(gpioPin, onWakeLine, reinterpret_cast<void*>(static_cast<uintptr_t>(pin))) != ESP_OK) {
    LOG_ERR("WAKE", "Could not attach input wake handler to GPIO%d", pin);
    return;
  }
  wakePins[wakePinCount++] = gpioPin;
}
}  // namespace

void InputWake::begin() {
  if (wakeSignal != nullptr) return;
  wakeSignal = xSemaphoreCreateBinary();
  if (wakeSignal == nullptr) {
    LOG_ERR("WAKE", "Could not create input wake semaphore");
    return;
  }
  // The display BUSY line may already have installed the shared ISR service.
  const esp_err_t isrErr = gpio_install_isr_service(0);
  if (isrErr != ESP_OK && isrErr != ESP_ERR_INVALID_STATE) {
    LOG_ERR("WAKE", "Could not install GPIO ISR service (%d)", static_cast<int>(isrErr));
    return;
  }

  const auto& board = BoardConfig::ACTIVE;
  // ADC-ladder boards report keys through analog levels that cannot raise a
  // GPIO interrupt, so only plain digital keys take part.
  if (board.inputStyle == BoardConfig::InputStyle::DigitalButtons) {
    for (const int8_t pin : {board.input.back, board.input.confirm, board.input.left, board.input.right,
                             board.input.up, board.input.down, board.input.power}) {
      addWakePin(pin);
    }
  }
  if (board.touch.controller != BoardConfig::TouchController::None) addWakePin(board.touch.irq);

  if (wakePinCount > 0 && esp_sleep_enable_gpio_wakeup() != ESP_OK) {
    LOG_ERR("WAKE", "Could not enable GPIO wake from light sleep");
  }
  LOG_INF("WAKE", "Input wake armed on %u line(s)", static_cast<unsigned>(wakePinCount));
}

void InputWake::wait(const uint32_t timeoutMs) {
  if (wakeSignal == nullptr || wakePinCount == 0) {
    delay(timeoutMs);
    return;
  }
  for (size_t i = 0; i < wakePinCount; ++i) {
    const gpio_num_t pin = wakePins[i];
    // Trigger on the opposite of the level present now, so a press, a release
    // and a touch INT pulse of either polarity all end the wait.
    gpio_wakeup_enable(pin, gpio_get_level(pin) ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL);
    gpio_intr_enable(pin);
  }
  xSemaphoreTake(wakeSignal, pdMS_TO_TICKS(timeoutMs));
}

#else

void InputWake::begin() {}

void InputWake::wait(const uint32_t timeoutMs) { delay(timeoutMs); }

#endif
