// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/renderer_settings.h"

#include <limits>

#include "base/logging.h"
#include "cc/proto/renderer_settings.pb.h"
#include "cc/resources/platform_color.h"

namespace cc {

RendererSettings::RendererSettings()
    : allow_antialiasing(true),
      force_antialiasing(false),
      force_blending_with_shaders(false),
      partial_swap_enabled(false),
      finish_rendering_on_resize(false),
      should_clear_root_render_pass(true),
      disable_display_vsync(false),
      release_overlay_resources_after_gpu_query(false),
      refresh_rate(60.0),
      highp_threshold_min(0),
      texture_id_allocation_chunk_size(64),
      use_gpu_memory_buffer_resources(false),
      preferred_tile_format(PlatformColor::BestTextureFormat()) {}

RendererSettings::RendererSettings(const RendererSettings& other) = default;

RendererSettings::~RendererSettings() {
}

void RendererSettings::ToProtobuf(proto::RendererSettings* proto) const {
  proto->set_allow_antialiasing(allow_antialiasing);
  proto->set_force_antialiasing(force_antialiasing);
  proto->set_force_blending_with_shaders(force_blending_with_shaders);
  proto->set_partial_swap_enabled(partial_swap_enabled);
  proto->set_finish_rendering_on_resize(finish_rendering_on_resize);
  proto->set_should_clear_root_render_pass(should_clear_root_render_pass);
  proto->set_disable_display_vsync(disable_display_vsync);
  proto->set_release_overlay_resources_after_gpu_query(
      release_overlay_resources_after_gpu_query);
  proto->set_refresh_rate(refresh_rate);
  proto->set_highp_threshold_min(highp_threshold_min);
  proto->set_texture_id_allocation_chunk_size(texture_id_allocation_chunk_size);
  proto->set_use_gpu_memory_buffer_resources(use_gpu_memory_buffer_resources);
  proto->set_preferred_tile_format(preferred_tile_format);
}

void RendererSettings::FromProtobuf(const proto::RendererSettings& proto) {
  allow_antialiasing = proto.allow_antialiasing();
  force_antialiasing = proto.force_antialiasing();
  force_blending_with_shaders = proto.force_blending_with_shaders();
  partial_swap_enabled = proto.partial_swap_enabled();
  finish_rendering_on_resize = proto.finish_rendering_on_resize();
  should_clear_root_render_pass = proto.should_clear_root_render_pass();
  disable_display_vsync = proto.disable_display_vsync();
  release_overlay_resources_after_gpu_query =
      proto.release_overlay_resources_after_gpu_query();
  refresh_rate = proto.refresh_rate();
  highp_threshold_min = proto.highp_threshold_min();
  texture_id_allocation_chunk_size = proto.texture_id_allocation_chunk_size();
  use_gpu_memory_buffer_resources = proto.use_gpu_memory_buffer_resources();

  DCHECK_LE(proto.preferred_tile_format(),
            static_cast<uint32_t>(RESOURCE_FORMAT_MAX));
  preferred_tile_format =
      static_cast<ResourceFormat>(proto.preferred_tile_format());
}

bool RendererSettings::operator==(const RendererSettings& other) const {
  return allow_antialiasing == other.allow_antialiasing &&
         force_antialiasing == other.force_antialiasing &&
         force_blending_with_shaders == other.force_blending_with_shaders &&
         partial_swap_enabled == other.partial_swap_enabled &&
         finish_rendering_on_resize == other.finish_rendering_on_resize &&
         should_clear_root_render_pass == other.should_clear_root_render_pass &&
         disable_display_vsync == other.disable_display_vsync &&
         release_overlay_resources_after_gpu_query ==
             other.release_overlay_resources_after_gpu_query &&
         refresh_rate == other.refresh_rate &&
         highp_threshold_min == other.highp_threshold_min &&
         texture_id_allocation_chunk_size ==
             other.texture_id_allocation_chunk_size &&
         use_gpu_memory_buffer_resources ==
             other.use_gpu_memory_buffer_resources &&
         preferred_tile_format == other.preferred_tile_format;
}

}  // namespace cc
