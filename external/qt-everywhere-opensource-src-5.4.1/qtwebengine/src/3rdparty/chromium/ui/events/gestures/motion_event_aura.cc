// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gestures/motion_event_aura.h"

#include "base/logging.h"
#include "ui/events/gestures/gesture_configuration.h"

namespace ui {

MotionEventAura::MotionEventAura()
    : pointer_count_(0), cached_action_index_(-1) {
}

MotionEventAura::MotionEventAura(
    size_t pointer_count,
    const base::TimeTicks& last_touch_time,
    Action cached_action,
    int cached_action_index,
    const PointData (&active_touches)[GestureSequence::kMaxGesturePoints])
    : pointer_count_(pointer_count),
      last_touch_time_(last_touch_time),
      cached_action_(cached_action),
      cached_action_index_(cached_action_index) {
  DCHECK(pointer_count_);
  for (size_t i = 0; i < pointer_count; ++i)
    active_touches_[i] = active_touches[i];
}

MotionEventAura::~MotionEventAura() {}

MotionEventAura::PointData MotionEventAura::GetPointDataFromTouchEvent(
    const TouchEvent& touch) {
  PointData point_data;
  point_data.x = touch.x();
  point_data.y = touch.y();
  point_data.raw_x = touch.root_location_f().x();
  point_data.raw_y = touch.root_location_f().y();
  point_data.touch_id = touch.touch_id();
  point_data.pressure = touch.force();
  point_data.source_device_id = touch.source_device_id();

  // TODO(tdresser): at some point we should start using both radii if they are
  // available, but for now we use the max.
  point_data.major_radius = std::max(touch.radius_x(), touch.radius_y());
  if (!point_data.major_radius)
    point_data.major_radius = GestureConfiguration::default_radius();
  return point_data;
}

void MotionEventAura::OnTouch(const TouchEvent& touch) {
  switch (touch.type()) {
    case ET_TOUCH_PRESSED:
      AddTouch(touch);
      break;
    case ET_TOUCH_RELEASED:
    case ET_TOUCH_CANCELLED:
      // Removing these touch points needs to be postponed until after the
      // MotionEvent has been dispatched. This cleanup occurs in
      // CleanupRemovedTouchPoints.
      UpdateTouch(touch);
      break;
    case ET_TOUCH_MOVED:
      UpdateTouch(touch);
      break;
    default:
      NOTREACHED();
      break;
  }

  UpdateCachedAction(touch);
  last_touch_time_ = touch.time_stamp() + base::TimeTicks();
}

int MotionEventAura::GetId() const {
  return GetPointerId(0);
}

MotionEvent::Action MotionEventAura::GetAction() const {
  return cached_action_;
}

int MotionEventAura::GetActionIndex() const {
  DCHECK(cached_action_ == ACTION_POINTER_DOWN ||
         cached_action_ == ACTION_POINTER_UP);
  DCHECK_GE(cached_action_index_, 0);
  DCHECK_LE(cached_action_index_, static_cast<int>(pointer_count_));
  return cached_action_index_;
}

size_t MotionEventAura::GetPointerCount() const { return pointer_count_; }

int MotionEventAura::GetPointerId(size_t pointer_index) const {
  DCHECK_LE(pointer_index, pointer_count_);
  return active_touches_[pointer_index].touch_id;
}

float MotionEventAura::GetX(size_t pointer_index) const {
  DCHECK_LE(pointer_index, pointer_count_);
  return active_touches_[pointer_index].x;
}

float MotionEventAura::GetY(size_t pointer_index) const {
  DCHECK_LE(pointer_index, pointer_count_);
  return active_touches_[pointer_index].y;
}

float MotionEventAura::GetRawX(size_t pointer_index) const {
  DCHECK_LE(pointer_index, pointer_count_);
  return active_touches_[pointer_index].raw_x;
}

float MotionEventAura::GetRawY(size_t pointer_index) const {
  DCHECK_LE(pointer_index, pointer_count_);
  return active_touches_[pointer_index].raw_y;
}

float MotionEventAura::GetTouchMajor(size_t pointer_index) const {
  DCHECK_LE(pointer_index, pointer_count_);
  return active_touches_[pointer_index].major_radius * 2;
}

float MotionEventAura::GetPressure(size_t pointer_index) const {
  DCHECK_LE(pointer_index, pointer_count_);
  return active_touches_[pointer_index].pressure;
}

base::TimeTicks MotionEventAura::GetEventTime() const {
  return last_touch_time_;
}

size_t MotionEventAura::GetHistorySize() const { return 0; }

base::TimeTicks MotionEventAura::GetHistoricalEventTime(
    size_t historical_index) const {
  NOTIMPLEMENTED();
  return base::TimeTicks();
}

float MotionEventAura::GetHistoricalTouchMajor(size_t pointer_index,
                                             size_t historical_index) const {
  NOTIMPLEMENTED();
  return 0;
}

float MotionEventAura::GetHistoricalX(size_t pointer_index,
                                    size_t historical_index) const {
  NOTIMPLEMENTED();
  return 0;
}

float MotionEventAura::GetHistoricalY(size_t pointer_index,
                                    size_t historical_index) const {
  NOTIMPLEMENTED();
  return 0;
}

MotionEvent::ToolType MotionEventAura::GetToolType(size_t pointer_index) const {
  NOTIMPLEMENTED();
  return MotionEvent::TOOL_TYPE_UNKNOWN;
}

int MotionEventAura::GetButtonState() const {
  NOTIMPLEMENTED();
  return 0;
}

scoped_ptr<MotionEvent> MotionEventAura::Clone() const {
  return scoped_ptr<MotionEvent>(new MotionEventAura(pointer_count_,
                                                     last_touch_time_,
                                                     cached_action_,
                                                     cached_action_index_,
                                                     active_touches_));
}
scoped_ptr<MotionEvent> MotionEventAura::Cancel() const {
  return scoped_ptr<MotionEvent>(new MotionEventAura(
      pointer_count_, last_touch_time_, ACTION_CANCEL, -1, active_touches_));
}

void MotionEventAura::CleanupRemovedTouchPoints(const TouchEvent& event) {
  if (event.type() != ET_TOUCH_RELEASED &&
      event.type() != ET_TOUCH_CANCELLED) {
    return;
  }

  int index_to_delete = static_cast<int>(GetIndexFromId(event.touch_id()));
  pointer_count_--;
  active_touches_[index_to_delete] = active_touches_[pointer_count_];
}

MotionEventAura::PointData::PointData()
    : x(0),
      y(0),
      raw_x(0),
      raw_y(0),
      touch_id(0),
      pressure(0),
      source_device_id(0),
      major_radius(0) {
}

int MotionEventAura::GetSourceDeviceId(size_t pointer_index) const {
  DCHECK_LE(pointer_index, pointer_count_);
  return active_touches_[pointer_index].source_device_id;
}

void MotionEventAura::AddTouch(const TouchEvent& touch) {
  if (pointer_count_ == static_cast<size_t>(GestureSequence::kMaxGesturePoints))
    return;

  active_touches_[pointer_count_] = GetPointDataFromTouchEvent(touch);
  pointer_count_++;
}


void MotionEventAura::UpdateTouch(const TouchEvent& touch) {
  active_touches_[GetIndexFromId(touch.touch_id())] =
      GetPointDataFromTouchEvent(touch);
}

void MotionEventAura::UpdateCachedAction(const TouchEvent& touch) {
  DCHECK(pointer_count_);
  switch (touch.type()) {
    case ET_TOUCH_PRESSED:
      if (pointer_count_ == 1) {
        cached_action_ = ACTION_DOWN;
      } else {
        cached_action_ = ACTION_POINTER_DOWN;
        cached_action_index_ =
            static_cast<int>(GetIndexFromId(touch.touch_id()));
      }
      break;
    case ET_TOUCH_RELEASED:
      if (pointer_count_ == 1) {
        cached_action_ = ACTION_UP;
      } else {
        cached_action_ = ACTION_POINTER_UP;
        cached_action_index_ =
            static_cast<int>(GetIndexFromId(touch.touch_id()));
        DCHECK_LE(cached_action_index_, static_cast<int>(pointer_count_));
      }
      break;
    case ET_TOUCH_CANCELLED:
      cached_action_ = ACTION_CANCEL;
      break;
    case ET_TOUCH_MOVED:
      cached_action_ = ACTION_MOVE;
      break;
    default:
      NOTREACHED();
      break;
  }
}

size_t MotionEventAura::GetIndexFromId(int id) const {
  for (size_t i = 0; i < pointer_count_; ++i) {
    if (active_touches_[i].touch_id == id)
      return i;
  }
  NOTREACHED();
  return 0;
}

}  // namespace ui
