// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_settings.h"

#include "cc/proto/layer_tree_settings.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

void VerifySerializeAndDeserialize(const LayerTreeSettings& settings1) {
  proto::LayerTreeSettings proto;
  settings1.ToProtobuf(&proto);
  LayerTreeSettings settings2;
  settings2.FromProtobuf(proto);
  EXPECT_EQ(settings1, settings2);
}

TEST(LayerTreeSettingsTest, DefaultValues) {
  LayerTreeSettings settings;
  VerifySerializeAndDeserialize(settings);
}

TEST(LayerTreeSettingsTest, AllMembersChanged) {
  LayerTreeSettings settings;
  RendererSettings rs;
  rs.texture_id_allocation_chunk_size =
      rs.texture_id_allocation_chunk_size * 3 + 1;
  settings.renderer_settings = rs;
  settings.single_thread_proxy_scheduler =
      !settings.single_thread_proxy_scheduler;
  settings.use_external_begin_frame_source =
      !settings.use_external_begin_frame_source;
  settings.main_frame_before_activation_enabled =
      !settings.main_frame_before_activation_enabled;
  settings.using_synchronous_renderer_compositor =
      !settings.using_synchronous_renderer_compositor;
  settings.can_use_lcd_text = !settings.can_use_lcd_text;
  settings.use_distance_field_text = !settings.use_distance_field_text;
  settings.gpu_rasterization_enabled = !settings.gpu_rasterization_enabled;
  settings.gpu_rasterization_forced = !settings.gpu_rasterization_forced;
  settings.gpu_rasterization_msaa_sample_count =
      settings.gpu_rasterization_msaa_sample_count * 3 + 1;
  settings.gpu_rasterization_skewport_target_time_in_seconds =
      settings.gpu_rasterization_skewport_target_time_in_seconds * 3 + 1;
  settings.create_low_res_tiling = !settings.create_low_res_tiling;
  settings.scrollbar_animator = LayerTreeSettings::THINNING;
  settings.scrollbar_fade_delay_ms = settings.scrollbar_fade_delay_ms * 3 + 1;
  settings.scrollbar_fade_resize_delay_ms =
      settings.scrollbar_fade_resize_delay_ms * 3 + 1;
  settings.scrollbar_fade_duration_ms =
      settings.scrollbar_fade_duration_ms * 3 + 1;
  settings.solid_color_scrollbar_color = SK_ColorCYAN;
  settings.timeout_and_draw_when_animation_checkerboards =
      !settings.timeout_and_draw_when_animation_checkerboards;
  settings.layer_transforms_should_scale_layer_contents =
      !settings.layer_transforms_should_scale_layer_contents;
  settings.layers_always_allowed_lcd_text =
      !settings.layers_always_allowed_lcd_text;
  settings.minimum_contents_scale = 0.42f;
  settings.low_res_contents_scale_factor = 0.43f;
  settings.top_controls_show_threshold = 0.44f;
  settings.top_controls_hide_threshold = 0.45f;
  settings.background_animation_rate = 0.46f;
  settings.default_tile_size = gfx::Size(47, 48);
  settings.max_untiled_layer_size = gfx::Size(49, 50);
  settings.minimum_occlusion_tracking_size = gfx::Size(51, 52);
  settings.tiling_interest_area_padding =
      settings.tiling_interest_area_padding * 3 + 1;
  settings.skewport_target_time_in_seconds = 0.53f;
  settings.skewport_extrapolation_limit_in_screen_pixels =
      settings.skewport_extrapolation_limit_in_screen_pixels * 3 + 1;
  settings.max_memory_for_prepaint_percentage =
      settings.max_memory_for_prepaint_percentage * 3 + 1;
  settings.use_zero_copy = !settings.use_zero_copy;
  settings.use_partial_raster = !settings.use_partial_raster;
  settings.enable_elastic_overscroll = !settings.enable_elastic_overscroll;
  settings.use_image_texture_targets.push_back(54);
  settings.use_image_texture_targets.push_back(55);
  settings.ignore_root_layer_flings = !settings.ignore_root_layer_flings;
  settings.scheduled_raster_task_limit =
      settings.scheduled_raster_task_limit * 3 + 1;
  settings.use_occlusion_for_tile_prioritization =
      !settings.use_occlusion_for_tile_prioritization;
  settings.wait_for_beginframe_interval =
      !settings.wait_for_beginframe_interval;
  settings.max_staging_buffer_usage_in_bytes =
      settings.max_staging_buffer_usage_in_bytes * 3 + 1;
  settings.memory_policy_ = ManagedMemoryPolicy(
      56, gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE, 57);
  LayerTreeDebugState debug_state;
  debug_state.show_fps_counter = !debug_state.show_fps_counter;
  settings.initial_debug_state = debug_state;
  VerifySerializeAndDeserialize(settings);
}

TEST(LayerTreeSettingsTest, ArbitraryValues) {
  LayerTreeSettings settings;
  RendererSettings rs;
  rs.texture_id_allocation_chunk_size = 42;
  settings.renderer_settings = rs;
  settings.single_thread_proxy_scheduler = true;
  settings.use_external_begin_frame_source = true;
  settings.main_frame_before_activation_enabled = true;
  settings.using_synchronous_renderer_compositor = false;
  settings.can_use_lcd_text = false;
  settings.use_distance_field_text = false;
  settings.gpu_rasterization_enabled = false;
  settings.gpu_rasterization_forced = true;
  settings.gpu_rasterization_msaa_sample_count = 10;
  settings.gpu_rasterization_skewport_target_time_in_seconds = 0.909f;
  settings.create_low_res_tiling = false;
  settings.scrollbar_animator = LayerTreeSettings::LINEAR_FADE;
  settings.scrollbar_fade_delay_ms = 13;
  settings.scrollbar_fade_resize_delay_ms = 61;
  settings.scrollbar_fade_duration_ms = 23;
  settings.solid_color_scrollbar_color = SK_ColorCYAN;
  settings.timeout_and_draw_when_animation_checkerboards = true;
  settings.layer_transforms_should_scale_layer_contents = true;
  settings.layers_always_allowed_lcd_text = true;
  settings.minimum_contents_scale = 0.314f;
  settings.low_res_contents_scale_factor = 0.49f;
  settings.top_controls_hide_threshold = 0.666f;
  settings.top_controls_hide_threshold = 0.51f;
  settings.background_animation_rate = 0.52f;
  settings.default_tile_size = gfx::Size(53, 54);
  settings.max_untiled_layer_size = gfx::Size(55, 56);
  settings.minimum_occlusion_tracking_size = gfx::Size(57, 58);
  settings.tiling_interest_area_padding = 59;
  settings.skewport_target_time_in_seconds = 0.6f;
  settings.skewport_extrapolation_limit_in_screen_pixels = 61;
  settings.max_memory_for_prepaint_percentage = 62;
  settings.use_zero_copy = true;
  settings.use_partial_raster = true;
  settings.enable_elastic_overscroll = false;
  settings.use_image_texture_targets.push_back(10);
  settings.use_image_texture_targets.push_back(19);
  settings.ignore_root_layer_flings = true;
  settings.scheduled_raster_task_limit = 41;
  settings.use_occlusion_for_tile_prioritization = true;
  settings.wait_for_beginframe_interval = true;
  settings.max_staging_buffer_usage_in_bytes = 70;
  settings.memory_policy_ = ManagedMemoryPolicy(
      71, gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE, 77);
  LayerTreeDebugState debug_state;
  debug_state.show_fps_counter = true;
  settings.initial_debug_state = debug_state;
  VerifySerializeAndDeserialize(settings);
}

}  // namespace
}  // namespace cc
