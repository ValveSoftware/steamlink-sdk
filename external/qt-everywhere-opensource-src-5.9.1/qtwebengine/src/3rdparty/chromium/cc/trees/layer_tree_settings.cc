// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_settings.h"

#include "cc/proto/gfx_conversions.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace cc {

LayerTreeSettings::LayerTreeSettings()
    : default_tile_size(gfx::Size(256, 256)),
      max_untiled_layer_size(gfx::Size(512, 512)),
      minimum_occlusion_tracking_size(gfx::Size(160, 160)),
      gpu_memory_policy(64 * 1024 * 1024,
                        gpu::MemoryAllocation::CUTOFF_ALLOW_EVERYTHING,
                        ManagedMemoryPolicy::kDefaultNumResourcesLimit),
      software_memory_policy(128 * 1024 * 1024,
                             gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE,
                             ManagedMemoryPolicy::kDefaultNumResourcesLimit) {}

LayerTreeSettings::LayerTreeSettings(const LayerTreeSettings& other) = default;
LayerTreeSettings::~LayerTreeSettings() = default;

bool LayerTreeSettings::operator==(const LayerTreeSettings& other) const {
  return renderer_settings == other.renderer_settings &&
         single_thread_proxy_scheduler == other.single_thread_proxy_scheduler &&
         main_frame_before_activation_enabled ==
             other.main_frame_before_activation_enabled &&
         using_synchronous_renderer_compositor ==
             other.using_synchronous_renderer_compositor &&
         enable_latency_recovery == other.enable_latency_recovery &&
         can_use_lcd_text == other.can_use_lcd_text &&
         use_distance_field_text == other.use_distance_field_text &&
         gpu_rasterization_enabled == other.gpu_rasterization_enabled &&
         gpu_rasterization_forced == other.gpu_rasterization_forced &&
         async_worker_context_enabled == other.async_worker_context_enabled &&
         gpu_rasterization_msaa_sample_count ==
             other.gpu_rasterization_msaa_sample_count &&
         create_low_res_tiling == other.create_low_res_tiling &&
         scrollbar_animator == other.scrollbar_animator &&
         scrollbar_fade_delay == other.scrollbar_fade_delay &&
         scrollbar_fade_resize_delay == other.scrollbar_fade_resize_delay &&
         scrollbar_fade_duration == other.scrollbar_fade_duration &&
         solid_color_scrollbar_color == other.solid_color_scrollbar_color &&
         timeout_and_draw_when_animation_checkerboards ==
             other.timeout_and_draw_when_animation_checkerboards &&
         layer_transforms_should_scale_layer_contents ==
             other.layer_transforms_should_scale_layer_contents &&
         layers_always_allowed_lcd_text ==
             other.layers_always_allowed_lcd_text &&
         minimum_contents_scale == other.minimum_contents_scale &&
         low_res_contents_scale_factor == other.low_res_contents_scale_factor &&
         top_controls_show_threshold == other.top_controls_show_threshold &&
         top_controls_hide_threshold == other.top_controls_hide_threshold &&
         background_animation_rate == other.background_animation_rate &&
         default_tile_size == other.default_tile_size &&
         max_untiled_layer_size == other.max_untiled_layer_size &&
         minimum_occlusion_tracking_size ==
             other.minimum_occlusion_tracking_size &&
         tiling_interest_area_padding == other.tiling_interest_area_padding &&
         skewport_target_time_in_seconds ==
             other.skewport_target_time_in_seconds &&
         skewport_extrapolation_limit_in_screen_pixels ==
             other.skewport_extrapolation_limit_in_screen_pixels &&
         max_memory_for_prepaint_percentage ==
             other.max_memory_for_prepaint_percentage &&
         use_zero_copy == other.use_zero_copy &&
         use_partial_raster == other.use_partial_raster &&
         enable_elastic_overscroll == other.enable_elastic_overscroll &&
         ignore_root_layer_flings == other.ignore_root_layer_flings &&
         scheduled_raster_task_limit == other.scheduled_raster_task_limit &&
         use_occlusion_for_tile_prioritization ==
             other.use_occlusion_for_tile_prioritization &&
         verify_clip_tree_calculations == other.verify_clip_tree_calculations &&
         image_decode_tasks_enabled == other.image_decode_tasks_enabled &&
         max_staging_buffer_usage_in_bytes ==
             other.max_staging_buffer_usage_in_bytes &&
         gpu_memory_policy == other.gpu_memory_policy &&
         software_memory_policy == other.software_memory_policy &&
         LayerTreeDebugState::Equal(initial_debug_state,
                                    other.initial_debug_state) &&
         use_cached_picture_raster == other.use_cached_picture_raster;
}

SchedulerSettings LayerTreeSettings::ToSchedulerSettings() const {
  SchedulerSettings scheduler_settings;
  scheduler_settings.main_frame_before_activation_enabled =
      main_frame_before_activation_enabled;
  scheduler_settings.timeout_and_draw_when_animation_checkerboards =
      timeout_and_draw_when_animation_checkerboards;
  scheduler_settings.using_synchronous_renderer_compositor =
      using_synchronous_renderer_compositor;
  scheduler_settings.enable_latency_recovery = enable_latency_recovery;
  scheduler_settings.background_frame_interval =
      base::TimeDelta::FromSecondsD(1.0 / background_animation_rate);
  scheduler_settings.abort_commit_before_compositor_frame_sink_creation =
      abort_commit_before_compositor_frame_sink_creation;
  return scheduler_settings;
}

}  // namespace cc
