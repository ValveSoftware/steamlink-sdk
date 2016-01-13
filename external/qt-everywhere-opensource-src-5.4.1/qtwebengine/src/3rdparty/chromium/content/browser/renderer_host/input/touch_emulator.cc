// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touch_emulator.h"

#include "content/browser/renderer_host/input/motion_event_web.h"
#include "content/browser/renderer_host/input/web_input_event_util.h"
#include "content/common/input/web_touch_event_traits.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "grit/content_resources.h"
#include "third_party/WebKit/public/platform/WebCursorInfo.h"
#include "ui/events/gesture_detection/gesture_config_helper.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/screen.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace content {

namespace {

ui::GestureProvider::Config GetGestureProviderConfig() {
  // TODO(dgozman): Use different configs to emulate mobile/desktop as
  // requested by renderer.
  ui::GestureProvider::Config config = ui::DefaultGestureProviderConfig();
  config.gesture_begin_end_types_enabled = false;
  config.gesture_detector_config.swipe_enabled = false;
  config.gesture_detector_config.two_finger_tap_enabled = false;
  return config;
}

// Time between two consecutive mouse moves, during which second mouse move
// is not converted to touch.
const double kMouseMoveDropIntervalSeconds = 5.f / 1000;

} // namespace

TouchEmulator::TouchEmulator(TouchEmulatorClient* client)
    : client_(client),
      gesture_provider_(GetGestureProviderConfig(), this),
      enabled_(false),
      allow_pinch_(false) {
  DCHECK(client_);
  ResetState();

  bool use_2x = gfx::Screen::GetNativeScreen()->
      GetPrimaryDisplay().device_scale_factor() > 1.5f;
  float cursor_scale_factor = use_2x ? 2.f : 1.f;
  InitCursorFromResource(&touch_cursor_,
      cursor_scale_factor,
      use_2x ? IDR_DEVTOOLS_TOUCH_CURSOR_ICON_2X :
          IDR_DEVTOOLS_TOUCH_CURSOR_ICON);
  InitCursorFromResource(&pinch_cursor_,
      cursor_scale_factor,
      use_2x ? IDR_DEVTOOLS_PINCH_CURSOR_ICON_2X :
          IDR_DEVTOOLS_PINCH_CURSOR_ICON);

  WebCursor::CursorInfo cursor_info;
  cursor_info.type = blink::WebCursorInfo::TypePointer;
  pointer_cursor_.InitFromCursorInfo(cursor_info);

  // TODO(dgozman): Use synthetic secondary touch to support multi-touch.
  gesture_provider_.SetMultiTouchZoomSupportEnabled(false);
  // TODO(dgozman): Enable double tap if requested by the renderer.
  // TODO(dgozman): Don't break double-tap-based pinch with shift handling.
  gesture_provider_.SetDoubleTapSupportForPlatformEnabled(false);
}

TouchEmulator::~TouchEmulator() {
  // We cannot cleanup properly in destructor, as we need roundtrip to the
  // renderer for ack. Instead, the owner should call Disable, and only
  // destroy this object when renderer is dead.
}

void TouchEmulator::ResetState() {
  last_mouse_event_was_move_ = false;
  last_mouse_move_timestamp_ = 0;
  mouse_pressed_ = false;
  shift_pressed_ = false;
  touch_active_ = false;
  suppress_next_fling_cancel_ = false;
  pinch_scale_ = 1.f;
  pinch_gesture_active_ = false;
}

void TouchEmulator::Enable(bool allow_pinch) {
  if (!enabled_) {
    enabled_ = true;
    ResetState();
  }
  allow_pinch_ = allow_pinch;
  UpdateCursor();
}

void TouchEmulator::Disable() {
  if (!enabled_)
    return;

  enabled_ = false;
  UpdateCursor();
  CancelTouch();
}

void TouchEmulator::InitCursorFromResource(
    WebCursor* cursor, float scale, int resource_id) {
  gfx::Image& cursor_image =
      content::GetContentClient()->GetNativeImageNamed(resource_id);
  WebCursor::CursorInfo cursor_info;
  cursor_info.type = blink::WebCursorInfo::TypeCustom;
  cursor_info.image_scale_factor = scale;
  cursor_info.custom_image = cursor_image.AsBitmap();
  cursor_info.hotspot =
      gfx::Point(cursor_image.Width() / 2, cursor_image.Height() / 2);
#if defined(OS_WIN)
  cursor_info.external_handle = 0;
#endif

  cursor->InitFromCursorInfo(cursor_info);
}

bool TouchEmulator::HandleMouseEvent(const WebMouseEvent& mouse_event) {
  if (!enabled_)
    return false;

  if (mouse_event.button == WebMouseEvent::ButtonRight &&
      mouse_event.type == WebInputEvent::MouseDown) {
    client_->ShowContextMenuAtPoint(gfx::Point(mouse_event.x, mouse_event.y));
  }

  if (mouse_event.button != WebMouseEvent::ButtonLeft)
    return true;

  if (mouse_event.type == WebInputEvent::MouseMove) {
    if (last_mouse_event_was_move_ &&
        mouse_event.timeStampSeconds < last_mouse_move_timestamp_ +
            kMouseMoveDropIntervalSeconds)
      return true;

    last_mouse_event_was_move_ = true;
    last_mouse_move_timestamp_ = mouse_event.timeStampSeconds;
  } else {
    last_mouse_event_was_move_ = false;
  }

  if (mouse_event.type == WebInputEvent::MouseDown)
    mouse_pressed_ = true;
  else if (mouse_event.type == WebInputEvent::MouseUp)
    mouse_pressed_ = false;

  UpdateShiftPressed((mouse_event.modifiers & WebInputEvent::ShiftKey) != 0);

  if (FillTouchEventAndPoint(mouse_event) &&
      gesture_provider_.OnTouchEvent(MotionEventWeb(touch_event_))) {
    client_->ForwardTouchEvent(touch_event_);
  }

  // Do not pass mouse events to the renderer.
  return true;
}

bool TouchEmulator::HandleMouseWheelEvent(const WebMouseWheelEvent& event) {
  if (!enabled_)
    return false;

  // Send mouse wheel for easy scrolling when there is no active touch.
  return touch_active_;
}

bool TouchEmulator::HandleKeyboardEvent(const WebKeyboardEvent& event) {
  if (!enabled_)
    return false;

  if (!UpdateShiftPressed((event.modifiers & WebInputEvent::ShiftKey) != 0))
    return false;

  if (!mouse_pressed_)
    return false;

  // Note: The necessary pinch events will be lazily inserted by
  // |OnGestureEvent| depending on the state of |shift_pressed_|, using the
  // scroll stream as the event driver.
  if (shift_pressed_) {
    // TODO(dgozman): Add secondary touch point and set anchor.
  } else {
    // TODO(dgozman): Remove secondary touch point and anchor.
  }

  // Never block keyboard events.
  return false;
}

bool TouchEmulator::HandleTouchEventAck(InputEventAckState ack_result) {
  const bool event_consumed = ack_result == INPUT_EVENT_ACK_STATE_CONSUMED;
  gesture_provider_.OnTouchEventAck(event_consumed);
  // TODO(dgozman): Disable emulation when real touch events are available.
  return true;
}

void TouchEmulator::OnGestureEvent(const ui::GestureEventData& gesture) {
  WebGestureEvent gesture_event =
      CreateWebGestureEventFromGestureEventData(gesture);

  switch (gesture_event.type) {
    case WebInputEvent::Undefined:
      NOTREACHED() << "Undefined WebInputEvent type";
      // Bail without sending the junk event to the client.
      return;

    case WebInputEvent::GestureScrollBegin:
      client_->ForwardGestureEvent(gesture_event);
      // PinchBegin must always follow ScrollBegin.
      if (InPinchGestureMode())
        PinchBegin(gesture_event);
      break;

    case WebInputEvent::GestureScrollUpdate:
      if (InPinchGestureMode()) {
        // Convert scrolls to pinches while shift is pressed.
        if (!pinch_gesture_active_)
          PinchBegin(gesture_event);
        else
          PinchUpdate(gesture_event);
      } else {
        // Pass scroll update further. If shift was released, end the pinch.
        if (pinch_gesture_active_)
          PinchEnd(gesture_event);
        client_->ForwardGestureEvent(gesture_event);
      }
      break;

    case WebInputEvent::GestureScrollEnd:
      // PinchEnd must precede ScrollEnd.
      if (pinch_gesture_active_)
        PinchEnd(gesture_event);
      client_->ForwardGestureEvent(gesture_event);
      break;

    case WebInputEvent::GestureFlingStart:
      // PinchEnd must precede FlingStart.
      if (pinch_gesture_active_)
        PinchEnd(gesture_event);
      if (InPinchGestureMode()) {
        // No fling in pinch mode. Forward scroll end instead of fling start.
        suppress_next_fling_cancel_ = true;
        ScrollEnd(gesture_event);
      } else {
        suppress_next_fling_cancel_ = false;
        client_->ForwardGestureEvent(gesture_event);
      }
      break;

    case WebInputEvent::GestureFlingCancel:
      // If fling start was suppressed, we should not send fling cancel either.
      if (!suppress_next_fling_cancel_)
        client_->ForwardGestureEvent(gesture_event);
      suppress_next_fling_cancel_ = false;
      break;

    default:
      // Everything else goes through.
      client_->ForwardGestureEvent(gesture_event);
  }
}

void TouchEmulator::CancelTouch() {
  if (!touch_active_)
    return;

  WebTouchEventTraits::ResetTypeAndTouchStates(
      WebInputEvent::TouchCancel,
      (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF(),
      &touch_event_);
  touch_active_ = false;
  if (gesture_provider_.OnTouchEvent(MotionEventWeb(touch_event_)))
    client_->ForwardTouchEvent(touch_event_);
}

void TouchEmulator::UpdateCursor() {
  if (!enabled_)
    client_->SetCursor(pointer_cursor_);
  else
    client_->SetCursor(InPinchGestureMode() ? pinch_cursor_ : touch_cursor_);
}

bool TouchEmulator::UpdateShiftPressed(bool shift_pressed) {
  if (shift_pressed_ == shift_pressed)
    return false;
  shift_pressed_ = shift_pressed;
  UpdateCursor();
  return true;
}

void TouchEmulator::PinchBegin(const WebGestureEvent& event) {
  DCHECK(InPinchGestureMode());
  DCHECK(!pinch_gesture_active_);
  pinch_gesture_active_ = true;
  pinch_anchor_ = gfx::Point(event.x, event.y);
  pinch_scale_ = 1.f;
  FillPinchEvent(event);
  pinch_event_.type = WebInputEvent::GesturePinchBegin;
  client_->ForwardGestureEvent(pinch_event_);
}

void TouchEmulator::PinchUpdate(const WebGestureEvent& event) {
  DCHECK(pinch_gesture_active_);
  int dy = pinch_anchor_.y() - event.y;
  float scale = exp(dy * 0.002f);
  FillPinchEvent(event);
  pinch_event_.type = WebInputEvent::GesturePinchUpdate;
  pinch_event_.data.pinchUpdate.scale = scale / pinch_scale_;
  client_->ForwardGestureEvent(pinch_event_);
  pinch_scale_ = scale;
}

void TouchEmulator::PinchEnd(const WebGestureEvent& event) {
  DCHECK(pinch_gesture_active_);
  pinch_gesture_active_ = false;
  FillPinchEvent(event);
  pinch_event_.type = WebInputEvent::GesturePinchEnd;
  client_->ForwardGestureEvent(pinch_event_);
}

void TouchEmulator::FillPinchEvent(const WebInputEvent& event) {
  pinch_event_.timeStampSeconds = event.timeStampSeconds;
  pinch_event_.modifiers = event.modifiers;
  pinch_event_.sourceDevice = blink::WebGestureDeviceTouchscreen;
  pinch_event_.x = pinch_anchor_.x();
  pinch_event_.y = pinch_anchor_.y();
}

void TouchEmulator::ScrollEnd(const WebGestureEvent& event) {
  WebGestureEvent scroll_event;
  scroll_event.timeStampSeconds = event.timeStampSeconds;
  scroll_event.modifiers = event.modifiers;
  scroll_event.sourceDevice = blink::WebGestureDeviceTouchscreen;
  scroll_event.type = WebInputEvent::GestureScrollEnd;
  client_->ForwardGestureEvent(scroll_event);
}

bool TouchEmulator::FillTouchEventAndPoint(const WebMouseEvent& mouse_event) {
  if (mouse_event.type != WebInputEvent::MouseDown &&
      mouse_event.type != WebInputEvent::MouseMove &&
      mouse_event.type != WebInputEvent::MouseUp) {
    return false;
  }

  WebInputEvent::Type eventType;
  switch (mouse_event.type) {
    case WebInputEvent::MouseDown:
      eventType = WebInputEvent::TouchStart;
      touch_active_ = true;
      break;
    case WebInputEvent::MouseMove:
      eventType = WebInputEvent::TouchMove;
      break;
    case WebInputEvent::MouseUp:
      eventType = WebInputEvent::TouchEnd;
      touch_active_ = false;
      break;
    default:
      eventType = WebInputEvent::Undefined;
      NOTREACHED();
  }
  touch_event_.touchesLength = 1;
  touch_event_.modifiers = mouse_event.modifiers;
  WebTouchEventTraits::ResetTypeAndTouchStates(
      eventType, mouse_event.timeStampSeconds, &touch_event_);

  WebTouchPoint& point = touch_event_.touches[0];
  point.id = 0;
  point.radiusX = point.radiusY = 1.f;
  point.force = 1.f;
  point.rotationAngle = 0.f;
  point.position.x = mouse_event.x;
  point.screenPosition.x = mouse_event.globalX;
  point.position.y = mouse_event.y;
  point.screenPosition.y = mouse_event.globalY;

  return true;
}

bool TouchEmulator::InPinchGestureMode() const {
  return shift_pressed_ && allow_pinch_;
}

}  // namespace content
