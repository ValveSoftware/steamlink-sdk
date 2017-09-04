// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_SCROLL_NODE_H_
#define CC_TREES_SCROLL_NODE_H_

#include "cc/base/cc_export.h"
#include "cc/output/filter_operations.h"
#include "ui/gfx/geometry/size.h"

namespace base {
namespace trace_event {
class TracedValue;
}  // namespace trace_event
}  // namespace base

namespace cc {

struct CC_EXPORT ScrollNode {
  ScrollNode();
  ScrollNode(const ScrollNode& other);

  int id;
  int parent_id;

  // The layer id that corresponds to the layer contents that are scrolled.
  // Unlike |id|, this id is stable across frames that don't change the
  // composited layer list.
  int owner_id;

  // This is used for subtrees that should not be scrolled independently. For
  // example, when there is a layer that is not scrollable itself but is inside
  // a scrolling layer.
  bool scrollable;

  uint32_t main_thread_scrolling_reasons;
  bool contains_non_fast_scrollable_region;

  // Size of the clipped area, not including non-overlay scrollbars. Overlay
  // scrollbars do not affect the clipped area.
  gfx::Size scroll_clip_layer_bounds;

  // Bounds of the overflow scrolling area.
  gfx::Size bounds;

  bool max_scroll_offset_affected_by_page_scale;
  bool is_inner_viewport_scroll_layer;
  bool is_outer_viewport_scroll_layer;

  // This offset is used when |scrollable| is false and there isn't a transform
  // node already present that covers this offset.
  gfx::Vector2dF offset_to_transform_parent;

  bool should_flatten;
  bool user_scrollable_horizontal;
  bool user_scrollable_vertical;
  ElementId element_id;
  int transform_id;

  bool operator==(const ScrollNode& other) const;
  void AsValueInto(base::trace_event::TracedValue* value) const;
};

}  // namespace cc

#endif  // CC_TREES_SCROLL_NODE_H_
