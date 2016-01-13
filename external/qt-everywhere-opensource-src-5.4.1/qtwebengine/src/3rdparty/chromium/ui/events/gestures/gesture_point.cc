// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gestures/gesture_point.h"

#include <cmath>

#include "base/basictypes.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/gestures/gesture_configuration.h"
#include "ui/events/gestures/gesture_types.h"

namespace ui {

GesturePoint::GesturePoint()
    : first_touch_time_(0.0),
      second_last_touch_time_(0.0),
      last_touch_time_(0.0),
      second_last_tap_time_(0.0),
      last_tap_time_(0.0),
      velocity_calculator_(
          GestureConfiguration::points_buffered_for_velocity()),
      point_id_(-1),
      touch_id_(-1),
      source_device_id_(-1) {
  max_touch_move_in_pixels_for_click_squared_ =
      GestureConfiguration::max_touch_move_in_pixels_for_click() *
      GestureConfiguration::max_touch_move_in_pixels_for_click();
  max_distance_between_taps_for_double_tap_squared_ =
      GestureConfiguration::max_distance_between_taps_for_double_tap() *
      GestureConfiguration::max_distance_between_taps_for_double_tap();
}

GesturePoint::~GesturePoint() {}

void GesturePoint::Reset() {
  first_touch_time_ = second_last_touch_time_ = last_touch_time_ = 0.0;
  ResetVelocity();
  point_id_ = -1;
  clear_enclosing_rectangle();
  source_device_id_ = -1;
}

void GesturePoint::ResetVelocity() {
  velocity_calculator_.ClearHistory();
}

gfx::Vector2dF GesturePoint::ScrollDelta() const {
  return last_touch_position_ - second_last_touch_position_;
}

void GesturePoint::UpdateValues(const TouchEvent& event) {
  const int64 event_timestamp_microseconds =
      event.time_stamp().InMicroseconds();
  if (event.type() == ui::ET_TOUCH_MOVED) {
    velocity_calculator_.PointSeen(event.location().x(),
                                   event.location().y(),
                                   event_timestamp_microseconds);
    gfx::Vector2d sd(ScrollVelocityDirection(velocity_calculator_.XVelocity()),
                     ScrollVelocityDirection(velocity_calculator_.YVelocity()));
  }

  last_touch_time_ = event.time_stamp().InSecondsF();
  last_touch_position_ = event.location_f();

  if (event.type() == ui::ET_TOUCH_PRESSED) {
    ResetVelocity();
    clear_enclosing_rectangle();
    first_touch_time_ = last_touch_time_;
    first_touch_position_ = event.location();
    second_last_touch_position_ = last_touch_position_;
    second_last_touch_time_ = last_touch_time_;
    velocity_calculator_.PointSeen(event.location().x(),
                                   event.location().y(),
                                   event_timestamp_microseconds);
  }

  UpdateEnclosingRectangle(event);
}

void GesturePoint::UpdateForTap() {
  // Update the tap-position and time, and reset every other state.
  second_last_tap_position_ = last_tap_position_;
  second_last_tap_time_ = last_tap_time_;
  last_tap_time_ = last_touch_time_;
  last_tap_position_ = last_touch_position_;
}

void GesturePoint::UpdateForScroll() {
  second_last_touch_position_ = last_touch_position_;
  second_last_touch_time_ = last_touch_time_;
}

bool GesturePoint::IsInClickWindow(const TouchEvent& event) const {
  return IsInClickTimeWindow() && IsInsideTouchSlopRegion(event);
}

bool GesturePoint::IsInDoubleClickWindow(const TouchEvent& event) const {
  return IsInClickAggregateTimeWindow(last_tap_time_, last_touch_time_) &&
         IsPointInsideDoubleTapTouchSlopRegion(
             event.location(), last_tap_position_);
}

bool GesturePoint::IsInTripleClickWindow(const TouchEvent& event) const {
  return IsInClickAggregateTimeWindow(last_tap_time_, last_touch_time_) &&
         IsInClickAggregateTimeWindow(second_last_tap_time_, last_tap_time_) &&
         IsPointInsideDoubleTapTouchSlopRegion(
             event.location(), last_tap_position_) &&
         IsPointInsideDoubleTapTouchSlopRegion(last_tap_position_,
                                      second_last_tap_position_);
}

bool GesturePoint::IsInScrollWindow(const TouchEvent& event) const {
  return event.type() == ui::ET_TOUCH_MOVED &&
         !IsInsideTouchSlopRegion(event);
}

bool GesturePoint::IsInFlickWindow(const TouchEvent& event) {
  return IsOverMinFlickSpeed() &&
         event.type() != ui::ET_TOUCH_CANCELLED;
}

int GesturePoint::ScrollVelocityDirection(float v) {
  if (v < -GestureConfiguration::min_scroll_velocity())
    return -1;
  else if (v > GestureConfiguration::min_scroll_velocity())
    return 1;
  else
    return 0;
}

bool GesturePoint::DidScroll(const TouchEvent& event, int dist) const {
  gfx::Vector2dF d = last_touch_position_ - second_last_touch_position_;
  return fabs(d.x()) > dist || fabs(d.y()) > dist;
}

bool GesturePoint::IsInHorizontalRailWindow() const {
  gfx::Vector2dF d = last_touch_position_ - second_last_touch_position_;
  return std::abs(d.x()) >
      GestureConfiguration::rail_start_proportion() * std::abs(d.y());
}

bool GesturePoint::IsInVerticalRailWindow() const {
  gfx::Vector2dF d = last_touch_position_ - second_last_touch_position_;
  return std::abs(d.y()) >
      GestureConfiguration::rail_start_proportion() * std::abs(d.x());
}

bool GesturePoint::BreaksHorizontalRail() {
  float vx = XVelocity();
  float vy = YVelocity();
  return fabs(vy) > GestureConfiguration::rail_break_proportion() * fabs(vx) +
      GestureConfiguration::min_rail_break_velocity();
}

bool GesturePoint::BreaksVerticalRail() {
  float vx = XVelocity();
  float vy = YVelocity();
  return fabs(vx) > GestureConfiguration::rail_break_proportion() * fabs(vy) +
      GestureConfiguration::min_rail_break_velocity();
}

bool GesturePoint::IsInClickTimeWindow() const {
  double duration = last_touch_time_ - first_touch_time_;
  return duration >=
      GestureConfiguration::min_touch_down_duration_in_seconds_for_click() &&
      duration <
      GestureConfiguration::max_touch_down_duration_in_seconds_for_click();
}

bool GesturePoint::IsInClickAggregateTimeWindow(double before,
                                                double after) const {
  double duration =  after - before;
  return duration < GestureConfiguration::max_seconds_between_double_click();
}

bool GesturePoint::IsInsideTouchSlopRegion(const TouchEvent& event) const {
  const gfx::PointF& p1 = event.location();
  const gfx::PointF& p2 = first_touch_position_;
  float dx = p1.x() - p2.x();
  float dy = p1.y() - p2.y();
  float distance = dx * dx + dy * dy;
  return distance < max_touch_move_in_pixels_for_click_squared_;
}

bool GesturePoint::IsPointInsideDoubleTapTouchSlopRegion(gfx::PointF p1,
                                                         gfx::PointF p2) const {
  float dx = p1.x() - p2.x();
  float dy = p1.y() - p2.y();
  float distance = dx * dx + dy * dy;
  return distance < max_distance_between_taps_for_double_tap_squared_;
}

bool GesturePoint::IsOverMinFlickSpeed() {
  return velocity_calculator_.VelocitySquared() >
      GestureConfiguration::min_flick_speed_squared();
}

void GesturePoint::UpdateEnclosingRectangle(const TouchEvent& event) {
  int radius;

  // Ignore this TouchEvent if it has a radius larger than the maximum
  // allowed radius size.
  if (event.radius_x() > GestureConfiguration::max_radius() ||
      event.radius_y() > GestureConfiguration::max_radius())
    return;

  // If the device provides at least one of the radius values, take the larger
  // of the two and use this as both the x radius and the y radius of the
  // touch region. Otherwise use the default radius value.
  // TODO(tdanderson): Implement a more specific check for the exact
  // information provided by the device (0-2 radii values, force, angle) and
  // use this to compute a more representative rectangular touch region.
  if (event.radius_x() || event.radius_y())
    radius = std::max(event.radius_x(), event.radius_y());
  else
    radius = GestureConfiguration::default_radius();

  gfx::RectF rect(event.location_f().x() - radius,
                  event.location_f().y() - radius,
                  radius * 2,
                  radius * 2);
  if (IsInClickWindow(event))
    enclosing_rect_.Union(rect);
  else
    enclosing_rect_ = rect;
}

}  // namespace ui
