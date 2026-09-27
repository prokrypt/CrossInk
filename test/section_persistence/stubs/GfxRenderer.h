#pragma once

#include <cstdint>

class FontCacheManager;

class GfxRenderer {
 public:
  enum Orientation { Portrait, LandscapeClockwise, PortraitInverted, LandscapeCounterClockwise };

  Orientation getOrientation() const { return Portrait; }
  FontCacheManager* getFontCacheManager() const { return nullptr; }
  bool isSdCardFont(int) const { return false; }
  bool releaseSdCardFontForLowMemory(int, bool = false) { return false; }
};
