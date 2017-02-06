// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/web_input_event_traits.h"

#include <bitset>
#include <limits>

#include "base/logging.h"
#include "base/strings/stringprintf.h"

using base::StringAppendF;
using base::SStringPrintf;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;
using std::numeric_limits;

namespace content {
namespace {

const int kInvalidTouchIndex = -1;

void ApppendEventDetails(const WebKeyboardEvent& event, std::string* result) {
  StringAppendF(result,
                "{\n WinCode: %d\n NativeCode: %d\n IsSystem: %d\n"
                " Text: %s\n UnmodifiedText: %s\n KeyIdentifier: %s\n}",
                event.windowsKeyCode,
                event.nativeKeyCode,
                event.isSystemKey,
                reinterpret_cast<const char*>(event.text),
                reinterpret_cast<const char*>(event.unmodifiedText),
                reinterpret_cast<const char*>(event.keyIdentifier));
}

void ApppendEventDetails(const WebMouseEvent& event, std::string* result) {
  StringAppendF(result,
                "{\n Button: %d\n Pos: (%d, %d)\n WindowPos: (%d, %d)\n"
                " GlobalPos: (%d, %d)\n Movement: (%d, %d)\n Clicks: %d\n}",
                event.button,
                event.x,
                event.y,
                event.windowX,
                event.windowY,
                event.globalX,
                event.globalY,
                event.movementX,
                event.movementY,
                event.clickCount);
}

void ApppendEventDetails(const WebMouseWheelEvent& event, std::string* result) {
  StringAppendF(
      result,
      "{\n Delta: (%f, %f)\n WheelTicks: (%f, %f)\n Accel: (%f, %f)\n"
      " ScrollByPage: %d\n HasPreciseScrollingDeltas: %d\n"
      " Phase: (%d, %d)\n CanRubberband: (%d, %d)\n}",
      event.deltaX, event.deltaY, event.wheelTicksX, event.wheelTicksY,
      event.accelerationRatioX, event.accelerationRatioY, event.scrollByPage,
      event.hasPreciseScrollingDeltas, event.phase, event.momentumPhase,
      event.canRubberbandLeft, event.canRubberbandRight);
}

void ApppendEventDetails(const WebGestureEvent& event, std::string* result) {
  StringAppendF(result,
                "{\n Pos: (%d, %d)\n GlobalPos: (%d, %d)\n SourceDevice: %d\n"
                " RawData: (%f, %f, %f, %f, %d)\n}",
                event.x,
                event.y,
                event.globalX,
                event.globalY,
                event.sourceDevice,
                event.data.scrollUpdate.deltaX,
                event.data.scrollUpdate.deltaY,
                event.data.scrollUpdate.velocityX,
                event.data.scrollUpdate.velocityY,
                event.data.scrollUpdate.previousUpdateInSequencePrevented);
}

void ApppendTouchPointDetails(const WebTouchPoint& point, std::string* result) {
  StringAppendF(result,
                 "  (ID: %d, State: %d, ScreenPos: (%f, %f), Pos: (%f, %f),"
                     " Radius: (%f, %f), Rot: %f, Force: %f,"
                     " Tilt: (%d, %d)),\n",
  point.id,
  point.state,
  point.screenPosition.x,
  point.screenPosition.y,
  point.position.x,
  point.position.y,
  point.radiusX,
  point.radiusY,
  point.rotationAngle,
  point.force,
  point.tiltX,
  point.tiltY);
}

void ApppendEventDetails(const WebTouchEvent& event, std::string* result) {
  StringAppendF(result,
                "{\n Touches: %u, DispatchType: %d, CausesScrolling: %d,"
                " uniqueTouchEventId: %u\n[\n",
                event.touchesLength, event.dispatchType,
                event.movedBeyondSlopRegion, event.uniqueTouchEventId);
  for (unsigned i = 0; i < event.touchesLength; ++i)
    ApppendTouchPointDetails(event.touches[i], result);
  result->append(" ]\n}");
}

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
          WebInputEvent::DispatchType::ListenersForcedNonBlockingPassive,
      "Enum not ordered correctly");
  return static_cast<WebInputEvent::DispatchType>(
      std::min(static_cast<int>(type_1), static_cast<int>(type_2)));
}

bool CanCoalesce(const WebTouchEvent& event_to_coalesce,
                 const WebTouchEvent& event) {
  if (event.type != event_to_coalesce.type ||
      event.type != WebInputEvent::TouchMove ||
      event.modifiers != event_to_coalesce.modifiers ||
      event.touchesLength != event_to_coalesce.touchesLength ||
      event.touchesLength > WebTouchEvent::touchesLengthCap)
    return false;

  static_assert(WebTouchEvent::touchesLengthCap <= sizeof(int32_t) * 8U,
                "suboptimal touchesLengthCap size");
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

struct WebInputEventToString {
  template <class EventType>
  bool Execute(const WebInputEvent& event, std::string* result) const {
    SStringPrintf(result, "%s (Time: %lf, Modifiers: %d)\n",
                  WebInputEventTraits::GetName(event.type),
                  event.timeStampSeconds,
                  event.modifiers);
    const EventType& typed_event = static_cast<const EventType&>(event);
    ApppendEventDetails(typed_event, result);
    return true;
  }
};

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
    // New events get coalesced into older events, and the newer timestamp
    // should always be preserved.
    const double time_stamp_seconds = event_to_coalesce.timeStampSeconds;
    Coalesce(static_cast<const EventType&>(event_to_coalesce),
             static_cast<EventType*>(event));
    event->timeStampSeconds = time_stamp_seconds;
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
    CASE_TYPE(TouchScrollStarted);
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

std::string WebInputEventTraits::ToString(const WebInputEvent& event) {
  std::string result;
  Apply(WebInputEventToString(), event.type, event, &result);
  return result;
}

size_t WebInputEventTraits::GetSize(WebInputEvent::Type type) {
  size_t size = 0;
  Apply(WebInputEventSize(), type, type, &size);
  return size;
}

ScopedWebInputEvent WebInputEventTraits::Clone(const WebInputEvent& event) {
  ScopedWebInputEvent scoped_event;
  Apply(WebInputEventClone(), event.type, event, &scoped_event);
  return scoped_event;
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

bool WebInputEventTraits::ShouldBlockEventStream(const WebInputEvent& event) {
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
      return false;

    // TouchCancel and TouchScrollStarted should always be non-blocking.
    case WebInputEvent::TouchCancel:
    case WebInputEvent::TouchScrollStarted:
      DCHECK_NE(WebInputEvent::Blocking,
                static_cast<const WebTouchEvent&>(event).dispatchType);
      return false;

    // Touch start and touch end indicate whether they are non-blocking
    // (aka uncancelable) on the event.
    case WebInputEvent::TouchStart:
    case WebInputEvent::TouchEnd:
      return static_cast<const WebTouchEvent&>(event).dispatchType ==
             WebInputEvent::Blocking;

    // Touch move events may be non-blocking but are always explicitly
    // acknowledge by the renderer so they block the event stream.
    case WebInputEvent::TouchMove:
    default:
      return true;
  }
}

bool WebInputEventTraits::CanCauseScroll(
    const blink::WebMouseWheelEvent& event) {
#if defined(USE_AURA)
  // Scroll events generated from the mouse wheel when the control key is held
  // don't trigger scrolling. Instead, they may cause zooming.
  return event.hasPreciseScrollingDeltas ||
         (event.modifiers & blink::WebInputEvent::ControlKey) == 0;
#else
  return true;
#endif
}

uint32_t WebInputEventTraits::GetUniqueTouchEventId(
    const WebInputEvent& event) {
  if (WebInputEvent::isTouchEventType(event.type)) {
    return static_cast<const WebTouchEvent&>(event).uniqueTouchEventId;
  }
  return 0U;
}

}  // namespace content
