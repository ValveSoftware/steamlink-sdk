// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/fullscreen_window_manager.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "ui/gfx/test/ui_cocoa_test_helper.h"

typedef ui::CocoaTest FullscreenWindowManagerTest;

TEST_F(FullscreenWindowManagerTest, EnterExit) {
  base::scoped_nsobject<FullscreenWindowManager> manager(
      [[FullscreenWindowManager alloc] initWithWindow:test_window()
                                        desiredScreen:[NSScreen mainScreen]]);

  NSApplicationPresentationOptions current_options =
      [NSApp presentationOptions];
  EXPECT_EQ(NSApplicationPresentationDefault, current_options);

  [manager enterFullscreenMode];
  current_options = [NSApp presentationOptions];
  EXPECT_EQ(static_cast<NSApplicationPresentationOptions>(
                NSApplicationPresentationHideDock |
                NSApplicationPresentationHideMenuBar),
            current_options);

  [manager exitFullscreenMode];
  current_options = [NSApp presentationOptions];
  EXPECT_EQ(NSApplicationPresentationDefault, current_options);
}
