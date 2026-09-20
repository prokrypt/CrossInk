#pragma once

#include <FontCacheManager.h>

#include <cstdint>

class GfxRenderer {
 public:
  bool isSdCardFont(int) const { return false; }
  bool releaseSdCardFontForLowMemory(int, bool = false) { return false; }
  FontCacheManager* getFontCacheManager() { return &fonts_; }

 private:
  FontCacheManager fonts_;
};
