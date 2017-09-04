// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer_sticky_position_constraint.h"
#include "cc/proto/gfx_conversions.h"
#include "cc/proto/layer_sticky_position_constraint.pb.h"

namespace cc {

LayerStickyPositionConstraint::LayerStickyPositionConstraint()
    : is_sticky(false),
      is_anchored_left(false),
      is_anchored_right(false),
      is_anchored_top(false),
      is_anchored_bottom(false),
      left_offset(0.f),
      right_offset(0.f),
      top_offset(0.f),
      bottom_offset(0.f) {}

LayerStickyPositionConstraint::LayerStickyPositionConstraint(
    const LayerStickyPositionConstraint& other)
    : is_sticky(other.is_sticky),
      is_anchored_left(other.is_anchored_left),
      is_anchored_right(other.is_anchored_right),
      is_anchored_top(other.is_anchored_top),
      is_anchored_bottom(other.is_anchored_bottom),
      left_offset(other.left_offset),
      right_offset(other.right_offset),
      top_offset(other.top_offset),
      bottom_offset(other.bottom_offset),
      parent_relative_sticky_box_offset(
          other.parent_relative_sticky_box_offset),
      scroll_container_relative_sticky_box_rect(
          other.scroll_container_relative_sticky_box_rect),
      scroll_container_relative_containing_block_rect(
          other.scroll_container_relative_containing_block_rect) {}

void LayerStickyPositionConstraint::ToProtobuf(
    proto::LayerStickyPositionConstraint* proto) const {
  proto->set_is_sticky(is_sticky);
  proto->set_is_anchored_left(is_anchored_left);
  proto->set_is_anchored_right(is_anchored_right);
  proto->set_is_anchored_top(is_anchored_top);
  proto->set_is_anchored_bottom(is_anchored_bottom);
  proto->set_left_offset(left_offset);
  proto->set_right_offset(right_offset);
  proto->set_top_offset(top_offset);
  proto->set_bottom_offset(bottom_offset);
  PointToProto(parent_relative_sticky_box_offset,
               proto->mutable_parent_relative_sticky_box_offset());
  RectToProto(scroll_container_relative_sticky_box_rect,
              proto->mutable_scroll_container_relative_sticky_box_rect());
  RectToProto(scroll_container_relative_containing_block_rect,
              proto->mutable_scroll_container_relative_containing_block_rect());
}

void LayerStickyPositionConstraint::FromProtobuf(
    const proto::LayerStickyPositionConstraint& proto) {
  is_sticky = proto.is_sticky();
  is_anchored_left = proto.is_anchored_left();
  is_anchored_right = proto.is_anchored_right();
  is_anchored_top = proto.is_anchored_top();
  is_anchored_bottom = proto.is_anchored_bottom();
  left_offset = proto.left_offset();
  right_offset = proto.right_offset();
  top_offset = proto.top_offset();
  bottom_offset = proto.bottom_offset();
  parent_relative_sticky_box_offset =
      ProtoToPoint(proto.parent_relative_sticky_box_offset());
  scroll_container_relative_sticky_box_rect =
      ProtoToRect(proto.scroll_container_relative_sticky_box_rect());
  scroll_container_relative_containing_block_rect =
      ProtoToRect(proto.scroll_container_relative_containing_block_rect());
}

bool LayerStickyPositionConstraint::operator==(
    const LayerStickyPositionConstraint& other) const {
  if (!is_sticky && !other.is_sticky)
    return true;
  return is_sticky == other.is_sticky &&
         is_anchored_left == other.is_anchored_left &&
         is_anchored_right == other.is_anchored_right &&
         is_anchored_top == other.is_anchored_top &&
         is_anchored_bottom == other.is_anchored_bottom &&
         left_offset == other.left_offset &&
         right_offset == other.right_offset && top_offset == other.top_offset &&
         bottom_offset == other.bottom_offset &&
         parent_relative_sticky_box_offset ==
             other.parent_relative_sticky_box_offset &&
         scroll_container_relative_sticky_box_rect ==
             other.scroll_container_relative_sticky_box_rect &&
         scroll_container_relative_containing_block_rect ==
             other.scroll_container_relative_containing_block_rect;
}

bool LayerStickyPositionConstraint::operator!=(
    const LayerStickyPositionConstraint& other) const {
  return !(*this == other);
}

}  // namespace cc
