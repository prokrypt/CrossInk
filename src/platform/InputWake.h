#pragma once

#include <cstdint>

// Idle wait for the main loop that ends early when a digital key or the touch
// controller's INT line changes level, including from automatic light sleep.
// Without it, a press made while the loop sleeps waits for the next poll tick.
namespace InputWake {
// Registers the wake lines. Call once after the input pins are configured.
void begin();

// Waits up to timeoutMs, returning as soon as a wake line changes level.
void wait(uint32_t timeoutMs);
}  // namespace InputWake
