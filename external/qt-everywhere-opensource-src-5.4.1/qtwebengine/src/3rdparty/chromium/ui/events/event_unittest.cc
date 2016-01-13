// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom4/keycode_converter.h"
#include "ui/events/test/events_test_utils.h"

#if defined(USE_X11)
#include <X11/Xlib.h>
#include "ui/events/test/events_test_utils_x11.h"
#include "ui/gfx/x/x11_types.h"
#endif

namespace ui {

TEST(EventTest, NoNativeEvent) {
  KeyEvent keyev(ET_KEY_PRESSED, VKEY_SPACE, 0, false);
  EXPECT_FALSE(keyev.HasNativeEvent());
}

TEST(EventTest, NativeEvent) {
#if defined(OS_WIN)
  MSG native_event = { NULL, WM_KEYUP, VKEY_A, 0 };
  KeyEvent keyev(native_event, false);
  EXPECT_TRUE(keyev.HasNativeEvent());
#elif defined(USE_X11)
  ScopedXI2Event event;
  event.InitKeyEvent(ET_KEY_RELEASED, VKEY_A, 0);
  KeyEvent keyev(event, false);
  EXPECT_TRUE(keyev.HasNativeEvent());
#endif
}

TEST(EventTest, GetCharacter) {
  // Check if Control+Enter returns 10.
  KeyEvent keyev1(ET_KEY_PRESSED, VKEY_RETURN, EF_CONTROL_DOWN, false);
  EXPECT_EQ(10, keyev1.GetCharacter());
  // Check if Enter returns 13.
  KeyEvent keyev2(ET_KEY_PRESSED, VKEY_RETURN, 0, false);
  EXPECT_EQ(13, keyev2.GetCharacter());

#if defined(USE_X11)
  // For X11, test the functions with native_event() as well. crbug.com/107837
  ScopedXI2Event event;
  event.InitKeyEvent(ET_KEY_PRESSED, VKEY_RETURN, EF_CONTROL_DOWN);
  KeyEvent keyev3(event, false);
  EXPECT_EQ(10, keyev3.GetCharacter());

  event.InitKeyEvent(ET_KEY_PRESSED, VKEY_RETURN, 0);
  KeyEvent keyev4(event, false);
  EXPECT_EQ(13, keyev4.GetCharacter());
#endif
}

TEST(EventTest, ClickCount) {
  const gfx::Point origin(0, 0);
  MouseEvent mouseev(ET_MOUSE_PRESSED, origin, origin, 0, 0);
  for (int i = 1; i <=3 ; ++i) {
    mouseev.SetClickCount(i);
    EXPECT_EQ(i, mouseev.GetClickCount());
  }
}

TEST(EventTest, RepeatedClick) {
  const gfx::Point origin(0, 0);
  MouseEvent mouse_ev1(ET_MOUSE_PRESSED, origin, origin, 0, 0);
  MouseEvent mouse_ev2(ET_MOUSE_PRESSED, origin, origin, 0, 0);
  LocatedEventTestApi test_ev1(&mouse_ev1);
  LocatedEventTestApi test_ev2(&mouse_ev2);

  base::TimeDelta start = base::TimeDelta::FromMilliseconds(0);
  base::TimeDelta soon = start + base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta later = start + base::TimeDelta::FromMilliseconds(1000);

  // Close point.
  test_ev1.set_location(gfx::Point(0, 0));
  test_ev2.set_location(gfx::Point(1, 0));
  test_ev1.set_time_stamp(start);
  test_ev2.set_time_stamp(soon);
  EXPECT_TRUE(MouseEvent::IsRepeatedClickEvent(mouse_ev1, mouse_ev2));

  // Too far.
  test_ev1.set_location(gfx::Point(0, 0));
  test_ev2.set_location(gfx::Point(10, 0));
  test_ev1.set_time_stamp(start);
  test_ev2.set_time_stamp(soon);
  EXPECT_FALSE(MouseEvent::IsRepeatedClickEvent(mouse_ev1, mouse_ev2));

  // Too long a time between clicks.
  test_ev1.set_location(gfx::Point(0, 0));
  test_ev2.set_location(gfx::Point(0, 0));
  test_ev1.set_time_stamp(start);
  test_ev2.set_time_stamp(later);
  EXPECT_FALSE(MouseEvent::IsRepeatedClickEvent(mouse_ev1, mouse_ev2));
}

// Tests that an event only increases the click count and gets marked as a
// double click if a release event was seen for the previous click. This
// prevents the same PRESSED event from being processed twice:
// http://crbug.com/389162
TEST(EventTest, DoubleClickRequiresRelease) {
  const gfx::Point origin1(0, 0);
  const gfx::Point origin2(100, 0);
  scoped_ptr<MouseEvent> ev;
  base::TimeDelta start = base::TimeDelta::FromMilliseconds(0);

  ev.reset(new MouseEvent(ET_MOUSE_PRESSED, origin1, origin1, 0, 0));
  ev->set_time_stamp(start);
  EXPECT_EQ(1, MouseEvent::GetRepeatCount(*ev));
  ev.reset(new MouseEvent(ET_MOUSE_PRESSED, origin1, origin1, 0, 0));
  ev->set_time_stamp(start);
  EXPECT_EQ(1, MouseEvent::GetRepeatCount(*ev));

  ev.reset(new MouseEvent(ET_MOUSE_PRESSED, origin2, origin2, 0, 0));
  ev->set_time_stamp(start);
  EXPECT_EQ(1, MouseEvent::GetRepeatCount(*ev));
  ev.reset(new MouseEvent(ET_MOUSE_RELEASED, origin2, origin2, 0, 0));
  ev->set_time_stamp(start);
  EXPECT_EQ(1, MouseEvent::GetRepeatCount(*ev));
  ev.reset(new MouseEvent(ET_MOUSE_PRESSED, origin2, origin2, 0, 0));
  ev->set_time_stamp(start);
  EXPECT_EQ(2, MouseEvent::GetRepeatCount(*ev));
  ev.reset(new MouseEvent(ET_MOUSE_RELEASED, origin2, origin2, 0, 0));
  ev->set_time_stamp(start);
  EXPECT_EQ(2, MouseEvent::GetRepeatCount(*ev));
  MouseEvent::ResetLastClickForTest();
}

// Tests that clicking right and then left clicking does not generate a double
// click.
TEST(EventTest, SingleClickRightLeft) {
  const gfx::Point origin(0, 0);
  scoped_ptr<MouseEvent> ev;
  base::TimeDelta start = base::TimeDelta::FromMilliseconds(0);

  ev.reset(new MouseEvent(ET_MOUSE_PRESSED, origin, origin,
                          ui::EF_RIGHT_MOUSE_BUTTON,
                          ui::EF_RIGHT_MOUSE_BUTTON));
  ev->set_time_stamp(start);
  EXPECT_EQ(1, MouseEvent::GetRepeatCount(*ev));
  ev.reset(new MouseEvent(ET_MOUSE_PRESSED, origin, origin,
                          ui::EF_LEFT_MOUSE_BUTTON,
                          ui::EF_LEFT_MOUSE_BUTTON));
  ev->set_time_stamp(start);
  EXPECT_EQ(1, MouseEvent::GetRepeatCount(*ev));
  ev.reset(new MouseEvent(ET_MOUSE_RELEASED, origin, origin,
                          ui::EF_LEFT_MOUSE_BUTTON,
                          ui::EF_LEFT_MOUSE_BUTTON));
  ev->set_time_stamp(start);
  EXPECT_EQ(1, MouseEvent::GetRepeatCount(*ev));
  ev.reset(new MouseEvent(ET_MOUSE_PRESSED, origin, origin,
                          ui::EF_LEFT_MOUSE_BUTTON,
                          ui::EF_LEFT_MOUSE_BUTTON));
  ev->set_time_stamp(start);
  EXPECT_EQ(2, MouseEvent::GetRepeatCount(*ev));
  MouseEvent::ResetLastClickForTest();
}

TEST(EventTest, KeyEvent) {
  static const struct {
    KeyboardCode key_code;
    int flags;
    uint16 character;
  } kTestData[] = {
    { VKEY_A, 0, 'a' },
    { VKEY_A, EF_SHIFT_DOWN, 'A' },
    { VKEY_A, EF_CAPS_LOCK_DOWN, 'A' },
    { VKEY_A, EF_SHIFT_DOWN | EF_CAPS_LOCK_DOWN, 'a' },
    { VKEY_A, EF_CONTROL_DOWN, 0x01 },
    { VKEY_A, EF_SHIFT_DOWN | EF_CONTROL_DOWN, '\x01' },
    { VKEY_Z, 0, 'z' },
    { VKEY_Z, EF_SHIFT_DOWN, 'Z' },
    { VKEY_Z, EF_CAPS_LOCK_DOWN, 'Z' },
    { VKEY_Z, EF_SHIFT_DOWN | EF_CAPS_LOCK_DOWN, 'z' },
    { VKEY_Z, EF_CONTROL_DOWN, '\x1A' },
    { VKEY_Z, EF_SHIFT_DOWN | EF_CONTROL_DOWN, '\x1A' },

    { VKEY_2, EF_CONTROL_DOWN, '\0' },
    { VKEY_2, EF_SHIFT_DOWN | EF_CONTROL_DOWN, '\0' },
    { VKEY_6, EF_CONTROL_DOWN, '\0' },
    { VKEY_6, EF_SHIFT_DOWN | EF_CONTROL_DOWN, '\x1E' },
    { VKEY_OEM_MINUS, EF_CONTROL_DOWN, '\0' },
    { VKEY_OEM_MINUS, EF_SHIFT_DOWN | EF_CONTROL_DOWN, '\x1F' },
    { VKEY_OEM_4, EF_CONTROL_DOWN, '\x1B' },
    { VKEY_OEM_4, EF_SHIFT_DOWN | EF_CONTROL_DOWN, '\0' },
    { VKEY_OEM_5, EF_CONTROL_DOWN, '\x1C' },
    { VKEY_OEM_5, EF_SHIFT_DOWN | EF_CONTROL_DOWN, '\0' },
    { VKEY_OEM_6, EF_CONTROL_DOWN, '\x1D' },
    { VKEY_OEM_6, EF_SHIFT_DOWN | EF_CONTROL_DOWN, '\0' },
    { VKEY_RETURN, EF_CONTROL_DOWN, '\x0A' },

    { VKEY_0, 0, '0' },
    { VKEY_0, EF_SHIFT_DOWN, ')' },
    { VKEY_0, EF_SHIFT_DOWN | EF_CAPS_LOCK_DOWN, ')' },
    { VKEY_0, EF_SHIFT_DOWN | EF_CONTROL_DOWN, '\0' },

    { VKEY_9, 0, '9' },
    { VKEY_9, EF_SHIFT_DOWN, '(' },
    { VKEY_9, EF_SHIFT_DOWN | EF_CAPS_LOCK_DOWN, '(' },
    { VKEY_9, EF_SHIFT_DOWN | EF_CONTROL_DOWN, '\0' },

    { VKEY_NUMPAD0, EF_CONTROL_DOWN, '\0' },
    { VKEY_NUMPAD0, EF_SHIFT_DOWN, '0' },

    { VKEY_NUMPAD9, EF_CONTROL_DOWN, '\0' },
    { VKEY_NUMPAD9, EF_SHIFT_DOWN, '9' },

    { VKEY_TAB, EF_CONTROL_DOWN, '\0' },
    { VKEY_TAB, EF_SHIFT_DOWN, '\t' },

    { VKEY_MULTIPLY, EF_CONTROL_DOWN, '\0' },
    { VKEY_MULTIPLY, EF_SHIFT_DOWN, '*' },
    { VKEY_ADD, EF_CONTROL_DOWN, '\0' },
    { VKEY_ADD, EF_SHIFT_DOWN, '+' },
    { VKEY_SUBTRACT, EF_CONTROL_DOWN, '\0' },
    { VKEY_SUBTRACT, EF_SHIFT_DOWN, '-' },
    { VKEY_DECIMAL, EF_CONTROL_DOWN, '\0' },
    { VKEY_DECIMAL, EF_SHIFT_DOWN, '.' },
    { VKEY_DIVIDE, EF_CONTROL_DOWN, '\0' },
    { VKEY_DIVIDE, EF_SHIFT_DOWN, '/' },

    { VKEY_OEM_1, EF_CONTROL_DOWN, '\0' },
    { VKEY_OEM_1, EF_SHIFT_DOWN, ':' },
    { VKEY_OEM_PLUS, EF_CONTROL_DOWN, '\0' },
    { VKEY_OEM_PLUS, EF_SHIFT_DOWN, '+' },
    { VKEY_OEM_COMMA, EF_CONTROL_DOWN, '\0' },
    { VKEY_OEM_COMMA, EF_SHIFT_DOWN, '<' },
    { VKEY_OEM_PERIOD, EF_CONTROL_DOWN, '\0' },
    { VKEY_OEM_PERIOD, EF_SHIFT_DOWN, '>' },
    { VKEY_OEM_3, EF_CONTROL_DOWN, '\0' },
    { VKEY_OEM_3, EF_SHIFT_DOWN, '~' },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestData); ++i) {
    KeyEvent key(ET_KEY_PRESSED,
                 kTestData[i].key_code,
                 kTestData[i].flags,
                 false);
    EXPECT_EQ(kTestData[i].character, key.GetCharacter())
        << " Index:" << i << " key_code:" << kTestData[i].key_code;
  }
}

TEST(EventTest, KeyEventDirectUnicode) {
  KeyEvent key(ET_KEY_PRESSED, VKEY_UNKNOWN, EF_SHIFT_DOWN, false);
  key.set_character(0x1234U);
  EXPECT_EQ(0x1234U, key.GetCharacter());
  KeyEvent key2(ET_KEY_RELEASED, VKEY_UNKNOWN, EF_CONTROL_DOWN, false);
  key2.set_character(0x4321U);
  EXPECT_EQ(0x4321U, key2.GetCharacter());
}

TEST(EventTest, NormalizeKeyEventFlags) {
#if defined(USE_X11)
  // Normalize flags when KeyEvent is created from XEvent.
  ScopedXI2Event event;
  {
    event.InitKeyEvent(ET_KEY_PRESSED, VKEY_SHIFT, EF_SHIFT_DOWN);
    KeyEvent keyev(event, false);
    EXPECT_EQ(EF_SHIFT_DOWN, keyev.flags());
  }
  {
    event.InitKeyEvent(ET_KEY_RELEASED, VKEY_SHIFT, EF_SHIFT_DOWN);
    KeyEvent keyev(event, false);
    EXPECT_EQ(EF_NONE, keyev.flags());
  }
  {
    event.InitKeyEvent(ET_KEY_PRESSED, VKEY_CONTROL, EF_CONTROL_DOWN);
    KeyEvent keyev(event, false);
    EXPECT_EQ(EF_CONTROL_DOWN, keyev.flags());
  }
  {
    event.InitKeyEvent(ET_KEY_RELEASED, VKEY_CONTROL, EF_CONTROL_DOWN);
    KeyEvent keyev(event, false);
    EXPECT_EQ(EF_NONE, keyev.flags());
  }
  {
    event.InitKeyEvent(ET_KEY_PRESSED, VKEY_MENU,  EF_ALT_DOWN);
    KeyEvent keyev(event, false);
    EXPECT_EQ(EF_ALT_DOWN, keyev.flags());
  }
  {
    event.InitKeyEvent(ET_KEY_RELEASED, VKEY_MENU, EF_ALT_DOWN);
    KeyEvent keyev(event, false);
    EXPECT_EQ(EF_NONE, keyev.flags());
  }
#endif

  // Do not normalize flags for synthesized events without
  // KeyEvent::NormalizeFlags called explicitly.
  {
    KeyEvent keyev(ET_KEY_PRESSED, VKEY_SHIFT, EF_SHIFT_DOWN, false);
    EXPECT_EQ(EF_SHIFT_DOWN, keyev.flags());
  }
  {
    KeyEvent keyev(ET_KEY_RELEASED, VKEY_SHIFT, EF_SHIFT_DOWN, false);
    EXPECT_EQ(EF_SHIFT_DOWN, keyev.flags());
    keyev.NormalizeFlags();
    EXPECT_EQ(EF_NONE, keyev.flags());
  }
  {
    KeyEvent keyev(ET_KEY_PRESSED, VKEY_CONTROL, EF_CONTROL_DOWN, false);
    EXPECT_EQ(EF_CONTROL_DOWN, keyev.flags());
  }
  {
    KeyEvent keyev(ET_KEY_RELEASED, VKEY_CONTROL, EF_CONTROL_DOWN, false);
    EXPECT_EQ(EF_CONTROL_DOWN, keyev.flags());
    keyev.NormalizeFlags();
    EXPECT_EQ(EF_NONE, keyev.flags());
  }
  {
    KeyEvent keyev(ET_KEY_PRESSED, VKEY_MENU,  EF_ALT_DOWN, false);
    EXPECT_EQ(EF_ALT_DOWN, keyev.flags());
  }
  {
    KeyEvent keyev(ET_KEY_RELEASED, VKEY_MENU, EF_ALT_DOWN, false);
    EXPECT_EQ(EF_ALT_DOWN, keyev.flags());
    keyev.NormalizeFlags();
    EXPECT_EQ(EF_NONE, keyev.flags());
  }
}

TEST(EventTest, KeyEventCopy) {
  KeyEvent key(ET_KEY_PRESSED, VKEY_A, EF_NONE, false);
  scoped_ptr<KeyEvent> copied_key(new KeyEvent(key));
  EXPECT_EQ(copied_key->type(), key.type());
  EXPECT_EQ(copied_key->key_code(), key.key_code());
}

TEST(EventTest, KeyEventCode) {
  KeycodeConverter* conv = KeycodeConverter::GetInstance();

  const char kCodeForSpace[] = "Space";
  const uint16 kNativeCodeSpace = conv->CodeToNativeKeycode(kCodeForSpace);
  ASSERT_NE(conv->InvalidNativeKeycode(), kNativeCodeSpace);

  {
    KeyEvent key(ET_KEY_PRESSED, VKEY_SPACE, kCodeForSpace, EF_NONE, false);
    EXPECT_EQ(kCodeForSpace, key.code());
  }
  {
    // Regardless the KeyEvent.key_code (VKEY_RETURN), code should be
    // the specified value.
    KeyEvent key(ET_KEY_PRESSED, VKEY_RETURN, kCodeForSpace, EF_NONE, false);
    EXPECT_EQ(kCodeForSpace, key.code());
  }
  {
    // If the synthetic event is initialized without code, it returns
    // an empty string.
    // TODO(komatsu): Fill a fallback value assuming the US keyboard layout.
    KeyEvent key(ET_KEY_PRESSED, VKEY_SPACE, EF_NONE, false);
    EXPECT_TRUE(key.code().empty());
  }
#if defined(USE_X11)
  {
    // KeyEvent converts from the native keycode (XKB) to the code.
    ScopedXI2Event xevent;
    xevent.InitKeyEvent(ET_KEY_PRESSED, VKEY_SPACE, kNativeCodeSpace);
    KeyEvent key(xevent, false);
    EXPECT_EQ(kCodeForSpace, key.code());
  }
#endif  // USE_X11
#if defined(OS_WIN)
  {
    // Test a non extended key.
    ASSERT_EQ((kNativeCodeSpace & 0xFF), kNativeCodeSpace);

    const LPARAM lParam = GetLParamFromScanCode(kNativeCodeSpace);
    MSG native_event = { NULL, WM_KEYUP, VKEY_SPACE, lParam };
    KeyEvent key(native_event, false);

    // KeyEvent converts from the native keycode (scan code) to the code.
    EXPECT_EQ(kCodeForSpace, key.code());
  }
  {
    const char kCodeForHome[]  = "Home";
    const uint16 kNativeCodeHome  = 0xe047;

    // 'Home' is an extended key with 0xe000 bits.
    ASSERT_NE((kNativeCodeHome & 0xFF), kNativeCodeHome);
    const LPARAM lParam = GetLParamFromScanCode(kNativeCodeHome);

    MSG native_event = { NULL, WM_KEYUP, VKEY_HOME, lParam };
    KeyEvent key(native_event, false);

    // KeyEvent converts from the native keycode (scan code) to the code.
    EXPECT_EQ(kCodeForHome, key.code());
  }
#endif  // OS_WIN
}

#if defined(USE_X11) || defined(OS_WIN)
TEST(EventTest, AutoRepeat) {
  KeycodeConverter* conv = KeycodeConverter::GetInstance();

  const uint16 kNativeCodeA = conv->CodeToNativeKeycode("KeyA");
  const uint16 kNativeCodeB = conv->CodeToNativeKeycode("KeyB");
#if defined(USE_X11)
  ScopedXI2Event native_event_a_pressed;
  native_event_a_pressed.InitKeyEvent(ET_KEY_PRESSED, VKEY_A, kNativeCodeA);
  ScopedXI2Event native_event_a_released;
  native_event_a_released.InitKeyEvent(ET_KEY_RELEASED, VKEY_A, kNativeCodeA);
  ScopedXI2Event native_event_b_pressed;
  native_event_b_pressed.InitKeyEvent(ET_KEY_PRESSED, VKEY_B, kNativeCodeB);
  ScopedXI2Event native_event_a_pressed_nonstandard_state;
  native_event_a_pressed_nonstandard_state.InitKeyEvent(
      ET_KEY_PRESSED, VKEY_A, kNativeCodeA);
  // IBUS-GTK uses the mask (1 << 25) to detect reposted event.
  static_cast<XEvent*>(native_event_a_pressed_nonstandard_state)->xkey.state |=
      1 << 25;
#elif defined(OS_WIN)
  const LPARAM lParam_a = GetLParamFromScanCode(kNativeCodeA);
  const LPARAM lParam_b = GetLParamFromScanCode(kNativeCodeB);
  MSG native_event_a_pressed = { NULL, WM_KEYDOWN, VKEY_A, lParam_a };
  MSG native_event_a_released = { NULL, WM_KEYUP, VKEY_A, lParam_a };
  MSG native_event_b_pressed = { NULL, WM_KEYUP, VKEY_B, lParam_b };
#endif
  KeyEvent key_a1(native_event_a_pressed, false);
  EXPECT_FALSE(key_a1.IsRepeat());
  KeyEvent key_a1_released(native_event_a_released, false);
  EXPECT_FALSE(key_a1_released.IsRepeat());

  KeyEvent key_a2(native_event_a_pressed, false);
  EXPECT_FALSE(key_a2.IsRepeat());
  KeyEvent key_a2_repeated(native_event_a_pressed, false);
  EXPECT_TRUE(key_a2_repeated.IsRepeat());
  KeyEvent key_a2_released(native_event_a_released, false);
  EXPECT_FALSE(key_a2_released.IsRepeat());

  KeyEvent key_a3(native_event_a_pressed, false);
  EXPECT_FALSE(key_a3.IsRepeat());
  KeyEvent key_b(native_event_b_pressed, false);
  EXPECT_FALSE(key_b.IsRepeat());
  KeyEvent key_a3_again(native_event_a_pressed, false);
  EXPECT_FALSE(key_a3_again.IsRepeat());
  KeyEvent key_a3_repeated(native_event_a_pressed, false);
  EXPECT_TRUE(key_a3_repeated.IsRepeat());
  KeyEvent key_a3_repeated2(native_event_a_pressed, false);
  EXPECT_TRUE(key_a3_repeated2.IsRepeat());
  KeyEvent key_a3_released(native_event_a_released, false);
  EXPECT_FALSE(key_a3_released.IsRepeat());

#if defined(USE_X11)
  KeyEvent key_a4_pressed(native_event_a_pressed, false);
  EXPECT_FALSE(key_a4_pressed.IsRepeat());

  KeyEvent key_a4_pressed_nonstandard_state(
      native_event_a_pressed_nonstandard_state, false);
  EXPECT_FALSE(key_a4_pressed_nonstandard_state.IsRepeat());
#endif
}
#endif  // USE_X11 || OS_WIN

}  // namespace ui
