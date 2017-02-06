// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/converters/blink/blink_input_events_type_converters.h"

#include <utility>

#include "base/logging.h"
#include "base/time/time.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace mojo {
namespace {

// TODO(majidvp): remove this and directly use ui::EventFlagsToWebEventModifiers
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
  if (flags & ui::EF_CAPS_LOCK_ON)
    modifiers |= blink::WebInputEvent::CapsLockOn;
  return modifiers;
}

// TODO(majidvp): remove this and directly use ui::EventFlagsToWebEventModifiers
int EventFlagsToWebInputEventModifiers(int flags) {
  return (flags & ui::EF_SHIFT_DOWN ? blink::WebInputEvent::ShiftKey : 0) |
         (flags & ui::EF_CONTROL_DOWN ? blink::WebInputEvent::ControlKey : 0) |
         (flags & ui::EF_CAPS_LOCK_ON ? blink::WebInputEvent::CapsLockOn : 0) |
         (flags & ui::EF_ALT_DOWN ? blink::WebInputEvent::AltKey : 0);
}

int GetClickCount(int flags) {
  if (flags & ui::EF_IS_TRIPLE_CLICK)
    return 3;
  else if (flags & ui::EF_IS_DOUBLE_CLICK)
    return 2;

  return 1;
}

void SetWebMouseEventLocation(const ui::LocatedEvent& located_event,
                              blink::WebMouseEvent* web_event) {
  web_event->x = static_cast<int>(located_event.x());
  web_event->y = static_cast<int>(located_event.y());
  web_event->globalX = static_cast<int>(located_event.root_location_f().x());
  web_event->globalY = static_cast<int>(located_event.root_location_f().y());
}

std::unique_ptr<blink::WebInputEvent> BuildWebMouseEventFrom(
    const ui::PointerEvent& event) {
  std::unique_ptr<blink::WebMouseEvent> web_event(new blink::WebMouseEvent);

  web_event->pointerType = blink::WebPointerProperties::PointerType::Mouse;
  SetWebMouseEventLocation(event, web_event.get());

  web_event->modifiers = EventFlagsToWebEventModifiers(event.flags());
  web_event->timeStampSeconds = ui::EventTimeStampToSeconds(event.time_stamp());

  web_event->button = blink::WebMouseEvent::ButtonNone;
  if (event.flags() & ui::EF_LEFT_MOUSE_BUTTON)
    web_event->button = blink::WebMouseEvent::ButtonLeft;
  if (event.flags() & ui::EF_MIDDLE_MOUSE_BUTTON)
    web_event->button = blink::WebMouseEvent::ButtonMiddle;
  if (event.flags() & ui::EF_RIGHT_MOUSE_BUTTON)
    web_event->button = blink::WebMouseEvent::ButtonRight;

  switch (event.type()) {
    case ui::ET_POINTER_DOWN:
      web_event->type = blink::WebInputEvent::MouseDown;
      break;
    case ui::ET_POINTER_UP:
      web_event->type = blink::WebInputEvent::MouseUp;
      break;
    case ui::ET_POINTER_MOVED:
      web_event->type = blink::WebInputEvent::MouseMove;
      break;
    case ui::ET_MOUSE_EXITED:
      web_event->type = blink::WebInputEvent::MouseLeave;
      break;
    default:
      NOTIMPLEMENTED() << "Received unexpected event: " << event.type();
      break;
  }

  web_event->clickCount = GetClickCount(event.flags());

  return std::move(web_event);
}

std::unique_ptr<blink::WebInputEvent> BuildWebKeyboardEvent(
    const ui::KeyEvent& event) {
  std::unique_ptr<blink::WebKeyboardEvent> web_event(
      new blink::WebKeyboardEvent);

  web_event->modifiers = EventFlagsToWebInputEventModifiers(event.flags());
  web_event->timeStampSeconds = ui::EventTimeStampToSeconds(event.time_stamp());

  switch (event.type()) {
    case ui::ET_KEY_PRESSED:
      web_event->type = event.is_char() ? blink::WebInputEvent::Char
                                        : blink::WebInputEvent::RawKeyDown;
      break;
    case ui::ET_KEY_RELEASED:
      web_event->type = blink::WebInputEvent::KeyUp;
      break;
    default:
      NOTREACHED();
  }

  if (web_event->modifiers & blink::WebInputEvent::AltKey)
    web_event->isSystemKey = true;

  web_event->windowsKeyCode = event.GetLocatedWindowsKeyboardCode();
  web_event->nativeKeyCode =
      ui::KeycodeConverter::DomCodeToNativeKeycode(event.code());
  web_event->text[0] = event.GetText();
  web_event->unmodifiedText[0] = event.GetUnmodifiedText();

  web_event->setKeyIdentifierFromWindowsKeyCode();
  return std::move(web_event);
}

std::unique_ptr<blink::WebInputEvent> BuildWebMouseWheelEventFrom(
    const ui::MouseWheelEvent& event) {
  std::unique_ptr<blink::WebMouseWheelEvent> web_event(
      new blink::WebMouseWheelEvent);
  web_event->type = blink::WebInputEvent::MouseWheel;
  web_event->button = blink::WebMouseEvent::ButtonNone;
  web_event->modifiers = EventFlagsToWebEventModifiers(event.flags());
  web_event->timeStampSeconds = ui::EventTimeStampToSeconds(event.time_stamp());

  SetWebMouseEventLocation(event, web_event.get());

  // TODO(rjkroege): Update the following code once Blink supports
  // DOM Level 3 wheel events
  // (http://www.w3.org/TR/DOM-Level-3-Events/#events-wheelevents)
  web_event->deltaX = event.x_offset();
  web_event->deltaY = event.y_offset();

  web_event->wheelTicksX = web_event->deltaX / ui::MouseWheelEvent::kWheelDelta;
  web_event->wheelTicksY = web_event->deltaY / ui::MouseWheelEvent::kWheelDelta;

  // TODO(moshayedi): ui::WheelEvent currently only supports WHEEL_MODE_LINE.
  // Add support for other wheel modes once ui::WheelEvent has support for them.
  web_event->hasPreciseScrollingDeltas = false;
  web_event->scrollByPage = false;

  return std::move(web_event);
}

void SetWebTouchEventLocation(const ui::PointerEvent& event,
                              blink::WebTouchPoint* touch) {
  touch->position.x = event.x();
  touch->position.y = event.y();
  touch->screenPosition.x = event.root_location_f().x();
  touch->screenPosition.y = event.root_location_f().y();
}

std::unique_ptr<blink::WebInputEvent> BuildWebTouchEvent(
    const ui::PointerEvent& event) {
  std::unique_ptr<blink::WebTouchEvent> web_event(new blink::WebTouchEvent);
  blink::WebTouchPoint* touch = &web_event->touches[event.pointer_id()];

  // TODO(jonross): we will need to buffer input events, as blink expects all
  // active touch points to be in each WebInputEvent (crbug.com/578160)
  SetWebTouchEventLocation(event, touch);
  touch->pointerType = blink::WebPointerProperties::PointerType::Touch;
  touch->radiusX = event.pointer_details().radius_x;
  touch->radiusY = event.pointer_details().radius_y;

  web_event->modifiers = EventFlagsToWebEventModifiers(event.flags());
  web_event->timeStampSeconds = ui::EventTimeStampToSeconds(event.time_stamp());
  web_event->uniqueTouchEventId = ui::GetNextTouchEventId();

  switch (event.type()) {
    case ui::ET_POINTER_DOWN:
      web_event->type = blink::WebInputEvent::TouchStart;
      touch->state = blink::WebTouchPoint::StatePressed;
      break;
    case ui::ET_POINTER_UP:
      web_event->type = blink::WebInputEvent::TouchEnd;
      touch->state = blink::WebTouchPoint::StateReleased;
      break;
    case ui::ET_POINTER_MOVED:
      web_event->type = blink::WebInputEvent::TouchMove;
      touch->state = blink::WebTouchPoint::StateMoved;
      break;
    case ui::ET_POINTER_CANCELLED:
      web_event->type = blink::WebInputEvent::TouchCancel;
      touch->state = blink::WebTouchPoint::StateCancelled;
      break;
    default:
      NOTIMPLEMENTED() << "Received non touch pointer event action: "
                       << event.type();
      break;
  }

  return std::move(web_event);
}

}  // namespace

// static
std::unique_ptr<blink::WebInputEvent>
TypeConverter<std::unique_ptr<blink::WebInputEvent>, ui::Event>::Convert(
    const ui::Event& event) {
  DCHECK(event.IsKeyEvent() || event.IsPointerEvent() ||
         event.IsMouseWheelEvent());
  switch (event.type()) {
    case ui::ET_POINTER_DOWN:
    case ui::ET_POINTER_UP:
    case ui::ET_POINTER_CANCELLED:
    case ui::ET_POINTER_MOVED:
    case ui::ET_POINTER_EXITED:
      if (event.IsMousePointerEvent())
        return BuildWebMouseEventFrom(*event.AsPointerEvent());
      else if (event.IsTouchPointerEvent())
        return BuildWebTouchEvent(*event.AsPointerEvent());
      else
        return nullptr;
    case ui::ET_MOUSEWHEEL:
      return BuildWebMouseWheelEventFrom(*event.AsMouseWheelEvent());
    case ui::ET_KEY_PRESSED:
    case ui::ET_KEY_RELEASED:
      return BuildWebKeyboardEvent(*event.AsKeyEvent());
    default:
      return nullptr;
  }
}

}  // namespace mojo
