#pragma once

#include <algorithm>

namespace SwipeAdjustment {

// A recognized short swipe changes one 5-point step; a full-axis swipe can
// cover the entire 0-100 range. Both one- and two-finger gestures use this scale.
inline int amount(const int distance, const int axisSize) {
  if (axisSize < 2) return 0;
  const int minimumDistance = std::max(60, axisSize * 6 / 100);
  if (distance < minimumDistance) return 0;
  const int maximumDistance = axisSize - 1;
  const int travel = std::max(1, maximumDistance - minimumDistance);
  return 5 * (1 + (std::min(distance, maximumDistance) - minimumDistance) * 19 / travel);
}

// Edge slides adjust the frontlight in 1-point steps: the first step lands as
// soon as the slide is recognized, then every ~0.4% of the axis adds one more,
// so 40% of the axis (320 px on an 800 px edge) covers the full 0-100 range.
inline int edgeAmount(const int distance, const int axisSize) {
  if (axisSize < 2) return 0;
  const int minimumDistance = std::max(60, axisSize * 6 / 100);
  if (distance < minimumDistance) return 0;
  const int fullRangeTravel = std::max(1, axisSize * 40 / 100);
  return std::min(100, 1 + (distance - minimumDistance) * 100 / fullRangeTravel);
}

inline int targetValue(const int initialValue, const bool increase, const int adjustment) {
  return std::clamp(initialValue + (increase ? adjustment : -adjustment), 0, 100);
}

}  // namespace SwipeAdjustment
