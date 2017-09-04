// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/screen.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"

namespace display {

TEST(ScreenTest, GetPrimaryDisplaySize) {
  // We aren't actually testing that it's correct, just that it's sane.
  const gfx::Size size = Screen::GetScreen()->GetPrimaryDisplay().size();
  EXPECT_GE(size.width(), 1);
  EXPECT_GE(size.height(), 1);
}

TEST(ScreenTest, GetNumDisplays) {
  // We aren't actually testing that it's correct, just that it's sane.
  EXPECT_GE(Screen::GetScreen()->GetNumDisplays(), 1);
}

}  // namespace
