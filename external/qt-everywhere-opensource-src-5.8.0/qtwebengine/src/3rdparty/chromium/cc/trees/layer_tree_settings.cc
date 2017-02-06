// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_settings.h"

#include "cc/proto/gfx_conversions.h"
#include "cc/proto/layer_tree_settings.pb.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace cc {

namespace {

proto::LayerTreeSettings_ScrollbarAnimator
LayerTreeSettingsScrollbarAnimatorToProto(
    const LayerTreeSettings::ScrollbarAnimator& animator) {
  switch (animator) {
    case LayerTreeSettings::ScrollbarAnimator::NO_ANIMATOR:
      return proto::LayerTreeSettings_ScrollbarAnimator_NO_ANIMATOR;
    case LayerTreeSettings::ScrollbarAnimator::LINEAR_FADE:
      return proto::LayerTreeSettings_ScrollbarAnimator_LINEAR_FADE;
    case LayerTreeSettings::ScrollbarAnimator::THINNING:
      return proto::LayerTreeSettings_ScrollbarAnimator_THINNING;
  }
  NOTREACHED() << "proto::LayerTreeSettings_ScrollbarAnimator_UNKNOWN";
  return proto::LayerTreeSettings_ScrollbarAnimator_UNKNOWN;
}

LayerTreeSettings::ScrollbarAnimator
LayerTreeSettingsScrollbarAnimatorFromProto(
    const proto::LayerTreeSettings_ScrollbarAnimator& animator) {
  switch (animator) {
    case proto::LayerTreeSettings_ScrollbarAnimator_NO_ANIMATOR:
      return LayerTreeSettings::ScrollbarAnimator::NO_ANIMATOR;
    case proto::LayerTreeSettings_ScrollbarAnimator_LINEAR_FADE:
      return LayerTreeSettings::ScrollbarAnimator::LINEAR_FADE;
    case proto::LayerTreeSettings_ScrollbarAnimator_THINNING:
      return LayerTreeSettings::ScrollbarAnimator::THINNING;
    case proto::LayerTreeSettings_ScrollbarAnimator_UNKNOWN:
      NOTREACHED() << "proto::LayerTreeSettings_ScrollbarAnimator_UNKNOWN";
      return LayerTreeSettings::ScrollbarAnimator::NO_ANIMATOR;
  }
  return LayerTreeSettings::ScrollbarAnimator::NO_ANIMATOR;
}

}  // namespace

LayerTreeSettings::LayerTreeSettings()
    : default_tile_size(gfx::Size(256, 256)),
      max_untiled_layer_size(gfx::Size(512, 512)),
      minimum_occlusion_tracking_size(gfx::Size(160, 160)),
      use_image_texture_targets(
          static_cast<size_t>(gfx::BufferFormat::LAST) + 1,
          GL_TEXTURE_2D),
      memory_policy_(64 * 1024 * 1024,
                     gpu::MemoryAllocation::CUTOFF_ALLOW_EVERYTHING,
                     ManagedMemoryPolicy::kDefaultNumResourcesLimit) {}

LayerTreeSettings::LayerTreeSettings(const LayerTreeSettings& other) = default;
LayerTreeSettings::~LayerTreeSettings() = default;

bool LayerTreeSettings::operator==(const LayerTreeSettings& other) const {
  return renderer_settings == other.renderer_settings &&
         single_thread_proxy_scheduler == other.single_thread_proxy_scheduler &&
         use_external_begin_frame_source ==
             other.use_external_begin_frame_source &&
         main_frame_before_activation_enabled ==
             other.main_frame_before_activation_enabled &&
         using_synchronous_renderer_compositor ==
             other.using_synchronous_renderer_compositor &&
         can_use_lcd_text == other.can_use_lcd_text &&
         use_distance_field_text == other.use_distance_field_text &&
         gpu_rasterization_enabled == other.gpu_rasterization_enabled &&
         gpu_rasterization_forced == other.gpu_rasterization_forced &&
         async_worker_context_enabled == other.async_worker_context_enabled &&
         gpu_rasterization_msaa_sample_count ==
             other.gpu_rasterization_msaa_sample_count &&
         create_low_res_tiling == other.create_low_res_tiling &&
         scrollbar_animator == other.scrollbar_animator &&
         scrollbar_fade_delay_ms == other.scrollbar_fade_delay_ms &&
         scrollbar_fade_resize_delay_ms ==
             other.scrollbar_fade_resize_delay_ms &&
         scrollbar_fade_duration_ms == other.scrollbar_fade_duration_ms &&
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
         use_image_texture_targets == other.use_image_texture_targets &&
         ignore_root_layer_flings == other.ignore_root_layer_flings &&
         scheduled_raster_task_limit == other.scheduled_raster_task_limit &&
         use_occlusion_for_tile_prioritization ==
             other.use_occlusion_for_tile_prioritization &&
         verify_clip_tree_calculations == other.verify_clip_tree_calculations &&
         image_decode_tasks_enabled == other.image_decode_tasks_enabled &&
         wait_for_beginframe_interval == other.wait_for_beginframe_interval &&
         max_staging_buffer_usage_in_bytes ==
             other.max_staging_buffer_usage_in_bytes &&
         memory_policy_ == other.memory_policy_ &&
         LayerTreeDebugState::Equal(initial_debug_state,
                                    other.initial_debug_state) &&
         use_cached_picture_raster == other.use_cached_picture_raster;
}

void LayerTreeSettings::ToProtobuf(proto::LayerTreeSettings* proto) const {
  renderer_settings.ToProtobuf(proto->mutable_renderer_settings());
  proto->set_single_thread_proxy_scheduler(single_thread_proxy_scheduler);
  proto->set_use_external_begin_frame_source(use_external_begin_frame_source);
  proto->set_main_frame_before_activation_enabled(
      main_frame_before_activation_enabled);
  proto->set_using_synchronous_renderer_compositor(
      using_synchronous_renderer_compositor);
  proto->set_can_use_lcd_text(can_use_lcd_text);
  proto->set_use_distance_field_text(use_distance_field_text);
  proto->set_gpu_rasterization_enabled(gpu_rasterization_enabled);
  proto->set_gpu_rasterization_forced(gpu_rasterization_forced);
  proto->set_async_worker_context_enabled(async_worker_context_enabled);
  proto->set_gpu_rasterization_msaa_sample_count(
      gpu_rasterization_msaa_sample_count);
  proto->set_create_low_res_tiling(create_low_res_tiling);
  proto->set_scrollbar_animator(
      LayerTreeSettingsScrollbarAnimatorToProto(scrollbar_animator));
  proto->set_scrollbar_fade_delay_ms(scrollbar_fade_delay_ms);
  proto->set_scrollbar_fade_resize_delay_ms(scrollbar_fade_resize_delay_ms);
  proto->set_scrollbar_fade_duration_ms(scrollbar_fade_duration_ms);
  proto->set_solid_color_scrollbar_color(solid_color_scrollbar_color);
  proto->set_timeout_and_draw_when_animation_checkerboards(
      timeout_and_draw_when_animation_checkerboards);
  proto->set_layer_transforms_should_scale_layer_contents(
      layer_transforms_should_scale_layer_contents);
  proto->set_layers_always_allowed_lcd_text(layers_always_allowed_lcd_text);
  proto->set_minimum_contents_scale(minimum_contents_scale);
  proto->set_low_res_contents_scale_factor(low_res_contents_scale_factor);
  proto->set_top_controls_show_threshold(top_controls_show_threshold);
  proto->set_top_controls_hide_threshold(top_controls_hide_threshold);
  proto->set_background_animation_rate(background_animation_rate);
  SizeToProto(default_tile_size, proto->mutable_default_tile_size());
  SizeToProto(max_untiled_layer_size, proto->mutable_max_untiled_layer_size());
  SizeToProto(minimum_occlusion_tracking_size,
              proto->mutable_minimum_occlusion_tracking_size());
  proto->set_tiling_interest_area_padding(tiling_interest_area_padding);
  proto->set_skewport_target_time_in_seconds(skewport_target_time_in_seconds);
  proto->set_skewport_extrapolation_limit_in_screen_pixels(
      skewport_extrapolation_limit_in_screen_pixels);
  proto->set_max_memory_for_prepaint_percentage(
      max_memory_for_prepaint_percentage);
  proto->set_use_zero_copy(use_zero_copy);
  proto->set_use_partial_raster(use_partial_raster);
  proto->set_enable_elastic_overscroll(enable_elastic_overscroll);
  proto->set_ignore_root_layer_flings(ignore_root_layer_flings);
  proto->set_scheduled_raster_task_limit(scheduled_raster_task_limit);
  proto->set_use_occlusion_for_tile_prioritization(
      use_occlusion_for_tile_prioritization);
  proto->set_image_decode_tasks_enabled(image_decode_tasks_enabled);
  proto->set_wait_for_beginframe_interval(wait_for_beginframe_interval);
  proto->set_max_staging_buffer_usage_in_bytes(
      max_staging_buffer_usage_in_bytes);
  memory_policy_.ToProtobuf(proto->mutable_memory_policy());
  initial_debug_state.ToProtobuf(proto->mutable_initial_debug_state());
  proto->set_use_cached_picture_raster(use_cached_picture_raster);

  for (unsigned u : use_image_texture_targets)
    proto->add_use_image_texture_targets(u);
}

void LayerTreeSettings::FromProtobuf(const proto::LayerTreeSettings& proto) {
  renderer_settings.FromProtobuf(proto.renderer_settings());
  single_thread_proxy_scheduler = proto.single_thread_proxy_scheduler();
  use_external_begin_frame_source = proto.use_external_begin_frame_source();
  main_frame_before_activation_enabled =
      proto.main_frame_before_activation_enabled();
  using_synchronous_renderer_compositor =
      proto.using_synchronous_renderer_compositor();
  can_use_lcd_text = proto.can_use_lcd_text();
  use_distance_field_text = proto.use_distance_field_text();
  gpu_rasterization_enabled = proto.gpu_rasterization_enabled();
  gpu_rasterization_forced = proto.gpu_rasterization_forced();
  async_worker_context_enabled = proto.async_worker_context_enabled();
  gpu_rasterization_msaa_sample_count =
      proto.gpu_rasterization_msaa_sample_count();
  create_low_res_tiling = proto.create_low_res_tiling();
  scrollbar_animator =
      LayerTreeSettingsScrollbarAnimatorFromProto(proto.scrollbar_animator());
  scrollbar_fade_delay_ms = proto.scrollbar_fade_delay_ms();
  scrollbar_fade_resize_delay_ms = proto.scrollbar_fade_resize_delay_ms();
  scrollbar_fade_duration_ms = proto.scrollbar_fade_duration_ms();
  solid_color_scrollbar_color = proto.solid_color_scrollbar_color();
  timeout_and_draw_when_animation_checkerboards =
      proto.timeout_and_draw_when_animation_checkerboards();
  layer_transforms_should_scale_layer_contents =
      proto.layer_transforms_should_scale_layer_contents();
  layers_always_allowed_lcd_text = proto.layers_always_allowed_lcd_text();
  minimum_contents_scale = proto.minimum_contents_scale();
  low_res_contents_scale_factor = proto.low_res_contents_scale_factor();
  top_controls_show_threshold = proto.top_controls_show_threshold();
  top_controls_hide_threshold = proto.top_controls_hide_threshold();
  background_animation_rate = proto.background_animation_rate();
  default_tile_size = ProtoToSize(proto.default_tile_size());
  max_untiled_layer_size = ProtoToSize(proto.max_untiled_layer_size());
  minimum_occlusion_tracking_size =
      ProtoToSize(proto.minimum_occlusion_tracking_size());
  tiling_interest_area_padding = proto.tiling_interest_area_padding();
  skewport_target_time_in_seconds = proto.skewport_target_time_in_seconds();
  skewport_extrapolation_limit_in_screen_pixels =
      proto.skewport_extrapolation_limit_in_screen_pixels();
  max_memory_for_prepaint_percentage =
      proto.max_memory_for_prepaint_percentage();
  use_zero_copy = proto.use_zero_copy();
  use_partial_raster = proto.use_partial_raster();
  enable_elastic_overscroll = proto.enable_elastic_overscroll();
  // |use_image_texture_targets| contains default values, so clear first.
  use_image_texture_targets.clear();
  ignore_root_layer_flings = proto.ignore_root_layer_flings();
  scheduled_raster_task_limit = proto.scheduled_raster_task_limit();
  use_occlusion_for_tile_prioritization =
      proto.use_occlusion_for_tile_prioritization();
  image_decode_tasks_enabled = proto.image_decode_tasks_enabled();
  wait_for_beginframe_interval = proto.wait_for_beginframe_interval();
  max_staging_buffer_usage_in_bytes = proto.max_staging_buffer_usage_in_bytes();
  memory_policy_.FromProtobuf(proto.memory_policy());
  initial_debug_state.FromProtobuf(proto.initial_debug_state());
  use_cached_picture_raster = proto.use_cached_picture_raster();

  for (int i = 0; i < proto.use_image_texture_targets_size(); ++i)
    use_image_texture_targets.push_back(proto.use_image_texture_targets(i));
}

SchedulerSettings LayerTreeSettings::ToSchedulerSettings() const {
  SchedulerSettings scheduler_settings;
  scheduler_settings.use_external_begin_frame_source =
      use_external_begin_frame_source;
  scheduler_settings.main_frame_before_activation_enabled =
      main_frame_before_activation_enabled;
  scheduler_settings.timeout_and_draw_when_animation_checkerboards =
      timeout_and_draw_when_animation_checkerboards;
  scheduler_settings.using_synchronous_renderer_compositor =
      using_synchronous_renderer_compositor;
  scheduler_settings.throttle_frame_production = wait_for_beginframe_interval;
  scheduler_settings.background_frame_interval =
      base::TimeDelta::FromSecondsD(1.0 / background_animation_rate);
  scheduler_settings.abort_commit_before_output_surface_creation =
      abort_commit_before_output_surface_creation;
  return scheduler_settings;
}

}  // namespace cc
