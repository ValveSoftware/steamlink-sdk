// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/web_input_event.h"

#include "ui/events/base_event_utils.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"

#if defined(OS_WIN)
#include "ui/events/blink/web_input_event_builders_win.h"
#endif

namespace ui {

namespace {

gfx::Point GetScreenLocationFromEvent(
    const LocatedEvent& event,
    const base::Callback<gfx::Point(const LocatedEvent& event)>&
        screen_location_callback) {
  DCHECK(!screen_location_callback.is_null());
  return event.target() ? screen_location_callback.Run(event)
                        : event.root_location();
}

blink::WebPointerProperties::PointerType EventPointerTypeToWebPointerType(
    EventPointerType pointer_type) {
  switch (pointer_type) {
    case EventPointerType::POINTER_TYPE_UNKNOWN:
      return blink::WebPointerProperties::PointerType::Unknown;
    case EventPointerType::POINTER_TYPE_MOUSE:
      return blink::WebPointerProperties::PointerType::Mouse;
    case EventPointerType::POINTER_TYPE_PEN:
      return blink::WebPointerProperties::PointerType::Pen;
    case EventPointerType::POINTER_TYPE_ERASER:
      return blink::WebPointerProperties::PointerType::Eraser;
    case EventPointerType::POINTER_TYPE_TOUCH:
      return blink::WebPointerProperties::PointerType::Touch;
  }
  NOTREACHED() << "Unexpected EventPointerType";
  return blink::WebPointerProperties::PointerType::Unknown;
}

// Creates a WebGestureEvent from a GestureEvent. Note that it does not
// populate the event coordinates (i.e. |x|, |y|, |globalX|, and |globalY|). So
// the caller must populate these fields.
blink::WebGestureEvent MakeWebGestureEventFromUIEvent(
    const GestureEvent& event) {
  return CreateWebGestureEvent(event.details(), event.time_stamp(),
                               event.location_f(), event.root_location_f(),
                               event.flags(), event.unique_touch_event_id());
}

}  // namespace

#if defined(OS_WIN)
// On Windows, we can just use the builtin WebKit factory methods to fully
// construct our pre-translated events.

blink::WebMouseEvent MakeUntranslatedWebMouseEventFromNativeEvent(
    const base::NativeEvent& native_event,
    const base::TimeTicks& time_stamp,
    blink::WebPointerProperties::PointerType pointer_type) {
  return WebMouseEventBuilder::Build(
      native_event.hwnd, native_event.message, native_event.wParam,
      native_event.lParam, EventTimeStampToSeconds(time_stamp), pointer_type);
}

blink::WebMouseWheelEvent MakeUntranslatedWebMouseWheelEventFromNativeEvent(
    const base::NativeEvent& native_event,
    const base::TimeTicks& time_stamp,
    blink::WebPointerProperties::PointerType pointer_type) {
  return WebMouseWheelEventBuilder::Build(
      native_event.hwnd, native_event.message, native_event.wParam,
      native_event.lParam, EventTimeStampToSeconds(time_stamp), pointer_type);
}

blink::WebKeyboardEvent MakeWebKeyboardEventFromNativeEvent(
    const base::NativeEvent& native_event,
    const base::TimeTicks& time_stamp) {
  return WebKeyboardEventBuilder::Build(
      native_event.hwnd, native_event.message, native_event.wParam,
      native_event.lParam, EventTimeStampToSeconds(time_stamp));
}
#endif  // defined(OS_WIN)

blink::WebKeyboardEvent MakeWebKeyboardEventFromUiEvent(const KeyEvent& event) {
  blink::WebKeyboardEvent webkit_event;

  webkit_event.timeStampSeconds = EventTimeStampToSeconds(event.time_stamp());
  webkit_event.modifiers = EventFlagsToWebEventModifiers(event.flags()) |
                           DomCodeToWebInputEventModifiers(event.code());

  switch (event.type()) {
    case ET_KEY_PRESSED:
      webkit_event.type = event.is_char() ? blink::WebInputEvent::Char
                                          : blink::WebInputEvent::RawKeyDown;
      break;
    case ET_KEY_RELEASED:
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
      NonLocatedToLocatedKeypadKeyboardCode(event.key_code(), event.code());
  webkit_event.nativeKeyCode =
      KeycodeConverter::DomCodeToNativeKeycode(event.code());
  webkit_event.domCode = static_cast<int>(event.code());
  webkit_event.domKey = static_cast<int>(event.GetDomKey());
  webkit_event.unmodifiedText[0] = event.GetUnmodifiedText();
  webkit_event.text[0] = event.GetText();

  return webkit_event;
}

blink::WebMouseWheelEvent MakeWebMouseWheelEventFromUiEvent(
    const ScrollEvent& event) {
  blink::WebMouseWheelEvent webkit_event;

  webkit_event.type = blink::WebInputEvent::MouseWheel;
  webkit_event.button = blink::WebMouseEvent::Button::NoButton;
  webkit_event.modifiers = EventFlagsToWebEventModifiers(event.flags());
  webkit_event.timeStampSeconds = EventTimeStampToSeconds(event.time_stamp());
  webkit_event.hasPreciseScrollingDeltas = true;

  float offset_ordinal_x = event.x_offset_ordinal();
  float offset_ordinal_y = event.y_offset_ordinal();
  webkit_event.deltaX = event.x_offset();
  webkit_event.deltaY = event.y_offset();

  if (offset_ordinal_x != 0.f && webkit_event.deltaX != 0.f)
    webkit_event.accelerationRatioX = offset_ordinal_x / webkit_event.deltaX;
  webkit_event.wheelTicksX = webkit_event.deltaX / MouseWheelEvent::kWheelDelta;
  webkit_event.wheelTicksY = webkit_event.deltaY / MouseWheelEvent::kWheelDelta;
  if (offset_ordinal_y != 0.f && webkit_event.deltaY != 0.f)
    webkit_event.accelerationRatioY = offset_ordinal_y / webkit_event.deltaY;

  webkit_event.pointerType =
      EventPointerTypeToWebPointerType(event.pointer_details().pointer_type);
  return webkit_event;
}

blink::WebGestureEvent MakeWebGestureEventFromUiEvent(
    const ScrollEvent& event) {
  blink::WebGestureEvent webkit_event;

  switch (event.type()) {
    case ET_SCROLL_FLING_START:
      webkit_event.type = blink::WebInputEvent::GestureFlingStart;
      webkit_event.data.flingStart.velocityX = event.x_offset();
      webkit_event.data.flingStart.velocityY = event.y_offset();
      break;
    case ET_SCROLL_FLING_CANCEL:
      webkit_event.type = blink::WebInputEvent::GestureFlingCancel;
      break;
    case ET_SCROLL:
      NOTREACHED() << "Invalid gesture type: " << event.type();
      break;
    default:
      NOTREACHED() << "Unknown gesture type: " << event.type();
  }

  webkit_event.sourceDevice = blink::WebGestureDeviceTouchpad;
  webkit_event.modifiers = EventFlagsToWebEventModifiers(event.flags());
  webkit_event.timeStampSeconds = EventTimeStampToSeconds(event.time_stamp());
  return webkit_event;
}

blink::WebMouseEvent MakeWebMouseEventFromUiEvent(const MouseEvent& event);
blink::WebMouseWheelEvent MakeWebMouseWheelEventFromUiEvent(
    const MouseWheelEvent& event);

// General approach:
//
// Event only carries a subset of possible event data provided to UI by the host
// platform. WebKit utilizes a larger subset of that information, and includes
// some built in cracking functionality that we rely on to obtain this
// information cleanly and consistently.
//
// The only place where an Event's data differs from what the underlying
// base::NativeEvent would provide is position data. We would like to provide
// coordinates relative to its hosting window, rather than the top level
// platform window. To do this a callback is accepted to allow for clients to
// map the coordinates.
//
// The approach is to fully construct a blink::WebInputEvent from the
// Event's base::NativeEvent, and then replace the coordinate fields with
// the translated values from the Event.
//
// The exception is mouse events on linux. The MouseEvent contains enough
// necessary information to construct a WebMouseEvent. So instead of extracting
// the information from the XEvent, which can be tricky when supporting both
// XInput2 and XInput, the WebMouseEvent is constructed from the
// MouseEvent. This will not be necessary once only XInput2 is supported.
//

blink::WebMouseEvent MakeWebMouseEvent(
    const MouseEvent& event,
    const base::Callback<gfx::Point(const LocatedEvent& event)>&
        screen_location_callback) {
  // Construct an untranslated event from the platform event data.
  blink::WebMouseEvent webkit_event =
#if defined(OS_WIN)
      // On Windows we have WM_ events comming from desktop and pure Events
      // comming from metro mode.
      event.native_event().message && (event.type() != ET_MOUSE_EXITED)
          ? MakeUntranslatedWebMouseEventFromNativeEvent(
                event.native_event(), event.time_stamp(),
                EventPointerTypeToWebPointerType(
                    event.pointer_details().pointer_type))
          : MakeWebMouseEventFromUiEvent(event);
#else
      MakeWebMouseEventFromUiEvent(event);
#endif
  // Replace the event's coordinate fields with translated position data from
  // |event|.
  webkit_event.windowX = webkit_event.x = event.x();
  webkit_event.windowY = webkit_event.y = event.y();

#if defined(OS_WIN)
  if (event.native_event().message)
    return webkit_event;
#endif

  const gfx::Point screen_point =
      GetScreenLocationFromEvent(event, screen_location_callback);
  webkit_event.globalX = screen_point.x();
  webkit_event.globalY = screen_point.y();

  return webkit_event;
}

blink::WebMouseWheelEvent MakeWebMouseWheelEvent(
    const MouseWheelEvent& event,
    const base::Callback<gfx::Point(const LocatedEvent& event)>&
        screen_location_callback) {
#if defined(OS_WIN)
  // Construct an untranslated event from the platform event data.
  blink::WebMouseWheelEvent webkit_event =
      event.native_event().message
          ? MakeUntranslatedWebMouseWheelEventFromNativeEvent(
                event.native_event(), event.time_stamp(),
                EventPointerTypeToWebPointerType(
                    event.pointer_details().pointer_type))
          : MakeWebMouseWheelEventFromUiEvent(event);
#else
  blink::WebMouseWheelEvent webkit_event =
      MakeWebMouseWheelEventFromUiEvent(event);
#endif

  // Replace the event's coordinate fields with translated position data from
  // |event|.
  webkit_event.windowX = webkit_event.x = event.x();
  webkit_event.windowY = webkit_event.y = event.y();

  const gfx::Point screen_point =
      GetScreenLocationFromEvent(event, screen_location_callback);
  webkit_event.globalX = screen_point.x();
  webkit_event.globalY = screen_point.y();

  return webkit_event;
}

blink::WebMouseWheelEvent MakeWebMouseWheelEvent(
    const ScrollEvent& event,
    const base::Callback<gfx::Point(const LocatedEvent& event)>&
        screen_location_callback) {
#if defined(OS_WIN)
  // Construct an untranslated event from the platform event data.
  blink::WebMouseWheelEvent webkit_event =
      event.native_event().message
          ? MakeUntranslatedWebMouseWheelEventFromNativeEvent(
                event.native_event(), event.time_stamp(),
                EventPointerTypeToWebPointerType(
                    event.pointer_details().pointer_type))
          : MakeWebMouseWheelEventFromUiEvent(event);
#else
  blink::WebMouseWheelEvent webkit_event =
      MakeWebMouseWheelEventFromUiEvent(event);
#endif

  // Replace the event's coordinate fields with translated position data from
  // |event|.
  webkit_event.windowX = webkit_event.x = event.x();
  webkit_event.windowY = webkit_event.y = event.y();

  const gfx::Point screen_point =
      GetScreenLocationFromEvent(event, screen_location_callback);
  webkit_event.globalX = screen_point.x();
  webkit_event.globalY = screen_point.y();

  return webkit_event;
}

blink::WebKeyboardEvent MakeWebKeyboardEvent(const KeyEvent& event) {
// Windows can figure out whether or not to construct a RawKeyDown or a Char
// WebInputEvent based on the type of message carried in
// event.native_event(). X11 is not so fortunate, there is no separate
// translated event type, so DesktopHostLinux sends an extra KeyEvent with
// is_char() == true. We need to pass the KeyEvent to the X11 function
// to detect this case so the right event type can be constructed.
#if defined(OS_WIN)
  if (event.HasNativeEvent()) {
    // Key events require no translation.
    blink::WebKeyboardEvent webkit_event(MakeWebKeyboardEventFromNativeEvent(
        event.native_event(), event.time_stamp()));
    webkit_event.modifiers |= DomCodeToWebInputEventModifiers(event.code());
    webkit_event.domCode = static_cast<int>(event.code());
    webkit_event.domKey = static_cast<int>(event.GetDomKey());
    return webkit_event;
  }
#endif
  return MakeWebKeyboardEventFromUiEvent(event);
}

blink::WebGestureEvent MakeWebGestureEvent(
    const GestureEvent& event,
    const base::Callback<gfx::Point(const LocatedEvent& event)>&
        screen_location_callback) {
  blink::WebGestureEvent gesture_event = MakeWebGestureEventFromUIEvent(event);

  gesture_event.x = event.x();
  gesture_event.y = event.y();

  const gfx::Point screen_point =
      GetScreenLocationFromEvent(event, screen_location_callback);
  gesture_event.globalX = screen_point.x();
  gesture_event.globalY = screen_point.y();

  return gesture_event;
}

blink::WebGestureEvent MakeWebGestureEvent(
    const ScrollEvent& event,
    const base::Callback<gfx::Point(const LocatedEvent& event)>&
        screen_location_callback) {
  blink::WebGestureEvent gesture_event = MakeWebGestureEventFromUiEvent(event);
  gesture_event.x = event.x();
  gesture_event.y = event.y();

  const gfx::Point screen_point =
      GetScreenLocationFromEvent(event, screen_location_callback);
  gesture_event.globalX = screen_point.x();
  gesture_event.globalY = screen_point.y();

  return gesture_event;
}

blink::WebGestureEvent MakeWebGestureEventFlingCancel() {
  blink::WebGestureEvent gesture_event;

  // All other fields are ignored on a GestureFlingCancel event.
  gesture_event.type = blink::WebInputEvent::GestureFlingCancel;
  gesture_event.timeStampSeconds = EventTimeStampToSeconds(EventTimeForNow());
  gesture_event.sourceDevice = blink::WebGestureDeviceTouchpad;
  return gesture_event;
}

blink::WebMouseEvent MakeWebMouseEventFromUiEvent(const MouseEvent& event) {
  blink::WebMouseEvent webkit_event;

  webkit_event.modifiers = EventFlagsToWebEventModifiers(event.flags());
  webkit_event.timeStampSeconds = EventTimeStampToSeconds(event.time_stamp());
  webkit_event.button = blink::WebMouseEvent::Button::NoButton;
  int button_flags = event.flags();
  if (event.type() == ET_MOUSE_PRESSED || event.type() == ET_MOUSE_RELEASED) {
    // We want to use changed_button_flags() for mouse pressed & released.
    // These flags can be used only if they are set which is not always the case
    // (see e.g. GetChangedMouseButtonFlagsFromNative() in events_win.cc).
    if (event.changed_button_flags())
      button_flags = event.changed_button_flags();
  }
  if (button_flags & EF_LEFT_MOUSE_BUTTON)
    webkit_event.button = blink::WebMouseEvent::Button::Left;
  if (button_flags & EF_MIDDLE_MOUSE_BUTTON)
    webkit_event.button = blink::WebMouseEvent::Button::Middle;
  if (button_flags & EF_RIGHT_MOUSE_BUTTON)
    webkit_event.button = blink::WebMouseEvent::Button::Right;

  switch (event.type()) {
    case ET_MOUSE_PRESSED:
      webkit_event.type = blink::WebInputEvent::MouseDown;
      webkit_event.clickCount = event.GetClickCount();
      break;
    case ET_MOUSE_RELEASED:
      webkit_event.type = blink::WebInputEvent::MouseUp;
      webkit_event.clickCount = event.GetClickCount();
      break;
    case ET_MOUSE_EXITED:
// TODO(chaopeng) this fix only for chromeos now, should convert ET_MOUSE_EXITED
// to MouseLeave when crbug.com/450631 fixed.
#if defined(OS_CHROMEOS)
      webkit_event.type = blink::WebInputEvent::MouseLeave;
      break;
#endif
    case ET_MOUSE_ENTERED:
    case ET_MOUSE_MOVED:
    case ET_MOUSE_DRAGGED:
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

blink::WebMouseWheelEvent MakeWebMouseWheelEventFromUiEvent(
    const MouseWheelEvent& event) {
  blink::WebMouseWheelEvent webkit_event;

  webkit_event.type = blink::WebInputEvent::MouseWheel;
  webkit_event.button = blink::WebMouseEvent::Button::NoButton;
  webkit_event.modifiers = EventFlagsToWebEventModifiers(event.flags());
  webkit_event.timeStampSeconds = EventTimeStampToSeconds(event.time_stamp());

  webkit_event.deltaX = event.x_offset();
  webkit_event.deltaY = event.y_offset();

  webkit_event.wheelTicksX = webkit_event.deltaX / MouseWheelEvent::kWheelDelta;
  webkit_event.wheelTicksY = webkit_event.deltaY / MouseWheelEvent::kWheelDelta;

  webkit_event.tiltX = roundf(event.pointer_details().tilt_x);
  webkit_event.tiltY = roundf(event.pointer_details().tilt_y);
  webkit_event.force = event.pointer_details().force;
  webkit_event.pointerType =
      EventPointerTypeToWebPointerType(event.pointer_details().pointer_type);

  return webkit_event;
}

}  // namespace ui
