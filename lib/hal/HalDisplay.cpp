#include <BoardConfig.h>
#include <HalDisplay.h>
#include <HalGPIO.h>
#include <HalPowerManager.h>
#include <Logging.h>

#include "HalSpiBus.h"

// Global HalDisplay instance
HalDisplay display;

#define SD_SPI_MISO 7

namespace {
// EInkDisplay::setBusyWaitHooks() only accepts plain function pointers, so these
// forward to the powerManager singleton instead of capturing state.
#if !FREEINK_SD_SDMMC
// The panel needs no SPI traffic while its waveform runs (~0.5 s), so lend the
// shared bus to SD access for that window instead of stalling every read.
bool spiLentDuringBusyWait = false;
#endif

void onDisplayBusyWaitBegin() {
  powerManager.beginDisplayBusyWait();
#if !FREEINK_SD_SDMMC
  spiLentDuringBusyWait = HalSpiBus::getInstance().releaseForIdle();
#endif
}

void onDisplayBusyWaitEnd() {
#if !FREEINK_SD_SDMMC
  if (spiLentDuringBusyWait) {
    spiLentDuringBusyWait = false;
    HalSpiBus::getInstance().reacquireAfterIdle();
  }
#endif
  powerManager.endDisplayBusyWait();
}
}  // namespace

HalDisplay::HalDisplay() : einkDisplay(EPD_SCLK, EPD_MOSI, EPD_CS, EPD_DC, EPD_RST, EPD_BUSY) {}

HalDisplay::~HalDisplay() {}

void HalDisplay::begin(bool seamless) {
  HalSpiBus::Lock spiLock;

  // Set X3-specific panel mode before initializing.
  if (gpio.deviceIsX3()) {
    einkDisplay.setDisplayX3();
  }

  einkDisplay.begin();
  // Keep tickless idle from light-sleeping mid-refresh; safe even before
  // powerManager.begin() runs since the hooks no-op until the lock exists.
  einkDisplay.setBusyWaitHooks(&onDisplayBusyWaitBegin, &onDisplayBusyWaitEnd);

  if (seamless) {
    // Defuse the SDK's X3 _x3InitialFullSyncsRemaining counter (no-op on X4)
    // so the first paint isn't promoted to FULL (~770ms). Skips the wakeup-
    // gated requestResync() below for the same reason.
    einkDisplay.skipInitialResync();
    return;
  }
  // Request resync after specific wakeup events to ensure clean display state.
  const auto wakeupReason = gpio.getWakeupReason();
  if (wakeupReason == HalGPIO::WakeupReason::PowerButton || wakeupReason == HalGPIO::WakeupReason::AfterFlash ||
      wakeupReason == HalGPIO::WakeupReason::Other) {
    einkDisplay.requestResync();
  }
}

void HalDisplay::clearScreen(uint8_t color) const { einkDisplay.clearScreen(color); }

void HalDisplay::drawImage(const uint8_t* imageData, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                           bool fromProgmem) const {
  einkDisplay.drawImage(imageData, x, y, w, h, fromProgmem);
}

void HalDisplay::drawImageTransparent(const uint8_t* imageData, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                      bool fromProgmem) const {
  einkDisplay.drawImageTransparent(imageData, x, y, w, h, fromProgmem);
}

EInkDisplay::RefreshMode convertRefreshMode(HalDisplay::RefreshMode mode) {
  switch (mode) {
    case HalDisplay::FULL_REFRESH:
      return EInkDisplay::FULL_REFRESH;
    case HalDisplay::HALF_REFRESH:
      return EInkDisplay::HALF_REFRESH;
    case HalDisplay::FAST_REFRESH:
    default:
      return EInkDisplay::FAST_REFRESH;
  }
}

void HalDisplay::displayBuffer(HalDisplay::RefreshMode mode, bool turnOffScreen) {
  HalSpiBus::Lock spiLock;

  if (gpio.deviceIsX3() && mode == RefreshMode::HALF_REFRESH) {
    einkDisplay.requestResync(1);
  }

  einkDisplay.displayBuffer(convertRefreshMode(mode), turnOffScreen);
}

void HalDisplay::setInverted(bool inverted) {
  HalSpiBus::Lock spiLock;
  einkDisplay.setInverted(inverted);
}

void HalDisplay::displayBufferAsync(HalDisplay::RefreshMode mode) {
  if (gpio.deviceIsX3() && mode == RefreshMode::HALF_REFRESH) {
    einkDisplay.requestResync(1);
  }

  einkDisplay.displayBufferAsyncNoShadow(convertRefreshMode(mode));
}

void HalDisplay::waitRefreshComplete() { einkDisplay.waitRefreshComplete(); }

void HalDisplay::displayBufferDeferred(HalDisplay::RefreshMode mode) {
  HalSpiBus::Lock spiLock;
  einkDisplay.displayBufferAsync(convertRefreshMode(mode));
}

bool HalDisplay::isRefreshPending() const { return einkDisplay.isRefreshPending(); }

bool HalDisplay::isRefreshBusy() { return einkDisplay.refreshBusy(); }

bool HalDisplay::supportsAsyncRefresh() const { return einkDisplay.supportsAsyncRefresh(); }

// DC-balance policy. The SDK marks each controller whose gray waveforms pass its
// compile-time balance gate (FREEINK_BALANCED_GRAY_*, GrayscaleCapabilities.h);
// an unmarked controller keeps only the OEM 4-gray banks known to net zero
// (SSD1677 lut_factory_quality, X4 UC8279 kQualityBank: Absolute only), and X3
// gets no gray at all.
#ifndef FREEINK_BALANCED_GRAY_SSD1677
#define FREEINK_BALANCED_GRAY_SSD1677 0
#endif
#ifndef FREEINK_BALANCED_GRAY_UC8179
#define FREEINK_BALANCED_GRAY_UC8179 0
#endif
#ifndef FREEINK_BALANCED_GRAY_UC8279
#define FREEINK_BALANCED_GRAY_UC8279 0
#endif
#ifndef FREEINK_BALANCED_GRAY_X3
#define FREEINK_BALANCED_GRAY_X3 0
#endif

static bool grayscaleModeBalanced(const HalDisplay::GrayscaleMode mode) {
  using Controller = BoardConfig::DisplayController;
  using Mode = HalDisplay::GrayscaleMode;
  if (gpio.deviceIsX3()) return FREEINK_BALANCED_GRAY_X3;
  switch (BoardConfig::ACTIVE.displayController) {
    case Controller::SSD1677:
      return mode == Mode::Absolute || (mode == Mode::Overlay && FREEINK_BALANCED_GRAY_SSD1677);
    case Controller::UC8179:
      return FREEINK_BALANCED_GRAY_UC8179;
    case Controller::UC8279:
      return mode == Mode::Absolute || FREEINK_BALANCED_GRAY_UC8279;
    default:
      return false;
  }
}

HalDisplay::GrayscaleCapabilities HalDisplay::grayscaleCapabilities(GrayscaleMode mode) const {
  if (!grayscaleModeBalanced(mode)) return {};
  return einkDisplay.grayscaleCapabilities(mode);
}

bool HalDisplay::supportsAsyncGrayscaleBase() const { return grayscaleCapabilities().asyncBase; }

bool HalDisplay::displayGrayscaleBase(GrayscaleMode mode, RefreshMode fallback, bool turnOffScreen) {
  if (!grayscaleModeBalanced(mode)) {
    LOG_ERR("DISP", "Refusing unbalanced grayscale base mode %u", static_cast<unsigned>(mode));
    return false;
  }
  HalSpiBus::Lock spiLock;
  if (gpio.deviceIsX3() && fallback == HALF_REFRESH) einkDisplay.requestResync(1);
  balancedGrayArmed = einkDisplay.displayGrayscaleBase(mode, convertRefreshMode(fallback), turnOffScreen);
  return balancedGrayArmed;
}

void HalDisplay::refreshDisplay(HalDisplay::RefreshMode mode, bool turnOffScreen) {
  HalSpiBus::Lock spiLock;

  if (gpio.deviceIsX3() && mode == RefreshMode::HALF_REFRESH) {
    einkDisplay.requestResync(1);
  }

  einkDisplay.refreshDisplay(convertRefreshMode(mode), turnOffScreen);
}

bool HalDisplay::isInverted() const { return einkDisplay.isInverted(); }

void HalDisplay::deepSleep() {
  HalSpiBus::Lock spiLock;
  einkDisplay.deepSleep();
}

uint8_t* HalDisplay::getFrameBuffer() const { return einkDisplay.getFrameBuffer(); }

uint8_t* HalDisplay::lendFrameBufferStorage(uint32_t* sizeOut) { return einkDisplay.lendBuildStorage(sizeOut); }

void HalDisplay::returnFrameBufferStorage() { einkDisplay.returnBuildStorage(); }

void HalDisplay::copyGrayscaleBuffers(const uint8_t* lsbBuffer, const uint8_t* msbBuffer) {
  einkDisplay.copyGrayscaleBuffers(lsbBuffer, msbBuffer);
}

void HalDisplay::displayGrayscaleBase(RefreshMode fallback, bool turnOffScreen) {
  // X3: a HALF or FULL fallback means the caller wants a clean base (e.g. the
  // sleep cover, a full-screen swap from arbitrary prior content). Without
  // this, the X3 grayscale base takes its gentle differential happy path and
  // the prior home/reader frame ghosts through the soft aa_pre_bw_mid
  // waveform. Forcing a resync makes displayGrayscaleBase clear first,
  // matching displayBuffer(HALF)/displayBuffer(FULL).
  if (gpio.deviceIsX3() && fallback != RefreshMode::FAST_REFRESH) {
    einkDisplay.requestResync(1);
  }

  if (grayscaleModeBalanced(GrayscaleMode::Overlay)) {
    einkDisplay.displayGrayscaleBase(convertRefreshMode(fallback), turnOffScreen);
    return;
  }
  // The overlay base runs one-way conditioning waveforms (X3 preBwMid, UC8179 /
  // UC8279 XTF_PRE_BW_MID); a plain OTP refresh shows the same B/W frame.
  HalSpiBus::Lock spiLock;
  einkDisplay.displayBuffer(convertRefreshMode(fallback), turnOffScreen);
}

void HalDisplay::preconditionGrayscale() {
  if (grayscaleModeBalanced(GrayscaleMode::Overlay)) einkDisplay.preconditionGrayscale();
}

void HalDisplay::preconditionGrayscale(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  if (grayscaleModeBalanced(GrayscaleMode::Overlay)) einkDisplay.preconditionGrayscale(x, y, w, h);
}

void HalDisplay::copyGrayscaleLsbBuffers(const uint8_t* lsbBuffer) { einkDisplay.copyGrayscaleLsbBuffers(lsbBuffer); }

void HalDisplay::copyGrayscaleMsbBuffers(const uint8_t* msbBuffer) { einkDisplay.copyGrayscaleMsbBuffers(msbBuffer); }

void HalDisplay::cleanupGrayscaleBuffers(const uint8_t* bwBuffer) {
  balancedGrayArmed = false;
  einkDisplay.cleanupGrayscaleBuffers(bwBuffer);
}

void HalDisplay::displayGrayBuffer(bool turnOffScreen) {
  // Backstop: overlay gray runs only where it is balanced; Absolute/Direct only
  // after a balanced base.
  const bool armed = balancedGrayArmed || grayscaleModeBalanced(GrayscaleMode::Overlay);
  balancedGrayArmed = false;
  if (!armed) {
    LOG_ERR("DISP", "Skipping gray waveform without a balanced base");
    return;
  }
  HalSpiBus::Lock spiLock;
  einkDisplay.displayGrayBuffer(turnOffScreen);
  // An unbalanced UC8279 SDK routes the next Fast through the one-way
  // XTF_PRE_BW_MID transition; a resync makes it an OTP GC instead. SSD1677
  // already promotes the next paint to HALF after an absolute gray.
  if (!FREEINK_BALANCED_GRAY_UC8279 &&
      BoardConfig::ACTIVE.displayController == BoardConfig::DisplayController::UC8279) {
    einkDisplay.requestResync();
  }
}

void HalDisplay::writeGrayscalePlaneStrip(bool lsbPlane, const uint8_t* rows, uint16_t yStart, uint16_t numRows) {
  HalSpiBus::Lock spiLock;
  einkDisplay.writeGrayscalePlaneStrip(lsbPlane ? EInkDisplay::GRAY_PLANE_LSB : EInkDisplay::GRAY_PLANE_MSB, rows,
                                       yStart, numRows);
}

bool HalDisplay::shouldSkipImageBlanking() const {
  // CrossInk's extra white-image pass is redundant on UC8179. Its driver
  // always supports async display; the existing query also excludes inverted
  // output, a pending inversion transition, and an uninitialized driver.
  return BoardConfig::ACTIVE.displayController == BoardConfig::DisplayController::UC8179 &&
         einkDisplay.supportsAsyncRefresh();
}

bool HalDisplay::displayGrayscaleBaseAsync(HalDisplay::RefreshMode fallback) {
  if (!grayscaleModeBalanced(GrayscaleMode::Overlay)) {
    displayGrayscaleBase(fallback);
    return false;
  }
  HalSpiBus::Lock spiLock;
  return einkDisplay.displayGrayscaleBaseAsync(convertRefreshMode(fallback));
}

bool HalDisplay::supportsDeferredGrayscaleBase() const {
  return grayscaleModeBalanced(GrayscaleMode::Overlay) && einkDisplay.supportsDeferredGrayscaleBase();
}

bool HalDisplay::supportsStripGrayscale() const { return grayscaleCapabilities().stripUploads; }

uint16_t HalDisplay::getDisplayWidth() const { return einkDisplay.getDisplayWidth(); }

uint16_t HalDisplay::getDisplayHeight() const { return einkDisplay.getDisplayHeight(); }

uint16_t HalDisplay::getDisplayWidthBytes() const { return einkDisplay.getDisplayWidthBytes(); }

uint32_t HalDisplay::getBufferSize() const { return einkDisplay.getBufferSize(); }
