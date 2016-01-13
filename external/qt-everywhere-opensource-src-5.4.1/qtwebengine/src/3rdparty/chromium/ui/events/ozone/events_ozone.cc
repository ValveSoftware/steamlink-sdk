// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"

namespace ui {

void UpdateDeviceList() { NOTIMPLEMENTED(); }

base::TimeDelta EventTimeFromNative(const base::NativeEvent& native_event) {
  const ui::Event* event = static_cast<const ui::Event*>(native_event);
  return event->time_stamp();
}

int EventFlagsFromNative(const base::NativeEvent& native_event) {
  const ui::Event* event = static_cast<const ui::Event*>(native_event);
  return event->flags();
}

EventType EventTypeFromNative(const base::NativeEvent& native_event) {
  const ui::Event* event = static_cast<const ui::Event*>(native_event);
  return event->type();
}

gfx::Point EventSystemLocationFromNative(
    const base::NativeEvent& native_event) {
  const ui::LocatedEvent* e =
      static_cast<const ui::LocatedEvent*>(native_event);
  DCHECK(e->IsMouseEvent() || e->IsTouchEvent() || e->IsGestureEvent() ||
         e->IsScrollEvent());
  return e->location();
}

gfx::Point EventLocationFromNative(const base::NativeEvent& native_event) {
  return EventSystemLocationFromNative(native_event);
}

int GetChangedMouseButtonFlagsFromNative(
    const base::NativeEvent& native_event) {
  const ui::MouseEvent* event =
      static_cast<const ui::MouseEvent*>(native_event);
  DCHECK(event->IsMouseEvent());
  return event->changed_button_flags();
}

KeyboardCode KeyboardCodeFromNative(const base::NativeEvent& native_event) {
  const ui::KeyEvent* event = static_cast<const ui::KeyEvent*>(native_event);
  DCHECK(event->IsKeyEvent());
  return event->key_code();
}

const char* CodeFromNative(const base::NativeEvent& native_event) {
  const ui::KeyEvent* event = static_cast<const ui::KeyEvent*>(native_event);
  DCHECK(event->IsKeyEvent());
  return event->code().c_str();
}

uint32 PlatformKeycodeFromNative(const base::NativeEvent& native_event) {
  const ui::KeyEvent* event = static_cast<const ui::KeyEvent*>(native_event);
  DCHECK(event->IsKeyEvent());
  return event->platform_keycode();
}

gfx::Vector2d GetMouseWheelOffset(const base::NativeEvent& native_event) {
  const ui::MouseWheelEvent* event =
      static_cast<const ui::MouseWheelEvent*>(native_event);
  DCHECK(event->type() == ET_MOUSEWHEEL);
  return event->offset();
}

base::NativeEvent CopyNativeEvent(const base::NativeEvent& event) {
  return NULL;
}

void ReleaseCopiedNativeEvent(const base::NativeEvent& event) {
}

void ClearTouchIdIfReleased(const base::NativeEvent& xev) {
}

int GetTouchId(const base::NativeEvent& native_event) {
  const ui::TouchEvent* event =
      static_cast<const ui::TouchEvent*>(native_event);
  DCHECK(event->IsTouchEvent());
  return event->touch_id();
}

float GetTouchRadiusX(const base::NativeEvent& native_event) {
  const ui::TouchEvent* event =
      static_cast<const ui::TouchEvent*>(native_event);
  DCHECK(event->IsTouchEvent());
  return event->radius_x();
}

float GetTouchRadiusY(const base::NativeEvent& native_event) {
  const ui::TouchEvent* event =
      static_cast<const ui::TouchEvent*>(native_event);
  DCHECK(event->IsTouchEvent());
  return event->radius_y();
}

float GetTouchAngle(const base::NativeEvent& native_event) {
  const ui::TouchEvent* event =
      static_cast<const ui::TouchEvent*>(native_event);
  DCHECK(event->IsTouchEvent());
  return event->rotation_angle();
}

float GetTouchForce(const base::NativeEvent& native_event) {
  const ui::TouchEvent* event =
      static_cast<const ui::TouchEvent*>(native_event);
  DCHECK(event->IsTouchEvent());
  return event->force();
}

bool GetScrollOffsets(const base::NativeEvent& native_event,
                      float* x_offset,
                      float* y_offset,
                      float* x_offset_ordinal,
                      float* y_offset_ordinal,
                      int* finger_count) {
  NOTIMPLEMENTED();
  return false;
}

bool GetFlingData(const base::NativeEvent& native_event,
                  float* vx,
                  float* vy,
                  float* vx_ordinal,
                  float* vy_ordinal,
                  bool* is_cancel) {
  NOTIMPLEMENTED();
  return false;
}

bool GetGestureTimes(const base::NativeEvent& native_event,
                     double* start_time,
                     double* end_time) {
  *start_time = 0;
  *end_time = 0;
  return false;
}

void SetNaturalScroll(bool /* enabled */) { NOTIMPLEMENTED(); }

bool IsNaturalScrollEnabled() { return false; }

bool IsTouchpadEvent(const base::NativeEvent& event) {
  NOTIMPLEMENTED();
  return false;
}

int GetModifiersFromKeyState() {
  NOTIMPLEMENTED();
  return 0;
}

}  // namespace ui
