// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/web_input_event_aura.h"

#include "build/build_config.h"
#include "content/browser/renderer_host/input/web_input_event_util.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"

namespace content {

namespace {

gfx::Point GetScreenLocationFromEvent(const ui::LocatedEvent& event) {
  if (!event.target())
    return event.root_location();

  aura::Window* root =
      static_cast<aura::Window*>(event.target())->GetRootWindow();
  aura::client::ScreenPositionClient* spc =
      aura::client::GetScreenPositionClient(root);
  if (!spc)
    return event.root_location();

  gfx::Point screen_location(event.root_location());
  spc->ConvertPointToScreen(root, &screen_location);
  return screen_location;
}

blink::WebPointerProperties::PointerType EventPointerTypeToWebPointerType(
    ui::EventPointerType pointer_type) {
  switch (pointer_type) {
    case ui::EventPointerType::POINTER_TYPE_UNKNOWN:
      return blink::WebPointerProperties::PointerType::Unknown;
    case ui::EventPointerType::POINTER_TYPE_MOUSE:
      return blink::WebPointerProperties::PointerType::Mouse;
    case ui::EventPointerType::POINTER_TYPE_PEN:
      return blink::WebPointerProperties::PointerType::Pen;
    case ui::EventPointerType::POINTER_TYPE_TOUCH:
      return blink::WebPointerProperties::PointerType::Touch;
  }
  NOTREACHED() << "Unexpected EventPointerType";
  return blink::WebPointerProperties::PointerType::Unknown;
}

// Creates a WebGestureEvent from a ui::GestureEvent. Note that it does not
// populate the event coordinates (i.e. |x|, |y|, |globalX|, and |globalY|). So
// the caller must populate these fields.
blink::WebGestureEvent MakeWebGestureEventFromUIEvent(
    const ui::GestureEvent& event) {
  return ui::CreateWebGestureEvent(
      event.details(), event.time_stamp(), event.location_f(),
      event.root_location_f(), event.flags(), event.unique_touch_event_id());
}

}  // namespace

#if defined(OS_WIN)
blink::WebMouseEvent MakeUntranslatedWebMouseEventFromNativeEvent(
    const base::NativeEvent& native_event,
    const base::TimeTicks& time_stamp,
    blink::WebPointerProperties::PointerType pointer_type);
blink::WebMouseWheelEvent MakeUntranslatedWebMouseWheelEventFromNativeEvent(
    const base::NativeEvent& native_event,
    const base::TimeTicks& time_stamp,
    blink::WebPointerProperties::PointerType pointer_type);
blink::WebKeyboardEvent MakeWebKeyboardEventFromNativeEvent(
    const base::NativeEvent& native_event,
    const base::TimeTicks& time_stamp);
blink::WebGestureEvent MakeWebGestureEventFromNativeEvent(
    const base::NativeEvent& native_event,
    const base::TimeTicks& time_stamp);
#endif

blink::WebKeyboardEvent MakeWebKeyboardEventFromAuraEvent(
    const ui::KeyEvent& event) {
  blink::WebKeyboardEvent webkit_event;

  webkit_event.timeStampSeconds =
      ui::EventTimeStampToSeconds(event.time_stamp());
  webkit_event.modifiers = ui::EventFlagsToWebEventModifiers(event.flags()) |
                           DomCodeToWebInputEventModifiers(event.code());

  switch (event.type()) {
    case ui::ET_KEY_PRESSED:
      webkit_event.type = event.is_char() ? blink::WebInputEvent::Char :
          blink::WebInputEvent::RawKeyDown;
      break;
    case ui::ET_KEY_RELEASED:
      webkit_event.type = blink::WebInputEvent::KeyUp;
      break;
    default:
      NOTREACHED();
  }

  if (webkit_event.modifiers & blink::WebInputEvent::AltKey)
    webkit_event.isSystemKey = true;

  // TODO(dtapuska): crbug.com/570388. Ozone appears to deliver
  // key_code events that aren't "located" for the keypad like
  // Windows and X11 do and blink expects.
  webkit_event.windowsKeyCode =
      ui::NonLocatedToLocatedKeypadKeyboardCode(event.key_code(), event.code());
  webkit_event.nativeKeyCode =
    ui::KeycodeConverter::DomCodeToNativeKeycode(event.code());
  webkit_event.domCode = static_cast<int>(event.code());
  webkit_event.domKey = static_cast<int>(event.GetDomKey());
  webkit_event.unmodifiedText[0] = event.GetUnmodifiedText();
  webkit_event.text[0] = event.GetText();

  webkit_event.setKeyIdentifierFromWindowsKeyCode();

  return webkit_event;
}

blink::WebMouseWheelEvent MakeWebMouseWheelEventFromAuraEvent(
    const ui::ScrollEvent& event) {
  blink::WebMouseWheelEvent webkit_event;

  webkit_event.type = blink::WebInputEvent::MouseWheel;
  webkit_event.button = blink::WebMouseEvent::ButtonNone;
  webkit_event.modifiers = ui::EventFlagsToWebEventModifiers(event.flags());
  webkit_event.timeStampSeconds =
      ui::EventTimeStampToSeconds(event.time_stamp());
  webkit_event.hasPreciseScrollingDeltas = true;

  float offset_ordinal_x = 0.f;
  float offset_ordinal_y = 0.f;
  if ((event.flags() & ui::EF_SHIFT_DOWN) != 0 && event.x_offset() == 0) {
    webkit_event.deltaX = event.y_offset();
    webkit_event.deltaY = 0;
    offset_ordinal_x = event.y_offset_ordinal();
    offset_ordinal_y = event.x_offset_ordinal();
  } else {
    webkit_event.deltaX = event.x_offset();
    webkit_event.deltaY = event.y_offset();
    offset_ordinal_x = event.x_offset_ordinal();
    offset_ordinal_y = event.y_offset_ordinal();
  }

  if (offset_ordinal_x != 0.f && webkit_event.deltaX != 0.f)
    webkit_event.accelerationRatioX = offset_ordinal_x / webkit_event.deltaX;
  webkit_event.wheelTicksX = webkit_event.deltaX / kPixelsPerTick;
  webkit_event.wheelTicksY = webkit_event.deltaY / kPixelsPerTick;
  if (offset_ordinal_y != 0.f && webkit_event.deltaY != 0.f)
    webkit_event.accelerationRatioY = offset_ordinal_y / webkit_event.deltaY;

  webkit_event.pointerType =
      EventPointerTypeToWebPointerType(event.pointer_details().pointer_type);
  return webkit_event;
}

blink::WebGestureEvent MakeWebGestureEventFromAuraEvent(
    const ui::ScrollEvent& event) {
  blink::WebGestureEvent webkit_event;

  switch (event.type()) {
    case ui::ET_SCROLL_FLING_START:
      webkit_event.type = blink::WebInputEvent::GestureFlingStart;
      webkit_event.data.flingStart.velocityX = event.x_offset();
      webkit_event.data.flingStart.velocityY = event.y_offset();
      break;
    case ui::ET_SCROLL_FLING_CANCEL:
      webkit_event.type = blink::WebInputEvent::GestureFlingCancel;
      break;
    case ui::ET_SCROLL:
      NOTREACHED() << "Invalid gesture type: " << event.type();
      break;
    default:
      NOTREACHED() << "Unknown gesture type: " << event.type();
  }

  webkit_event.sourceDevice = blink::WebGestureDeviceTouchpad;
  webkit_event.modifiers = ui::EventFlagsToWebEventModifiers(event.flags());
  webkit_event.timeStampSeconds =
      ui::EventTimeStampToSeconds(event.time_stamp());
  return webkit_event;
}

blink::WebMouseEvent MakeWebMouseEventFromAuraEvent(
    const ui::MouseEvent& event);
blink::WebMouseWheelEvent MakeWebMouseWheelEventFromAuraEvent(
    const ui::MouseWheelEvent& event);

// General approach:
//
// ui::Event only carries a subset of possible event data provided to Aura by
// the host platform. WebKit utilizes a larger subset of that information than
// Aura itself. WebKit includes some built in cracking functionality that we
// rely on to obtain this information cleanly and consistently.
//
// The only place where an ui::Event's data differs from what the underlying
// base::NativeEvent would provide is position data, since we would like to
// provide coordinates relative to the aura::Window that is hosting the
// renderer, not the top level platform window.
//
// The approach is to fully construct a blink::WebInputEvent from the
// ui::Event's base::NativeEvent, and then replace the coordinate fields with
// the translated values from the ui::Event.
//
// The exception is mouse events on linux. The ui::MouseEvent contains enough
// necessary information to construct a WebMouseEvent. So instead of extracting
// the information from the XEvent, which can be tricky when supporting both
// XInput2 and XInput, the WebMouseEvent is constructed from the
// ui::MouseEvent. This will not be necessary once only XInput2 is supported.
//

blink::WebMouseEvent MakeWebMouseEvent(const ui::MouseEvent& event) {
  // Construct an untranslated event from the platform event data.
  blink::WebMouseEvent webkit_event =
#if defined(OS_WIN)
      // On Windows we have WM_ events comming from desktop and pure aura
      // events comming from metro mode.
      event.native_event().message && (event.type() != ui::ET_MOUSE_EXITED)
          ? MakeUntranslatedWebMouseEventFromNativeEvent(
                event.native_event(), event.time_stamp(),
                EventPointerTypeToWebPointerType(
                    event.pointer_details().pointer_type))
          : MakeWebMouseEventFromAuraEvent(event);
#else
      MakeWebMouseEventFromAuraEvent(event);
#endif
  // Replace the event's coordinate fields with translated position data from
  // |event|.
  webkit_event.windowX = webkit_event.x = event.x();
  webkit_event.windowY = webkit_event.y = event.y();

#if defined(OS_WIN)
  if (event.native_event().message)
    return webkit_event;
#endif
  const gfx::Point screen_point = GetScreenLocationFromEvent(event);
  webkit_event.globalX = screen_point.x();
  webkit_event.globalY = screen_point.y();

  return webkit_event;
}

blink::WebMouseWheelEvent MakeWebMouseWheelEvent(
    const ui::MouseWheelEvent& event) {
#if defined(OS_WIN)
  // Construct an untranslated event from the platform event data.
  blink::WebMouseWheelEvent webkit_event =
      event.native_event().message
          ? MakeUntranslatedWebMouseWheelEventFromNativeEvent(
                event.native_event(), event.time_stamp(),
                EventPointerTypeToWebPointerType(
                    event.pointer_details().pointer_type))
          : MakeWebMouseWheelEventFromAuraEvent(event);
#else
  blink::WebMouseWheelEvent webkit_event =
      MakeWebMouseWheelEventFromAuraEvent(event);
#endif

  // Replace the event's coordinate fields with translated position data from
  // |event|.
  webkit_event.windowX = webkit_event.x = event.x();
  webkit_event.windowY = webkit_event.y = event.y();

  const gfx::Point screen_point = GetScreenLocationFromEvent(event);
  webkit_event.globalX = screen_point.x();
  webkit_event.globalY = screen_point.y();

  return webkit_event;
}

blink::WebMouseWheelEvent MakeWebMouseWheelEvent(const ui::ScrollEvent& event) {
#if defined(OS_WIN)
  // Construct an untranslated event from the platform event data.
  blink::WebMouseWheelEvent webkit_event =
      event.native_event().message
          ? MakeUntranslatedWebMouseWheelEventFromNativeEvent(
                event.native_event(), event.time_stamp(),
                EventPointerTypeToWebPointerType(
                    event.pointer_details().pointer_type))
          : MakeWebMouseWheelEventFromAuraEvent(event);
#else
  blink::WebMouseWheelEvent webkit_event =
      MakeWebMouseWheelEventFromAuraEvent(event);
#endif

  // Replace the event's coordinate fields with translated position data from
  // |event|.
  webkit_event.windowX = webkit_event.x = event.x();
  webkit_event.windowY = webkit_event.y = event.y();

  const gfx::Point screen_point = GetScreenLocationFromEvent(event);
  webkit_event.globalX = screen_point.x();
  webkit_event.globalY = screen_point.y();

  return webkit_event;
}

blink::WebKeyboardEvent MakeWebKeyboardEvent(const ui::KeyEvent& event) {
  // Windows can figure out whether or not to construct a RawKeyDown or a Char
  // WebInputEvent based on the type of message carried in
  // event.native_event(). X11 is not so fortunate, there is no separate
  // translated event type, so DesktopHostLinux sends an extra KeyEvent with
  // is_char() == true. We need to pass the ui::KeyEvent to the X11 function
  // to detect this case so the right event type can be constructed.
#if defined(OS_WIN)
  if (event.HasNativeEvent()) {
    // Key events require no translation by the aura system.
    blink::WebKeyboardEvent webkit_event(MakeWebKeyboardEventFromNativeEvent(
        event.native_event(), event.time_stamp()));
    webkit_event.modifiers |= DomCodeToWebInputEventModifiers(event.code());
    webkit_event.domCode = static_cast<int>(event.code());
    webkit_event.domKey = static_cast<int>(event.GetDomKey());
    return webkit_event;
  }
#endif
  return MakeWebKeyboardEventFromAuraEvent(event);
}

blink::WebGestureEvent MakeWebGestureEvent(const ui::GestureEvent& event) {
  blink::WebGestureEvent gesture_event;
#if defined(OS_WIN)
  if (event.HasNativeEvent())
    gesture_event = MakeWebGestureEventFromNativeEvent(event.native_event(),
                                                       event.time_stamp());
  else
    gesture_event = MakeWebGestureEventFromUIEvent(event);
#else
  gesture_event = MakeWebGestureEventFromUIEvent(event);
#endif

  gesture_event.x = event.x();
  gesture_event.y = event.y();

  const gfx::Point screen_point = GetScreenLocationFromEvent(event);
  gesture_event.globalX = screen_point.x();
  gesture_event.globalY = screen_point.y();

  return gesture_event;
}

blink::WebGestureEvent MakeWebGestureEvent(const ui::ScrollEvent& event) {
  blink::WebGestureEvent gesture_event;

#if defined(OS_WIN)
  gesture_event = MakeWebGestureEventFromNativeEvent(event.native_event(),
                                                     event.time_stamp());
#else
  gesture_event = MakeWebGestureEventFromAuraEvent(event);
#endif

  gesture_event.x = event.x();
  gesture_event.y = event.y();

  const gfx::Point screen_point = GetScreenLocationFromEvent(event);
  gesture_event.globalX = screen_point.x();
  gesture_event.globalY = screen_point.y();

  return gesture_event;
}

blink::WebGestureEvent MakeWebGestureEventFlingCancel() {
  blink::WebGestureEvent gesture_event;

  // All other fields are ignored on a GestureFlingCancel event.
  gesture_event.type = blink::WebInputEvent::GestureFlingCancel;
  gesture_event.timeStampSeconds =
      ui::EventTimeStampToSeconds(ui::EventTimeForNow());
  gesture_event.sourceDevice = blink::WebGestureDeviceTouchpad;
  return gesture_event;
}

blink::WebMouseEvent MakeWebMouseEventFromAuraEvent(
    const ui::MouseEvent& event) {
  blink::WebMouseEvent webkit_event;

  webkit_event.modifiers = ui::EventFlagsToWebEventModifiers(event.flags());
  webkit_event.timeStampSeconds =
      ui::EventTimeStampToSeconds(event.time_stamp());
  webkit_event.button = blink::WebMouseEvent::ButtonNone;
  int button_flags = event.flags();
  if (event.type() == ui::ET_MOUSE_PRESSED ||
      event.type() == ui::ET_MOUSE_RELEASED) {
    // We want to use changed_button_flags() for mouse pressed & released.
    // These flags can be used only if they are set which is not always the case
    // (see e.g. GetChangedMouseButtonFlagsFromNative() in events_win.cc).
    if (event.changed_button_flags())
      button_flags = event.changed_button_flags();
  }
  if (button_flags & ui::EF_LEFT_MOUSE_BUTTON)
    webkit_event.button = blink::WebMouseEvent::ButtonLeft;
  if (button_flags & ui::EF_MIDDLE_MOUSE_BUTTON)
    webkit_event.button = blink::WebMouseEvent::ButtonMiddle;
  if (button_flags & ui::EF_RIGHT_MOUSE_BUTTON)
    webkit_event.button = blink::WebMouseEvent::ButtonRight;

  switch (event.type()) {
    case ui::ET_MOUSE_PRESSED:
      webkit_event.type = blink::WebInputEvent::MouseDown;
      webkit_event.clickCount = event.GetClickCount();
      break;
    case ui::ET_MOUSE_RELEASED:
      webkit_event.type = blink::WebInputEvent::MouseUp;
      webkit_event.clickCount = event.GetClickCount();
      break;
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_MOUSE_EXITED:
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_DRAGGED:
      webkit_event.type = blink::WebInputEvent::MouseMove;
      break;
    default:
      NOTIMPLEMENTED() << "Received unexpected event: " << event.type();
      break;
  }

  webkit_event.tiltX = roundf(event.pointer_details().tilt_x);
  webkit_event.tiltY = roundf(event.pointer_details().tilt_y);
  webkit_event.force = event.pointer_details().force;
  webkit_event.pointerType =
      EventPointerTypeToWebPointerType(event.pointer_details().pointer_type);

  return webkit_event;
}

blink::WebMouseWheelEvent MakeWebMouseWheelEventFromAuraEvent(
    const ui::MouseWheelEvent& event) {
  blink::WebMouseWheelEvent webkit_event;

  webkit_event.type = blink::WebInputEvent::MouseWheel;
  webkit_event.button = blink::WebMouseEvent::ButtonNone;
  webkit_event.modifiers = ui::EventFlagsToWebEventModifiers(event.flags());
  webkit_event.timeStampSeconds =
      ui::EventTimeStampToSeconds(event.time_stamp());

  if ((event.flags() & ui::EF_SHIFT_DOWN) != 0 && event.x_offset() == 0) {
    webkit_event.deltaX = event.y_offset();
    webkit_event.deltaY = 0;
  } else {
    webkit_event.deltaX = event.x_offset();
    webkit_event.deltaY = event.y_offset();
  }

  webkit_event.wheelTicksX = webkit_event.deltaX / kPixelsPerTick;
  webkit_event.wheelTicksY = webkit_event.deltaY / kPixelsPerTick;

  webkit_event.tiltX = roundf(event.pointer_details().tilt_x);
  webkit_event.tiltY = roundf(event.pointer_details().tilt_y);
  webkit_event.force = event.pointer_details().force;
  webkit_event.pointerType =
      EventPointerTypeToWebPointerType(event.pointer_details().pointer_type);

  return webkit_event;
}

}  // namespace content
