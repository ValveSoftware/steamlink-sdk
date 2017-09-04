// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_EFFECT_NODE_H_
#define CC_TREES_EFFECT_NODE_H_

#include "cc/base/cc_export.h"
#include "cc/output/filter_operations.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size_f.h"

namespace base {
namespace trace_event {
class TracedValue;
}  // namespace trace_event
}  // namespace base

namespace cc {

class RenderSurfaceImpl;

struct CC_EXPORT EffectNode {
  EffectNode();
  EffectNode(const EffectNode& other);

  int id;
  int parent_id;
  int owner_id;

  float opacity;
  float screen_space_opacity;

  FilterOperations filters;
  FilterOperations background_filters;
  gfx::PointF filters_origin;

  SkXfermode::Mode blend_mode;

  gfx::Vector2dF surface_contents_scale;

  gfx::Size unscaled_mask_target_size;

  bool has_render_surface;
  RenderSurfaceImpl* render_surface;
  bool has_copy_request;
  bool hidden_by_backface_visibility;
  bool double_sided;
  bool is_drawn;
  // TODO(jaydasika) : Delete this after implementation of
  // SetHideLayerAndSubtree is cleaned up. (crbug.com/595843)
  bool subtree_hidden;
  bool has_potential_filter_animation;
  bool has_potential_opacity_animation;
  bool is_currently_animating_filter;
  bool is_currently_animating_opacity;
  // We need to track changes to effects on the compositor to compute damage
  // rect.
  bool effect_changed;
  int num_copy_requests_in_subtree;
  bool has_unclipped_descendants;
  int transform_id;
  int clip_id;
  // Effect node id of which this effect contributes to.
  int target_id;
  int mask_layer_id;

  bool operator==(const EffectNode& other) const;

  void AsValueInto(base::trace_event::TracedValue* value) const;
};

}  // namespace cc

#endif  // CC_TREES_EFFECT_NODE_H_
