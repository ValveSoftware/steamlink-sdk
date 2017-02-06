// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "cc/input/layer_selection_bound.h"
#include "cc/proto/gfx_conversions.h"
#include "cc/proto/layer_selection_bound.pb.h"

namespace cc {
namespace {

proto::LayerSelectionBound::SelectionBoundType
LayerSelectionBoundTypeToProtobuf(const gfx::SelectionBound::Type& type) {
  switch (type) {
    case gfx::SelectionBound::LEFT:
      return proto::LayerSelectionBound_SelectionBoundType_LEFT;
    case gfx::SelectionBound::RIGHT:
      return proto::LayerSelectionBound_SelectionBoundType_RIGHT;
    case gfx::SelectionBound::CENTER:
      return proto::LayerSelectionBound_SelectionBoundType_CENTER;
    case gfx::SelectionBound::EMPTY:
      return proto::LayerSelectionBound_SelectionBoundType_EMPTY;
  }
  NOTREACHED() << "proto::LayerSelectionBound_SelectionBoundType_UNKNOWN";
  return proto::LayerSelectionBound_SelectionBoundType_UNKNOWN;
}

gfx::SelectionBound::Type LayerSelectionBoundTypeFromProtobuf(
    const proto::LayerSelectionBound::SelectionBoundType& type) {
  switch (type) {
    case proto::LayerSelectionBound_SelectionBoundType_LEFT:
      return gfx::SelectionBound::LEFT;
    case proto::LayerSelectionBound_SelectionBoundType_RIGHT:
      return gfx::SelectionBound::RIGHT;
    case proto::LayerSelectionBound_SelectionBoundType_CENTER:
      return gfx::SelectionBound::CENTER;
    case proto::LayerSelectionBound_SelectionBoundType_EMPTY:
      return gfx::SelectionBound::EMPTY;
    case proto::LayerSelectionBound_SelectionBoundType_UNKNOWN:
      NOTREACHED() << "proto::LayerSelectionBound_SelectionBoundType_UNKNOWN";
      return gfx::SelectionBound::EMPTY;
  }
  return gfx::SelectionBound::EMPTY;
}

}  // namespace

LayerSelectionBound::LayerSelectionBound()
    : type(gfx::SelectionBound::EMPTY), layer_id(0) {}

LayerSelectionBound::~LayerSelectionBound() {
}

bool LayerSelectionBound::operator==(const LayerSelectionBound& other) const {
  return type == other.type && layer_id == other.layer_id &&
         edge_top == other.edge_top && edge_bottom == other.edge_bottom;
}

bool LayerSelectionBound::operator!=(const LayerSelectionBound& other) const {
  return !(*this == other);
}

void LayerSelectionBound::ToProtobuf(proto::LayerSelectionBound* proto) const {
  proto->set_type(LayerSelectionBoundTypeToProtobuf(type));
  PointToProto(edge_top, proto->mutable_edge_top());
  PointToProto(edge_bottom, proto->mutable_edge_bottom());
  proto->set_layer_id(layer_id);
}

void LayerSelectionBound::FromProtobuf(
    const proto::LayerSelectionBound& proto) {
  type = LayerSelectionBoundTypeFromProtobuf(proto.type());
  edge_top = ProtoToPoint(proto.edge_top());
  edge_bottom = ProtoToPoint(proto.edge_bottom());
  layer_id = proto.layer_id();
}

void LayerSelectionToProtobuf(const LayerSelection& selection,
                              proto::LayerSelection* proto) {
  selection.start.ToProtobuf(proto->mutable_start());
  selection.end.ToProtobuf(proto->mutable_end());
  proto->set_is_editable(selection.is_editable);
  proto->set_is_empty_text_form_control(selection.is_empty_text_form_control);
}

void LayerSelectionFromProtobuf(LayerSelection* selection,
                                const proto::LayerSelection& proto) {
  selection->start.FromProtobuf(proto.start());
  selection->end.FromProtobuf(proto.end());
  selection->is_editable = proto.is_editable();
  selection->is_empty_text_form_control = proto.is_empty_text_form_control();
}

}  // namespace cc
