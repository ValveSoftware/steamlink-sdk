// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/layer_selection_bound.h"

#include "cc/proto/layer_selection_bound.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"

namespace cc {
namespace {

void VerifySerializeAndDeserializeProto(const LayerSelectionBound& bound1) {
  proto::LayerSelectionBound proto;
  bound1.ToProtobuf(&proto);
  LayerSelectionBound bound2;
  bound2.FromProtobuf(proto);
  EXPECT_EQ(bound1, bound2);
}

void VerifySerializeAndDeserializeLayerSelectionProto(
    const LayerSelection& selection1) {
  proto::LayerSelection proto;
  LayerSelectionToProtobuf(selection1, &proto);
  LayerSelection selection2;
  LayerSelectionFromProtobuf(&selection2, proto);
  EXPECT_EQ(selection1, selection2);
}

TEST(LayerSelectionBoundTest, AllTypePermutations) {
  LayerSelectionBound bound;
  bound.type = gfx::SelectionBound::LEFT;
  bound.edge_top = gfx::Point(3, 14);
  bound.edge_bottom = gfx::Point(6, 28);
  bound.layer_id = 42;

  VerifySerializeAndDeserializeProto(bound);
  bound.type = gfx::SelectionBound::RIGHT;
  VerifySerializeAndDeserializeProto(bound);
  bound.type = gfx::SelectionBound::CENTER;
  VerifySerializeAndDeserializeProto(bound);
  bound.type = gfx::SelectionBound::EMPTY;
  VerifySerializeAndDeserializeProto(bound);
}

TEST(LayerSelectionTest, AllSelectionPermutations) {
  LayerSelectionBound start;
  start.type = gfx::SelectionBound::LEFT;
  start.edge_top = gfx::Point(3, 14);
  start.edge_bottom = gfx::Point(6, 28);
  start.layer_id = 42;

  LayerSelectionBound end;
  end.type = gfx::SelectionBound::RIGHT;
  end.edge_top = gfx::Point(14, 3);
  end.edge_bottom = gfx::Point(28, 6);
  end.layer_id = 24;

  LayerSelection selection;
  selection.start = start;
  selection.end = end;
  selection.is_editable = true;
  selection.is_empty_text_form_control = true;

  VerifySerializeAndDeserializeLayerSelectionProto(selection);
  selection.is_empty_text_form_control = false;
  VerifySerializeAndDeserializeLayerSelectionProto(selection);
  selection.is_editable = false;
  VerifySerializeAndDeserializeLayerSelectionProto(selection);
  selection.is_empty_text_form_control = true;
  VerifySerializeAndDeserializeLayerSelectionProto(selection);
}

}  // namespace
}  // namespace cc
