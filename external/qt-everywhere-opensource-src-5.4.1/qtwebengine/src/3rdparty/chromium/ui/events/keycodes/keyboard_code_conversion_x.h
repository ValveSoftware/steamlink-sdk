// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_KEYCODES_KEYBOARD_CODE_CONVERSION_X_H_
#define UI_EVENTS_KEYCODES_KEYBOARD_CODE_CONVERSION_X_H_

#include "base/basictypes.h"
#include "ui/events/events_base_export.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

typedef union _XEvent XEvent;

namespace ui {

EVENTS_BASE_EXPORT KeyboardCode KeyboardCodeFromXKeyEvent(XEvent* xev);

EVENTS_BASE_EXPORT KeyboardCode KeyboardCodeFromXKeysym(unsigned int keysym);

EVENTS_BASE_EXPORT const char* CodeFromXEvent(XEvent* xev);

// Returns a character on a standard US PC keyboard from an XEvent.
EVENTS_BASE_EXPORT uint16 GetCharacterFromXEvent(XEvent* xev);

// Converts a KeyboardCode into an X KeySym.
EVENTS_BASE_EXPORT int XKeysymForWindowsKeyCode(KeyboardCode keycode,
                                                bool shift);

// Converts an X keycode into ui::KeyboardCode.
EVENTS_BASE_EXPORT KeyboardCode
    DefaultKeyboardCodeFromHardwareKeycode(unsigned int hardware_code);

}  // namespace ui

#endif  // UI_EVENTS_KEYCODES_KEYBOARD_CODE_CONVERSION_X_H_
