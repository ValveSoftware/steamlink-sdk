// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/web_input_event_traits.h"

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

namespace ui {
namespace {

void ApppendEventDetails(const WebKeyboardEvent& event, std::string* result) {
  StringAppendF(result,
                "{\n WinCode: %d\n NativeCode: %d\n IsSystem: %d\n"
                " Text: %s\n UnmodifiedText: %s\n}",
                event.windowsKeyCode,
                event.nativeKeyCode,
                event.isSystemKey,
                reinterpret_cast<const char*>(event.text),
                reinterpret_cast<const char*>(event.unmodifiedText));
}

void ApppendEventDetails(const WebMouseEvent& event, std::string* result) {
  StringAppendF(result,
                "{\n Button: %d\n Pos: (%d, %d)\n WindowPos: (%d, %d)\n"
                " GlobalPos: (%d, %d)\n Movement: (%d, %d)\n Clicks: %d\n}",
                static_cast<int>(event.button),
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
      " Phase: (%d, %d)",
      event.deltaX, event.deltaY, event.wheelTicksX, event.wheelTicksY,
      event.accelerationRatioX, event.accelerationRatioY, event.scrollByPage,
      event.hasPreciseScrollingDeltas, event.phase, event.momentumPhase);
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

struct WebInputEventToString {
  template <class EventType>
  bool Execute(const WebInputEvent& event, std::string* result) const {
    SStringPrintf(result, "%s (Time: %lf, Modifiers: %d)\n",
                  WebInputEvent::GetName(event.type), event.timeStampSeconds,
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

// static
LatencyInfo WebInputEventTraits::CreateLatencyInfoForWebGestureEvent(
    WebGestureEvent event) {
  SourceEventType source_event_type = SourceEventType::UNKNOWN;
  if (event.sourceDevice == blink::WebGestureDevice::WebGestureDeviceTouchpad) {
    source_event_type = SourceEventType::WHEEL;
  } else if (event.sourceDevice ==
             blink::WebGestureDevice::WebGestureDeviceTouchscreen) {
    source_event_type = SourceEventType::TOUCH;
  }
  LatencyInfo latency_info(source_event_type);
  return latency_info;
}

}  // namespace ui
