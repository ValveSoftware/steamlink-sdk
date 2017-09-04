// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/event_with_latency_info.h"

#include <bitset>
#include <limits>

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using std::numeric_limits;

namespace content {
namespace {

const int kInvalidTouchIndex = -1;

float GetUnacceleratedDelta(float accelerated_delta, float acceleration_ratio) {
  return accelerated_delta * acceleration_ratio;
}

float GetAccelerationRatio(float accelerated_delta, float unaccelerated_delta) {
  if (unaccelerated_delta == 0.f || accelerated_delta == 0.f)
    return 1.f;
  return unaccelerated_delta / accelerated_delta;
}

// Returns |kInvalidTouchIndex| iff |event| lacks a touch with an ID of |id|.
int GetIndexOfTouchID(const WebTouchEvent& event, int id) {
  for (unsigned i = 0; i < event.touchesLength; ++i) {
    if (event.touches[i].id == id)
      return i;
  }
  return kInvalidTouchIndex;
}

WebInputEvent::DispatchType MergeDispatchTypes(
    WebInputEvent::DispatchType type_1,
    WebInputEvent::DispatchType type_2) {
  static_assert(WebInputEvent::DispatchType::Blocking <
                    WebInputEvent::DispatchType::EventNonBlocking,
                "Enum not ordered correctly");
  static_assert(WebInputEvent::DispatchType::EventNonBlocking <
                    WebInputEvent::DispatchType::ListenersNonBlockingPassive,
                "Enum not ordered correctly");
  static_assert(
      WebInputEvent::DispatchType::ListenersNonBlockingPassive <
          WebInputEvent::DispatchType::ListenersForcedNonBlockingDueToFling,
      "Enum not ordered correctly");
  return static_cast<WebInputEvent::DispatchType>(
      std::min(static_cast<int>(type_1), static_cast<int>(type_2)));
}

}  // namespace

namespace internal {

bool CanCoalesce(const WebMouseEvent& event_to_coalesce,
                 const WebMouseEvent& event) {
  return event.type == event_to_coalesce.type &&
         event.type == WebInputEvent::MouseMove;
}

void Coalesce(const WebMouseEvent& event_to_coalesce, WebMouseEvent* event) {
  DCHECK(CanCoalesce(event_to_coalesce, *event));
  // Accumulate movement deltas.
  int x = event->movementX;
  int y = event->movementY;
  *event = event_to_coalesce;
  event->movementX += x;
  event->movementY += y;
}

bool CanCoalesce(const WebMouseWheelEvent& event_to_coalesce,
                 const WebMouseWheelEvent& event) {
  return event.modifiers == event_to_coalesce.modifiers &&
         event.scrollByPage == event_to_coalesce.scrollByPage &&
         event.phase == event_to_coalesce.phase &&
         event.momentumPhase == event_to_coalesce.momentumPhase &&
         event.hasPreciseScrollingDeltas ==
             event_to_coalesce.hasPreciseScrollingDeltas;
}

void Coalesce(const WebMouseWheelEvent& event_to_coalesce,
              WebMouseWheelEvent* event) {
  DCHECK(CanCoalesce(event_to_coalesce, *event));
  float unaccelerated_x =
      GetUnacceleratedDelta(event->deltaX, event->accelerationRatioX) +
      GetUnacceleratedDelta(event_to_coalesce.deltaX,
                            event_to_coalesce.accelerationRatioX);
  float unaccelerated_y =
      GetUnacceleratedDelta(event->deltaY, event->accelerationRatioY) +
      GetUnacceleratedDelta(event_to_coalesce.deltaY,
                            event_to_coalesce.accelerationRatioY);
  float old_deltaX = event->deltaX;
  float old_deltaY = event->deltaY;
  float old_wheelTicksX = event->wheelTicksX;
  float old_wheelTicksY = event->wheelTicksY;
  float old_movementX = event->movementX;
  float old_movementY = event->movementY;
  *event = event_to_coalesce;
  event->deltaX += old_deltaX;
  event->deltaY += old_deltaY;
  event->wheelTicksX += old_wheelTicksX;
  event->wheelTicksY += old_wheelTicksY;
  event->movementX += old_movementX;
  event->movementY += old_movementY;
  event->accelerationRatioX =
      GetAccelerationRatio(event->deltaX, unaccelerated_x);
  event->accelerationRatioY =
      GetAccelerationRatio(event->deltaY, unaccelerated_y);
}

bool CanCoalesce(const WebTouchEvent& event_to_coalesce,
                 const WebTouchEvent& event) {
  if (event.type != event_to_coalesce.type ||
      event.type != WebInputEvent::TouchMove ||
      event.modifiers != event_to_coalesce.modifiers ||
      event.touchesLength != event_to_coalesce.touchesLength ||
      event.touchesLength > WebTouchEvent::kTouchesLengthCap)
    return false;

  static_assert(WebTouchEvent::kTouchesLengthCap <= sizeof(int32_t) * 8U,
                "suboptimal kTouchesLengthCap size");
  // Ensure that we have a 1-to-1 mapping of pointer ids between touches.
  std::bitset<WebTouchEvent::kTouchesLengthCap> unmatched_event_touches(
      (1 << event.touchesLength) - 1);
  for (unsigned i = 0; i < event_to_coalesce.touchesLength; ++i) {
    int event_touch_index =
        GetIndexOfTouchID(event, event_to_coalesce.touches[i].id);
    if (event_touch_index == kInvalidTouchIndex)
      return false;
    if (!unmatched_event_touches[event_touch_index])
      return false;
    unmatched_event_touches[event_touch_index] = false;
  }
  return unmatched_event_touches.none();
}

void Coalesce(const WebTouchEvent& event_to_coalesce, WebTouchEvent* event) {
  DCHECK(CanCoalesce(event_to_coalesce, *event));
  // The WebTouchPoints include absolute position information. So it is
  // sufficient to simply replace the previous event with the new event->
  // However, it is necessary to make sure that all the points have the
  // correct state, i.e. the touch-points that moved in the last event, but
  // didn't change in the current event, will have Stationary state. It is
  // necessary to change them back to Moved state.
  WebTouchEvent old_event = *event;
  *event = event_to_coalesce;
  for (unsigned i = 0; i < event->touchesLength; ++i) {
    int i_old = GetIndexOfTouchID(old_event, event->touches[i].id);
    if (old_event.touches[i_old].state == blink::WebTouchPoint::StateMoved)
      event->touches[i].state = blink::WebTouchPoint::StateMoved;
  }
  event->movedBeyondSlopRegion |= old_event.movedBeyondSlopRegion;
  event->dispatchType = MergeDispatchTypes(old_event.dispatchType,
                                           event_to_coalesce.dispatchType);
}

bool CanCoalesce(const WebGestureEvent& event_to_coalesce,
                 const WebGestureEvent& event) {
  if (event.type != event_to_coalesce.type ||
      event.sourceDevice != event_to_coalesce.sourceDevice ||
      event.modifiers != event_to_coalesce.modifiers)
    return false;

  if (event.type == WebInputEvent::GestureScrollUpdate)
    return true;

  // GesturePinchUpdate scales can be combined only if they share a focal point,
  // e.g., with double-tap drag zoom.
  if (event.type == WebInputEvent::GesturePinchUpdate &&
      event.x == event_to_coalesce.x && event.y == event_to_coalesce.y)
    return true;

  return false;
}

void Coalesce(const WebGestureEvent& event_to_coalesce,
              WebGestureEvent* event) {
  DCHECK(CanCoalesce(event_to_coalesce, *event));
  if (event->type == WebInputEvent::GestureScrollUpdate) {
    event->data.scrollUpdate.deltaX +=
        event_to_coalesce.data.scrollUpdate.deltaX;
    event->data.scrollUpdate.deltaY +=
        event_to_coalesce.data.scrollUpdate.deltaY;
    DCHECK_EQ(
        event->data.scrollUpdate.previousUpdateInSequencePrevented,
        event_to_coalesce.data.scrollUpdate.previousUpdateInSequencePrevented);
  } else if (event->type == WebInputEvent::GesturePinchUpdate) {
    event->data.pinchUpdate.scale *= event_to_coalesce.data.pinchUpdate.scale;
    // Ensure the scale remains bounded above 0 and below Infinity so that
    // we can reliably perform operations like log on the values.
    if (event->data.pinchUpdate.scale < numeric_limits<float>::min())
      event->data.pinchUpdate.scale = numeric_limits<float>::min();
    else if (event->data.pinchUpdate.scale > numeric_limits<float>::max())
      event->data.pinchUpdate.scale = numeric_limits<float>::max();
  }
}

bool CanCoalesce(const blink::WebInputEvent& event_to_coalesce,
                 const blink::WebInputEvent& event) {
  if (blink::WebInputEvent::isGestureEventType(event_to_coalesce.type) &&
      blink::WebInputEvent::isGestureEventType(event.type)) {
    return CanCoalesce(
        static_cast<const blink::WebGestureEvent&>(event_to_coalesce),
        static_cast<const blink::WebGestureEvent&>(event));
  }
  if (blink::WebInputEvent::isMouseEventType(event_to_coalesce.type) &&
      blink::WebInputEvent::isMouseEventType(event.type)) {
    return CanCoalesce(
        static_cast<const blink::WebMouseEvent&>(event_to_coalesce),
        static_cast<const blink::WebMouseEvent&>(event));
  }
  if (blink::WebInputEvent::isTouchEventType(event_to_coalesce.type) &&
      blink::WebInputEvent::isTouchEventType(event.type)) {
    return CanCoalesce(
        static_cast<const blink::WebTouchEvent&>(event_to_coalesce),
        static_cast<const blink::WebTouchEvent&>(event));
  }
  if (event_to_coalesce.type == blink::WebInputEvent::MouseWheel &&
      event.type == blink::WebInputEvent::MouseWheel) {
    return CanCoalesce(
        static_cast<const blink::WebMouseWheelEvent&>(event_to_coalesce),
        static_cast<const blink::WebMouseWheelEvent&>(event));
  }
  return false;
}

void Coalesce(const blink::WebInputEvent& event_to_coalesce,
              blink::WebInputEvent* event) {
  if (blink::WebInputEvent::isGestureEventType(event_to_coalesce.type) &&
      blink::WebInputEvent::isGestureEventType(event->type)) {
    Coalesce(static_cast<const blink::WebGestureEvent&>(event_to_coalesce),
             static_cast<blink::WebGestureEvent*>(event));
    return;
  }
  if (blink::WebInputEvent::isMouseEventType(event_to_coalesce.type) &&
      blink::WebInputEvent::isMouseEventType(event->type)) {
    Coalesce(static_cast<const blink::WebMouseEvent&>(event_to_coalesce),
             static_cast<blink::WebMouseEvent*>(event));
    return;
  }
  if (blink::WebInputEvent::isTouchEventType(event_to_coalesce.type) &&
      blink::WebInputEvent::isTouchEventType(event->type)) {
    Coalesce(static_cast<const blink::WebTouchEvent&>(event_to_coalesce),
             static_cast<blink::WebTouchEvent*>(event));
    return;
  }
  if (event_to_coalesce.type == blink::WebInputEvent::MouseWheel &&
      event->type == blink::WebInputEvent::MouseWheel) {
    Coalesce(static_cast<const blink::WebMouseWheelEvent&>(event_to_coalesce),
             static_cast<blink::WebMouseWheelEvent*>(event));
  }
}

}  // namespace internal

ScopedWebInputEventWithLatencyInfo::ScopedWebInputEventWithLatencyInfo(
    ui::ScopedWebInputEvent event,
    const ui::LatencyInfo& latency_info)
    : event_(std::move(event)), latency_(latency_info) {
}

ScopedWebInputEventWithLatencyInfo::~ScopedWebInputEventWithLatencyInfo() {}

bool ScopedWebInputEventWithLatencyInfo::CanCoalesceWith(
    const ScopedWebInputEventWithLatencyInfo& other) const {
  return internal::CanCoalesce(other.event(), event());
}

void ScopedWebInputEventWithLatencyInfo::CoalesceWith(
    const ScopedWebInputEventWithLatencyInfo& other) {
  // |other| should be a newer event than |this|.
  if (other.latency_.trace_id() >= 0 && latency_.trace_id() >= 0)
    DCHECK_GT(other.latency_.trace_id(), latency_.trace_id());

  // New events get coalesced into older events, and the newer timestamp
  // should always be preserved.
  const double time_stamp_seconds = other.event().timeStampSeconds;
  internal::Coalesce(other.event(), event_.get());
  event_->timeStampSeconds = time_stamp_seconds;

  // When coalescing two input events, we keep the oldest LatencyInfo
  // since it will represent the longest latency.
  other.latency_ = latency_;
  other.latency_.set_coalesced();
}

const blink::WebInputEvent& ScopedWebInputEventWithLatencyInfo::event() const {
  return *event_;
}

blink::WebInputEvent& ScopedWebInputEventWithLatencyInfo::event() {
  return *event_;
}

}  // namespace content
