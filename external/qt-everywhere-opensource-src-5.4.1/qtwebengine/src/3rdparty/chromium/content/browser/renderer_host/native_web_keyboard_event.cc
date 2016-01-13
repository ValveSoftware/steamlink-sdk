// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/native_web_keyboard_event.h"

#include "ui/events/event_constants.h"

namespace content {

int GetModifiersFromNativeWebKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  int modifiers = ui::EF_NONE;
  if (event.modifiers & NativeWebKeyboardEvent::ShiftKey)
    modifiers |= ui::EF_SHIFT_DOWN;
  if (event.modifiers & NativeWebKeyboardEvent::ControlKey)
    modifiers |= ui::EF_CONTROL_DOWN;
  if (event.modifiers & NativeWebKeyboardEvent::AltKey)
    modifiers |= ui::EF_ALT_DOWN;
#if defined(OS_MACOSX)
  if (event.modifiers & NativeWebKeyboardEvent::MetaKey)
    modifiers |= ui::EF_COMMAND_DOWN;
#endif
  return modifiers;
}

}  // namespace content
