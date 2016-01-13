// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/motion_event_web.h"

#include "base/logging.h"
#include "content/common/input/web_touch_event_traits.h"

using blink::WebInputEvent;
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
      cached_action_index_(GetActionIndexFrom(event)) {
  DCHECK_GT(GetPointerCount(), 0U);
}

MotionEventWeb::~MotionEventWeb() {}

int MotionEventWeb::GetId() const {
  return 0;
}

MotionEventWeb::Action MotionEventWeb::GetAction() const {
  return cached_action_;
}

int MotionEventWeb::GetActionIndex() const { return cached_action_index_; }

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
  // TODO(jdduke): We should be a bit more careful about axes here.
  return 2.f * std::max(event_.touches[pointer_index].radiusX,
                        event_.touches[pointer_index].radiusY);
}

float MotionEventWeb::GetPressure(size_t pointer_index) const {
  return 0.f;
}

base::TimeTicks MotionEventWeb::GetEventTime() const {
  return base::TimeTicks() +
         base::TimeDelta::FromMicroseconds(event_.timeStampSeconds *
                                           base::Time::kMicrosecondsPerSecond);
}

size_t MotionEventWeb::GetHistorySize() const { return 0; }

base::TimeTicks MotionEventWeb::GetHistoricalEventTime(
    size_t historical_index) const {
  NOTIMPLEMENTED();
  return base::TimeTicks();
}

float MotionEventWeb::GetHistoricalTouchMajor(size_t pointer_index,
                                              size_t historical_index) const {
  NOTIMPLEMENTED();
  return 0.f;
}

float MotionEventWeb::GetHistoricalX(size_t pointer_index,
                                     size_t historical_index) const {
  NOTIMPLEMENTED();
  return 0.f;
}

float MotionEventWeb::GetHistoricalY(size_t pointer_index,
                                     size_t historical_index) const {
  NOTIMPLEMENTED();
  return 0.f;
}

ui::MotionEvent::ToolType MotionEventWeb::GetToolType(
    size_t pointer_index) const {
  NOTIMPLEMENTED();
  return TOOL_TYPE_UNKNOWN;
}

int MotionEventWeb::GetButtonState() const {
  NOTIMPLEMENTED();
  return 0;
}

scoped_ptr<ui::MotionEvent> MotionEventWeb::Clone() const {
  return scoped_ptr<MotionEvent>(new MotionEventWeb(event_));
}

scoped_ptr<ui::MotionEvent> MotionEventWeb::Cancel() const {
  WebTouchEvent cancel_event(event_);
  WebTouchEventTraits::ResetTypeAndTouchStates(
      blink::WebInputEvent::TouchCancel,
      // TODO(rbyers): Shouldn't we use a fresh timestamp?
      event_.timeStampSeconds,
      &cancel_event);
  return scoped_ptr<MotionEvent>(new MotionEventWeb(cancel_event));
}

}  // namespace content
