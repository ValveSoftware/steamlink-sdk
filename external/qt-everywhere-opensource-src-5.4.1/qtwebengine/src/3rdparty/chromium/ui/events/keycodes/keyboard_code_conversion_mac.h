// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_KEYCODES_KEYBOARD_CODE_CONVERSION_MAC_H_
#define UI_EVENTS_KEYCODES_KEYBOARD_CODE_CONVERSION_MAC_H_

#import <Cocoa/Cocoa.h>

#include "base/basictypes.h"
#include "ui/events/events_base_export.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ui {

// We use windows virtual keycodes throughout our keyboard event related code,
// including unit tests. But Mac uses a different set of virtual keycodes.
// This function converts a windows virtual keycode into Mac's virtual key code
// and corresponding unicode character. |flags| is the Cocoa modifiers mask
// such as NSControlKeyMask, NSShiftKeyMask, etc.
// When success, the corresponding Mac's virtual key code will be returned.
// The corresponding unicode character will be stored in |character|, and the
// corresponding unicode character ignoring the modifiers will be stored in
// |characterIgnoringModifiers|.
// -1 will be returned if the keycode can't be converted.
// This function is mainly for simulating keyboard events in unit tests.
// See |KeyboardCodeFromNSEvent| for reverse conversion.
EVENTS_BASE_EXPORT int MacKeyCodeForWindowsKeyCode(
    KeyboardCode keycode,
    NSUInteger flags,
    unichar* character,
    unichar* characterIgnoringModifiers);

// This implementation cribbed from:
//   third_party/WebKit/Source/web/mac/WebInputEventFactory.mm
// Converts |event| into a |KeyboardCode|.  The mapping is not direct as the Mac
// has a different notion of key codes.
EVENTS_BASE_EXPORT KeyboardCode KeyboardCodeFromNSEvent(NSEvent* event);

EVENTS_BASE_EXPORT const char* CodeFromNSEvent(NSEvent* event);

} // namespace ui

#endif  // UI_EVENTS_KEYCODES_KEYBOARD_CODE_CONVERSION_MAC_H_
