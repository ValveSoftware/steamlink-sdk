// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/web_input_event_util.h"

#include "base/strings/string_util.h"
#include "content/common/input/web_touch_event_traits.h"
#include "ui/events/gesture_detection/gesture_event_data.h"
#include "ui/events/gesture_detection/motion_event.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;
using ui::MotionEvent;

namespace {

const char* GetKeyIdentifier(ui::KeyboardCode key_code) {
  switch (key_code) {
    case ui::VKEY_MENU:
      return "Alt";
    case ui::VKEY_CONTROL:
      return "Control";
    case ui::VKEY_SHIFT:
      return "Shift";
    case ui::VKEY_CAPITAL:
      return "CapsLock";
    case ui::VKEY_LWIN:
    case ui::VKEY_RWIN:
      return "Win";
    case ui::VKEY_CLEAR:
      return "Clear";
    case ui::VKEY_DOWN:
      return "Down";
    case ui::VKEY_END:
      return "End";
    case ui::VKEY_RETURN:
      return "Enter";
    case ui::VKEY_EXECUTE:
      return "Execute";
    case ui::VKEY_F1:
      return "F1";
    case ui::VKEY_F2:
      return "F2";
    case ui::VKEY_F3:
      return "F3";
    case ui::VKEY_F4:
      return "F4";
    case ui::VKEY_F5:
      return "F5";
    case ui::VKEY_F6:
      return "F6";
    case ui::VKEY_F7:
      return "F7";
    case ui::VKEY_F8:
      return "F8";
    case ui::VKEY_F9:
      return "F9";
    case ui::VKEY_F10:
      return "F10";
    case ui::VKEY_F11:
      return "F11";
    case ui::VKEY_F12:
      return "F12";
    case ui::VKEY_F13:
      return "F13";
    case ui::VKEY_F14:
      return "F14";
    case ui::VKEY_F15:
      return "F15";
    case ui::VKEY_F16:
      return "F16";
    case ui::VKEY_F17:
      return "F17";
    case ui::VKEY_F18:
      return "F18";
    case ui::VKEY_F19:
      return "F19";
    case ui::VKEY_F20:
      return "F20";
    case ui::VKEY_F21:
      return "F21";
    case ui::VKEY_F22:
      return "F22";
    case ui::VKEY_F23:
      return "F23";
    case ui::VKEY_F24:
      return "F24";
    case ui::VKEY_HELP:
      return "Help";
    case ui::VKEY_HOME:
      return "Home";
    case ui::VKEY_INSERT:
      return "Insert";
    case ui::VKEY_LEFT:
      return "Left";
    case ui::VKEY_NEXT:
      return "PageDown";
    case ui::VKEY_PRIOR:
      return "PageUp";
    case ui::VKEY_PAUSE:
      return "Pause";
    case ui::VKEY_SNAPSHOT:
      return "PrintScreen";
    case ui::VKEY_RIGHT:
      return "Right";
    case ui::VKEY_SCROLL:
      return "Scroll";
    case ui::VKEY_SELECT:
      return "Select";
    case ui::VKEY_UP:
      return "Up";
    case ui::VKEY_DELETE:
      return "U+007F";  // Standard says that DEL becomes U+007F.
    case ui::VKEY_MEDIA_NEXT_TRACK:
      return "MediaNextTrack";
    case ui::VKEY_MEDIA_PREV_TRACK:
      return "MediaPreviousTrack";
    case ui::VKEY_MEDIA_STOP:
      return "MediaStop";
    case ui::VKEY_MEDIA_PLAY_PAUSE:
      return "MediaPlayPause";
    case ui::VKEY_VOLUME_MUTE:
      return "VolumeMute";
    case ui::VKEY_VOLUME_DOWN:
      return "VolumeDown";
    case ui::VKEY_VOLUME_UP:
      return "VolumeUp";
    default:
      return NULL;
  };
}

WebInputEvent::Type ToWebInputEventType(MotionEvent::Action action) {
  switch (action) {
    case MotionEvent::ACTION_DOWN:
      return WebInputEvent::TouchStart;
    case MotionEvent::ACTION_MOVE:
      return WebInputEvent::TouchMove;
    case MotionEvent::ACTION_UP:
      return WebInputEvent::TouchEnd;
    case MotionEvent::ACTION_CANCEL:
      return WebInputEvent::TouchCancel;
    case MotionEvent::ACTION_POINTER_DOWN:
      return WebInputEvent::TouchStart;
    case MotionEvent::ACTION_POINTER_UP:
      return WebInputEvent::TouchEnd;
  }
  NOTREACHED() << "Invalid MotionEvent::Action.";
  return WebInputEvent::Undefined;
}

// Note that |is_action_pointer| is meaningful only in the context of
// |ACTION_POINTER_UP| and |ACTION_POINTER_DOWN|; other actions map directly to
// WebTouchPoint::State.
WebTouchPoint::State ToWebTouchPointState(MotionEvent::Action action,
                                          bool is_action_pointer) {
  switch (action) {
    case MotionEvent::ACTION_DOWN:
      return WebTouchPoint::StatePressed;
    case MotionEvent::ACTION_MOVE:
      return WebTouchPoint::StateMoved;
    case MotionEvent::ACTION_UP:
      return WebTouchPoint::StateReleased;
    case MotionEvent::ACTION_CANCEL:
      return WebTouchPoint::StateCancelled;
    case MotionEvent::ACTION_POINTER_DOWN:
      return is_action_pointer ? WebTouchPoint::StatePressed
                               : WebTouchPoint::StateStationary;
    case MotionEvent::ACTION_POINTER_UP:
      return is_action_pointer ? WebTouchPoint::StateReleased
                               : WebTouchPoint::StateStationary;
  }
  NOTREACHED() << "Invalid MotionEvent::Action.";
  return WebTouchPoint::StateUndefined;
}

WebTouchPoint CreateWebTouchPoint(const MotionEvent& event,
                                  size_t pointer_index) {
  WebTouchPoint touch;
  touch.id = event.GetPointerId(pointer_index);
  touch.state = ToWebTouchPointState(
      event.GetAction(),
      static_cast<int>(pointer_index) == event.GetActionIndex());
  touch.position.x = event.GetX(pointer_index);
  touch.position.y = event.GetY(pointer_index);
  touch.screenPosition.x = event.GetRawX(pointer_index);
  touch.screenPosition.y = event.GetRawY(pointer_index);
  touch.radiusX = touch.radiusY = event.GetTouchMajor(pointer_index) * 0.5f;
  touch.force = event.GetPressure(pointer_index);

  return touch;
}

}  // namespace

namespace content {

void UpdateWindowsKeyCodeAndKeyIdentifier(blink::WebKeyboardEvent* event,
                                          ui::KeyboardCode windows_key_code) {
  event->windowsKeyCode = windows_key_code;

  const char* id = GetKeyIdentifier(windows_key_code);
  if (id) {
    base::strlcpy(event->keyIdentifier, id, sizeof(event->keyIdentifier) - 1);
  } else {
    base::snprintf(event->keyIdentifier,
                   sizeof(event->keyIdentifier),
                   "U+%04X",
                   base::ToUpperASCII(static_cast<int>(windows_key_code)));
  }
}

blink::WebTouchEvent CreateWebTouchEventFromMotionEvent(
    const ui::MotionEvent& event) {
  blink::WebTouchEvent result;

  WebTouchEventTraits::ResetType(
      ToWebInputEventType(event.GetAction()),
      (event.GetEventTime() - base::TimeTicks()).InSecondsF(),
      &result);

  result.touchesLength =
      std::min(event.GetPointerCount(),
               static_cast<size_t>(WebTouchEvent::touchesLengthCap));
  DCHECK_GT(result.touchesLength, 0U);

  for (size_t i = 0; i < result.touchesLength; ++i)
    result.touches[i] = CreateWebTouchPoint(event, i);

  return result;
}

WebGestureEvent CreateWebGestureEventFromGestureEventData(
    const ui::GestureEventData& data) {
  WebGestureEvent gesture;
  gesture.x = data.x;
  gesture.y = data.y;
  gesture.globalX = data.raw_x;
  gesture.globalY = data.raw_y;
  gesture.timeStampSeconds = (data.time - base::TimeTicks()).InSecondsF();
  gesture.sourceDevice = blink::WebGestureDeviceTouchscreen;

  switch (data.type()) {
    case ui::ET_GESTURE_SHOW_PRESS:
      gesture.type = WebInputEvent::GestureShowPress;
      gesture.data.showPress.width = data.details.bounding_box_f().width();
      gesture.data.showPress.height = data.details.bounding_box_f().height();
      break;
    case ui::ET_GESTURE_DOUBLE_TAP:
      gesture.type = WebInputEvent::GestureDoubleTap;
      DCHECK_EQ(1, data.details.tap_count());
      gesture.data.tap.tapCount = data.details.tap_count();
      gesture.data.tap.width = data.details.bounding_box_f().width();
      gesture.data.tap.height = data.details.bounding_box_f().height();
      break;
    case ui::ET_GESTURE_TAP:
      gesture.type = WebInputEvent::GestureTap;
      DCHECK_EQ(1, data.details.tap_count());
      gesture.data.tap.tapCount = data.details.tap_count();
      gesture.data.tap.width = data.details.bounding_box_f().width();
      gesture.data.tap.height = data.details.bounding_box_f().height();
      break;
    case ui::ET_GESTURE_TAP_UNCONFIRMED:
      gesture.type = WebInputEvent::GestureTapUnconfirmed;
      DCHECK_EQ(1, data.details.tap_count());
      gesture.data.tap.tapCount = data.details.tap_count();
      gesture.data.tap.width = data.details.bounding_box_f().width();
      gesture.data.tap.height = data.details.bounding_box_f().height();
      break;
    case ui::ET_GESTURE_LONG_PRESS:
      gesture.type = WebInputEvent::GestureLongPress;
      gesture.data.longPress.width = data.details.bounding_box_f().width();
      gesture.data.longPress.height = data.details.bounding_box_f().height();
      break;
    case ui::ET_GESTURE_LONG_TAP:
      gesture.type = WebInputEvent::GestureLongTap;
      gesture.data.longPress.width = data.details.bounding_box_f().width();
      gesture.data.longPress.height = data.details.bounding_box_f().height();
      break;
    case ui::ET_GESTURE_SCROLL_BEGIN:
      gesture.type = WebInputEvent::GestureScrollBegin;
      gesture.data.scrollBegin.deltaXHint = data.details.scroll_x_hint();
      gesture.data.scrollBegin.deltaYHint = data.details.scroll_y_hint();
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      gesture.type = WebInputEvent::GestureScrollUpdate;
      gesture.data.scrollUpdate.deltaX = data.details.scroll_x();
      gesture.data.scrollUpdate.deltaY = data.details.scroll_y();
      break;
    case ui::ET_GESTURE_SCROLL_END:
      gesture.type = WebInputEvent::GestureScrollEnd;
      break;
    case ui::ET_SCROLL_FLING_START:
      gesture.type = WebInputEvent::GestureFlingStart;
      gesture.data.flingStart.velocityX = data.details.velocity_x();
      gesture.data.flingStart.velocityY = data.details.velocity_y();
      break;
    case ui::ET_SCROLL_FLING_CANCEL:
      gesture.type = WebInputEvent::GestureFlingCancel;
      break;
    case ui::ET_GESTURE_PINCH_BEGIN:
      gesture.type = WebInputEvent::GesturePinchBegin;
      break;
    case ui::ET_GESTURE_PINCH_UPDATE:
      gesture.type = WebInputEvent::GesturePinchUpdate;
      gesture.data.pinchUpdate.scale = data.details.scale();
      break;
    case ui::ET_GESTURE_PINCH_END:
      gesture.type = WebInputEvent::GesturePinchEnd;
      break;
    case ui::ET_GESTURE_TAP_CANCEL:
      gesture.type = WebInputEvent::GestureTapCancel;
      break;
    case ui::ET_GESTURE_TAP_DOWN:
      gesture.type = WebInputEvent::GestureTapDown;
      gesture.data.tapDown.width = data.details.bounding_box_f().width();
      gesture.data.tapDown.height = data.details.bounding_box_f().height();
      break;
    case ui::ET_GESTURE_BEGIN:
    case ui::ET_GESTURE_END:
      NOTREACHED() << "ET_GESTURE_BEGIN and ET_GESTURE_END are only produced "
                   << "in Aura, and should never end up here.";
      break;
    default:
      NOTREACHED() << "ui::EventType provided wasn't a valid gesture event.";
      break;
  }

  return gesture;
}

}  // namespace content
