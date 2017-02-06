// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/synthetic_web_input_event_builders.h"

#include "base/logging.h"
#include "content/common/input/web_touch_event_traits.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace content {

using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using blink::WebGestureEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

WebMouseEvent SyntheticWebMouseEventBuilder::Build(
    blink::WebInputEvent::Type type) {
  WebMouseEvent result;
  result.type = type;
  return result;
}

WebMouseEvent SyntheticWebMouseEventBuilder::Build(
    blink::WebInputEvent::Type type,
    int window_x,
    int window_y,
    int modifiers) {
  DCHECK(WebInputEvent::isMouseEventType(type));
  WebMouseEvent result = Build(type);
  result.x = window_x;
  result.y = window_y;
  result.windowX = window_x;
  result.windowY = window_y;
  result.modifiers = modifiers;

  if (type == WebInputEvent::MouseDown || type == WebInputEvent::MouseUp)
    result.button = WebMouseEvent::ButtonLeft;
  else
    result.button = WebMouseEvent::ButtonNone;

  return result;
}

WebMouseWheelEvent SyntheticWebMouseWheelEventBuilder::Build(
    WebMouseWheelEvent::Phase phase) {
  WebMouseWheelEvent result;
  result.type = WebInputEvent::MouseWheel;
  result.phase = phase;
  return result;
}

WebMouseWheelEvent SyntheticWebMouseWheelEventBuilder::Build(float x,
                                                             float y,
                                                             float dx,
                                                             float dy,
                                                             int modifiers,
                                                             bool precise) {
  return Build(x, y, 0, 0, dx, dy, modifiers, precise);
}

WebMouseWheelEvent SyntheticWebMouseWheelEventBuilder::Build(float x,
                                                             float y,
                                                             float global_x,
                                                             float global_y,
                                                             float dx,
                                                             float dy,
                                                             int modifiers,
                                                             bool precise) {
  WebMouseWheelEvent result;
  result.type = WebInputEvent::MouseWheel;
  result.globalX = global_x;
  result.globalY = global_y;
  result.x = x;
  result.y = y;
  result.deltaX = dx;
  result.deltaY = dy;
  if (dx)
    result.wheelTicksX = dx > 0.0f ? 1.0f : -1.0f;
  if (dy)
    result.wheelTicksY = dy > 0.0f ? 1.0f : -1.0f;
  result.modifiers = modifiers;
  result.hasPreciseScrollingDeltas = precise;
  return result;
}

WebKeyboardEvent SyntheticWebKeyboardEventBuilder::Build(
    WebInputEvent::Type type) {
  DCHECK(WebInputEvent::isKeyboardEventType(type));
  WebKeyboardEvent result;
  result.type = type;
  result.windowsKeyCode = ui::VKEY_L;  // non-null made up value.
  return result;
}

WebGestureEvent SyntheticWebGestureEventBuilder::Build(
    WebInputEvent::Type type,
    blink::WebGestureDevice source_device) {
  DCHECK(WebInputEvent::isGestureEventType(type));
  WebGestureEvent result;
  result.type = type;
  result.sourceDevice = source_device;
  if (type == WebInputEvent::GestureTap ||
      type == WebInputEvent::GestureTapUnconfirmed ||
      type == WebInputEvent::GestureDoubleTap) {
    result.data.tap.tapCount = 1;
    result.data.tap.width = 10;
    result.data.tap.height = 10;
  }
  return result;
}

WebGestureEvent SyntheticWebGestureEventBuilder::BuildScrollBegin(
    float dx_hint,
    float dy_hint,
    blink::WebGestureDevice source_device) {
  WebGestureEvent result =
      Build(WebInputEvent::GestureScrollBegin, source_device);
  result.data.scrollBegin.deltaXHint = dx_hint;
  result.data.scrollBegin.deltaYHint = dy_hint;
  return result;
}

WebGestureEvent SyntheticWebGestureEventBuilder::BuildScrollUpdate(
    float dx,
    float dy,
    int modifiers,
    blink::WebGestureDevice source_device) {
  WebGestureEvent result =
      Build(WebInputEvent::GestureScrollUpdate, source_device);
  result.data.scrollUpdate.deltaX = dx;
  result.data.scrollUpdate.deltaY = dy;
  result.modifiers = modifiers;
  return result;
}

WebGestureEvent SyntheticWebGestureEventBuilder::BuildPinchUpdate(
    float scale,
    float anchor_x,
    float anchor_y,
    int modifiers,
    blink::WebGestureDevice source_device) {
  WebGestureEvent result =
      Build(WebInputEvent::GesturePinchUpdate, source_device);
  result.data.pinchUpdate.scale = scale;
  result.x = anchor_x;
  result.y = anchor_y;
  result.globalX = anchor_x;
  result.globalY = anchor_y;
  result.modifiers = modifiers;
  return result;
}

WebGestureEvent SyntheticWebGestureEventBuilder::BuildFling(
    float velocity_x,
    float velocity_y,
    blink::WebGestureDevice source_device) {
  WebGestureEvent result = Build(WebInputEvent::GestureFlingStart,
                                 source_device);
  result.data.flingStart.velocityX = velocity_x;
  result.data.flingStart.velocityY = velocity_y;
  return result;
}

SyntheticWebTouchEvent::SyntheticWebTouchEvent() : WebTouchEvent() {
  uniqueTouchEventId = ui::GetNextTouchEventId();
  SetTimestamp(base::TimeTicks::Now());
}

void SyntheticWebTouchEvent::ResetPoints() {
  int point = 0;
  for (unsigned int i = 0; i < touchesLength; ++i) {
    if (touches[i].state == WebTouchPoint::StateReleased)
      continue;

    touches[point] = touches[i];
    touches[point].state = WebTouchPoint::StateStationary;
    ++point;
  }
  touchesLength = point;
  type = WebInputEvent::Undefined;
  movedBeyondSlopRegion = false;
  uniqueTouchEventId = ui::GetNextTouchEventId();
}

int SyntheticWebTouchEvent::PressPoint(float x, float y) {
  if (touchesLength == touchesLengthCap)
    return -1;
  WebTouchPoint& point = touches[touchesLength];
  point.id = touchesLength;
  point.position.x = point.screenPosition.x = x;
  point.position.y = point.screenPosition.y = y;
  point.state = WebTouchPoint::StatePressed;
  point.radiusX = point.radiusY = 1.f;
  point.rotationAngle = 1.f;
  point.force = 1.f;
  point.tiltX = point.tiltY = 0;
  ++touchesLength;
  WebTouchEventTraits::ResetType(
      WebInputEvent::TouchStart, timeStampSeconds, this);
  return point.id;
}

void SyntheticWebTouchEvent::MovePoint(int index, float x, float y) {
  CHECK_GE(index, 0);
  CHECK_LT(index, touchesLengthCap);
  // Always set this bit to avoid otherwise unexpected touchmove suppression.
  // The caller can opt-out explicitly, if necessary.
  movedBeyondSlopRegion = true;
  WebTouchPoint& point = touches[index];
  point.position.x = point.screenPosition.x = x;
  point.position.y = point.screenPosition.y = y;
  touches[index].state = WebTouchPoint::StateMoved;
  WebTouchEventTraits::ResetType(
      WebInputEvent::TouchMove, timeStampSeconds, this);
}

void SyntheticWebTouchEvent::ReleasePoint(int index) {
  CHECK_GE(index, 0);
  CHECK_LT(index, touchesLengthCap);
  touches[index].state = WebTouchPoint::StateReleased;
  WebTouchEventTraits::ResetType(
      WebInputEvent::TouchEnd, timeStampSeconds, this);
}

void SyntheticWebTouchEvent::CancelPoint(int index) {
  CHECK_GE(index, 0);
  CHECK_LT(index, touchesLengthCap);
  touches[index].state = WebTouchPoint::StateCancelled;
  WebTouchEventTraits::ResetType(
      WebInputEvent::TouchCancel, timeStampSeconds, this);
}

void SyntheticWebTouchEvent::SetTimestamp(base::TimeTicks timestamp) {
  timeStampSeconds = ui::EventTimeStampToSeconds(timestamp);
}

}  // namespace content
