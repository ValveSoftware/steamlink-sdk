// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/keycodes/dom4/keycode_converter.h"

#include <map>

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"

using ui::KeycodeConverter;

namespace {

#if defined(OS_WIN)
const size_t kExpectedMappedKeyCount = 138;
#elif defined(OS_LINUX)
const size_t kExpectedMappedKeyCount = 145;
#elif defined(OS_MACOSX)
const size_t kExpectedMappedKeyCount = 118;
#else
const size_t kExpectedMappedKeyCount = 0;
#endif

const uint32_t kUsbNonExistentKeycode = 0xffffff;
const uint32_t kUsbUsBackslash =        0x070031;
const uint32_t kUsbNonUsHash =          0x070032;

TEST(UsbKeycodeMap, Basic) {
  ui::KeycodeConverter* key_converter = ui::KeycodeConverter::GetInstance();
  // Verify that the first element in the table is the "invalid" code.
  const ui::KeycodeMapEntry* keycode_map =
      key_converter->GetKeycodeMapForTest();
  EXPECT_EQ(key_converter->InvalidUsbKeycode(), keycode_map[0].usb_keycode);
  EXPECT_EQ(key_converter->InvalidNativeKeycode(),
            keycode_map[0].native_keycode);
  EXPECT_STREQ(key_converter->InvalidKeyboardEventCode(), "Unidentified");
  EXPECT_EQ(key_converter->InvalidNativeKeycode(),
            key_converter->CodeToNativeKeycode("Unidentified"));

  // Verify that there are no duplicate entries in the mapping.
  std::map<uint32_t, uint16_t> usb_to_native;
  std::map<uint16_t, uint32_t> native_to_usb;
  size_t numEntries = key_converter->NumKeycodeMapEntriesForTest();
  for (size_t i = 0; i < numEntries; ++i) {
    const ui::KeycodeMapEntry* entry = &keycode_map[i];
    // Don't test keys with no native keycode mapping on this platform.
    if (entry->native_keycode == key_converter->InvalidNativeKeycode())
      continue;

    // Verify UsbKeycodeToNativeKeycode works for this key.
    EXPECT_EQ(entry->native_keycode,
              key_converter->UsbKeycodeToNativeKeycode(entry->usb_keycode));

    // Verify CodeToNativeKeycode and NativeKeycodeToCode work correctly.
    if (entry->code) {
      EXPECT_EQ(entry->native_keycode,
                key_converter->CodeToNativeKeycode(entry->code));
      EXPECT_STREQ(entry->code,
                   key_converter->NativeKeycodeToCode(entry->native_keycode));
    }
    else {
      EXPECT_EQ(key_converter->InvalidNativeKeycode(),
                key_converter->CodeToNativeKeycode(entry->code));
    }

    // Verify that the USB or native codes aren't duplicated.
    EXPECT_EQ(0U, usb_to_native.count(entry->usb_keycode))
        << " duplicate of USB code 0x" << std::hex << std::setfill('0')
        << std::setw(6) << entry->usb_keycode
        << " to native 0x"
        << std::setw(4) << entry->native_keycode
        << " (previous was 0x"
        << std::setw(4) << usb_to_native[entry->usb_keycode]
        << ")";
    usb_to_native[entry->usb_keycode] = entry->native_keycode;
    EXPECT_EQ(0U, native_to_usb.count(entry->native_keycode))
        << " duplicate of native code 0x" << std::hex << std::setfill('0')
        << std::setw(4) << entry->native_keycode
        << " to USB 0x"
        << std::setw(6) << entry->usb_keycode
        << " (previous was 0x"
        << std::setw(6) << native_to_usb[entry->native_keycode]
        << ")";
    native_to_usb[entry->native_keycode] = entry->usb_keycode;
  }
  ASSERT_EQ(usb_to_native.size(), native_to_usb.size());

  // Verify that the number of mapped keys is what we expect, i.e. we haven't
  // lost any, and if we've added some then the expectation has been updated.
  EXPECT_EQ(kExpectedMappedKeyCount, usb_to_native.size());
}

TEST(UsbKeycodeMap, NonExistent) {
  // Verify that UsbKeycodeToNativeKeycode works for a non-existent USB keycode.
  ui::KeycodeConverter* key_converter = ui::KeycodeConverter::GetInstance();
  EXPECT_EQ(key_converter->InvalidNativeKeycode(),
            key_converter->UsbKeycodeToNativeKeycode(kUsbNonExistentKeycode));
}

TEST(UsbKeycodeMap, UsBackslashIsNonUsHash) {
  // Verify that UsbKeycodeToNativeKeycode treats the non-US "hash" key
  // as equivalent to the US "backslash" key.
  ui::KeycodeConverter* key_converter = ui::KeycodeConverter::GetInstance();
  EXPECT_EQ(key_converter->UsbKeycodeToNativeKeycode(kUsbUsBackslash),
            key_converter->UsbKeycodeToNativeKeycode(kUsbNonUsHash));
}

}  // namespace
