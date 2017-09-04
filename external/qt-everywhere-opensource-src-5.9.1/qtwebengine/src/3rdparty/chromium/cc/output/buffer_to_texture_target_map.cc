// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "cc/output/buffer_to_texture_target_map.h"

#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "gpu/GLES2/gl2extchromium.h"

namespace cc {

BufferToTextureTargetMap StringToBufferToTextureTargetMap(
    const std::string& str) {
  BufferToTextureTargetMap map;
  std::vector<std::string> entries =
      base::SplitString(str, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (const auto& entry : entries) {
    std::vector<std::string> fields = base::SplitString(
        entry, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    CHECK_EQ(fields.size(), 3u);
    uint32_t usage = 0;
    uint32_t format = 0;
    uint32_t target = 0;
    bool succeeded = base::StringToUint(fields[0], &usage) &&
                     base::StringToUint(fields[1], &format) &&
                     base::StringToUint(fields[2], &target);
    CHECK(succeeded);
    CHECK_LE(usage, static_cast<uint32_t>(gfx::BufferUsage::LAST));
    CHECK_LE(format, static_cast<uint32_t>(gfx::BufferFormat::LAST));
    map.insert(BufferToTextureTargetMap::value_type(
        BufferToTextureTargetKey(static_cast<gfx::BufferUsage>(usage),
                                 static_cast<gfx::BufferFormat>(format)),
        target));
  }
  return map;
}

std::string BufferToTextureTargetMapToString(
    const BufferToTextureTargetMap& map) {
  std::string str;
  for (const auto& entry : map) {
    if (!str.empty())
      str += ";";
    str += base::UintToString(static_cast<uint32_t>(entry.first.first));
    str += ",";
    str += base::UintToString(static_cast<uint32_t>(entry.first.second));
    str += ",";
    str += base::UintToString(entry.second);
  }
  return str;
}

BufferToTextureTargetMap DefaultBufferToTextureTargetMapForTesting() {
  BufferToTextureTargetMap image_targets;
  for (int usage_idx = 0; usage_idx <= static_cast<int>(gfx::BufferUsage::LAST);
       ++usage_idx) {
    gfx::BufferUsage usage = static_cast<gfx::BufferUsage>(usage_idx);
    for (int format_idx = 0;
         format_idx <= static_cast<int>(gfx::BufferFormat::LAST);
         ++format_idx) {
      gfx::BufferFormat format = static_cast<gfx::BufferFormat>(format_idx);
      image_targets.insert(BufferToTextureTargetMap::value_type(
          BufferToTextureTargetKey(usage, format), GL_TEXTURE_2D));
    }
  }

  return image_targets;
}

}  // namespace cc
