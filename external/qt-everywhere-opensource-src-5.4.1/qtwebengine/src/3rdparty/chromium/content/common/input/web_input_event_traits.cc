// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/web_input_event_traits.h"

#include <bitset>
#include <limits>

#include "base/logging.h"

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

bool CanCoalesce(const WebKeyboardEvent& event_to_coalesce,
                 const WebKeyboardEvent& event) {
  return false;
}

void Coalesce(const WebKeyboardEvent& event_to_coalesce,
              WebKeyboardEvent* event) {
  DCHECK(CanCoalesce(event_to_coalesce, *event));
}

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

float GetUnacceleratedDelta(float accelerated_delta, float acceleration_ratio) {
  return accelerated_delta * acceleration_ratio;
}

float GetAccelerationRatio(float accelerated_delta, float unaccelerated_delta) {
  if (unaccelerated_delta == 0.f || accelerated_delta == 0.f)
    return 1.f;
  return unaccelerated_delta / accelerated_delta;
}

void Coalesce(const WebMouseWheelEvent& event_to_coalesce,
              WebMouseWheelEvent* event) {
  DCHECK(CanCoalesce(event_to_coalesce, *event));
  float unaccelerated_x =
      GetUnacceleratedDelta(event->deltaX,
                            event->accelerationRatioX) +
      GetUnacceleratedDelta(event_to_coalesce.deltaX,
                            event_to_coalesce.accelerationRatioX);
  float unaccelerated_y =
      GetUnacceleratedDelta(event->deltaY,
                            event->accelerationRatioY) +
      GetUnacceleratedDelta(event_to_coalesce.deltaY,
                            event_to_coalesce.accelerationRatioY);
  event->deltaX += event_to_coalesce.deltaX;
  event->deltaY += event_to_coalesce.deltaY;
  event->wheelTicksX += event_to_coalesce.wheelTicksX;
  event->wheelTicksY += event_to_coalesce.wheelTicksY;
  event->accelerationRatioX =
      GetAccelerationRatio(event->deltaX, unaccelerated_x);
  event->accelerationRatioY =
      GetAccelerationRatio(event->deltaY, unaccelerated_y);
  DCHECK_GE(event_to_coalesce.timeStampSeconds, event->timeStampSeconds);
  event->timeStampSeconds = event_to_coalesce.timeStampSeconds;
}

// Returns |kInvalidTouchIndex| iff |event| lacks a touch with an ID of |id|.
int GetIndexOfTouchID(const WebTouchEvent& event, int id) {
  for (unsigned i = 0; i < event.touchesLength; ++i) {
    if (event.touches[i].id == id)
      return i;
  }
  return kInvalidTouchIndex;
}

bool CanCoalesce(const WebTouchEvent& event_to_coalesce,
                 const WebTouchEvent& event) {
  if (event.type != event_to_coalesce.type ||
      event.type != WebInputEvent::TouchMove ||
      event.modifiers != event_to_coalesce.modifiers ||
      event.touchesLength != event_to_coalesce.touchesLength ||
      event.touchesLength > WebTouchEvent::touchesLengthCap)
    return false;

  COMPILE_ASSERT(WebTouchEvent::touchesLengthCap <= sizeof(int32_t) * 8U,
                 suboptimal_touches_length_cap_size);
  // Ensure that we have a 1-to-1 mapping of pointer ids between touches.
  std::bitset<WebTouchEvent::touchesLengthCap> unmatched_event_touches(
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
      event.x == event_to_coalesce.x &&
      event.y == event_to_coalesce.y)
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

struct WebInputEventSize {
  template <class EventType>
  bool Execute(WebInputEvent::Type /* type */, size_t* type_size) const {
    *type_size = sizeof(EventType);
    return true;
  }
};

struct WebInputEventClone {
  template <class EventType>
  bool Execute(const WebInputEvent& event,
               ScopedWebInputEvent* scoped_event) const {
    DCHECK_EQ(sizeof(EventType), event.size);
    *scoped_event = ScopedWebInputEvent(
        new EventType(static_cast<const EventType&>(event)));
    return true;
  }
};

struct WebInputEventDelete {
  template <class EventType>
  bool Execute(WebInputEvent* event, bool* /* dummy_var */) const {
    if (!event)
      return false;
    DCHECK_EQ(sizeof(EventType), event->size);
    delete static_cast<EventType*>(event);
    return true;
  }
};

struct WebInputEventCanCoalesce {
  template <class EventType>
  bool Execute(const WebInputEvent& event_to_coalesce,
               const WebInputEvent* event) const {
    if (event_to_coalesce.type != event->type)
      return false;
    DCHECK_EQ(sizeof(EventType), event->size);
    DCHECK_EQ(sizeof(EventType), event_to_coalesce.size);
    return CanCoalesce(static_cast<const EventType&>(event_to_coalesce),
                       *static_cast<const EventType*>(event));
  }
};

struct WebInputEventCoalesce {
  template <class EventType>
  bool Execute(const WebInputEvent& event_to_coalesce,
               WebInputEvent* event) const {
    Coalesce(static_cast<const EventType&>(event_to_coalesce),
             static_cast<EventType*>(event));
    return true;
  }
};

template <typename Operator, typename ArgIn, typename ArgOut>
bool Apply(Operator op,
           WebInputEvent::Type type,
           const ArgIn& arg_in,
           ArgOut* arg_out) {
  if (WebInputEvent::isMouseEventType(type))
    return op.template Execute<WebMouseEvent>(arg_in, arg_out);
  else if (type == WebInputEvent::MouseWheel)
    return op.template Execute<WebMouseWheelEvent>(arg_in, arg_out);
  else if (WebInputEvent::isKeyboardEventType(type))
    return op.template Execute<WebKeyboardEvent>(arg_in, arg_out);
  else if (WebInputEvent::isTouchEventType(type))
    return op.template Execute<WebTouchEvent>(arg_in, arg_out);
  else if (WebInputEvent::isGestureEventType(type))
    return op.template Execute<WebGestureEvent>(arg_in, arg_out);

  NOTREACHED() << "Unknown webkit event type " << type;
  return false;
}

}  // namespace

const char* WebInputEventTraits::GetName(WebInputEvent::Type type) {
#define CASE_TYPE(t) case WebInputEvent::t:  return #t
  switch(type) {
    CASE_TYPE(Undefined);
    CASE_TYPE(MouseDown);
    CASE_TYPE(MouseUp);
    CASE_TYPE(MouseMove);
    CASE_TYPE(MouseEnter);
    CASE_TYPE(MouseLeave);
    CASE_TYPE(ContextMenu);
    CASE_TYPE(MouseWheel);
    CASE_TYPE(RawKeyDown);
    CASE_TYPE(KeyDown);
    CASE_TYPE(KeyUp);
    CASE_TYPE(Char);
    CASE_TYPE(GestureScrollBegin);
    CASE_TYPE(GestureScrollEnd);
    CASE_TYPE(GestureScrollUpdate);
    CASE_TYPE(GestureFlingStart);
    CASE_TYPE(GestureFlingCancel);
    CASE_TYPE(GestureShowPress);
    CASE_TYPE(GestureTap);
    CASE_TYPE(GestureTapUnconfirmed);
    CASE_TYPE(GestureTapDown);
    CASE_TYPE(GestureTapCancel);
    CASE_TYPE(GestureDoubleTap);
    CASE_TYPE(GestureTwoFingerTap);
    CASE_TYPE(GestureLongPress);
    CASE_TYPE(GestureLongTap);
    CASE_TYPE(GesturePinchBegin);
    CASE_TYPE(GesturePinchEnd);
    CASE_TYPE(GesturePinchUpdate);
    CASE_TYPE(TouchStart);
    CASE_TYPE(TouchMove);
    CASE_TYPE(TouchEnd);
    CASE_TYPE(TouchCancel);
    default:
      // Must include default to let blink::WebInputEvent add new event types
      // before they're added here.
      DLOG(WARNING) <<
          "Unhandled WebInputEvent type in WebInputEventTraits::GetName.\n";
      break;
  }
#undef CASE_TYPE
  return "";
}

size_t WebInputEventTraits::GetSize(WebInputEvent::Type type) {
  size_t size = 0;
  Apply(WebInputEventSize(), type, type, &size);
  return size;
}

ScopedWebInputEvent WebInputEventTraits::Clone(const WebInputEvent& event) {
  ScopedWebInputEvent scoped_event;
  Apply(WebInputEventClone(), event.type, event, &scoped_event);
  return scoped_event.Pass();
}

void WebInputEventTraits::Delete(WebInputEvent* event) {
  if (!event)
    return;
  bool dummy_var = false;
  Apply(WebInputEventDelete(), event->type, event, &dummy_var);
}

bool WebInputEventTraits::CanCoalesce(const WebInputEvent& event_to_coalesce,
                                      const WebInputEvent& event) {
  // Early out before casting.
  if (event_to_coalesce.type != event.type)
    return false;
  return Apply(WebInputEventCanCoalesce(),
               event.type,
               event_to_coalesce,
               &event);
}

void WebInputEventTraits::Coalesce(const WebInputEvent& event_to_coalesce,
                                   WebInputEvent* event) {
  DCHECK(event);
  Apply(WebInputEventCoalesce(), event->type, event_to_coalesce, event);
}

bool WebInputEventTraits::IgnoresAckDisposition(const WebInputEvent& event) {
  switch (event.type) {
    case WebInputEvent::MouseDown:
    case WebInputEvent::MouseUp:
    case WebInputEvent::MouseEnter:
    case WebInputEvent::MouseLeave:
    case WebInputEvent::ContextMenu:
    case WebInputEvent::GestureScrollBegin:
    case WebInputEvent::GestureScrollEnd:
    case WebInputEvent::GestureShowPress:
    case WebInputEvent::GestureTapUnconfirmed:
    case WebInputEvent::GestureTapDown:
    case WebInputEvent::GestureTapCancel:
    case WebInputEvent::GesturePinchBegin:
    case WebInputEvent::GesturePinchEnd:
    case WebInputEvent::TouchCancel:
      return true;
    case WebInputEvent::TouchStart:
    case WebInputEvent::TouchMove:
    case WebInputEvent::TouchEnd:
      return !static_cast<const WebTouchEvent&>(event).cancelable;
    default:
      return false;
  }
}

}  // namespace content
