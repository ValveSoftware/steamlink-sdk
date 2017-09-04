// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/trace_event_argument.h"
#include "cc/base/math_util.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/proto/gfx_conversions.h"
#include "cc/trees/element_id.h"
#include "cc/trees/scroll_node.h"

namespace cc {

ScrollNode::ScrollNode()
    : id(-1),
      parent_id(-1),
      owner_id(-1),
      scrollable(false),
      main_thread_scrolling_reasons(
          MainThreadScrollingReason::kNotScrollingOnMain),
      contains_non_fast_scrollable_region(false),
      max_scroll_offset_affected_by_page_scale(false),
      is_inner_viewport_scroll_layer(false),
      is_outer_viewport_scroll_layer(false),
      should_flatten(false),
      user_scrollable_horizontal(false),
      user_scrollable_vertical(false),
      transform_id(0) {}

ScrollNode::ScrollNode(const ScrollNode& other) = default;

bool ScrollNode::operator==(const ScrollNode& other) const {
  return id == other.id && parent_id == other.parent_id &&
         owner_id == other.owner_id && scrollable == other.scrollable &&
         main_thread_scrolling_reasons == other.main_thread_scrolling_reasons &&
         contains_non_fast_scrollable_region ==
             other.contains_non_fast_scrollable_region &&
         scroll_clip_layer_bounds == other.scroll_clip_layer_bounds &&
         bounds == other.bounds &&
         max_scroll_offset_affected_by_page_scale ==
             other.max_scroll_offset_affected_by_page_scale &&
         is_inner_viewport_scroll_layer ==
             other.is_inner_viewport_scroll_layer &&
         is_outer_viewport_scroll_layer ==
             other.is_outer_viewport_scroll_layer &&
         offset_to_transform_parent == other.offset_to_transform_parent &&
         should_flatten == other.should_flatten &&
         user_scrollable_horizontal == other.user_scrollable_horizontal &&
         user_scrollable_vertical == other.user_scrollable_vertical &&
         element_id == other.element_id && transform_id == other.transform_id;
}

void ScrollNode::AsValueInto(base::trace_event::TracedValue* value) const {
  value->SetInteger("id", id);
  value->SetInteger("parent_id", parent_id);
  value->SetInteger("owner_id", owner_id);
  value->SetBoolean("scrollable", scrollable);
  MathUtil::AddToTracedValue("scroll_clip_layer_bounds",
                             scroll_clip_layer_bounds, value);
  MathUtil::AddToTracedValue("bounds", bounds, value);
  MathUtil::AddToTracedValue("offset_to_transform_parent",
                             offset_to_transform_parent, value);
  value->SetBoolean("should_flatten", should_flatten);
  value->SetBoolean("user_scrollable_horizontal", user_scrollable_horizontal);
  value->SetBoolean("user_scrollable_vertical", user_scrollable_vertical);

  element_id.AddToTracedValue(value);
  value->SetInteger("transform_id", transform_id);
}

}  // namespace cc
