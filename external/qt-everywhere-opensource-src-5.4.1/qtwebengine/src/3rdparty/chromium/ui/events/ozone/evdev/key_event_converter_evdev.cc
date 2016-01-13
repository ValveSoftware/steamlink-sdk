// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/key_event_converter_evdev.h"

#include <errno.h>
#include <linux/input.h>

#include "base/message_loop/message_loop.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/ozone/evdev/event_modifiers_evdev.h"
#include "ui/ozone/public/event_factory_ozone.h"

namespace ui {

namespace {

ui::KeyboardCode KeyboardCodeFromButton(unsigned int code) {
  static const ui::KeyboardCode kLinuxBaseKeyMap[] = {
      ui::VKEY_UNKNOWN,            // KEY_RESERVED
      ui::VKEY_ESCAPE,             // KEY_ESC
      ui::VKEY_1,                  // KEY_1
      ui::VKEY_2,                  // KEY_2
      ui::VKEY_3,                  // KEY_3
      ui::VKEY_4,                  // KEY_4
      ui::VKEY_5,                  // KEY_5
      ui::VKEY_6,                  // KEY_6
      ui::VKEY_7,                  // KEY_7
      ui::VKEY_8,                  // KEY_8
      ui::VKEY_9,                  // KEY_9
      ui::VKEY_0,                  // KEY_0
      ui::VKEY_OEM_MINUS,          // KEY_MINUS
      ui::VKEY_OEM_PLUS,           // KEY_EQUAL
      ui::VKEY_BACK,               // KEY_BACKSPACE
      ui::VKEY_TAB,                // KEY_TAB
      ui::VKEY_Q,                  // KEY_Q
      ui::VKEY_W,                  // KEY_W
      ui::VKEY_E,                  // KEY_E
      ui::VKEY_R,                  // KEY_R
      ui::VKEY_T,                  // KEY_T
      ui::VKEY_Y,                  // KEY_Y
      ui::VKEY_U,                  // KEY_U
      ui::VKEY_I,                  // KEY_I
      ui::VKEY_O,                  // KEY_O
      ui::VKEY_P,                  // KEY_P
      ui::VKEY_OEM_4,              // KEY_LEFTBRACE
      ui::VKEY_OEM_6,              // KEY_RIGHTBRACE
      ui::VKEY_RETURN,             // KEY_ENTER
      ui::VKEY_CONTROL,            // KEY_LEFTCTRL
      ui::VKEY_A,                  // KEY_A
      ui::VKEY_S,                  // KEY_S
      ui::VKEY_D,                  // KEY_D
      ui::VKEY_F,                  // KEY_F
      ui::VKEY_G,                  // KEY_G
      ui::VKEY_H,                  // KEY_H
      ui::VKEY_J,                  // KEY_J
      ui::VKEY_K,                  // KEY_K
      ui::VKEY_L,                  // KEY_L
      ui::VKEY_OEM_1,              // KEY_SEMICOLON
      ui::VKEY_OEM_7,              // KEY_APOSTROPHE
      ui::VKEY_OEM_3,              // KEY_GRAVE
      ui::VKEY_SHIFT,              // KEY_LEFTSHIFT
      ui::VKEY_OEM_5,              // KEY_BACKSLASH
      ui::VKEY_Z,                  // KEY_Z
      ui::VKEY_X,                  // KEY_X
      ui::VKEY_C,                  // KEY_C
      ui::VKEY_V,                  // KEY_V
      ui::VKEY_B,                  // KEY_B
      ui::VKEY_N,                  // KEY_N
      ui::VKEY_M,                  // KEY_M
      ui::VKEY_OEM_COMMA,          // KEY_COMMA
      ui::VKEY_OEM_PERIOD,         // KEY_DOT
      ui::VKEY_OEM_2,              // KEY_SLASH
      ui::VKEY_SHIFT,              // KEY_RIGHTSHIFT
      ui::VKEY_MULTIPLY,           // KEY_KPASTERISK
      ui::VKEY_MENU,               // KEY_LEFTALT
      ui::VKEY_SPACE,              // KEY_SPACE
      ui::VKEY_CAPITAL,            // KEY_CAPSLOCK
      ui::VKEY_F1,                 // KEY_F1
      ui::VKEY_F2,                 // KEY_F2
      ui::VKEY_F3,                 // KEY_F3
      ui::VKEY_F4,                 // KEY_F4
      ui::VKEY_F5,                 // KEY_F5
      ui::VKEY_F6,                 // KEY_F6
      ui::VKEY_F7,                 // KEY_F7
      ui::VKEY_F8,                 // KEY_F8
      ui::VKEY_F9,                 // KEY_F9
      ui::VKEY_F10,                // KEY_F10
      ui::VKEY_NUMLOCK,            // KEY_NUMLOCK
      ui::VKEY_SCROLL,             // KEY_SCROLLLOCK
      ui::VKEY_NUMPAD7,            // KEY_KP7
      ui::VKEY_NUMPAD8,            // KEY_KP8
      ui::VKEY_NUMPAD9,            // KEY_KP9
      ui::VKEY_SUBTRACT,           // KEY_KPMINUS
      ui::VKEY_NUMPAD4,            // KEY_KP4
      ui::VKEY_NUMPAD5,            // KEY_KP5
      ui::VKEY_NUMPAD6,            // KEY_KP6
      ui::VKEY_ADD,                // KEY_KPPLUS
      ui::VKEY_NUMPAD1,            // KEY_KP1
      ui::VKEY_NUMPAD2,            // KEY_KP2
      ui::VKEY_NUMPAD3,            // KEY_KP3
      ui::VKEY_NUMPAD0,            // KEY_KP0
      ui::VKEY_DECIMAL,            // KEY_KPDOT
      ui::VKEY_UNKNOWN,            // (unassigned)
      ui::VKEY_DBE_DBCSCHAR,       // KEY_ZENKAKUHANKAKU
      ui::VKEY_OEM_102,            // KEY_102ND
      ui::VKEY_F11,                // KEY_F11
      ui::VKEY_F12,                // KEY_F12
      ui::VKEY_UNKNOWN,            // KEY_RO
      ui::VKEY_UNKNOWN,            // KEY_KATAKANA
      ui::VKEY_UNKNOWN,            // KEY_HIRAGANA
      ui::VKEY_CONVERT,            // KEY_HENKAN
      ui::VKEY_UNKNOWN,            // KEY_KATAKANAHIRAGANA
      ui::VKEY_NONCONVERT,         // KEY_MUHENKAN
      ui::VKEY_UNKNOWN,            // KEY_KPJPCOMMA
      ui::VKEY_RETURN,             // KEY_KPENTER
      ui::VKEY_CONTROL,            // KEY_RIGHTCTRL
      ui::VKEY_DIVIDE,             // KEY_KPSLASH
      ui::VKEY_PRINT,              // KEY_SYSRQ
      ui::VKEY_MENU,               // KEY_RIGHTALT
      ui::VKEY_RETURN,             // KEY_LINEFEED
      ui::VKEY_HOME,               // KEY_HOME
      ui::VKEY_UP,                 // KEY_UP
      ui::VKEY_PRIOR,              // KEY_PAGEUP
      ui::VKEY_LEFT,               // KEY_LEFT
      ui::VKEY_RIGHT,              // KEY_RIGHT
      ui::VKEY_END,                // KEY_END
      ui::VKEY_DOWN,               // KEY_DOWN
      ui::VKEY_NEXT,               // KEY_PAGEDOWN
      ui::VKEY_INSERT,             // KEY_INSERT
      ui::VKEY_DELETE,             // KEY_DELETE
      ui::VKEY_UNKNOWN,            // KEY_MACRO
      ui::VKEY_VOLUME_MUTE,        // KEY_MUTE
      ui::VKEY_VOLUME_DOWN,        // KEY_VOLUMEDOWN
      ui::VKEY_VOLUME_UP,          // KEY_VOLUMEUP
      ui::VKEY_POWER,              // KEY_POWER
      ui::VKEY_OEM_PLUS,           // KEY_KPEQUAL
      ui::VKEY_UNKNOWN,            // KEY_KPPLUSMINUS
      ui::VKEY_PAUSE,              // KEY_PAUSE
      ui::VKEY_MEDIA_LAUNCH_APP1,  // KEY_SCALE
      ui::VKEY_DECIMAL,            // KEY_KPCOMMA
      ui::VKEY_HANGUL,             // KEY_HANGEUL
      ui::VKEY_HANJA,              // KEY_HANJA
      ui::VKEY_UNKNOWN,            // KEY_YEN
      ui::VKEY_LWIN,               // KEY_LEFTMETA
      ui::VKEY_RWIN,               // KEY_RIGHTMETA
      ui::VKEY_APPS,               // KEY_COMPOSE
  };

  if (code < arraysize(kLinuxBaseKeyMap))
    return kLinuxBaseKeyMap[code];

  LOG(ERROR) << "Unknown key code: " << code;
  return ui::VKEY_UNKNOWN;
}

int ModifierFromButton(unsigned int code) {
  switch (code) {
    case KEY_CAPSLOCK:
      return EVDEV_MODIFIER_CAPS_LOCK;
    case KEY_LEFTSHIFT:
    case KEY_RIGHTSHIFT:
      return EVDEV_MODIFIER_SHIFT;
    case KEY_LEFTCTRL:
    case KEY_RIGHTCTRL:
      return EVDEV_MODIFIER_CONTROL;
    case KEY_LEFTALT:
    case KEY_RIGHTALT:
      return EVDEV_MODIFIER_ALT;
    case BTN_LEFT:
      return EVDEV_MODIFIER_LEFT_MOUSE_BUTTON;
    case BTN_MIDDLE:
      return EVDEV_MODIFIER_MIDDLE_MOUSE_BUTTON;
    case BTN_RIGHT:
      return EVDEV_MODIFIER_RIGHT_MOUSE_BUTTON;
    case KEY_LEFTMETA:
    case KEY_RIGHTMETA:
      return EVDEV_MODIFIER_COMMAND;
    default:
      return EVDEV_MODIFIER_NONE;
  }
}

bool IsLockButton(unsigned int code) { return code == KEY_CAPSLOCK; }

}  // namespace

KeyEventConverterEvdev::KeyEventConverterEvdev(
    int fd,
    base::FilePath path,
    EventModifiersEvdev* modifiers,
    const EventDispatchCallback& callback)
    : EventConverterEvdev(callback),
      fd_(fd),
      path_(path),
      modifiers_(modifiers) {
  // TODO(spang): Initialize modifiers using EVIOCGKEY.
}

KeyEventConverterEvdev::~KeyEventConverterEvdev() {
  Stop();
  close(fd_);
}

void KeyEventConverterEvdev::Start() {
  base::MessageLoopForUI::current()->WatchFileDescriptor(
      fd_, true, base::MessagePumpLibevent::WATCH_READ, &controller_, this);
}

void KeyEventConverterEvdev::Stop() {
  controller_.StopWatchingFileDescriptor();
}

void KeyEventConverterEvdev::OnFileCanReadWithoutBlocking(int fd) {
  input_event inputs[4];
  ssize_t read_size = read(fd, inputs, sizeof(inputs));
  if (read_size < 0) {
    if (errno == EINTR || errno == EAGAIN)
      return;
    if (errno != ENODEV)
      PLOG(ERROR) << "error reading device " << path_.value();
    Stop();
    return;
  }

  CHECK_EQ(read_size % sizeof(*inputs), 0u);
  ProcessEvents(inputs, read_size / sizeof(*inputs));
}

void KeyEventConverterEvdev::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

void KeyEventConverterEvdev::ProcessEvents(const input_event* inputs,
                                           int count) {
  for (int i = 0; i < count; ++i) {
    const input_event& input = inputs[i];
    if (input.type == EV_KEY) {
      ConvertKeyEvent(input.code, input.value);
    } else if (input.type == EV_SYN) {
      // TODO(sadrul): Handle this case appropriately.
    }
  }
}

void KeyEventConverterEvdev::ConvertKeyEvent(int key, int value) {
  int down = (value != 0);
  int repeat = (value == 2);
  int modifier = ModifierFromButton(key);
  ui::KeyboardCode code = KeyboardCodeFromButton(key);

  if (!repeat && (modifier != EVDEV_MODIFIER_NONE)) {
    if (IsLockButton(key)) {
      // Locking modifier keys: CapsLock.
      modifiers_->UpdateModifierLock(modifier, down);
    } else {
      // Regular modifier keys: Shift, Ctrl, Alt, etc.
      modifiers_->UpdateModifier(modifier, down);
    }
  }

  int flags = modifiers_->GetModifierFlags();

  KeyEvent key_event(
      down ? ET_KEY_PRESSED : ET_KEY_RELEASED, code, flags, false);
  DispatchEventToCallback(&key_event);
}

}  // namespace ui
