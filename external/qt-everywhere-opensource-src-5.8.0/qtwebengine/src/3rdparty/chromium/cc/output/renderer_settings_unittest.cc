// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/renderer_settings.h"

#include "cc/proto/renderer_settings.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

void VerifySerializeAndDeserializeProto(const RendererSettings& settings1) {
  proto::RendererSettings proto;
  settings1.ToProtobuf(&proto);
  RendererSettings settings2;
  settings2.FromProtobuf(proto);
  EXPECT_EQ(settings1, settings2);
}

TEST(RendererSettingsTest, AllFieldsFlipped) {
  RendererSettings settings;
  settings.allow_antialiasing = false;
  settings.force_antialiasing = true;
  settings.force_blending_with_shaders = true;
  settings.partial_swap_enabled = true;
  settings.finish_rendering_on_resize = true;
  settings.should_clear_root_render_pass = false;
  settings.disable_display_vsync = true;
  settings.release_overlay_resources_after_gpu_query = true;
  settings.refresh_rate = 6.0;
  settings.highp_threshold_min = 1;
  settings.texture_id_allocation_chunk_size = 46;
  settings.use_gpu_memory_buffer_resources = true;
  settings.preferred_tile_format = RGBA_4444;
  VerifySerializeAndDeserializeProto(settings);
}

TEST(RendererSettingsTest, ArbitraryFieldValues) {
  RendererSettings settings;
  settings.allow_antialiasing = false;
  settings.force_antialiasing = true;
  settings.force_blending_with_shaders = false;
  settings.partial_swap_enabled = true;
  settings.finish_rendering_on_resize = false;
  settings.should_clear_root_render_pass = false;
  settings.disable_display_vsync = true;
  settings.release_overlay_resources_after_gpu_query = true;
  settings.refresh_rate = 999.0;
  settings.highp_threshold_min = 1;
  settings.texture_id_allocation_chunk_size = 12;
  settings.use_gpu_memory_buffer_resources = true;
  settings.preferred_tile_format = RGBA_4444;
  VerifySerializeAndDeserializeProto(settings);
}

}  // namespace
}  // namespace cc
