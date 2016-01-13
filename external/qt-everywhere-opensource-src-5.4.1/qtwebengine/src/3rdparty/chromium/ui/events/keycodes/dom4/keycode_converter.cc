// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/keycodes/dom4/keycode_converter.h"

namespace ui {

namespace {

#if defined(OS_WIN)
#define USB_KEYMAP(usb, xkb, win, mac, code) {usb, win, code}
#elif defined(OS_LINUX)
#define USB_KEYMAP(usb, xkb, win, mac, code) {usb, xkb, code}
#elif defined(OS_MACOSX)
#define USB_KEYMAP(usb, xkb, win, mac, code) {usb, mac, code}
#else
#define USB_KEYMAP(usb, xkb, win, mac, code) {usb, 0, code}
#endif
#include "ui/events/keycodes/dom4/keycode_converter_data.h"

const size_t kKeycodeMapEntries = arraysize(usb_keycode_map);

}  // namespace

KeycodeConverter::KeycodeConverter() {
}

KeycodeConverter* KeycodeConverter::GetInstance() {
  return Singleton<KeycodeConverter>::get();
}

size_t KeycodeConverter::NumKeycodeMapEntriesForTest() {
  return kKeycodeMapEntries;
}

const KeycodeMapEntry* KeycodeConverter::GetKeycodeMapForTest() {
  return &usb_keycode_map[0];
}

uint16_t KeycodeConverter::InvalidNativeKeycode() {
  return usb_keycode_map[0].native_keycode;
}

const char* KeycodeConverter::InvalidKeyboardEventCode() {
  return "Unidentified";
}

const char* KeycodeConverter::NativeKeycodeToCode(uint16_t native_keycode) {
  for (size_t i = 0; i < kKeycodeMapEntries; ++i) {
    if (usb_keycode_map[i].native_keycode == native_keycode) {
      if (usb_keycode_map[i].code != NULL)
        return usb_keycode_map[i].code;
      break;
    }
  }
  return InvalidKeyboardEventCode();
}

uint16_t KeycodeConverter::CodeToNativeKeycode(const char* code) {
  if (!code ||
      strcmp(code, InvalidKeyboardEventCode()) == 0) {
    return InvalidNativeKeycode();
  }

  for (size_t i = 0; i < kKeycodeMapEntries; ++i) {
    if (usb_keycode_map[i].code &&
        strcmp(usb_keycode_map[i].code, code) == 0) {
      return usb_keycode_map[i].native_keycode;
    }
  }
  return InvalidNativeKeycode();
}

// USB keycodes
// Note that USB keycodes are not part of any web standard.
// Please don't use USB keycodes in new code.

uint16_t KeycodeConverter::InvalidUsbKeycode() {
  return usb_keycode_map[0].usb_keycode;
}

uint16_t KeycodeConverter::UsbKeycodeToNativeKeycode(uint32_t usb_keycode) {
  // Deal with some special-cases that don't fit the 1:1 mapping.
  if (usb_keycode == 0x070032) // non-US hash.
    usb_keycode = 0x070031; // US backslash.
#if defined(OS_MACOSX)
  if (usb_keycode == 0x070046) // PrintScreen.
    usb_keycode = 0x070068; // F13.
#endif

  for (size_t i = 0; i < kKeycodeMapEntries; ++i) {
    if (usb_keycode_map[i].usb_keycode == usb_keycode)
      return usb_keycode_map[i].native_keycode;
  }
  return InvalidNativeKeycode();
}

uint32_t KeycodeConverter::NativeKeycodeToUsbKeycode(uint16_t native_keycode) {
  for (size_t i = 0; i < kKeycodeMapEntries; ++i) {
    if (usb_keycode_map[i].native_keycode == native_keycode)
      return usb_keycode_map[i].usb_keycode;
  }
  return InvalidUsbKeycode();
}

const char* KeycodeConverter::UsbKeycodeToCode(uint32_t usb_keycode) {
  for (size_t i = 0; i < kKeycodeMapEntries; ++i) {
    if (usb_keycode_map[i].usb_keycode == usb_keycode)
      return usb_keycode_map[i].code;
  }
  return InvalidKeyboardEventCode();
}

uint32_t KeycodeConverter::CodeToUsbKeycode(const char* code) {
  if (!code ||
      strcmp(code, InvalidKeyboardEventCode()) == 0) {
    return InvalidUsbKeycode();
  }

  for (size_t i = 0; i < kKeycodeMapEntries; ++i) {
    if (usb_keycode_map[i].code &&
        strcmp(usb_keycode_map[i].code, code) == 0) {
      return usb_keycode_map[i].usb_keycode;
    }
  }
  return InvalidUsbKeycode();
}

}  // namespace ui
