// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/osk_display_manager.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/win/windows_version.h"
#include "testing/gtest/include/gtest/gtest.h"

// This test validates the on screen keyboard path (tabtip.exe) which is read
// from the registry.
TEST(OnScreenKeyboardTest, OSKPath) {
  // The on screen keyboard is only available on Windows 8+.
  if (base::win::GetVersion() < base::win::VERSION_WIN8)
    return;

  ui::OnScreenKeyboardDisplayManager* keyboard_display_manager =
      ui::OnScreenKeyboardDisplayManager::GetInstance();
  EXPECT_NE(nullptr, keyboard_display_manager);

  base::string16 osk_path;
  EXPECT_TRUE(keyboard_display_manager->GetOSKPath(&osk_path));
  EXPECT_FALSE(osk_path.empty());
  EXPECT_TRUE(osk_path.find(L"tabtip.exe") != base::string16::npos);

  // The path read from the registry can be quoted. To check for the existence
  // of the file we use the base::PathExists function which internally uses the
  // GetFileAttributes API which does not accept quoted strings. Our workaround
  // is to look for quotes in the first and last position in the string and
  // erase them.
  if ((osk_path.front() == L'"') && (osk_path.back() == L'"'))
    osk_path = osk_path.substr(1, osk_path.size() - 2);

  EXPECT_TRUE(base::PathExists(base::FilePath(osk_path)));
}
