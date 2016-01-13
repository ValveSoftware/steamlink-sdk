// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/web_input_event_aura.h"

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"

#if defined(USE_X11)
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include "ui/events/test/events_test_utils_x11.h"
#include "ui/gfx/x/x11_types.h"
#endif

namespace content {

// Checks that MakeWebKeyboardEvent makes a DOM3 spec compliant key event.
// crbug.com/127142
TEST(WebInputEventAuraTest, TestMakeWebKeyboardEvent) {
#if defined(USE_X11)
  ui::ScopedXI2Event xev;
  {
    // Press Ctrl.
    xev.InitKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_CONTROL, 0);
    ui::KeyEvent event(xev, false /* is_char */);
    blink::WebKeyboardEvent webkit_event = MakeWebKeyboardEvent(&event);
    // However, modifier bit for Control in |webkit_event| should be set.
    EXPECT_EQ(webkit_event.modifiers, blink::WebInputEvent::ControlKey);
  }
  {
    // Release Ctrl.
    xev.InitKeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_CONTROL, ControlMask);
    ui::KeyEvent event(xev, false /* is_char */);
    blink::WebKeyboardEvent webkit_event = MakeWebKeyboardEvent(&event);
    // However, modifier bit for Control in |webkit_event| shouldn't be set.
    EXPECT_EQ(webkit_event.modifiers, 0);
  }
#endif
}

// Checks that MakeWebKeyboardEvent returns a correct windowsKeyCode.
TEST(WebInputEventAuraTest, TestMakeWebKeyboardEventWindowsKeyCode) {
#if defined(USE_X11)
  ui::ScopedXI2Event xev;
  {
    // Press left Ctrl.
    xev.InitKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_CONTROL, 0);
    XEvent* xevent = xev;
    xevent->xkey.keycode = XKeysymToKeycode(gfx::GetXDisplay(), XK_Control_L);
    ui::KeyEvent event(xev, false /* is_char */);
    blink::WebKeyboardEvent webkit_event = MakeWebKeyboardEvent(&event);
    // ui::VKEY_LCONTROL, instead of ui::VKEY_CONTROL, should be filled.
    EXPECT_EQ(ui::VKEY_LCONTROL, webkit_event.windowsKeyCode);
  }
  {
    // Press right Ctrl.
    xev.InitKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_CONTROL, 0);
    XEvent* xevent = xev;
    xevent->xkey.keycode = XKeysymToKeycode(gfx::GetXDisplay(), XK_Control_R);
    ui::KeyEvent event(xev, false /* is_char */);
    blink::WebKeyboardEvent webkit_event = MakeWebKeyboardEvent(&event);
    // ui::VKEY_RCONTROL, instead of ui::VKEY_CONTROL, should be filled.
    EXPECT_EQ(ui::VKEY_RCONTROL, webkit_event.windowsKeyCode);
  }
#elif defined(OS_WIN)
  // TODO(yusukes): Add tests for win_aura once keyboardEvent() in
  // third_party/WebKit/Source/web/win/WebInputEventFactory.cpp is modified
  // to return VKEY_[LR]XXX instead of VKEY_XXX.
  // https://bugs.webkit.org/show_bug.cgi?id=86694
#endif
}

}  // namespace content
