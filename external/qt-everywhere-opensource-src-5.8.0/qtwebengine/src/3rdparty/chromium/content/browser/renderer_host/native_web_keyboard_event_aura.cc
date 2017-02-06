// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/native_web_keyboard_event.h"

#include "base/logging.h"
#include "content/browser/renderer_host/web_input_event_aura.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"

namespace {

// We need to copy |os_event| in NativeWebKeyboardEvent because it is
// queued in RenderWidgetHost and may be passed and used
// RenderViewHostDelegate::HandledKeybardEvent after the original aura
// event is destroyed.
ui::Event* CopyEvent(const ui::Event* event) {
  return event ? ui::Event::Clone(*event).release() : nullptr;
}

}  // namespace

using blink::WebKeyboardEvent;

namespace content {

NativeWebKeyboardEvent::NativeWebKeyboardEvent()
    : os_event(NULL),
      skip_in_browser(false) {
}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(gfx::NativeEvent native_event)
    : NativeWebKeyboardEvent(static_cast<ui::KeyEvent&>(*native_event)) {
}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(const ui::KeyEvent& key_event)
    : WebKeyboardEvent(MakeWebKeyboardEvent(key_event)),
      os_event(CopyEvent(&key_event)),
      skip_in_browser(false) {
}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(
    const NativeWebKeyboardEvent& other)
    : WebKeyboardEvent(other),
      os_event(CopyEvent(other.os_event)),
      skip_in_browser(other.skip_in_browser) {
}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(const ui::KeyEvent& key_event,
                                               base::char16 character)
    : WebKeyboardEvent(MakeWebKeyboardEvent(key_event)),
      os_event(NULL),
      skip_in_browser(false) {
  type = blink::WebInputEvent::Char;
  windowsKeyCode = character;
  text[0] = character;
  unmodifiedText[0] = character;
  setKeyIdentifierFromWindowsKeyCode();
}

NativeWebKeyboardEvent& NativeWebKeyboardEvent::operator=(
    const NativeWebKeyboardEvent& other) {
  WebKeyboardEvent::operator=(other);
  delete os_event;
  os_event = CopyEvent(other.os_event);
  skip_in_browser = other.skip_in_browser;
  return *this;
}

NativeWebKeyboardEvent::~NativeWebKeyboardEvent() {
  delete os_event;
}

}  // namespace content
