// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/color_lut_cache.h"

#include <stdint.h>
#include <cmath>
#include <vector>

#include "gpu/command_buffer/client/gles2_interface.h"
#include "ui/gfx/color_transform.h"

// After a LUT has not been used for this many frames, we release it.
const uint32_t kMaxFramesUnused = 10;

ColorLUTCache::ColorLUTCache(gpu::gles2::GLES2Interface* gl)
    : lut_cache_(0), gl_(gl) {}

ColorLUTCache::~ColorLUTCache() {
  GLuint textures[10];
  size_t n = 0;
  for (const auto& cache_entry : lut_cache_) {
    textures[n++] = cache_entry.second.texture;
    if (n == arraysize(textures)) {
      gl_->DeleteTextures(n, textures);
      n = 0;
    }
  }
  if (n)
    gl_->DeleteTextures(n, textures);
}

namespace {

unsigned char FloatToLUT(float f) {
  return std::min<int>(255, std::max<int>(0, floorf(f * 255.0f + 0.5f)));
}
};

unsigned int ColorLUTCache::MakeLUT(const gfx::ColorSpace& from,
                                    gfx::ColorSpace to,
                                    int lut_samples) {
  if (to == gfx::ColorSpace()) {
    to = gfx::ColorSpace::CreateSRGB();
  }
  std::unique_ptr<gfx::ColorTransform> transform(
      gfx::ColorTransform::NewColorTransform(
          from, to, gfx::ColorTransform::Intent::INTENT_PERCEPTUAL));

  int lut_entries = lut_samples * lut_samples * lut_samples;
  float inverse = 1.0f / (lut_samples - 1);
  std::vector<unsigned char> lut(lut_entries * 4);
  std::vector<gfx::ColorTransform::TriStim> samples(lut_samples);
  unsigned char* lutp = lut.data();
  for (int y = 0; y < lut_samples; y++) {
    for (int v = 0; v < lut_samples; v++) {
      for (int u = 0; u < lut_samples; u++) {
        samples[u].set_x(y * inverse);
        samples[u].set_y(u * inverse);
        samples[u].set_z(v * inverse);
      }
      transform->transform(samples.data(), samples.size());
      for (int u = 0; u < lut_samples; u++) {
        *(lutp++) = FloatToLUT(samples[u].x());
        *(lutp++) = FloatToLUT(samples[u].y());
        *(lutp++) = FloatToLUT(samples[u].z());
        *(lutp++) = 255;  // alpha
      }
    }
  }

  unsigned int lut_texture;
  gl_->GenTextures(1, &lut_texture);
  gl_->BindTexture(GL_TEXTURE_2D, lut_texture);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gl_->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, lut_samples,
                  lut_samples * lut_samples, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                  lut.data());
  return lut_texture;
}

unsigned int ColorLUTCache::GetLUT(const gfx::ColorSpace& from,
                                   const gfx::ColorSpace& to,
                                   int lut_samples) {
  CacheKey key(from, std::make_pair(to, lut_samples));
  auto iter = lut_cache_.Get(key);
  if (iter != lut_cache_.end()) {
    iter->second.last_used_frame = current_frame_;
    return iter->second.texture;
  }

  unsigned int lut = MakeLUT(from, to, lut_samples);
  lut_cache_.Put(key, CacheVal(lut, current_frame_));
  return lut;
}

void ColorLUTCache::Swap() {
  current_frame_++;
  while (!lut_cache_.empty() &&
         current_frame_ - lut_cache_.rbegin()->second.last_used_frame >
             kMaxFramesUnused) {
    gl_->DeleteTextures(1, &lut_cache_.rbegin()->second.texture);
    lut_cache_.ShrinkToSize(lut_cache_.size() - 1);
  }
}
