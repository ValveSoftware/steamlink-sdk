// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/web_input_event_aura.h"

#include "content/browser/renderer_host/ui_events_helper.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"

#if defined(USE_OZONE)
#include "ui/events/keycodes/keyboard_code_conversion.h"
#endif

namespace content {

#if defined(USE_X11) || defined(USE_OZONE)
// From third_party/WebKit/Source/web/gtk/WebInputEventFactory.cpp:
blink::WebUChar GetControlCharacter(int windows_key_code, bool shift) {
  if (windows_key_code >= ui::VKEY_A &&
    windows_key_code <= ui::VKEY_Z) {
    // ctrl-A ~ ctrl-Z map to \x01 ~ \x1A
    return windows_key_code - ui::VKEY_A + 1;
  }
  if (shift) {
    // following graphics chars require shift key to input.
    switch (windows_key_code) {
      // ctrl-@ maps to \x00 (Null byte)
      case ui::VKEY_2:
        return 0;
      // ctrl-^ maps to \x1E (Record separator, Information separator two)
      case ui::VKEY_6:
        return 0x1E;
      // ctrl-_ maps to \x1F (Unit separator, Information separator one)
      case ui::VKEY_OEM_MINUS:
        return 0x1F;
      // Returns 0 for all other keys to avoid inputting unexpected chars.
      default:
        break;
    }
  } else {
    switch (windows_key_code) {
      // ctrl-[ maps to \x1B (Escape)
      case ui::VKEY_OEM_4:
        return 0x1B;
      // ctrl-\ maps to \x1C (File separator, Information separator four)
      case ui::VKEY_OEM_5:
        return 0x1C;
      // ctrl-] maps to \x1D (Group separator, Information separator three)
      case ui::VKEY_OEM_6:
        return 0x1D;
      // ctrl-Enter maps to \x0A (Line feed)
      case ui::VKEY_RETURN:
        return 0x0A;
      // Returns 0 for all other keys to avoid inputting unexpected chars.
      default:
        break;
    }
  }
  return 0;
}
#endif
#if defined(OS_WIN)
blink::WebMouseEvent MakeUntranslatedWebMouseEventFromNativeEvent(
    const base::NativeEvent& native_event);
blink::WebMouseWheelEvent MakeUntranslatedWebMouseWheelEventFromNativeEvent(
    const base::NativeEvent& native_event);
blink::WebKeyboardEvent MakeWebKeyboardEventFromNativeEvent(
    const base::NativeEvent& native_event);
blink::WebGestureEvent MakeWebGestureEventFromNativeEvent(
    const base::NativeEvent& native_event);
#elif defined(USE_X11)
blink::WebKeyboardEvent MakeWebKeyboardEventFromAuraEvent(
    ui::KeyEvent* event);
#elif defined(USE_OZONE)
blink::WebKeyboardEvent MakeWebKeyboardEventFromAuraEvent(
    ui::KeyEvent* event) {
  const base::NativeEvent& native_event = event->native_event();
  ui::EventType type = ui::EventTypeFromNative(native_event);
  blink::WebKeyboardEvent webkit_event;

  webkit_event.timeStampSeconds = event->time_stamp().InSecondsF();
  webkit_event.modifiers = EventFlagsToWebEventModifiers(event->flags());

  switch (type) {
    case ui::ET_KEY_PRESSED:
      webkit_event.type = event->is_char() ? blink::WebInputEvent::Char :
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

  wchar_t character = ui::KeyboardCodeFromNative(native_event);
  webkit_event.windowsKeyCode = character;
  webkit_event.nativeKeyCode = character;

  if (webkit_event.windowsKeyCode == ui::VKEY_RETURN)
    webkit_event.unmodifiedText[0] = '\r';
  else
    webkit_event.unmodifiedText[0] = ui::GetCharacterFromKeyCode(
        ui::KeyboardCodeFromNative(native_event),
        ui::EventFlagsFromNative(native_event));

  if (webkit_event.modifiers & blink::WebInputEvent::ControlKey) {
    webkit_event.text[0] =
        GetControlCharacter(
            webkit_event.windowsKeyCode,
            webkit_event.modifiers & blink::WebInputEvent::ShiftKey);
  } else {
    webkit_event.text[0] = webkit_event.unmodifiedText[0];
  }

  webkit_event.setKeyIdentifierFromWindowsKeyCode();

  return webkit_event;
}
#endif
#if defined(USE_X11) || defined(USE_OZONE)
blink::WebMouseWheelEvent MakeWebMouseWheelEventFromAuraEvent(
    ui::ScrollEvent* event) {
  blink::WebMouseWheelEvent webkit_event;

  webkit_event.type = blink::WebInputEvent::MouseWheel;
  webkit_event.button = blink::WebMouseEvent::ButtonNone;
  webkit_event.modifiers = EventFlagsToWebEventModifiers(event->flags());
  webkit_event.timeStampSeconds = event->time_stamp().InSecondsF();
  webkit_event.hasPreciseScrollingDeltas = true;

  float offset_ordinal_x = 0.f;
  float offset_ordinal_y = 0.f;
  if ((event->flags() & ui::EF_SHIFT_DOWN) != 0 && event->x_offset() == 0) {
    webkit_event.deltaX = event->y_offset();
    webkit_event.deltaY = 0;
    offset_ordinal_x = event->y_offset_ordinal();
    offset_ordinal_y = event->x_offset_ordinal();
  } else {
    webkit_event.deltaX = event->x_offset();
    webkit_event.deltaY = event->y_offset();
    offset_ordinal_x = event->x_offset_ordinal();
    offset_ordinal_y = event->y_offset_ordinal();
  }

  if (offset_ordinal_x != 0.f && webkit_event.deltaX != 0.f)
    webkit_event.accelerationRatioX = offset_ordinal_x / webkit_event.deltaX;
  webkit_event.wheelTicksX = webkit_event.deltaX / kPixelsPerTick;
  webkit_event.wheelTicksY = webkit_event.deltaY / kPixelsPerTick;
  if (offset_ordinal_y != 0.f && webkit_event.deltaY != 0.f)
    webkit_event.accelerationRatioY = offset_ordinal_y / webkit_event.deltaY;
  return webkit_event;
}

blink::WebGestureEvent MakeWebGestureEventFromAuraEvent(
    ui::ScrollEvent* event) {
  blink::WebGestureEvent webkit_event;

  switch (event->type()) {
    case ui::ET_SCROLL_FLING_START:
      webkit_event.type = blink::WebInputEvent::GestureFlingStart;
      webkit_event.data.flingStart.velocityX = event->x_offset();
      webkit_event.data.flingStart.velocityY = event->y_offset();
      break;
    case ui::ET_SCROLL_FLING_CANCEL:
      webkit_event.type = blink::WebInputEvent::GestureFlingCancel;
      break;
    case ui::ET_SCROLL:
      NOTREACHED() << "Invalid gesture type: " << event->type();
      break;
    default:
      NOTREACHED() << "Unknown gesture type: " << event->type();
  }

  webkit_event.sourceDevice = blink::WebGestureDeviceTouchpad;
  webkit_event.modifiers = EventFlagsToWebEventModifiers(event->flags());
  webkit_event.timeStampSeconds = event->time_stamp().InSecondsF();
  return webkit_event;
}

#endif

blink::WebMouseEvent MakeWebMouseEventFromAuraEvent(
    ui::MouseEvent* event);
blink::WebMouseWheelEvent MakeWebMouseWheelEventFromAuraEvent(
    ui::MouseWheelEvent* event);

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

blink::WebMouseEvent MakeWebMouseEvent(ui::MouseEvent* event) {
  // Construct an untranslated event from the platform event data.
  blink::WebMouseEvent webkit_event =
#if defined(OS_WIN)
  // On Windows we have WM_ events comming from desktop and pure aura
  // events comming from metro mode.
  event->native_event().message ?
      MakeUntranslatedWebMouseEventFromNativeEvent(event->native_event()) :
      MakeWebMouseEventFromAuraEvent(event);
#else
  MakeWebMouseEventFromAuraEvent(event);
#endif
  // Replace the event's coordinate fields with translated position data from
  // |event|.
  webkit_event.windowX = webkit_event.x = event->x();
  webkit_event.windowY = webkit_event.y = event->y();

#if defined(OS_WIN)
  if (event->native_event().message)
    return webkit_event;
#endif
  const gfx::Point root_point = event->root_location();
  webkit_event.globalX = root_point.x();
  webkit_event.globalY = root_point.y();

  return webkit_event;
}

blink::WebMouseWheelEvent MakeWebMouseWheelEvent(ui::MouseWheelEvent* event) {
#if defined(OS_WIN)
  // Construct an untranslated event from the platform event data.
  blink::WebMouseWheelEvent webkit_event = event->native_event().message ?
      MakeUntranslatedWebMouseWheelEventFromNativeEvent(event->native_event()) :
      MakeWebMouseWheelEventFromAuraEvent(event);
#else
  blink::WebMouseWheelEvent webkit_event =
      MakeWebMouseWheelEventFromAuraEvent(event);
#endif

  // Replace the event's coordinate fields with translated position data from
  // |event|.
  webkit_event.windowX = webkit_event.x = event->x();
  webkit_event.windowY = webkit_event.y = event->y();

  const gfx::Point root_point = event->root_location();
  webkit_event.globalX = root_point.x();
  webkit_event.globalY = root_point.y();

  return webkit_event;
}

blink::WebMouseWheelEvent MakeWebMouseWheelEvent(ui::ScrollEvent* event) {
#if defined(OS_WIN)
  // Construct an untranslated event from the platform event data.
  blink::WebMouseWheelEvent webkit_event =
      MakeUntranslatedWebMouseWheelEventFromNativeEvent(event->native_event());
#else
  blink::WebMouseWheelEvent webkit_event =
      MakeWebMouseWheelEventFromAuraEvent(event);
#endif

  // Replace the event's coordinate fields with translated position data from
  // |event|.
  webkit_event.windowX = webkit_event.x = event->x();
  webkit_event.windowY = webkit_event.y = event->y();

  const gfx::Point root_point = event->root_location();
  webkit_event.globalX = root_point.x();
  webkit_event.globalY = root_point.y();

  return webkit_event;
}

blink::WebKeyboardEvent MakeWebKeyboardEvent(ui::KeyEvent* event) {
  if (!event->HasNativeEvent())
    return blink::WebKeyboardEvent();

  // Windows can figure out whether or not to construct a RawKeyDown or a Char
  // WebInputEvent based on the type of message carried in
  // event->native_event(). X11 is not so fortunate, there is no separate
  // translated event type, so DesktopHostLinux sends an extra KeyEvent with
  // is_char() == true. We need to pass the ui::KeyEvent to the X11 function
  // to detect this case so the right event type can be constructed.
#if defined(OS_WIN)
  // Key events require no translation by the aura system.
  return MakeWebKeyboardEventFromNativeEvent(event->native_event());
#else
  return MakeWebKeyboardEventFromAuraEvent(event);
#endif
}

blink::WebGestureEvent MakeWebGestureEvent(ui::GestureEvent* event) {
  blink::WebGestureEvent gesture_event;
#if defined(OS_WIN)
  if (event->HasNativeEvent())
    gesture_event = MakeWebGestureEventFromNativeEvent(event->native_event());
  else
    gesture_event = MakeWebGestureEventFromUIEvent(*event);
#else
  gesture_event = MakeWebGestureEventFromUIEvent(*event);
#endif

  gesture_event.x = event->x();
  gesture_event.y = event->y();

  const gfx::Point root_point = event->root_location();
  gesture_event.globalX = root_point.x();
  gesture_event.globalY = root_point.y();

  return gesture_event;
}

blink::WebGestureEvent MakeWebGestureEvent(ui::ScrollEvent* event) {
  blink::WebGestureEvent gesture_event;

#if defined(OS_WIN)
  gesture_event = MakeWebGestureEventFromNativeEvent(event->native_event());
#else
  gesture_event = MakeWebGestureEventFromAuraEvent(event);
#endif

  gesture_event.x = event->x();
  gesture_event.y = event->y();

  const gfx::Point root_point = event->root_location();
  gesture_event.globalX = root_point.x();
  gesture_event.globalY = root_point.y();

  return gesture_event;
}

blink::WebGestureEvent MakeWebGestureEventFlingCancel() {
  blink::WebGestureEvent gesture_event;

  // All other fields are ignored on a GestureFlingCancel event.
  gesture_event.type = blink::WebInputEvent::GestureFlingCancel;
  gesture_event.sourceDevice = blink::WebGestureDeviceTouchpad;
  return gesture_event;
}

blink::WebMouseEvent MakeWebMouseEventFromAuraEvent(ui::MouseEvent* event) {
  blink::WebMouseEvent webkit_event;

  webkit_event.modifiers = EventFlagsToWebEventModifiers(event->flags());
  webkit_event.timeStampSeconds = event->time_stamp().InSecondsF();

  webkit_event.button = blink::WebMouseEvent::ButtonNone;
  if (event->flags() & ui::EF_LEFT_MOUSE_BUTTON)
    webkit_event.button = blink::WebMouseEvent::ButtonLeft;
  if (event->flags() & ui::EF_MIDDLE_MOUSE_BUTTON)
    webkit_event.button = blink::WebMouseEvent::ButtonMiddle;
  if (event->flags() & ui::EF_RIGHT_MOUSE_BUTTON)
    webkit_event.button = blink::WebMouseEvent::ButtonRight;

  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
      webkit_event.type = blink::WebInputEvent::MouseDown;
      webkit_event.clickCount = event->GetClickCount();
      break;
    case ui::ET_MOUSE_RELEASED:
      webkit_event.type = blink::WebInputEvent::MouseUp;
      webkit_event.clickCount = event->GetClickCount();
      break;
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_MOUSE_EXITED:
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_DRAGGED:
      webkit_event.type = blink::WebInputEvent::MouseMove;
      break;
    default:
      NOTIMPLEMENTED() << "Received unexpected event: " << event->type();
      break;
  }

  return webkit_event;
}

blink::WebMouseWheelEvent MakeWebMouseWheelEventFromAuraEvent(
    ui::MouseWheelEvent* event) {
  blink::WebMouseWheelEvent webkit_event;

  webkit_event.type = blink::WebInputEvent::MouseWheel;
  webkit_event.button = blink::WebMouseEvent::ButtonNone;
  webkit_event.modifiers = EventFlagsToWebEventModifiers(event->flags());
  webkit_event.timeStampSeconds = event->time_stamp().InSecondsF();

  if ((event->flags() & ui::EF_SHIFT_DOWN) != 0 && event->x_offset() == 0) {
    webkit_event.deltaX = event->y_offset();
    webkit_event.deltaY = 0;
  } else {
    webkit_event.deltaX = event->x_offset();
    webkit_event.deltaY = event->y_offset();
  }

  webkit_event.wheelTicksX = webkit_event.deltaX / kPixelsPerTick;
  webkit_event.wheelTicksY = webkit_event.deltaY / kPixelsPerTick;

  return webkit_event;
}

}  // namespace content
