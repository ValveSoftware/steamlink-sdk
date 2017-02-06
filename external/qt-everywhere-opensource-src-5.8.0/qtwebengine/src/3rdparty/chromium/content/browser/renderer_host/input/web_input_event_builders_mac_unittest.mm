// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/web_input_event_builders_mac.h"

#include <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#include <stddef.h>

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes.h"

using blink::WebKeyboardEvent;
using blink::WebInputEvent;
using content::WebKeyboardEventBuilder;

namespace {

struct KeyMappingEntry {
  int mac_key_code;
  unichar character;
  int windows_key_code;
  ui::DomCode dom_code;
  ui::DomKey dom_key;
};

struct ModifierKey {
  int mac_key_code;
  int left_or_right_mask;
  int non_specific_mask;
};

// Modifier keys, grouped into left/right pairs.
const ModifierKey kModifierKeys[] = {
    {56, 1 << 1, NSShiftKeyMask},      // Left Shift
    {60, 1 << 2, NSShiftKeyMask},      // Right Shift
    {55, 1 << 3, NSCommandKeyMask},    // Left Command
    {54, 1 << 4, NSCommandKeyMask},    // Right Command
    {58, 1 << 5, NSAlternateKeyMask},  // Left Alt
    {61, 1 << 6, NSAlternateKeyMask},  // Right Alt
    {59, 1 << 0, NSControlKeyMask},    // Left Control
    {62, 1 << 13, NSControlKeyMask},   // Right Control
};

NSEvent* BuildFakeKeyEvent(NSUInteger key_code,
                           unichar character,
                           NSUInteger modifier_flags,
                           NSEventType event_type) {
  NSString* string = [NSString stringWithCharacters:&character length:1];
  return [NSEvent keyEventWithType:event_type
                          location:NSZeroPoint
                     modifierFlags:modifier_flags
                         timestamp:0.0
                      windowNumber:0
                           context:nil
                        characters:string
       charactersIgnoringModifiers:string
                         isARepeat:NO
                           keyCode:key_code];
}

}  // namespace

// Test that arrow keys don't have numpad modifier set.
TEST(WebInputEventBuilderMacTest, ArrowKeyNumPad) {
  // Left
  NSEvent* mac_event = BuildFakeKeyEvent(0x7B, NSLeftArrowFunctionKey,
                                         NSNumericPadKeyMask, NSKeyDown);
  WebKeyboardEvent web_event = WebKeyboardEventBuilder::Build(mac_event);
  EXPECT_EQ(0, web_event.modifiers);
  EXPECT_EQ(ui::DomCode::ARROW_LEFT,
            static_cast<ui::DomCode>(web_event.domCode));
  EXPECT_EQ(ui::DomKey::ARROW_LEFT, web_event.domKey);

  // Right
  mac_event = BuildFakeKeyEvent(0x7C, NSRightArrowFunctionKey,
                                NSNumericPadKeyMask, NSKeyDown);
  web_event = WebKeyboardEventBuilder::Build(mac_event);
  EXPECT_EQ(0, web_event.modifiers);
  EXPECT_EQ(ui::DomCode::ARROW_RIGHT,
            static_cast<ui::DomCode>(web_event.domCode));
  EXPECT_EQ(ui::DomKey::ARROW_RIGHT, web_event.domKey);

  // Down
  mac_event = BuildFakeKeyEvent(0x7D, NSDownArrowFunctionKey,
                                NSNumericPadKeyMask, NSKeyDown);
  web_event = WebKeyboardEventBuilder::Build(mac_event);
  EXPECT_EQ(0, web_event.modifiers);
  EXPECT_EQ(ui::DomCode::ARROW_DOWN,
            static_cast<ui::DomCode>(web_event.domCode));
  EXPECT_EQ(ui::DomKey::ARROW_DOWN, web_event.domKey);

  // Up
  mac_event = BuildFakeKeyEvent(0x7E, NSUpArrowFunctionKey, NSNumericPadKeyMask,
                                NSKeyDown);
  web_event = WebKeyboardEventBuilder::Build(mac_event);
  EXPECT_EQ(0, web_event.modifiers);
  EXPECT_EQ(ui::DomCode::ARROW_UP, static_cast<ui::DomCode>(web_event.domCode));
  EXPECT_EQ(ui::DomKey::ARROW_UP, web_event.domKey);
}

// Test that control sequence generate the correct vkey code.
TEST(WebInputEventBuilderMacTest, ControlSequence) {
  // Ctrl-[ generates escape.
  NSEvent* mac_event =
      BuildFakeKeyEvent(0x21, 0x1b, NSControlKeyMask, NSKeyDown);
  WebKeyboardEvent web_event = WebKeyboardEventBuilder::Build(mac_event);
  EXPECT_EQ(ui::VKEY_OEM_4, web_event.windowsKeyCode);
  EXPECT_EQ(ui::DomCode::BRACKET_LEFT,
            static_cast<ui::DomCode>(web_event.domCode));
  // Will only pass on US layout.
  EXPECT_EQ(ui::DomKey::FromCharacter('['), web_event.domKey);
}

// Test that numpad keys get mapped correctly.
TEST(WebInputEventBuilderMacTest, NumPadMapping) {
  KeyMappingEntry table[] = {
      {65, '.', ui::VKEY_DECIMAL, ui::DomCode::NUMPAD_DECIMAL,
       ui::DomKey::FromCharacter('.')},
      {67, '*', ui::VKEY_MULTIPLY, ui::DomCode::NUMPAD_MULTIPLY,
       ui::DomKey::FromCharacter('*')},
      {69, '+', ui::VKEY_ADD, ui::DomCode::NUMPAD_ADD,
       ui::DomKey::FromCharacter('+')},
      {71, NSClearLineFunctionKey, ui::VKEY_CLEAR, ui::DomCode::NUM_LOCK,
       ui::DomKey::CLEAR},
      {75, '/', ui::VKEY_DIVIDE, ui::DomCode::NUMPAD_DIVIDE,
       ui::DomKey::FromCharacter('/')},
      {76, 3, ui::VKEY_RETURN, ui::DomCode::NUMPAD_ENTER, ui::DomKey::ENTER},
      {78, '-', ui::VKEY_SUBTRACT, ui::DomCode::NUMPAD_SUBTRACT,
       ui::DomKey::FromCharacter('-')},
      {81, '=', ui::VKEY_OEM_PLUS, ui::DomCode::NUMPAD_EQUAL,
       ui::DomKey::FromCharacter('=')},
      {82, '0', ui::VKEY_0, ui::DomCode::NUMPAD0,
       ui::DomKey::FromCharacter('0')},
      {83, '1', ui::VKEY_1, ui::DomCode::NUMPAD1,
       ui::DomKey::FromCharacter('1')},
      {84, '2', ui::VKEY_2, ui::DomCode::NUMPAD2,
       ui::DomKey::FromCharacter('2')},
      {85, '3', ui::VKEY_3, ui::DomCode::NUMPAD3,
       ui::DomKey::FromCharacter('3')},
      {86, '4', ui::VKEY_4, ui::DomCode::NUMPAD4,
       ui::DomKey::FromCharacter('4')},
      {87, '5', ui::VKEY_5, ui::DomCode::NUMPAD5,
       ui::DomKey::FromCharacter('5')},
      {88, '6', ui::VKEY_6, ui::DomCode::NUMPAD6,
       ui::DomKey::FromCharacter('6')},
      {89, '7', ui::VKEY_7, ui::DomCode::NUMPAD7,
       ui::DomKey::FromCharacter('7')},
      {91, '8', ui::VKEY_8, ui::DomCode::NUMPAD8,
       ui::DomKey::FromCharacter('8')},
      {92, '9', ui::VKEY_9, ui::DomCode::NUMPAD9,
       ui::DomKey::FromCharacter('9')},
  };

  for (size_t i = 0; i < arraysize(table); ++i) {
    NSEvent* mac_event = BuildFakeKeyEvent(table[i].mac_key_code,
                                           table[i].character, 0, NSKeyDown);
    WebKeyboardEvent web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(table[i].windows_key_code, web_event.windowsKeyCode);
    EXPECT_EQ(table[i].dom_code, static_cast<ui::DomCode>(web_event.domCode));
    EXPECT_EQ(table[i].dom_key, web_event.domKey);
  }
}

// Test that left- and right-hand modifier keys are interpreted correctly when
// pressed simultaneously.
TEST(WebInputEventFactoryTestMac, SimultaneousModifierKeys) {
  for (size_t i = 0; i < arraysize(kModifierKeys) / 2; ++i) {
    const ModifierKey& left = kModifierKeys[2 * i];
    const ModifierKey& right = kModifierKeys[2 * i + 1];
    // Press the left key.
    NSEvent* mac_event = BuildFakeKeyEvent(
        left.mac_key_code, 0, left.left_or_right_mask | left.non_specific_mask,
        NSFlagsChanged);
    WebKeyboardEvent web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(WebInputEvent::RawKeyDown, web_event.type);
    // Press the right key
    mac_event =
        BuildFakeKeyEvent(right.mac_key_code, 0,
                          left.left_or_right_mask | right.left_or_right_mask |
                              left.non_specific_mask,
                          NSFlagsChanged);
    web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(WebInputEvent::RawKeyDown, web_event.type);
    // Release the right key
    mac_event = BuildFakeKeyEvent(
        right.mac_key_code, 0, left.left_or_right_mask | left.non_specific_mask,
        NSFlagsChanged);
    // Release the left key
    mac_event = BuildFakeKeyEvent(left.mac_key_code, 0, 0, NSFlagsChanged);
    web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(WebInputEvent::KeyUp, web_event.type);
  }
}

// Test that individual modifier keys are still reported correctly, even if the
// undocumented left- or right-hand flags are not set.
TEST(WebInputEventBuilderMacTest, MissingUndocumentedModifierFlags) {
  for (size_t i = 0; i < arraysize(kModifierKeys); ++i) {
    const ModifierKey& key = kModifierKeys[i];
    NSEvent* mac_event = BuildFakeKeyEvent(
        key.mac_key_code, 0, key.non_specific_mask, NSFlagsChanged);
    WebKeyboardEvent web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(WebInputEvent::RawKeyDown, web_event.type);
    mac_event = BuildFakeKeyEvent(key.mac_key_code, 0, 0, NSFlagsChanged);
    web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(WebInputEvent::KeyUp, web_event.type);
  }
}

// Test system key events recognition.
TEST(WebInputEventBuilderMacTest, SystemKeyEvents) {
  // Cmd + B should not be treated as system event.
  NSEvent* macEvent =
      BuildFakeKeyEvent(kVK_ANSI_B, 'B', NSCommandKeyMask, NSKeyDown);
  WebKeyboardEvent webEvent = WebKeyboardEventBuilder::Build(macEvent);
  EXPECT_FALSE(webEvent.isSystemKey);

  // Cmd + I should not be treated as system event.
  macEvent = BuildFakeKeyEvent(kVK_ANSI_I, 'I', NSCommandKeyMask, NSKeyDown);
  webEvent = WebKeyboardEventBuilder::Build(macEvent);
  EXPECT_FALSE(webEvent.isSystemKey);

  // Cmd + <some other modifier> + <B|I> should be treated as system event.
  macEvent = BuildFakeKeyEvent(kVK_ANSI_B, 'B',
                               NSCommandKeyMask | NSShiftKeyMask, NSKeyDown);
  webEvent = WebKeyboardEventBuilder::Build(macEvent);
  EXPECT_TRUE(webEvent.isSystemKey);
  macEvent = BuildFakeKeyEvent(kVK_ANSI_I, 'I',
                               NSCommandKeyMask | NSControlKeyMask, NSKeyDown);
  webEvent = WebKeyboardEventBuilder::Build(macEvent);
  EXPECT_TRUE(webEvent.isSystemKey);

  // Cmd + <any letter other then B and I> should be treated as system event.
  macEvent = BuildFakeKeyEvent(kVK_ANSI_S, 'S', NSCommandKeyMask, NSKeyDown);
  webEvent = WebKeyboardEventBuilder::Build(macEvent);
  EXPECT_TRUE(webEvent.isSystemKey);

  // Cmd + <some other modifier> + <any letter other then B and I> should be
  // treated as system event.
  macEvent = BuildFakeKeyEvent(kVK_ANSI_S, 'S',
                               NSCommandKeyMask | NSShiftKeyMask, NSKeyDown);
  webEvent = WebKeyboardEventBuilder::Build(macEvent);
  EXPECT_TRUE(webEvent.isSystemKey);
}

// Test generating |windowsKeyCode| from |NSEvent| 'keydown'/'keyup', US
// keyboard and InputSource.
TEST(WebInputEventBuilderMacTest, USAlnumNSEventToKeyCode) {
  struct DomKeyTestCase {
    int mac_key_code;
    unichar character;
    unichar shift_character;
    ui::KeyboardCode key_code;
  } table[] = {
      {kVK_ANSI_0, '0', ')', ui::VKEY_0}, {kVK_ANSI_1, '1', '!', ui::VKEY_1},
      {kVK_ANSI_2, '2', '@', ui::VKEY_2}, {kVK_ANSI_3, '3', '#', ui::VKEY_3},
      {kVK_ANSI_4, '4', '$', ui::VKEY_4}, {kVK_ANSI_5, '5', '%', ui::VKEY_5},
      {kVK_ANSI_6, '6', '^', ui::VKEY_6}, {kVK_ANSI_7, '7', '&', ui::VKEY_7},
      {kVK_ANSI_8, '8', '*', ui::VKEY_8}, {kVK_ANSI_9, '9', '(', ui::VKEY_9},
      {kVK_ANSI_A, 'a', 'A', ui::VKEY_A}, {kVK_ANSI_B, 'b', 'B', ui::VKEY_B},
      {kVK_ANSI_C, 'c', 'C', ui::VKEY_C}, {kVK_ANSI_D, 'd', 'D', ui::VKEY_D},
      {kVK_ANSI_E, 'e', 'E', ui::VKEY_E}, {kVK_ANSI_F, 'f', 'F', ui::VKEY_F},
      {kVK_ANSI_G, 'g', 'G', ui::VKEY_G}, {kVK_ANSI_H, 'h', 'H', ui::VKEY_H},
      {kVK_ANSI_I, 'i', 'I', ui::VKEY_I}, {kVK_ANSI_J, 'j', 'J', ui::VKEY_J},
      {kVK_ANSI_K, 'k', 'K', ui::VKEY_K}, {kVK_ANSI_L, 'l', 'L', ui::VKEY_L},
      {kVK_ANSI_M, 'm', 'M', ui::VKEY_M}, {kVK_ANSI_N, 'n', 'N', ui::VKEY_N},
      {kVK_ANSI_O, 'o', 'O', ui::VKEY_O}, {kVK_ANSI_P, 'p', 'P', ui::VKEY_P},
      {kVK_ANSI_Q, 'q', 'Q', ui::VKEY_Q}, {kVK_ANSI_R, 'r', 'R', ui::VKEY_R},
      {kVK_ANSI_S, 's', 'S', ui::VKEY_S}, {kVK_ANSI_T, 't', 'T', ui::VKEY_T},
      {kVK_ANSI_U, 'u', 'U', ui::VKEY_U}, {kVK_ANSI_V, 'v', 'V', ui::VKEY_V},
      {kVK_ANSI_W, 'w', 'W', ui::VKEY_W}, {kVK_ANSI_X, 'x', 'X', ui::VKEY_X},
      {kVK_ANSI_Y, 'y', 'Y', ui::VKEY_Y}, {kVK_ANSI_Z, 'z', 'Z', ui::VKEY_Z}};

  for (const DomKeyTestCase& entry : table) {
    // Test without modifiers.
    NSEvent* mac_event =
        BuildFakeKeyEvent(entry.mac_key_code, entry.character, 0, NSKeyDown);
    WebKeyboardEvent web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(entry.key_code, web_event.windowsKeyCode);
    mac_event =
        BuildFakeKeyEvent(entry.mac_key_code, entry.character, 0, NSKeyUp);
    web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(entry.key_code, web_event.windowsKeyCode);
    // Test with Shift.
    mac_event = BuildFakeKeyEvent(entry.mac_key_code, entry.shift_character,
                                  NSShiftKeyMask, NSKeyDown);
    web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(entry.key_code, web_event.windowsKeyCode);
    mac_event = BuildFakeKeyEvent(entry.mac_key_code, entry.shift_character,
                                  NSShiftKeyMask, NSKeyUp);
    web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(entry.key_code, web_event.windowsKeyCode);
  }
}

// Test generating |windowsKeyCode| from |NSEvent| 'keydown'/'keyup', JIS
// keyboard and InputSource.
TEST(WebInputEventBuilderMacTest, JISNumNSEventToKeyCode) {
  struct DomKeyTestCase {
    int mac_key_code;
    unichar character;
    unichar shift_character;
    ui::KeyboardCode key_code;
  } table[] = {
      {kVK_ANSI_0, '0', '0', ui::VKEY_0},  {kVK_ANSI_1, '1', '!', ui::VKEY_1},
      {kVK_ANSI_2, '2', '\"', ui::VKEY_2}, {kVK_ANSI_3, '3', '#', ui::VKEY_3},
      {kVK_ANSI_4, '4', '$', ui::VKEY_4},  {kVK_ANSI_5, '5', '%', ui::VKEY_5},
      {kVK_ANSI_6, '6', '&', ui::VKEY_6},  {kVK_ANSI_7, '7', '\'', ui::VKEY_7},
      {kVK_ANSI_8, '8', '(', ui::VKEY_8},  {kVK_ANSI_9, '9', ')', ui::VKEY_9}};

  for (const DomKeyTestCase& entry : table) {
    // Test without modifiers.
    NSEvent* mac_event =
        BuildFakeKeyEvent(entry.mac_key_code, entry.character, 0, NSKeyDown);
    WebKeyboardEvent web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(entry.key_code, web_event.windowsKeyCode);
    mac_event =
        BuildFakeKeyEvent(entry.mac_key_code, entry.character, 0, NSKeyUp);
    web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(entry.key_code, web_event.windowsKeyCode);
    // Test with Shift.
    mac_event = BuildFakeKeyEvent(entry.mac_key_code, entry.shift_character,
                                  NSShiftKeyMask, NSKeyDown);
    web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(entry.key_code, web_event.windowsKeyCode);
    mac_event = BuildFakeKeyEvent(entry.mac_key_code, entry.shift_character,
                                  NSShiftKeyMask, NSKeyUp);
    web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(entry.key_code, web_event.windowsKeyCode);
  }
}

// Test generating |windowsKeyCode| from |NSEvent| 'keydown'/'keyup',
// US keyboard and Dvorak InputSource.
TEST(WebInputEventBuilderMacTest, USDvorakAlnumNSEventToKeyCode) {
  struct DomKeyTestCase {
    int mac_key_code;
    unichar character;
    ui::KeyboardCode key_code;
  } table[] = {
      {kVK_ANSI_0, '0', ui::VKEY_0}, {kVK_ANSI_1, '1', ui::VKEY_1},
      {kVK_ANSI_2, '2', ui::VKEY_2}, {kVK_ANSI_3, '3', ui::VKEY_3},
      {kVK_ANSI_4, '4', ui::VKEY_4}, {kVK_ANSI_5, '5', ui::VKEY_5},
      {kVK_ANSI_6, '6', ui::VKEY_6}, {kVK_ANSI_7, '7', ui::VKEY_7},
      {kVK_ANSI_8, '8', ui::VKEY_8}, {kVK_ANSI_9, '9', ui::VKEY_9},
      {kVK_ANSI_A, 'a', ui::VKEY_A}, {kVK_ANSI_B, 'x', ui::VKEY_X},
      {kVK_ANSI_C, 'j', ui::VKEY_J}, {kVK_ANSI_D, 'e', ui::VKEY_E},
      {kVK_ANSI_E, '.', ui::VKEY_OEM_PERIOD}, {kVK_ANSI_F, 'u', ui::VKEY_U},
      {kVK_ANSI_G, 'i', ui::VKEY_I}, {kVK_ANSI_H, 'd', ui::VKEY_D},
      {kVK_ANSI_I, 'c', ui::VKEY_C}, {kVK_ANSI_J, 'h', ui::VKEY_H},
      {kVK_ANSI_K, 't', ui::VKEY_T}, {kVK_ANSI_L, 'n', ui::VKEY_N},
      {kVK_ANSI_M, 'm', ui::VKEY_M}, {kVK_ANSI_N, 'b', ui::VKEY_B},
      {kVK_ANSI_O, 'r', ui::VKEY_R}, {kVK_ANSI_P, 'l', ui::VKEY_L},
      {kVK_ANSI_Q, '\'', ui::VKEY_OEM_7}, {kVK_ANSI_R, 'p', ui::VKEY_P},
      {kVK_ANSI_S, 'o', ui::VKEY_O}, {kVK_ANSI_T, 'y', ui::VKEY_Y},
      {kVK_ANSI_U, 'g', ui::VKEY_G}, {kVK_ANSI_V, 'k', ui::VKEY_K},
      {kVK_ANSI_W, ',', ui::VKEY_OEM_COMMA}, {kVK_ANSI_X, 'q', ui::VKEY_Q},
      {kVK_ANSI_Y, 'f', ui::VKEY_F}, {kVK_ANSI_Z, ';', ui::VKEY_OEM_1}};

  for (const DomKeyTestCase& entry : table) {
    // Test without modifiers.
    NSEvent* mac_event =
        BuildFakeKeyEvent(entry.mac_key_code, entry.character, 0, NSKeyDown);
    WebKeyboardEvent web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(entry.key_code, web_event.windowsKeyCode);
    mac_event =
        BuildFakeKeyEvent(entry.mac_key_code, entry.character, 0, NSKeyUp);
    web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(entry.key_code, web_event.windowsKeyCode);
  }
}

// Test conversion from key combination with Control to DomKey.
// TODO(chongz): Move DomKey tests for all platforms into one place.
// http://crbug.com/587589
// This test case only works for U.S. layout.
TEST(WebInputEventBuilderMacTest, DomKeyCtrlShift) {
  struct DomKeyTestCase {
    int mac_key_code;
    unichar character;
    unichar shift_character;
  } table[] = {
      {kVK_ANSI_0, '0', ')'}, {kVK_ANSI_1, '1', '!'}, {kVK_ANSI_2, '2', '@'},
      {kVK_ANSI_3, '3', '#'}, {kVK_ANSI_4, '4', '$'}, {kVK_ANSI_5, '5', '%'},
      {kVK_ANSI_6, '6', '^'}, {kVK_ANSI_7, '7', '&'}, {kVK_ANSI_8, '8', '*'},
      {kVK_ANSI_9, '9', '('}, {kVK_ANSI_A, 'a', 'A'}, {kVK_ANSI_B, 'b', 'B'},
      {kVK_ANSI_C, 'c', 'C'}, {kVK_ANSI_D, 'd', 'D'}, {kVK_ANSI_E, 'e', 'E'},
      {kVK_ANSI_F, 'f', 'F'}, {kVK_ANSI_G, 'g', 'G'}, {kVK_ANSI_H, 'h', 'H'},
      {kVK_ANSI_I, 'i', 'I'}, {kVK_ANSI_J, 'j', 'J'}, {kVK_ANSI_K, 'k', 'K'},
      {kVK_ANSI_L, 'l', 'L'}, {kVK_ANSI_M, 'm', 'M'}, {kVK_ANSI_N, 'n', 'N'},
      {kVK_ANSI_O, 'o', 'O'}, {kVK_ANSI_P, 'p', 'P'}, {kVK_ANSI_Q, 'q', 'Q'},
      {kVK_ANSI_R, 'r', 'R'}, {kVK_ANSI_S, 's', 'S'}, {kVK_ANSI_T, 't', 'T'},
      {kVK_ANSI_U, 'u', 'U'}, {kVK_ANSI_V, 'v', 'V'}, {kVK_ANSI_W, 'w', 'W'},
      {kVK_ANSI_X, 'x', 'X'}, {kVK_ANSI_Y, 'y', 'Y'}, {kVK_ANSI_Z, 'z', 'Z'}};

  for (const DomKeyTestCase& entry : table) {
    // Tests ctrl_dom_key.
    NSEvent* mac_event = BuildFakeKeyEvent(entry.mac_key_code, entry.character,
                                           NSControlKeyMask, NSKeyDown);
    WebKeyboardEvent web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(ui::DomKey::FromCharacter(entry.character), web_event.domKey);
    // Tests ctrl_shift_dom_key.
    mac_event = BuildFakeKeyEvent(entry.mac_key_code, entry.shift_character,
                                  NSControlKeyMask | NSShiftKeyMask, NSKeyDown);
    web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(ui::DomKey::FromCharacter(entry.shift_character),
              web_event.domKey);
  }
}

// This test case only works for U.S. layout.
TEST(WebInputEventBuilderMacTest, DomKeyCtrlAlt) {
  struct DomKeyTestCase {
    int mac_key_code;
    unichar alt_character;
    unichar ctrl_alt_character;
  } table[] = {{kVK_ANSI_0, u"º"[0], u"0"[0]}, {kVK_ANSI_1, u"¡"[0], u"1"[0]},
               {kVK_ANSI_2, u"™"[0], u"2"[0]}, {kVK_ANSI_3, u"£"[0], u"3"[0]},
               {kVK_ANSI_4, u"¢"[0], u"4"[0]}, {kVK_ANSI_5, u"∞"[0], u"5"[0]},
               {kVK_ANSI_6, u"§"[0], u"6"[0]}, {kVK_ANSI_7, u"¶"[0], u"7"[0]},
               {kVK_ANSI_8, u"•"[0], u"8"[0]}, {kVK_ANSI_9, u"ª"[0], u"9"[0]},
               {kVK_ANSI_A, u"å"[0], u"å"[0]}, {kVK_ANSI_B, u"∫"[0], u"∫"[0]},
               {kVK_ANSI_C, u"ç"[0], u"ç"[0]}, {kVK_ANSI_D, u"∂"[0], u"∂"[0]},
               {kVK_ANSI_F, u"ƒ"[0], u"ƒ"[0]}, {kVK_ANSI_G, u"©"[0], u"©"[0]},
               {kVK_ANSI_H, u"˙"[0], u"˙"[0]}, {kVK_ANSI_J, u"∆"[0], u"∆"[0]},
               {kVK_ANSI_K, u"˚"[0], u"˚"[0]}, {kVK_ANSI_L, u"¬"[0], u"¬"[0]},
               {kVK_ANSI_M, u"µ"[0], u"µ"[0]}, {kVK_ANSI_O, u"ø"[0], u"ø"[0]},
               {kVK_ANSI_P, u"π"[0], u"π"[0]}, {kVK_ANSI_Q, u"œ"[0], u"œ"[0]},
               {kVK_ANSI_R, u"®"[0], u"®"[0]}, {kVK_ANSI_S, u"ß"[0], u"ß"[0]},
               {kVK_ANSI_T, u"†"[0], u"†"[0]}, {kVK_ANSI_V, u"√"[0], u"√"[0]},
               {kVK_ANSI_W, u"∑"[0], u"∑"[0]}, {kVK_ANSI_X, u"≈"[0], u"≈"[0]},
               {kVK_ANSI_Y, u"¥"[0], u"¥"[0]}, {kVK_ANSI_Z, u"Ω"[0], u"Ω"[0]}};

  for (const DomKeyTestCase& entry : table) {
    // Tests alt_dom_key.
    NSEvent* mac_event = BuildFakeKeyEvent(
        entry.mac_key_code, entry.alt_character, NSAlternateKeyMask, NSKeyDown);
    WebKeyboardEvent web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(ui::DomKey::FromCharacter(entry.alt_character), web_event.domKey)
        << "a " << entry.alt_character;
    // Tests ctrl_alt_dom_key.
    mac_event =
        BuildFakeKeyEvent(entry.mac_key_code, entry.ctrl_alt_character,
                          NSControlKeyMask | NSAlternateKeyMask, NSKeyDown);
    web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(ui::DomKey::FromCharacter(entry.ctrl_alt_character),
              web_event.domKey)
        << "a_c " << entry.ctrl_alt_character;
  }

  struct DeadDomKeyTestCase {
    int mac_key_code;
    unichar alt_accent_character;
  } dead_key_table[] = {{kVK_ANSI_E, u"´"[0]},
                        {kVK_ANSI_I, u"ˆ"[0]},
                        {kVK_ANSI_N, u"˜"[0]},
                        {kVK_ANSI_U, u"¨"[0]}};

  for (const DeadDomKeyTestCase& entry : dead_key_table) {
    // Tests alt_accent_character.
    NSEvent* mac_event =
        BuildFakeKeyEvent(entry.mac_key_code, 0, NSAlternateKeyMask, NSKeyDown);
    WebKeyboardEvent web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(
        ui::DomKey::DeadKeyFromCombiningCharacter(entry.alt_accent_character),
        web_event.domKey)
        << "a " << entry.alt_accent_character;

    // Tests alt_accent_character with ctrl.
    mac_event =
        BuildFakeKeyEvent(entry.mac_key_code, 0,
                          NSControlKeyMask | NSAlternateKeyMask, NSKeyDown);
    web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(
        ui::DomKey::DeadKeyFromCombiningCharacter(entry.alt_accent_character),
        web_event.domKey)
        << "a_c " << entry.alt_accent_character;
  }
}

TEST(WebInputEventBuilderMacTest, DomKeyNonPrintable) {
  struct DomKeyTestCase {
    int mac_key_code;
    unichar character;
    ui::DomKey dom_key;
  } table[] = {
      {kVK_Return, kReturnCharCode, ui::DomKey::ENTER},
      {kVK_Tab, kTabCharCode, ui::DomKey::TAB},
      {kVK_Delete, kBackspaceCharCode, ui::DomKey::BACKSPACE},
      {kVK_Escape, kEscapeCharCode, ui::DomKey::ESCAPE},
      {kVK_F1, NSF1FunctionKey, ui::DomKey::F1},
      {kVK_F2, NSF2FunctionKey, ui::DomKey::F2},
      {kVK_F3, NSF3FunctionKey, ui::DomKey::F3},
      {kVK_F4, NSF4FunctionKey, ui::DomKey::F4},
      {kVK_F5, NSF5FunctionKey, ui::DomKey::F5},
      {kVK_F6, NSF6FunctionKey, ui::DomKey::F6},
      {kVK_F7, NSF7FunctionKey, ui::DomKey::F7},
      {kVK_F8, NSF8FunctionKey, ui::DomKey::F8},
      {kVK_F9, NSF9FunctionKey, ui::DomKey::F9},
      {kVK_F10, NSF10FunctionKey, ui::DomKey::F10},
      {kVK_F11, NSF11FunctionKey, ui::DomKey::F11},
      {kVK_F12, NSF12FunctionKey, ui::DomKey::F12},
      {kVK_F13, NSF13FunctionKey, ui::DomKey::F13},
      {kVK_F14, NSF14FunctionKey, ui::DomKey::F14},
      {kVK_F15, NSF15FunctionKey, ui::DomKey::F15},
      {kVK_F16, NSF16FunctionKey, ui::DomKey::F16},
      {kVK_F17, NSF17FunctionKey, ui::DomKey::F17},
      {kVK_F18, NSF18FunctionKey, ui::DomKey::F18},
      {kVK_F19, NSF19FunctionKey, ui::DomKey::F19},
      {kVK_F20, NSF20FunctionKey, ui::DomKey::F20},
      {kVK_Help, kHelpCharCode, ui::DomKey::HELP},
      {kVK_Home, NSHomeFunctionKey, ui::DomKey::HOME},
      {kVK_PageUp, NSPageUpFunctionKey, ui::DomKey::PAGE_UP},
      {kVK_ForwardDelete, NSDeleteFunctionKey, ui::DomKey::DEL},
      {kVK_End, NSEndFunctionKey, ui::DomKey::END},
      {kVK_PageDown, NSPageDownFunctionKey, ui::DomKey::PAGE_DOWN},
      {kVK_LeftArrow, NSLeftArrowFunctionKey, ui::DomKey::ARROW_LEFT},
      {kVK_RightArrow, NSRightArrowFunctionKey, ui::DomKey::ARROW_RIGHT},
      {kVK_DownArrow, NSDownArrowFunctionKey, ui::DomKey::ARROW_DOWN},
      {kVK_UpArrow, NSUpArrowFunctionKey, ui::DomKey::ARROW_UP}};

  for (const DomKeyTestCase& entry : table) {
    // Tests non-printable key.
    NSEvent* mac_event =
        BuildFakeKeyEvent(entry.mac_key_code, entry.character, 0, NSKeyDown);
    WebKeyboardEvent web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(entry.dom_key, web_event.domKey) << entry.mac_key_code;
    // Tests non-printable key with Shift.
    mac_event = BuildFakeKeyEvent(entry.mac_key_code, entry.character,
                                  NSShiftKeyMask, NSKeyDown);
    web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(entry.dom_key, web_event.domKey) << "s " << entry.mac_key_code;
  }
}

TEST(WebInputEventBuilderMacTest, DomKeyFlagsChanged) {
  struct DomKeyTestCase {
    int mac_key_code;
    ui::DomKey dom_key;
  } table[] = {{kVK_Command, ui::DomKey::META},
               {kVK_Shift, ui::DomKey::SHIFT},
               {kVK_RightShift, ui::DomKey::SHIFT},
               {kVK_CapsLock, ui::DomKey::CAPS_LOCK},
               {kVK_Option, ui::DomKey::ALT},
               {kVK_RightOption, ui::DomKey::ALT},
               {kVK_Control, ui::DomKey::CONTROL},
               {kVK_RightControl, ui::DomKey::CONTROL},
               {kVK_Function, ui::DomKey::FN}};

  for (const DomKeyTestCase& entry : table) {
    NSEvent* mac_event =
        BuildFakeKeyEvent(entry.mac_key_code, 0, 0, NSFlagsChanged);
    WebKeyboardEvent web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(entry.dom_key, web_event.domKey) << entry.mac_key_code;
  }
}
