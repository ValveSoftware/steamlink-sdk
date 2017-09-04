// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/buffer_to_texture_target_map.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_types.h"

namespace cc {
namespace {

// Ensures that a map populated with various values can be serialized to/from
// string successfully.
TEST(BufferToTextureTargetMapTest, SerializeRoundTrip) {
  BufferToTextureTargetMap test_map;
  uint32_t next_value = 0;
  for (int usage_idx = 0; usage_idx <= static_cast<int>(gfx::BufferUsage::LAST);
       ++usage_idx) {
    gfx::BufferUsage usage = static_cast<gfx::BufferUsage>(usage_idx);
    for (int format_idx = 0;
         format_idx <= static_cast<int>(gfx::BufferFormat::LAST);
         ++format_idx) {
      gfx::BufferFormat format = static_cast<gfx::BufferFormat>(format_idx);
      test_map.insert(BufferToTextureTargetMap::value_type(
          BufferToTextureTargetKey(usage, format), next_value++));
    }
  }

  std::string serialized_map = BufferToTextureTargetMapToString(test_map);
  BufferToTextureTargetMap deserialized_map =
      StringToBufferToTextureTargetMap(serialized_map);
  EXPECT_EQ(test_map, deserialized_map);
}

}  // namespace
}  // namespace cc
