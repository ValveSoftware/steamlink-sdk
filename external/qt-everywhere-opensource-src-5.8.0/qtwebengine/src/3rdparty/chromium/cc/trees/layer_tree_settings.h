// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_SETTINGS_H_
#define CC_TREES_LAYER_TREE_SETTINGS_H_

#include <stddef.h>

#include <vector>

#include "cc/base/cc_export.h"
#include "cc/debug/layer_tree_debug_state.h"
#include "cc/output/managed_memory_policy.h"
#include "cc/output/renderer_settings.h"
#include "cc/scheduler/scheduler_settings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

namespace proto {
class LayerTreeSettings;
}  // namespace proto

class CC_EXPORT LayerTreeSettings {
 public:
  LayerTreeSettings();
  LayerTreeSettings(const LayerTreeSettings& other);
  virtual ~LayerTreeSettings();

  bool operator==(const LayerTreeSettings& other) const;

  void ToProtobuf(proto::LayerTreeSettings* proto) const;
  void FromProtobuf(const proto::LayerTreeSettings& proto);

  SchedulerSettings ToSchedulerSettings() const;

  RendererSettings renderer_settings;
  bool single_thread_proxy_scheduler = true;
  // TODO(enne): Remove this after everything uses output surface begin frames.
  bool use_external_begin_frame_source = false;
  // TODO(enne): Temporary staging for unified begin frame source work.
  bool use_output_surface_begin_frame_source = false;
  bool main_frame_before_activation_enabled = false;
  bool using_synchronous_renderer_compositor = false;
  bool can_use_lcd_text = true;
  bool use_distance_field_text = false;
  bool gpu_rasterization_enabled = false;
  bool gpu_rasterization_forced = false;
  bool async_worker_context_enabled = false;
  int gpu_rasterization_msaa_sample_count = 0;
  float gpu_rasterization_skewport_target_time_in_seconds = 0.2f;
  bool create_low_res_tiling = false;

  enum ScrollbarAnimator {
    NO_ANIMATOR,
    LINEAR_FADE,
    THINNING,
  };
  ScrollbarAnimator scrollbar_animator = NO_ANIMATOR;
  int scrollbar_fade_delay_ms = 0;
  int scrollbar_fade_resize_delay_ms = 0;
  int scrollbar_fade_duration_ms = 0;
  SkColor solid_color_scrollbar_color = SK_ColorWHITE;
  bool timeout_and_draw_when_animation_checkerboards = true;
  bool layer_transforms_should_scale_layer_contents = false;
  bool layers_always_allowed_lcd_text = false;
  float minimum_contents_scale = 0.0625f;
  float low_res_contents_scale_factor = 0.25f;
  float top_controls_show_threshold = 0.5f;
  float top_controls_hide_threshold = 0.5f;
  double background_animation_rate = 1.0;
  gfx::Size default_tile_size;
  gfx::Size max_untiled_layer_size;
  gfx::Size minimum_occlusion_tracking_size;
  // 3000 pixels should give sufficient area for prepainting.
  // Note this value is specified with an ideal contents scale in mind. That
  // is, the ideal tiling would use this value as the padding.
  // TODO(vmpstr): Figure out a better number that doesn't depend on scale.
  int tiling_interest_area_padding = 3000;
  float skewport_target_time_in_seconds = 1.0f;
  int skewport_extrapolation_limit_in_screen_pixels = 2000;
  size_t max_memory_for_prepaint_percentage = 100;
  bool use_zero_copy = false;
  bool use_partial_raster = false;
  bool enable_elastic_overscroll = false;
  // An array of image texture targets for each GpuMemoryBuffer format.
  std::vector<unsigned> use_image_texture_targets;
  bool ignore_root_layer_flings = false;
  size_t scheduled_raster_task_limit = 32;
  bool use_occlusion_for_tile_prioritization = false;
  bool verify_clip_tree_calculations = false;
  bool image_decode_tasks_enabled = false;
  bool wait_for_beginframe_interval = true;
  bool abort_commit_before_output_surface_creation = true;
  bool use_layer_lists = false;
  int max_staging_buffer_usage_in_bytes = 32 * 1024 * 1024;
  ManagedMemoryPolicy memory_policy_;
  size_t gpu_decoded_image_budget_bytes = 96 * 1024 * 1024;
  size_t software_decoded_image_budget_bytes = 128 * 1024 * 1024;
  int max_preraster_distance_in_screen_pixels = 1000;

  // If set to true, the display item list will internally cache a SkPicture for
  // raster rather than directly using the display items.
  bool use_cached_picture_raster = true;

  LayerTreeDebugState initial_debug_state;
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_SETTINGS_H_
