// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_COLOR_LUT_CACHE_H_
#define CC_OUTPUT_COLOR_LUT_CACHE_H_

#include <map>

#include "base/containers/mru_cache.h"
#include "base/macros.h"
#include "ui/gfx/color_space.h"

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

class ColorLUTCache {
 public:
  explicit ColorLUTCache(gpu::gles2::GLES2Interface* gl);
  ~ColorLUTCache();

  unsigned int GetLUT(const gfx::ColorSpace& from,
                      const gfx::ColorSpace& to,
                      int lut_samples);

  // End of frame, assume all LUTs handed out are no longer used.
  void Swap();

 private:
  unsigned int MakeLUT(const gfx::ColorSpace& from,
                       gfx::ColorSpace to,
                       int lut_samples);

  typedef std::pair<gfx::ColorSpace, std::pair<gfx::ColorSpace, size_t>>
      CacheKey;

  struct CacheVal {
    CacheVal(unsigned int texture, uint32_t last_used_frame)
        : texture(texture), last_used_frame(last_used_frame) {}
    unsigned int texture;
    uint32_t last_used_frame;
  };

  base::MRUCache<CacheKey, CacheVal> lut_cache_;
  uint32_t current_frame_;
  gpu::gles2::GLES2Interface* gl_;
  DISALLOW_COPY_AND_ASSIGN(ColorLUTCache);
};

#endif  // CC_OUTPUT_COLOR_LUT_CACHE_H_
