// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MSVC++ requires this to be set before any other includes to get M_PI.
#define _USE_MATH_DEFINES

#include "content/browser/renderer_host/input/motion_event_web.h"

#include <cmath>

#include "base/logging.h"
#include "content/browser/renderer_host/input/web_input_event_util.h"
#include "content/common/input/web_touch_event_traits.h"

using blink::WebInputEvent;
using blink::WebPointerProperties;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace content {
namespace {

ui::MotionEvent::Action GetActionFrom(const WebTouchEvent& event) {
  DCHECK(event.touchesLength);
  switch (event.type) {
    case WebInputEvent::TouchStart:
      if (WebTouchEventTraits::AllTouchPointsHaveState(
              event, WebTouchPoint::StatePressed))
        return ui::MotionEvent::ACTION_DOWN;
      else
        return ui::MotionEvent::ACTION_POINTER_DOWN;
    case WebInputEvent::TouchEnd:
      if (WebTouchEventTraits::AllTouchPointsHaveState(
              event, WebTouchPoint::StateReleased))
        return ui::MotionEvent::ACTION_UP;
      else
        return ui::MotionEvent::ACTION_POINTER_UP;
    case WebInputEvent::TouchCancel:
      DCHECK(WebTouchEventTraits::AllTouchPointsHaveState(
          event, WebTouchPoint::StateCancelled));
      return ui::MotionEvent::ACTION_CANCEL;
    case WebInputEvent::TouchMove:
      return ui::MotionEvent::ACTION_MOVE;
    default:
      break;
  };
  NOTREACHED()
      << "Unable to derive a valid MotionEvent::Action from the WebTouchEvent.";
  return ui::MotionEvent::ACTION_CANCEL;
}

int GetActionIndexFrom(const WebTouchEvent& event) {
  for (size_t i = 0; i < event.touchesLength; ++i) {
    if (event.touches[i].state != WebTouchPoint::StateUndefined &&
        event.touches[i].state != WebTouchPoint::StateStationary)
      return i;
  }
  return -1;
}

}  // namespace

MotionEventWeb::MotionEventWeb(const WebTouchEvent& event)
    : event_(event),
      cached_action_(GetActionFrom(event)),
      cached_action_index_(GetActionIndexFrom(event)),
      unique_event_id_(event.uniqueTouchEventId) {
  DCHECK_GT(GetPointerCount(), 0U);
}

MotionEventWeb::~MotionEventWeb() {}

uint32_t MotionEventWeb::GetUniqueEventId() const {
  return unique_event_id_;
}

MotionEventWeb::Action MotionEventWeb::GetAction() const {
  return cached_action_;
}

int MotionEventWeb::GetActionIndex() const {
  DCHECK(cached_action_ == ACTION_POINTER_UP ||
         cached_action_ == ACTION_POINTER_DOWN)
      << "Invalid action for GetActionIndex(): " << cached_action_;
  DCHECK_GE(cached_action_index_, 0);
  DCHECK_LT(cached_action_index_, static_cast<int>(event_.touchesLength));
  return cached_action_index_;
}

size_t MotionEventWeb::GetPointerCount() const { return event_.touchesLength; }

int MotionEventWeb::GetPointerId(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  return event_.touches[pointer_index].id;
}

float MotionEventWeb::GetX(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  return event_.touches[pointer_index].position.x;
}

float MotionEventWeb::GetY(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  return event_.touches[pointer_index].position.y;
}

float MotionEventWeb::GetRawX(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  return event_.touches[pointer_index].screenPosition.x;
}

float MotionEventWeb::GetRawY(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  return event_.touches[pointer_index].screenPosition.y;
}

float MotionEventWeb::GetTouchMajor(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  return 2.f * std::max(event_.touches[pointer_index].radiusX,
                        event_.touches[pointer_index].radiusY);
}

float MotionEventWeb::GetTouchMinor(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  return 2.f * std::min(event_.touches[pointer_index].radiusX,
                        event_.touches[pointer_index].radiusY);
}

float MotionEventWeb::GetOrientation(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());

  float orientation_rad = event_.touches[pointer_index].rotationAngle
      * M_PI / 180.f;
  DCHECK(0 <= orientation_rad && orientation_rad <= M_PI_2)
      << "Unexpected touch rotation angle";

  if (GetToolType(pointer_index) == TOOL_TYPE_STYLUS) {
    const WebPointerProperties& pointer = event_.touches[pointer_index];

    if (pointer.tiltY <= 0 && pointer.tiltX < 0) {
      // Stylus is tilted to the left away from the user or straight
      // to the left thus the orientation should be within [pi/2,pi).
      orientation_rad += static_cast<float>(M_PI_2);
    } else if (pointer.tiltY < 0 && pointer.tiltX >= 0) {
      // Stylus is tilted to the right away from the user or straight away
      // from the user thus the orientation should be within [-pi,-pi/2).
      orientation_rad -= static_cast<float>(M_PI);
    } else if (pointer.tiltY >= 0 && pointer.tiltX > 0) {
      // Stylus is tilted to the right towards the user or straight
      // to the right thus the orientation should be within [-pi/2,0).
      orientation_rad -= static_cast<float>(M_PI_2);
    }
  } else if (event_.touches[pointer_index].radiusX
             > event_.touches[pointer_index].radiusY) {
    // The case radiusX == radiusY is omitted from here on purpose: for circles,
    // we want to pass the angle (which could be any value in such cases but
    // always seems to be set to zero) unchanged.
    orientation_rad -= static_cast<float>(M_PI_2);
  }

  return orientation_rad;
}

float MotionEventWeb::GetPressure(size_t pointer_index) const {
  return 0.f;
}

float MotionEventWeb::GetTilt(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());

  if (GetToolType(pointer_index) != TOOL_TYPE_STYLUS)
    return 0.f;

  const WebPointerProperties& pointer = event_.touches[pointer_index];

  float tilt_x_r = sin(pointer.tiltX * M_PI / 180.f);
  float tilt_x_z = cos(pointer.tiltX * M_PI / 180.f);
  float tilt_y_r = sin(pointer.tiltY * M_PI / 180.f);
  float tilt_y_z = cos(pointer.tiltY * M_PI / 180.f);
  float r_x = tilt_x_r * tilt_y_z;
  float r_y = tilt_y_r * tilt_x_z;
  float r = sqrt(r_x * r_x + r_y * r_y);
  float z = tilt_x_z * tilt_y_z;

  return atan2(r, z);
}

base::TimeTicks MotionEventWeb::GetEventTime() const {
  return base::TimeTicks() +
         base::TimeDelta::FromMicroseconds(event_.timeStampSeconds *
                                           base::Time::kMicrosecondsPerSecond);
}

ui::MotionEvent::ToolType MotionEventWeb::GetToolType(
    size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());

  const WebPointerProperties& pointer = event_.touches[pointer_index];

  switch (pointer.pointerType) {
    case WebPointerProperties::PointerType::Unknown:
      return TOOL_TYPE_UNKNOWN;
    case WebPointerProperties::PointerType::Mouse:
      return TOOL_TYPE_MOUSE;
    case WebPointerProperties::PointerType::Pen:
      return TOOL_TYPE_STYLUS;
    case WebPointerProperties::PointerType::Touch:
      return TOOL_TYPE_FINGER;
  }
  NOTREACHED() << "Unexpected pointerType";
  return TOOL_TYPE_UNKNOWN;
}

int MotionEventWeb::GetButtonState() const {
  return 0;
}

int MotionEventWeb::GetFlags() const {
  return WebEventModifiersToEventFlags(event_.modifiers);
}

}  // namespace content
