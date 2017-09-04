// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/renderer_settings.h"

#include <limits>

#include "base/logging.h"
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
      gl_composited_texture_quad_border(false),
      refresh_rate(60.0),
      highp_threshold_min(0),
      texture_id_allocation_chunk_size(64),
      use_gpu_memory_buffer_resources(false),
      preferred_tile_format(PlatformColor::BestTextureFormat()) {}

RendererSettings::RendererSettings(const RendererSettings& other) = default;

RendererSettings::~RendererSettings() {
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
         preferred_tile_format == other.preferred_tile_format &&
         buffer_to_texture_target_map == other.buffer_to_texture_target_map;
}

}  // namespace cc
