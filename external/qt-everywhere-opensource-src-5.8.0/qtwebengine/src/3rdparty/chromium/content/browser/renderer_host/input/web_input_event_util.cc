// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MSVC++ requires this to be set before any other includes to get M_PI.
#define _USE_MATH_DEFINES

#include "content/browser/renderer_host/input/web_input_event_util.h"

#include <cmath>

#include "base/strings/string_util.h"
#include "content/common/input/web_touch_event_traits.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/event_constants.h"
#include "ui/events/gesture_detection/gesture_event_data.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace content {

int WebEventModifiersToEventFlags(int modifiers) {
  int flags = 0;

  if (modifiers & blink::WebInputEvent::ShiftKey)
    flags |= ui::EF_SHIFT_DOWN;
  if (modifiers & blink::WebInputEvent::ControlKey)
    flags |= ui::EF_CONTROL_DOWN;
  if (modifiers & blink::WebInputEvent::AltKey)
    flags |= ui::EF_ALT_DOWN;
  if (modifiers & blink::WebInputEvent::MetaKey)
    flags |= ui::EF_COMMAND_DOWN;
  if (modifiers & blink::WebInputEvent::CapsLockOn)
    flags |= ui::EF_CAPS_LOCK_ON;
  if (modifiers & blink::WebInputEvent::NumLockOn)
    flags |= ui::EF_NUM_LOCK_ON;
  if (modifiers & blink::WebInputEvent::ScrollLockOn)
    flags |= ui::EF_SCROLL_LOCK_ON;
  if (modifiers & blink::WebInputEvent::LeftButtonDown)
    flags |= ui::EF_LEFT_MOUSE_BUTTON;
  if (modifiers & blink::WebInputEvent::MiddleButtonDown)
    flags |= ui::EF_MIDDLE_MOUSE_BUTTON;
  if (modifiers & blink::WebInputEvent::RightButtonDown)
    flags |= ui::EF_RIGHT_MOUSE_BUTTON;
  if (modifiers & blink::WebInputEvent::IsAutoRepeat)
    flags |= ui::EF_IS_REPEAT;

  return flags;
}

blink::WebInputEvent::Modifiers DomCodeToWebInputEventModifiers(
    ui::DomCode code) {
  switch (ui::KeycodeConverter::DomCodeToLocation(code)) {
    case ui::DomKeyLocation::LEFT:
      return blink::WebInputEvent::IsLeft;
    case ui::DomKeyLocation::RIGHT:
      return blink::WebInputEvent::IsRight;
    case ui::DomKeyLocation::NUMPAD:
      return blink::WebInputEvent::IsKeyPad;
    case ui::DomKeyLocation::STANDARD:
      break;
  }
  return static_cast<blink::WebInputEvent::Modifiers>(0);
}

}  // namespace content
