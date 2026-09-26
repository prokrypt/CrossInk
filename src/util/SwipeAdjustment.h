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

// Edge slides adjust the frontlight in 1-point steps at half the sensitivity
// of amount(): the first step lands as soon as the slide is recognized and a
// full-axis slide covers about half of the 0-100 range.
inline int edgeAmount(const int distance, const int axisSize) {
  if (axisSize < 2) return 0;
  const int minimumDistance = std::max(60, axisSize * 6 / 100);
  if (distance < minimumDistance) return 0;
  const int maximumDistance = axisSize - 1;
  const int travel = std::max(1, maximumDistance - minimumDistance);
  return 1 + (std::min(distance, maximumDistance) - minimumDistance) * 99 / (2 * travel);
}

// Live light swipes keep tracking the finger after it reverses: displacement is
// signed along the swipe's starting direction, so moving back past the
// touch-down point adjusts the other way on the same scale.
inline int signedAmount(const int displacement, const int axisSize) {
  return displacement < 0 ? -amount(-displacement, axisSize) : amount(displacement, axisSize);
}

inline int signedEdgeAmount(const int displacement, const int axisSize) {
  return displacement < 0 ? -edgeAmount(-displacement, axisSize) : edgeAmount(displacement, axisSize);
}

inline int targetValue(const int initialValue, const bool increase, const int adjustment) {
  return std::clamp(initialValue + (increase ? adjustment : -adjustment), 0, 100);
}

}  // namespace SwipeAdjustment
