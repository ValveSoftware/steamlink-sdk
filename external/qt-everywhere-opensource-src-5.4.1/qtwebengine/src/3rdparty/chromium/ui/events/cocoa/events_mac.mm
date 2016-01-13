// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event_utils.h"

#include <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "ui/events/cocoa/cocoa_event_utils.h"
#include "ui/events/event_utils.h"
#import "ui/events/keycodes/keyboard_code_conversion_mac.h"
#include "ui/gfx/point.h"
#include "ui/gfx/vector2d.h"

namespace ui {

void UpdateDeviceList() {
  NOTIMPLEMENTED();
}

EventType EventTypeFromNative(const base::NativeEvent& native_event) {
  NSEventType type = [native_event type];
  switch (type) {
    case NSKeyDown:
      return ET_KEY_PRESSED;
    case NSKeyUp:
      return ET_KEY_RELEASED;
    case NSLeftMouseDown:
    case NSRightMouseDown:
    case NSOtherMouseDown:
      return ET_MOUSE_PRESSED;
    case NSLeftMouseUp:
    case NSRightMouseUp:
    case NSOtherMouseUp:
      return ET_MOUSE_RELEASED;
    case NSLeftMouseDragged:
    case NSRightMouseDragged:
    case NSOtherMouseDragged:
      return ET_MOUSE_DRAGGED;
    case NSMouseMoved:
      return ET_MOUSE_MOVED;
    case NSScrollWheel:
      return ET_MOUSEWHEEL;
    case NSMouseEntered:
      return ET_MOUSE_ENTERED;
    case NSMouseExited:
      return ET_MOUSE_EXITED;
    case NSEventTypeSwipe:
      return ET_SCROLL_FLING_START;
    case NSFlagsChanged:
    case NSAppKitDefined:
    case NSSystemDefined:
    case NSApplicationDefined:
    case NSPeriodic:
    case NSCursorUpdate:
    case NSTabletPoint:
    case NSTabletProximity:
    case NSEventTypeGesture:
    case NSEventTypeMagnify:
    case NSEventTypeRotate:
    case NSEventTypeBeginGesture:
    case NSEventTypeEndGesture:
      NOTIMPLEMENTED() << type;
      break;
    default:
      NOTIMPLEMENTED() << type;
      break;
  }
  return ET_UNKNOWN;
}

int EventFlagsFromNative(const base::NativeEvent& event) {
  NSUInteger modifiers = [event modifierFlags];
  return EventFlagsFromNSEventWithModifiers(event, modifiers);
}

base::TimeDelta EventTimeFromNative(const base::NativeEvent& native_event) {
  NSTimeInterval since_system_startup = [native_event timestamp];
  // Truncate to extract seconds before doing floating point arithmetic.
  int64_t seconds = since_system_startup;
  since_system_startup -= seconds;
  int64_t microseconds = since_system_startup * 1000000;
  return base::TimeDelta::FromSeconds(seconds) +
      base::TimeDelta::FromMicroseconds(microseconds);
}

gfx::Point EventLocationFromNative(const base::NativeEvent& native_event) {
  NSWindow* window = [native_event window];
  if (!window) {
    NOTIMPLEMENTED();  // Point will be in screen coordinates.
    return gfx::Point();
  }
  NSPoint location = [native_event locationInWindow];
  NSRect content_rect = [window contentRectForFrameRect:[window frame]];
  return gfx::Point(location.x, NSHeight(content_rect) - location.y);
}

gfx::Point EventSystemLocationFromNative(
    const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
  return gfx::Point();
}

int EventButtonFromNative(const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
  return 0;
}

int GetChangedMouseButtonFlagsFromNative(
    const base::NativeEvent& native_event) {
  NSEventType type = [native_event type];
  switch (type) {
    case NSLeftMouseDown:
    case NSLeftMouseUp:
    case NSLeftMouseDragged:
      return EF_LEFT_MOUSE_BUTTON;
    case NSRightMouseDown:
    case NSRightMouseUp:
    case NSRightMouseDragged:
      return EF_RIGHT_MOUSE_BUTTON;
    case NSOtherMouseDown:
    case NSOtherMouseUp:
    case NSOtherMouseDragged:
      return EF_MIDDLE_MOUSE_BUTTON;
  }
  return 0;
}

gfx::Vector2d GetMouseWheelOffset(const base::NativeEvent& event) {
  // Empirically, a value of 0.1 is typical for one mousewheel click. Positive
  // values when scrolling up or to the left. Scrolling quickly results in a
  // higher delta per click, up to about 15.0. (Quartz documentation suggests
  // +/-10).
  // Multiply by 1000 to vaguely approximate WHEEL_DELTA on Windows (120).
  const CGFloat kWheelDeltaMultiplier = 1000;
  return gfx::Vector2d(kWheelDeltaMultiplier * [event deltaX],
                       kWheelDeltaMultiplier * [event deltaY]);
}

base::NativeEvent CopyNativeEvent(const base::NativeEvent& event) {
  return [event copy];
}

void ReleaseCopiedNativeEvent(const base::NativeEvent& event) {
  [event release];
}

void ClearTouchIdIfReleased(const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
}

int GetTouchId(const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
  return 0;
}

float GetTouchRadiusX(const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
  return 0.f;
}

float GetTouchRadiusY(const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
  return 0.f;
}

float GetTouchAngle(const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
  return 0.f;
}

float GetTouchForce(const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
  return 0.f;
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
  NOTIMPLEMENTED();
  return false;
}

void SetNaturalScroll(bool enabled) {
  NOTIMPLEMENTED();
}

bool IsNaturalScrollEnabled() {
  NOTIMPLEMENTED();
  return false;
}

bool IsTouchpadEvent(const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
  return false;
}

KeyboardCode KeyboardCodeFromNative(const base::NativeEvent& native_event) {
  return KeyboardCodeFromNSEvent(native_event);
}

const char* CodeFromNative(const base::NativeEvent& native_event) {
  return CodeFromNSEvent(native_event);
}

uint32 PlatformKeycodeFromNative(const base::NativeEvent& native_event) {
  return native_event.keyCode;
}

}  // namespace ui
