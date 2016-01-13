// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/point.h"
#include "ui/gfx/vector2d.h"

namespace ui {

// Stub implementations of platform-specific methods in events_util.h, built
// on platform sthat currently do not have a complete implementation of events.

void UpdateDeviceList() {
  NOTIMPLEMENTED();
}

EventType EventTypeFromNative(const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
  return ET_UNKNOWN;
}

int EventFlagsFromNative(const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
  return 0;
}

base::TimeDelta EventTimeFromNative(const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
  return base::TimeDelta();
}

gfx::Point EventLocationFromNative(const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
  return gfx::Point();
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
  NOTIMPLEMENTED();
  return 0;
}

gfx::Vector2d GetMouseWheelOffset(const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
  return gfx::Vector2d();
}

base::NativeEvent CopyNativeEvent(const base::NativeEvent& event) {
  NOTIMPLEMENTED() <<
      "Don't know how to copy base::NativeEvent for this platform";
  return NULL;
}

void ReleaseCopiedNativeEvent(const base::NativeEvent& event) {
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
  NOTIMPLEMENTED();
  return static_cast<KeyboardCode>(0);
}

const char* CodeFromNative(const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
  return "";
}

uint32 PlatformKeycodeFromNative(const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
  return 0;
}

}  // namespace ui
