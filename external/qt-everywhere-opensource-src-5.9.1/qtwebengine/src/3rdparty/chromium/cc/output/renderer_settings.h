// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_RENDERER_SETTINGS_H_
#define CC_OUTPUT_RENDERER_SETTINGS_H_

#include <stddef.h>

#include "cc/base/cc_export.h"
#include "cc/output/buffer_to_texture_target_map.h"
#include "cc/resources/resource_format.h"

namespace cc {

class CC_EXPORT RendererSettings {
 public:
  RendererSettings();
  RendererSettings(const RendererSettings& other);
  ~RendererSettings();

  bool allow_antialiasing;
  bool force_antialiasing;
  bool force_blending_with_shaders;
  bool partial_swap_enabled;
  bool finish_rendering_on_resize;
  bool should_clear_root_render_pass;
  bool disable_display_vsync;
  bool release_overlay_resources_after_gpu_query;
  bool gl_composited_texture_quad_border;

  double refresh_rate;
  int highp_threshold_min;
  size_t texture_id_allocation_chunk_size;
  bool use_gpu_memory_buffer_resources;
  ResourceFormat preferred_tile_format;
  BufferToTextureTargetMap buffer_to_texture_target_map;

  bool operator==(const RendererSettings& other) const;
};

}  // namespace cc

#endif  // CC_OUTPUT_RENDERER_SETTINGS_H_
