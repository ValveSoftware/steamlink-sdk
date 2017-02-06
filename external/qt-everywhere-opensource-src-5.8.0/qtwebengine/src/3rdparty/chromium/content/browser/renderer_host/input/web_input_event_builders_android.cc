// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/web_input_event_builders_android.h"

#include <android/input.h>

#include "base/logging.h"
#include "content/browser/renderer_host/input/web_input_event_util.h"
#include "ui/events/android/key_event_utils.h"
#include "ui/events/android/motion_event_android.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_code_conversion_android.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using blink::WebGestureEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebPointerProperties;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace content {

namespace {

int WebInputEventToAndroidModifier(int web_modifier) {
  int android_modifier = 0;
  // Currently only Shift, CapsLock are used, add other modifiers if required.
  if (web_modifier & WebInputEvent::ShiftKey)
    android_modifier |= AMETA_SHIFT_ON;
  if (web_modifier & WebInputEvent::CapsLockOn)
    android_modifier |= AMETA_CAPS_LOCK_ON;
  return android_modifier;
}

ui::DomKey GetDomKeyFromEvent(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& android_key_event,
    int keycode,
    int modifiers,
    int unicode_character) {
  // Synthetic key event, not enough information to get DomKey.
  if (android_key_event.is_null() && !unicode_character)
    return ui::DomKey::UNIDENTIFIED;

  if (!unicode_character && env) {
    // According to spec |kAllowedModifiers| should be Shift and AltGr, however
    // Android doesn't have AltGr key and ImeAdapter::getModifiers won't pass it
    // either.
    // According to discussion we want to honor CapsLock and possibly NumLock as
    // well. https://github.com/w3c/uievents/issues/70
    const int kAllowedModifiers =
        WebInputEvent::ShiftKey | WebInputEvent::CapsLockOn;
    int fallback_modifiers =
        WebInputEventToAndroidModifier(modifiers & kAllowedModifiers);

    unicode_character = ui::events::android::GetKeyEventUnicodeChar(
        env, android_key_event, fallback_modifiers);
  }

  ui::DomKey key = ui::GetDomKeyFromAndroidEvent(keycode, unicode_character);
  if (key != ui::DomKey::NONE)
    return key;
  return ui::DomKey::UNIDENTIFIED;
}

}  // namespace

WebKeyboardEvent WebKeyboardEventBuilder::Build(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& android_key_event,
    WebInputEvent::Type type,
    int modifiers,
    double time_sec,
    int keycode,
    int scancode,
    int unicode_character,
    bool is_system_key) {
  DCHECK(WebInputEvent::isKeyboardEventType(type));
  WebKeyboardEvent result;

  ui::DomCode dom_code = ui::DomCode::NONE;
  if (scancode)
    dom_code = ui::KeycodeConverter::NativeKeycodeToDomCode(scancode);
  result.type = type;
  result.modifiers = modifiers;
  result.timeStampSeconds = time_sec;
  result.windowsKeyCode = ui::LocatedToNonLocatedKeyboardCode(
      ui::KeyboardCodeFromAndroidKeyCode(keycode));
  result.modifiers |= DomCodeToWebInputEventModifiers(dom_code);
  result.nativeKeyCode = keycode;
  result.domCode = static_cast<int>(dom_code);
  result.domKey = GetDomKeyFromEvent(env, android_key_event, keycode, modifiers,
                                     unicode_character);
  result.unmodifiedText[0] = unicode_character;
  if (result.windowsKeyCode == ui::VKEY_RETURN) {
    // This is the same behavior as GTK:
    // We need to treat the enter key as a key press of character \r. This
    // is apparently just how webkit handles it and what it expects.
    result.unmodifiedText[0] = '\r';
  }
  result.text[0] = result.unmodifiedText[0];
  result.isSystemKey = is_system_key;
  result.setKeyIdentifierFromWindowsKeyCode();

  return result;
}

WebMouseEvent WebMouseEventBuilder::Build(
    WebInputEvent::Type type,
    WebMouseEvent::Button button,
    double time_sec,
    int window_x,
    int window_y,
    int modifiers,
    int click_count,
    WebPointerProperties::PointerType pointer_type) {

  DCHECK(WebInputEvent::isMouseEventType(type));
  WebMouseEvent result;

  result.type = type;
  result.pointerType = pointer_type;
  result.x = window_x;
  result.y = window_y;
  result.windowX = window_x;
  result.windowY = window_y;
  result.timeStampSeconds = time_sec;
  result.clickCount = click_count;
  result.modifiers = modifiers;

  if (type == WebInputEvent::MouseDown || type == WebInputEvent::MouseUp)
    result.button = button;
  else
    result.button = WebMouseEvent::ButtonNone;

  return result;
}

WebMouseWheelEvent WebMouseWheelEventBuilder::Build(float ticks_x,
                                                    float ticks_y,
                                                    float tick_multiplier,
                                                    double time_sec,
                                                    int window_x,
                                                    int window_y) {
  WebMouseWheelEvent result;

  result.type = WebInputEvent::MouseWheel;
  result.x = window_x;
  result.y = window_y;
  result.windowX = window_x;
  result.windowY = window_y;
  result.timeStampSeconds = time_sec;
  result.button = WebMouseEvent::ButtonNone;
  result.hasPreciseScrollingDeltas = true;
  result.deltaX = ticks_x * tick_multiplier;
  result.deltaY = ticks_y * tick_multiplier;
  result.wheelTicksX = ticks_x;
  result.wheelTicksY = ticks_y;

  return result;
}

WebGestureEvent WebGestureEventBuilder::Build(WebInputEvent::Type type,
                                              double time_sec,
                                              int x,
                                              int y) {
  DCHECK(WebInputEvent::isGestureEventType(type));
  WebGestureEvent result;

  result.type = type;
  result.x = x;
  result.y = y;
  result.timeStampSeconds = time_sec;
  result.sourceDevice = blink::WebGestureDeviceTouchscreen;

  return result;
}

}  // namespace content
