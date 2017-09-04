// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer_position_constraint.h"
#include "cc/proto/layer_position_constraint.pb.h"

namespace cc {

LayerPositionConstraint::LayerPositionConstraint()
    : is_fixed_position_(false),
      is_fixed_to_right_edge_(false),
      is_fixed_to_bottom_edge_(false) {
}

void LayerPositionConstraint::ToProtobuf(
    proto::LayerPositionConstraint* proto) const {
  proto->set_is_fixed_position(is_fixed_position_);
  proto->set_is_fixed_to_right_edge(is_fixed_to_right_edge_);
  proto->set_is_fixed_to_bottom_edge(is_fixed_to_bottom_edge_);
}

void LayerPositionConstraint::FromProtobuf(
    const proto::LayerPositionConstraint& proto) {
  is_fixed_position_ = proto.is_fixed_position();
  is_fixed_to_right_edge_ = proto.is_fixed_to_right_edge();
  is_fixed_to_bottom_edge_ = proto.is_fixed_to_bottom_edge();
}

bool LayerPositionConstraint::operator==(
    const LayerPositionConstraint& other) const {
  if (!is_fixed_position_ && !other.is_fixed_position_)
    return true;
  return is_fixed_position_ == other.is_fixed_position_ &&
          is_fixed_to_right_edge_ == other.is_fixed_to_right_edge_ &&
          is_fixed_to_bottom_edge_ == other.is_fixed_to_bottom_edge_;
}

bool LayerPositionConstraint::operator!=(
    const LayerPositionConstraint& other) const {
  return !(*this == other);
}

}  // namespace cc
