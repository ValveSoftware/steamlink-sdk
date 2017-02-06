// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/screen_mus.h"

#include "base/command_line.h"
#include "ui/display/display_switches.h"
#include "ui/display/screen.h"
#include "ui/views/mus/window_manager_connection.h"
#include "ui/views/test/scoped_views_test_helper.h"
#include "ui/views/test/views_test_base.h"

namespace views {
namespace {

TEST(ScreenMusTest, ConsistentDisplayInHighDPI) {
  base::MessageLoop message_loop(base::MessageLoop::TYPE_UI);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kForceDeviceScaleFactor, "2");
  ScopedViewsTestHelper test_helper;
  display::Screen* screen = display::Screen::GetScreen();
  std::vector<display::Display> displays = screen->GetAllDisplays();
  ASSERT_FALSE(displays.empty());
  for (const display::Display& display : displays) {
    EXPECT_EQ(2.f, display.device_scale_factor());
    EXPECT_EQ(display.work_area(), display.bounds());
  }
}

}  // namespace
}  // namespace views
