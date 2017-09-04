// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_CLIP_NODE_H_
#define CC_TREES_CLIP_NODE_H_

#include "cc/base/cc_export.h"
#include "ui/gfx/geometry/rect_f.h"

namespace base {
namespace trace_event {
class TracedValue;
}  // namespace trace_event
}  // namespace base

namespace cc {

struct CC_EXPORT ClipNode {
  ClipNode();
  ClipNode(const ClipNode& other);

  int id;
  int parent_id;
  int owner_id;

  enum class ClipType {
    // The node doesn't contribute a new clip. It exists only for caching clips
    // or for resetting clipping state.
    NONE,

    // The node contributes a new clip (that is, |clip| needs to be applied).
    APPLIES_LOCAL_CLIP
  };

  ClipType clip_type;

  // The clip rect that this node contributes, expressed in the space of its
  // transform node.
  gfx::RectF clip;

  // Clip nodes are used for two reasons. First, they are used for determining
  // which parts of each layer are visible. Second, they are used for
  // determining whether a clip needs to be applied when drawing a layer, and if
  // so, the rect that needs to be used. These can be different since not all
  // clips need to be applied directly to each layer. For example, a layer is
  // implicitly clipped by the bounds of its target render surface and by clips
  // applied to this surface. |combined_clip_in_target_space| is used for
  // computing visible rects, and |clip_in_target_space| is used for computing
  // clips applied at draw time. Both rects are expressed in the space of the
  // target transform node, and may include clips contributed by ancestors.
  gfx::RectF combined_clip_in_target_space;
  gfx::RectF clip_in_target_space;

  // The id of the transform node that defines the clip node's local space.
  int transform_id;

  // The id of the transform node that defines the clip node's target space.
  int target_transform_id;

  // The id of the effect node that defines the clip node's target space.
  int target_effect_id;

  // When true, |clip_in_target_space| does not include clips from ancestor
  // nodes.
  bool layer_clipping_uses_only_local_clip : 1;

  // True if target surface needs to be drawn with a clip applied.
  bool target_is_clipped : 1;

  // True if layers with this clip tree node need to be drawn with a clip
  // applied.
  bool layers_are_clipped : 1;
  bool layers_are_clipped_when_surfaces_disabled : 1;

  // Nodes that correspond to unclipped surfaces disregard ancestor clips.
  bool resets_clip : 1;

  bool operator==(const ClipNode& other) const;

  void AsValueInto(base::trace_event::TracedValue* value) const;
};

}  // namespace cc

#endif  // CC_TREES_CLIP_NODE_H_
