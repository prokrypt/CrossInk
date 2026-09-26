#pragma once

#include <HalDisplay.h>

#include <cstdint>

// Network workflows (WiFi, File Transfer, OTA, clock and KOReader sync) refresh with
// the fast waveform on every state change, like the rest of the menus. The
// state argument is kept so callers can share the helper without carrying
// transition bookkeeping in each activity.
class ScreenTransitionRefresh {
 public:
  HalDisplay::RefreshMode modeFor(const uint8_t /*screenState*/) const { return HalDisplay::FAST_REFRESH; }
};
