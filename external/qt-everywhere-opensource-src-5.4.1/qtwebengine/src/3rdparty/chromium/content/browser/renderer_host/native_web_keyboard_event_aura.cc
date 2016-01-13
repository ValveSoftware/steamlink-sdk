// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/native_web_keyboard_event.h"

#include "base/logging.h"
#include "content/browser/renderer_host/web_input_event_aura.h"
#include "ui/events/event.h"

namespace {

// We need to copy |os_event| in NativeWebKeyboardEvent because it is
// queued in RenderWidgetHost and may be passed and used
// RenderViewHostDelegate::HandledKeybardEvent after the original aura
// event is destroyed.
ui::Event* CopyEvent(ui::Event* event) {
  return event ? new ui::KeyEvent(*static_cast<ui::KeyEvent*>(event)) : NULL;
}

int EventFlagsToWebInputEventModifiers(int flags) {
  return
      (flags & ui::EF_SHIFT_DOWN ? blink::WebInputEvent::ShiftKey : 0) |
      (flags & ui::EF_CONTROL_DOWN ? blink::WebInputEvent::ControlKey : 0) |
      (flags & ui::EF_CAPS_LOCK_DOWN ? blink::WebInputEvent::CapsLockOn : 0) |
      (flags & ui::EF_ALT_DOWN ? blink::WebInputEvent::AltKey : 0);
}

}  // namespace

using blink::WebKeyboardEvent;

namespace content {

NativeWebKeyboardEvent::NativeWebKeyboardEvent()
    : os_event(NULL),
      skip_in_browser(false),
      match_edit_command(false) {
}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(gfx::NativeEvent native_event)
    : WebKeyboardEvent(MakeWebKeyboardEvent(
          static_cast<ui::KeyEvent*>(native_event))),
      os_event(CopyEvent(native_event)),
      skip_in_browser(false),
      match_edit_command(false) {
}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(
    const NativeWebKeyboardEvent& other)
    : WebKeyboardEvent(other),
      os_event(CopyEvent(other.os_event)),
      skip_in_browser(other.skip_in_browser),
      match_edit_command(false) {
}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(
    ui::EventType key_event_type,
    bool is_char,
    wchar_t character,
    int state,
    double time_stamp_seconds)
    : os_event(NULL),
      skip_in_browser(false),
      match_edit_command(false) {
  switch (key_event_type) {
    case ui::ET_KEY_PRESSED:
      type = is_char ? blink::WebInputEvent::Char :
          blink::WebInputEvent::RawKeyDown;
      break;
    case ui::ET_KEY_RELEASED:
      type = blink::WebInputEvent::KeyUp;
      break;
    default:
      NOTREACHED();
  }

  modifiers = EventFlagsToWebInputEventModifiers(state);
  timeStampSeconds = time_stamp_seconds;
  windowsKeyCode = character;
  nativeKeyCode = character;
  text[0] = character;
  unmodifiedText[0] = character;
  isSystemKey =
      (state & ui::EF_ALT_DOWN) != 0 && (state & ui::EF_ALTGR_DOWN) == 0;
  setKeyIdentifierFromWindowsKeyCode();
}

NativeWebKeyboardEvent& NativeWebKeyboardEvent::operator=(
    const NativeWebKeyboardEvent& other) {
  WebKeyboardEvent::operator=(other);
  delete os_event;
  os_event = CopyEvent(other.os_event);
  skip_in_browser = other.skip_in_browser;
  match_edit_command = other.match_edit_command;
  return *this;
}

NativeWebKeyboardEvent::~NativeWebKeyboardEvent() {
  delete os_event;
}

}  // namespace content
