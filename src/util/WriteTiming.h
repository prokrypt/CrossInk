#pragma once

#include <Arduino.h>
#include <Logging.h>

// Debug probe for persistent writes: logs how long the skip-if-identical check
// took, the total, and whether the write was skipped.
class ScopedWriteTimer {
 public:
  explicit ScopedWriteTimer(const char* label) : label_(label), start_(micros()) {}
  ScopedWriteTimer(const ScopedWriteTimer&) = delete;
  ScopedWriteTimer& operator=(const ScopedWriteTimer&) = delete;

  ~ScopedWriteTimer() {
    LOG_INF("WTIME", "%s %s check=%luus total=%luus", label_, skipped_ ? "skip" : "write", checkUs_,
            micros() - start_);
  }

  void markChecked(const bool skipped) {
    checkUs_ = micros() - start_;
    skipped_ = skipped;
  }

 private:
  const char* label_;
  unsigned long start_;
  unsigned long checkUs_ = 0;
  bool skipped_ = false;
};
