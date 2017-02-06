// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug/layer_tree_debug_state.h"

#include "cc/proto/layer_tree_debug_state.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

void VerifySerializeAndDeserializeProto(const LayerTreeDebugState& state1) {
  proto::LayerTreeDebugState proto;
  state1.ToProtobuf(&proto);
  LayerTreeDebugState state2;
  state2.FromProtobuf(proto);
  EXPECT_TRUE(LayerTreeDebugState::Equal(state1, state2));
}

TEST(LayerTreeDebugStateTest, AllFieldsTrue) {
  LayerTreeDebugState state;
  state.show_fps_counter = true;
  state.show_debug_borders = true;
  state.show_paint_rects = true;
  state.show_property_changed_rects = true;
  state.show_surface_damage_rects = true;
  state.show_screen_space_rects = true;
  state.show_replica_screen_space_rects = true;
  state.show_touch_event_handler_rects = true;
  state.show_wheel_event_handler_rects = true;
  state.show_scroll_event_handler_rects = true;
  state.show_non_fast_scrollable_rects = true;
  state.show_layer_animation_bounds_rects = true;
  state.slow_down_raster_scale_factor = 1;
  state.rasterize_only_visible_content = true;
  state.show_picture_borders = true;
  state.SetRecordRenderingStats(true);
  VerifySerializeAndDeserializeProto(state);
}

TEST(LayerTreeDebugStateTest, ArbitraryFieldValues) {
  LayerTreeDebugState state;
  state.show_fps_counter = true;
  state.show_debug_borders = true;
  state.show_paint_rects = false;
  state.show_property_changed_rects = true;
  state.show_surface_damage_rects = false;
  state.show_screen_space_rects = false;
  state.show_replica_screen_space_rects = true;
  state.show_touch_event_handler_rects = true;
  state.show_wheel_event_handler_rects = true;
  state.show_scroll_event_handler_rects = false;
  state.show_non_fast_scrollable_rects = true;
  state.show_layer_animation_bounds_rects = true;
  state.slow_down_raster_scale_factor = 42;
  state.rasterize_only_visible_content = false;
  state.show_picture_borders = false;
  state.SetRecordRenderingStats(true);
  VerifySerializeAndDeserializeProto(state);
}

}  // namespace
}  // namespace cc
