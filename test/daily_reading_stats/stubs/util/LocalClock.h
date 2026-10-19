#pragma once
#include <cstdint>

#include "CrossPointSettings.h"

// No daylight-saving rule in these tests: the offset is the standard one.
namespace LocalClock {
inline uint8_t offsetQAtUtc(uint16_t, uint8_t, uint8_t, uint8_t, uint8_t) { return SETTINGS.clockUtcOffsetQ; }
}  // namespace LocalClock
