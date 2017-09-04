// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/kiosk_mode_info.h"
#include "extensions/common/manifest_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using KioskModeInfoManifestTest = ManifestTest;

TEST_F(KioskModeInfoManifestTest, NoSecondaryApps) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("kiosk_secondary_app_no_secondary_app.json"));
  EXPECT_FALSE(KioskModeInfo::HasSecondaryApps(extension.get()));
}

TEST_F(KioskModeInfoManifestTest, MultipleSecondaryApps) {
  const std::string expected_ids[] = {
      "fiehokkcgaojmbhfhlpiheggjhaedjoc",
      "ihplaomghjbeafnpnjkhppmfpnmdihgd"};
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("kiosk_secondary_app_multi_apps.json"));
  EXPECT_TRUE(KioskModeInfo::HasSecondaryApps(extension.get()));
  KioskModeInfo* info = KioskModeInfo::Get(extension.get());
  EXPECT_NE(nullptr, info);
  std::vector<std::string> parsed_ids(info->secondary_app_ids);
  std::sort(parsed_ids.begin(), parsed_ids.end());
  EXPECT_TRUE(
      std::equal(parsed_ids.begin(), parsed_ids.end(), expected_ids));
}

TEST_F(KioskModeInfoManifestTest, RequiredPlatformVersionOptional) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("kiosk_required_platform_version_not_present.json"));
  KioskModeInfo* info = KioskModeInfo::Get(extension.get());
  EXPECT_TRUE(info->required_platform_version.empty());
}

TEST_F(KioskModeInfoManifestTest, RequiredPlatformVersion) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("kiosk_required_platform_version.json"));
  KioskModeInfo* info = KioskModeInfo::Get(extension.get());
  EXPECT_EQ("1234.0.0", info->required_platform_version);
}

TEST_F(KioskModeInfoManifestTest, RequiredPlatformVersionInvalid) {
  LoadAndExpectError("kiosk_required_platform_version_empty.json",
                     manifest_errors::kInvalidKioskRequiredPlatformVersion);
  LoadAndExpectError("kiosk_required_platform_version_invalid.json",
                     manifest_errors::kInvalidKioskRequiredPlatformVersion);
}

}  // namespace extensions
