// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/browser/api/device_permissions_prompt.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

class DevicePermissionsPromptTest : public testing::Test {};

TEST_F(DevicePermissionsPromptTest, HidPromptMessages) {
  scoped_refptr<Extension> extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                           .Set("name", "Test Application")
                           .Set("manifest_version", 2)
                           .Set("version", "1.0")
                           .Build())
          .Build();

  scoped_refptr<DevicePermissionsPrompt::Prompt> prompt =
      DevicePermissionsPrompt::CreateHidPromptForTest(extension.get(), false);
  EXPECT_EQ(base::ASCIIToUTF16("Select a HID device"), prompt->GetHeading());
  EXPECT_EQ(
      base::ASCIIToUTF16(
          "The application \"Test Application\" is requesting access to one of "
          "your devices."),
      prompt->GetPromptMessage());

  prompt =
      DevicePermissionsPrompt::CreateHidPromptForTest(extension.get(), true);
  EXPECT_EQ(base::ASCIIToUTF16("Select HID devices"), prompt->GetHeading());
  EXPECT_EQ(
      base::ASCIIToUTF16(
          "The application \"Test Application\" is requesting access to one or "
          "more of your devices."),
      prompt->GetPromptMessage());
}

TEST_F(DevicePermissionsPromptTest, UsbPromptMessages) {
  scoped_refptr<Extension> extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                           .Set("name", "Test Application")
                           .Set("manifest_version", 2)
                           .Set("version", "1.0")
                           .Build())
          .Build();

  scoped_refptr<DevicePermissionsPrompt::Prompt> prompt =
      DevicePermissionsPrompt::CreateUsbPromptForTest(extension.get(), false);
  EXPECT_EQ(base::ASCIIToUTF16("Select a USB device"), prompt->GetHeading());
  EXPECT_EQ(
      base::ASCIIToUTF16(
          "The application \"Test Application\" is requesting access to one of "
          "your devices."),
      prompt->GetPromptMessage());

  prompt =
      DevicePermissionsPrompt::CreateUsbPromptForTest(extension.get(), true);
  EXPECT_EQ(base::ASCIIToUTF16("Select USB devices"), prompt->GetHeading());
  EXPECT_EQ(
      base::ASCIIToUTF16(
          "The application \"Test Application\" is requesting access to one or "
          "more of your devices."),
      prompt->GetPromptMessage());
}

}  // namespace

}  // namespace extensions
