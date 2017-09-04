// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/trace_event_argument.h"
#include "cc/proto/gfx_conversions.h"
#include "cc/proto/skia_conversions.h"
#include "cc/trees/effect_node.h"

namespace cc {

EffectNode::EffectNode()
    : id(-1),
      parent_id(-1),
      owner_id(-1),
      opacity(1.f),
      screen_space_opacity(1.f),
      blend_mode(SkXfermode::kSrcOver_Mode),
      has_render_surface(false),
      render_surface(nullptr),
      has_copy_request(false),
      hidden_by_backface_visibility(false),
      double_sided(false),
      is_drawn(true),
      subtree_hidden(false),
      has_potential_filter_animation(false),
      has_potential_opacity_animation(false),
      is_currently_animating_filter(false),
      is_currently_animating_opacity(false),
      effect_changed(false),
      num_copy_requests_in_subtree(0),
      has_unclipped_descendants(false),
      transform_id(0),
      clip_id(0),
      target_id(1),
      mask_layer_id(-1) {}

EffectNode::EffectNode(const EffectNode& other) = default;

bool EffectNode::operator==(const EffectNode& other) const {
  return id == other.id && parent_id == other.parent_id &&
         owner_id == other.owner_id && opacity == other.opacity &&
         screen_space_opacity == other.screen_space_opacity &&
         has_render_surface == other.has_render_surface &&
         has_copy_request == other.has_copy_request &&
         filters == other.filters &&
         background_filters == other.background_filters &&
         filters_origin == other.filters_origin &&
         blend_mode == other.blend_mode &&
         surface_contents_scale == other.surface_contents_scale &&
         unscaled_mask_target_size == other.unscaled_mask_target_size &&
         hidden_by_backface_visibility == other.hidden_by_backface_visibility &&
         double_sided == other.double_sided && is_drawn == other.is_drawn &&
         subtree_hidden == other.subtree_hidden &&
         has_potential_filter_animation ==
             other.has_potential_filter_animation &&
         has_potential_opacity_animation ==
             other.has_potential_opacity_animation &&
         is_currently_animating_filter == other.is_currently_animating_filter &&
         is_currently_animating_opacity ==
             other.is_currently_animating_opacity &&
         effect_changed == other.effect_changed &&
         num_copy_requests_in_subtree == other.num_copy_requests_in_subtree &&
         transform_id == other.transform_id && clip_id == other.clip_id &&
         target_id == other.target_id && mask_layer_id == other.mask_layer_id;
}

void EffectNode::AsValueInto(base::trace_event::TracedValue* value) const {
  value->SetInteger("id", id);
  value->SetInteger("parent_id", parent_id);
  value->SetInteger("owner_id", owner_id);
  value->SetDouble("opacity", opacity);
  value->SetBoolean("has_render_surface", has_render_surface);
  value->SetBoolean("has_copy_request", has_copy_request);
  value->SetBoolean("double_sided", double_sided);
  value->SetBoolean("is_drawn", is_drawn);
  value->SetBoolean("has_potential_filter_animation",
                    has_potential_filter_animation);
  value->SetBoolean("has_potential_opacity_animation",
                    has_potential_opacity_animation);
  value->SetBoolean("effect_changed", effect_changed);
  value->SetInteger("num_copy_requests_in_subtree",
                    num_copy_requests_in_subtree);
  value->SetInteger("transform_id", transform_id);
  value->SetInteger("clip_id", clip_id);
  value->SetInteger("target_id", target_id);
  value->SetInteger("mask_layer_id", mask_layer_id);
}

}  // namespace cc
