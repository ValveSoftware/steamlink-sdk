// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/trace_event_argument.h"
#include "cc/base/math_util.h"
#include "cc/proto/cc_conversions.h"
#include "cc/proto/gfx_conversions.h"
#include "cc/trees/clip_node.h"

namespace cc {

ClipNode::ClipNode()
    : id(-1),
      parent_id(-1),
      owner_id(-1),
      clip_type(ClipType::NONE),
      transform_id(-1),
      target_transform_id(-1),
      target_effect_id(-1),
      layer_clipping_uses_only_local_clip(false),
      target_is_clipped(false),
      layers_are_clipped(false),
      layers_are_clipped_when_surfaces_disabled(false),
      resets_clip(false) {}

ClipNode::ClipNode(const ClipNode& other) = default;

bool ClipNode::operator==(const ClipNode& other) const {
  return id == other.id && parent_id == other.parent_id &&
         owner_id == other.owner_id && clip_type == other.clip_type &&
         clip == other.clip &&
         combined_clip_in_target_space == other.combined_clip_in_target_space &&
         clip_in_target_space == other.clip_in_target_space &&
         transform_id == other.transform_id &&
         target_transform_id == other.target_transform_id &&
         target_effect_id == other.target_effect_id &&
         layer_clipping_uses_only_local_clip ==
             other.layer_clipping_uses_only_local_clip &&
         target_is_clipped == other.target_is_clipped &&
         layers_are_clipped == other.layers_are_clipped &&
         layers_are_clipped_when_surfaces_disabled ==
             other.layers_are_clipped_when_surfaces_disabled &&
         resets_clip == other.resets_clip;
}

void ClipNode::AsValueInto(base::trace_event::TracedValue* value) const {
  value->SetInteger("id", id);
  value->SetInteger("parent_id", parent_id);
  value->SetInteger("owner_id", owner_id);
  value->SetInteger("clip_type", static_cast<int>(clip_type));
  MathUtil::AddToTracedValue("clip", clip, value);
  value->SetInteger("transform_id", transform_id);
  value->SetInteger("target_transform_id", target_transform_id);
  value->SetInteger("target_effect_id", target_effect_id);
  value->SetBoolean("layer_clipping_uses_only_local_clip",
                    layer_clipping_uses_only_local_clip);
  value->SetBoolean("target_is_clipped", target_is_clipped);
  value->SetBoolean("layers_are_clipped", layers_are_clipped);
  value->SetBoolean("layers_are_clipped_when_surfaces_disabled",
                    layers_are_clipped_when_surfaces_disabled);
  value->SetBoolean("resets_clip", resets_clip);
}

}  // namespace cc
