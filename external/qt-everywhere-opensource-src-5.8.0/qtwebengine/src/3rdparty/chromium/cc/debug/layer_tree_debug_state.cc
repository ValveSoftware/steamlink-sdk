// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug/layer_tree_debug_state.h"

#include "base/logging.h"
#include "cc/proto/layer_tree_debug_state.pb.h"

namespace cc {

// IMPORTANT: new fields must be added to Equal() and Unite()
LayerTreeDebugState::LayerTreeDebugState()
    : show_fps_counter(false),
      show_debug_borders(false),
      show_paint_rects(false),
      show_property_changed_rects(false),
      show_surface_damage_rects(false),
      show_screen_space_rects(false),
      show_replica_screen_space_rects(false),
      show_touch_event_handler_rects(false),
      show_wheel_event_handler_rects(false),
      show_scroll_event_handler_rects(false),
      show_non_fast_scrollable_rects(false),
      show_layer_animation_bounds_rects(false),
      slow_down_raster_scale_factor(0),
      rasterize_only_visible_content(false),
      show_picture_borders(false),
      record_rendering_stats_(false) {}

LayerTreeDebugState::LayerTreeDebugState(const LayerTreeDebugState& other) =
    default;

LayerTreeDebugState::~LayerTreeDebugState() {}

void LayerTreeDebugState::SetRecordRenderingStats(bool enabled) {
  record_rendering_stats_ = enabled;
}

bool LayerTreeDebugState::RecordRenderingStats() const {
  return record_rendering_stats_;
}

bool LayerTreeDebugState::ShowHudInfo() const {
  return show_fps_counter || ShowHudRects();
}

bool LayerTreeDebugState::ShowHudRects() const {
  return show_paint_rects || show_property_changed_rects ||
         show_surface_damage_rects || show_screen_space_rects ||
         show_replica_screen_space_rects || show_touch_event_handler_rects ||
         show_wheel_event_handler_rects || show_scroll_event_handler_rects ||
         show_non_fast_scrollable_rects || show_layer_animation_bounds_rects;
}

bool LayerTreeDebugState::ShowMemoryStats() const {
  return show_fps_counter;
}

void LayerTreeDebugState::ToProtobuf(proto::LayerTreeDebugState* proto) const {
  proto->set_show_fps_counter(show_fps_counter);
  proto->set_show_debug_borders(show_debug_borders);
  proto->set_show_paint_rects(show_paint_rects);
  proto->set_show_property_changed_rects(show_property_changed_rects);
  proto->set_show_surface_damage_rects(show_surface_damage_rects);
  proto->set_show_screen_space_rects(show_screen_space_rects);
  proto->set_show_replica_screen_space_rects(show_replica_screen_space_rects);
  proto->set_show_touch_event_handler_rects(show_touch_event_handler_rects);
  proto->set_show_wheel_event_handler_rects(show_wheel_event_handler_rects);
  proto->set_show_scroll_event_handler_rects(show_scroll_event_handler_rects);
  proto->set_show_non_fast_scrollable_rects(show_non_fast_scrollable_rects);
  proto->set_show_layer_animation_bounds_rects(
      show_layer_animation_bounds_rects);
  proto->set_slow_down_raster_scale_factor(slow_down_raster_scale_factor);
  proto->set_rasterize_only_visible_content(rasterize_only_visible_content);
  proto->set_show_picture_borders(show_picture_borders);
  proto->set_record_rendering_stats(record_rendering_stats_);
}

void LayerTreeDebugState::FromProtobuf(
    const proto::LayerTreeDebugState& proto) {
  show_fps_counter = proto.show_fps_counter();
  show_debug_borders = proto.show_debug_borders();
  show_paint_rects = proto.show_paint_rects();
  show_property_changed_rects = proto.show_property_changed_rects();
  show_surface_damage_rects = proto.show_surface_damage_rects();
  show_screen_space_rects = proto.show_screen_space_rects();
  show_replica_screen_space_rects = proto.show_replica_screen_space_rects();
  show_touch_event_handler_rects = proto.show_touch_event_handler_rects();
  show_wheel_event_handler_rects = proto.show_wheel_event_handler_rects();
  show_scroll_event_handler_rects = proto.show_scroll_event_handler_rects();
  show_non_fast_scrollable_rects = proto.show_non_fast_scrollable_rects();
  show_layer_animation_bounds_rects = proto.show_layer_animation_bounds_rects();
  slow_down_raster_scale_factor = proto.slow_down_raster_scale_factor();
  rasterize_only_visible_content = proto.rasterize_only_visible_content();
  show_picture_borders = proto.show_picture_borders();
  record_rendering_stats_ = proto.record_rendering_stats();
}

bool LayerTreeDebugState::Equal(const LayerTreeDebugState& a,
                                const LayerTreeDebugState& b) {
  return (
      a.show_fps_counter == b.show_fps_counter &&
      a.show_debug_borders == b.show_debug_borders &&
      a.show_paint_rects == b.show_paint_rects &&
      a.show_property_changed_rects == b.show_property_changed_rects &&
      a.show_surface_damage_rects == b.show_surface_damage_rects &&
      a.show_screen_space_rects == b.show_screen_space_rects &&
      a.show_replica_screen_space_rects == b.show_replica_screen_space_rects &&
      a.show_touch_event_handler_rects == b.show_touch_event_handler_rects &&
      a.show_wheel_event_handler_rects == b.show_wheel_event_handler_rects &&
      a.show_scroll_event_handler_rects == b.show_scroll_event_handler_rects &&
      a.show_non_fast_scrollable_rects == b.show_non_fast_scrollable_rects &&
      a.show_layer_animation_bounds_rects ==
          b.show_layer_animation_bounds_rects &&
      a.slow_down_raster_scale_factor == b.slow_down_raster_scale_factor &&
      a.rasterize_only_visible_content == b.rasterize_only_visible_content &&
      a.show_picture_borders == b.show_picture_borders &&
      a.record_rendering_stats_ == b.record_rendering_stats_);
}

LayerTreeDebugState LayerTreeDebugState::Unite(const LayerTreeDebugState& a,
                                               const LayerTreeDebugState& b) {
  LayerTreeDebugState r(a);

  r.show_fps_counter |= b.show_fps_counter;
  r.show_debug_borders |= b.show_debug_borders;

  r.show_paint_rects |= b.show_paint_rects;
  r.show_property_changed_rects |= b.show_property_changed_rects;
  r.show_surface_damage_rects |= b.show_surface_damage_rects;
  r.show_screen_space_rects |= b.show_screen_space_rects;
  r.show_replica_screen_space_rects |= b.show_replica_screen_space_rects;
  r.show_touch_event_handler_rects |= b.show_touch_event_handler_rects;
  r.show_wheel_event_handler_rects |= b.show_wheel_event_handler_rects;
  r.show_scroll_event_handler_rects |= b.show_scroll_event_handler_rects;
  r.show_non_fast_scrollable_rects |= b.show_non_fast_scrollable_rects;
  r.show_layer_animation_bounds_rects |= b.show_layer_animation_bounds_rects;

  if (b.slow_down_raster_scale_factor)
    r.slow_down_raster_scale_factor = b.slow_down_raster_scale_factor;
  r.rasterize_only_visible_content |= b.rasterize_only_visible_content;
  r.show_picture_borders |= b.show_picture_borders;

  r.record_rendering_stats_ |= b.record_rendering_stats_;

  return r;
}

}  // namespace cc
