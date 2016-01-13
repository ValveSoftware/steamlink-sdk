// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/ui_events_helper.h"

#include "content/common/input/web_touch_event_traits.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"

namespace {

int WebModifiersToUIFlags(int modifiers) {
  int flags = ui::EF_NONE;

  if (modifiers & blink::WebInputEvent::ShiftKey)
    flags |= ui::EF_SHIFT_DOWN;
  if (modifiers & blink::WebInputEvent::ControlKey)
    flags |= ui::EF_CONTROL_DOWN;
  if (modifiers & blink::WebInputEvent::AltKey)
    flags |= ui::EF_ALT_DOWN;

  if (modifiers & blink::WebInputEvent::LeftButtonDown)
    flags |= ui::EF_LEFT_MOUSE_BUTTON;
  if (modifiers & blink::WebInputEvent::RightButtonDown)
    flags |= ui::EF_RIGHT_MOUSE_BUTTON;
  if (modifiers & blink::WebInputEvent::MiddleButtonDown)
    flags |= ui::EF_MIDDLE_MOUSE_BUTTON;

  if (modifiers & blink::WebInputEvent::CapsLockOn)
    flags |= ui::EF_CAPS_LOCK_DOWN;

  return flags;
}

ui::EventType WebTouchPointStateToEventType(
    blink::WebTouchPoint::State state) {
  switch (state) {
    case blink::WebTouchPoint::StateReleased:
      return ui::ET_TOUCH_RELEASED;

    case blink::WebTouchPoint::StatePressed:
      return ui::ET_TOUCH_PRESSED;

    case blink::WebTouchPoint::StateMoved:
      return ui::ET_TOUCH_MOVED;

    case blink::WebTouchPoint::StateCancelled:
      return ui::ET_TOUCH_CANCELLED;

    default:
      return ui::ET_UNKNOWN;
  }
}

blink::WebTouchPoint::State TouchPointStateFromEvent(
    const ui::TouchEvent& event) {
  switch (event.type()) {
    case ui::ET_TOUCH_PRESSED:
      return blink::WebTouchPoint::StatePressed;
    case ui::ET_TOUCH_RELEASED:
      return blink::WebTouchPoint::StateReleased;
    case ui::ET_TOUCH_MOVED:
      return blink::WebTouchPoint::StateMoved;
    case ui::ET_TOUCH_CANCELLED:
      return blink::WebTouchPoint::StateCancelled;
    default:
      return blink::WebTouchPoint::StateUndefined;
  }
}

blink::WebInputEvent::Type TouchEventTypeFromEvent(
    const ui::TouchEvent& event) {
  switch (event.type()) {
    case ui::ET_TOUCH_PRESSED:
      return blink::WebInputEvent::TouchStart;
    case ui::ET_TOUCH_RELEASED:
      return blink::WebInputEvent::TouchEnd;
    case ui::ET_TOUCH_MOVED:
      return blink::WebInputEvent::TouchMove;
    case ui::ET_TOUCH_CANCELLED:
      return blink::WebInputEvent::TouchCancel;
    default:
      return blink::WebInputEvent::Undefined;
  }
}

}  // namespace

namespace content {

bool MakeUITouchEventsFromWebTouchEvents(
    const TouchEventWithLatencyInfo& touch_with_latency,
    ScopedVector<ui::TouchEvent>* list,
    TouchEventCoordinateSystem coordinate_system) {
  const blink::WebTouchEvent& touch = touch_with_latency.event;
  ui::EventType type = ui::ET_UNKNOWN;
  switch (touch.type) {
    case blink::WebInputEvent::TouchStart:
      type = ui::ET_TOUCH_PRESSED;
      break;
    case blink::WebInputEvent::TouchEnd:
      type = ui::ET_TOUCH_RELEASED;
      break;
    case blink::WebInputEvent::TouchMove:
      type = ui::ET_TOUCH_MOVED;
      break;
    case blink::WebInputEvent::TouchCancel:
      type = ui::ET_TOUCH_CANCELLED;
      break;
    default:
      NOTREACHED();
      return false;
  }

  int flags = WebModifiersToUIFlags(touch.modifiers);
  base::TimeDelta timestamp = base::TimeDelta::FromMicroseconds(
      static_cast<int64>(touch.timeStampSeconds * 1000000));
  for (unsigned i = 0; i < touch.touchesLength; ++i) {
    const blink::WebTouchPoint& point = touch.touches[i];
    if (WebTouchPointStateToEventType(point.state) != type)
      continue;
    // ui events start in the co-ordinate space of the EventDispatcher.
    gfx::PointF location;
    if (coordinate_system == LOCAL_COORDINATES)
      location = point.position;
    else
      location = point.screenPosition;
    ui::TouchEvent* uievent = new ui::TouchEvent(type,
          location,
          flags,
          point.id,
          timestamp,
          point.radiusX,
          point.radiusY,
          point.rotationAngle,
          point.force);
    uievent->set_latency(touch_with_latency.latency);
    list->push_back(uievent);
  }
  return true;
}

blink::WebGestureEvent MakeWebGestureEventFromUIEvent(
    const ui::GestureEvent& event) {
  blink::WebGestureEvent gesture_event;

  switch (event.type()) {
    case ui::ET_GESTURE_TAP:
      gesture_event.type = blink::WebInputEvent::GestureTap;
      gesture_event.data.tap.tapCount = event.details().tap_count();
      gesture_event.data.tap.width = event.details().bounding_box().width();
      gesture_event.data.tap.height = event.details().bounding_box().height();
      break;
    case ui::ET_GESTURE_TAP_DOWN:
      gesture_event.type = blink::WebInputEvent::GestureTapDown;
      gesture_event.data.tapDown.width =
          event.details().bounding_box().width();
      gesture_event.data.tapDown.height =
          event.details().bounding_box().height();
      break;
    case ui::ET_GESTURE_SHOW_PRESS:
      gesture_event.type = blink::WebInputEvent::GestureShowPress;
      gesture_event.data.showPress.width =
          event.details().bounding_box().width();
      gesture_event.data.showPress.height =
          event.details().bounding_box().height();
      break;
    case ui::ET_GESTURE_TAP_CANCEL:
      gesture_event.type = blink::WebInputEvent::GestureTapCancel;
      break;
    case ui::ET_GESTURE_SCROLL_BEGIN:
      gesture_event.type = blink::WebInputEvent::GestureScrollBegin;
      gesture_event.data.scrollBegin.deltaXHint =
          event.details().scroll_x_hint();
      gesture_event.data.scrollBegin.deltaYHint =
          event.details().scroll_y_hint();
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      gesture_event.type = blink::WebInputEvent::GestureScrollUpdate;
      gesture_event.data.scrollUpdate.deltaX = event.details().scroll_x();
      gesture_event.data.scrollUpdate.deltaY = event.details().scroll_y();
      break;
    case ui::ET_GESTURE_SCROLL_END:
      gesture_event.type = blink::WebInputEvent::GestureScrollEnd;
      break;
    case ui::ET_GESTURE_PINCH_BEGIN:
      gesture_event.type = blink::WebInputEvent::GesturePinchBegin;
      break;
    case ui::ET_GESTURE_PINCH_UPDATE:
      gesture_event.type = blink::WebInputEvent::GesturePinchUpdate;
      gesture_event.data.pinchUpdate.scale = event.details().scale();
      break;
    case ui::ET_GESTURE_PINCH_END:
      gesture_event.type = blink::WebInputEvent::GesturePinchEnd;
      break;
    case ui::ET_SCROLL_FLING_START:
      gesture_event.type = blink::WebInputEvent::GestureFlingStart;
      gesture_event.data.flingStart.velocityX = event.details().velocity_x();
      gesture_event.data.flingStart.velocityY = event.details().velocity_y();
      break;
    case ui::ET_SCROLL_FLING_CANCEL:
      gesture_event.type = blink::WebInputEvent::GestureFlingCancel;
      break;
    case ui::ET_GESTURE_LONG_PRESS:
      gesture_event.type = blink::WebInputEvent::GestureLongPress;
      gesture_event.data.longPress.width =
          event.details().bounding_box().width();
      gesture_event.data.longPress.height =
          event.details().bounding_box().height();
      break;
    case ui::ET_GESTURE_LONG_TAP:
      gesture_event.type = blink::WebInputEvent::GestureLongTap;
      gesture_event.data.longPress.width =
          event.details().bounding_box().width();
      gesture_event.data.longPress.height =
          event.details().bounding_box().height();
      break;
    case ui::ET_GESTURE_TWO_FINGER_TAP:
      gesture_event.type = blink::WebInputEvent::GestureTwoFingerTap;
      gesture_event.data.twoFingerTap.firstFingerWidth =
          event.details().first_finger_width();
      gesture_event.data.twoFingerTap.firstFingerHeight =
          event.details().first_finger_height();
      break;
    case ui::ET_GESTURE_BEGIN:
    case ui::ET_GESTURE_END:
    case ui::ET_GESTURE_SWIPE:
      gesture_event.type = blink::WebInputEvent::Undefined;
      break;
    default:
      NOTREACHED() << "Unknown gesture type: " << event.type();
  }

  gesture_event.sourceDevice = blink::WebGestureDeviceTouchscreen;
  gesture_event.modifiers = EventFlagsToWebEventModifiers(event.flags());
  gesture_event.timeStampSeconds = event.time_stamp().InSecondsF();

  return gesture_event;
}

int EventFlagsToWebEventModifiers(int flags) {
  int modifiers = 0;

  if (flags & ui::EF_SHIFT_DOWN)
    modifiers |= blink::WebInputEvent::ShiftKey;
  if (flags & ui::EF_CONTROL_DOWN)
    modifiers |= blink::WebInputEvent::ControlKey;
  if (flags & ui::EF_ALT_DOWN)
    modifiers |= blink::WebInputEvent::AltKey;
  // TODO(beng): MetaKey/META_MASK
  if (flags & ui::EF_LEFT_MOUSE_BUTTON)
    modifiers |= blink::WebInputEvent::LeftButtonDown;
  if (flags & ui::EF_MIDDLE_MOUSE_BUTTON)
    modifiers |= blink::WebInputEvent::MiddleButtonDown;
  if (flags & ui::EF_RIGHT_MOUSE_BUTTON)
    modifiers |= blink::WebInputEvent::RightButtonDown;
  if (flags & ui::EF_CAPS_LOCK_DOWN)
    modifiers |= blink::WebInputEvent::CapsLockOn;
  return modifiers;
}

blink::WebTouchPoint* UpdateWebTouchEventFromUIEvent(
    const ui::TouchEvent& event,
    blink::WebTouchEvent* web_event) {
  blink::WebTouchPoint* point = NULL;
  switch (event.type()) {
    case ui::ET_TOUCH_PRESSED:
      // Add a new touch point.
      if (web_event->touchesLength < blink::WebTouchEvent::touchesLengthCap) {
        point = &web_event->touches[web_event->touchesLength++];
        point->id = event.touch_id();
      }
      break;
    case ui::ET_TOUCH_RELEASED:
    case ui::ET_TOUCH_CANCELLED:
    case ui::ET_TOUCH_MOVED: {
      // The touch point should have been added to the event from an earlier
      // _PRESSED event. So find that.
      // At the moment, only a maximum of 4 touch-points are allowed. So a
      // simple loop should be sufficient.
      for (unsigned i = 0; i < web_event->touchesLength; ++i) {
        point = web_event->touches + i;
        if (point->id == event.touch_id())
          break;
        point = NULL;
      }
      break;
    }
    default:
      DLOG(WARNING) << "Unknown touch event " << event.type();
      break;
  }

  if (!point)
    return NULL;

  // The spec requires the radii values to be positive (and 1 when unknown).
  point->radiusX = std::max(1.f, event.radius_x());
  point->radiusY = std::max(1.f, event.radius_y());
  point->rotationAngle = event.rotation_angle();
  point->force = event.force();

  // Update the location and state of the point.
  point->state = TouchPointStateFromEvent(event);
  if (point->state == blink::WebTouchPoint::StateMoved) {
    // It is possible for badly written touch drivers to emit Move events even
    // when the touch location hasn't changed. In such cases, consume the event
    // and pretend nothing happened.
    if (point->position.x == event.x() && point->position.y == event.y())
      return NULL;
  }
  point->position.x = event.x();
  point->position.y = event.y();

  const gfx::PointF& root_point = event.root_location_f();
  point->screenPosition.x = root_point.x();
  point->screenPosition.y = root_point.y();

  // Mark the rest of the points as stationary.
  for (unsigned i = 0; i < web_event->touchesLength; ++i) {
    blink::WebTouchPoint* iter = web_event->touches + i;
    if (iter != point)
      iter->state = blink::WebTouchPoint::StateStationary;
  }

  // Update the type of the touch event.
  WebTouchEventTraits::ResetType(TouchEventTypeFromEvent(event),
                                 event.time_stamp().InSecondsF(),
                                 web_event);
  web_event->modifiers = EventFlagsToWebEventModifiers(event.flags());

  return point;
}

}  // namespace content
