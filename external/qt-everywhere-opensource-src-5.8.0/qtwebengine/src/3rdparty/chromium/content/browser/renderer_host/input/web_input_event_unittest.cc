// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/display/display.h"
#include "ui/display/display_switches.h"
#include "ui/events/event_constants.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "content/browser/renderer_host/input/web_input_event_builders_win.h"
#endif

using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;

namespace content {

#if defined(OS_WIN)
// This test validates that Pixel to DIP conversion occurs as needed in the
// WebMouseEventBuilder::Build function.
TEST(WebInputEventBuilderTest, TestMouseEventScale) {
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;

  display::Display::ResetForceDeviceScaleFactorForTesting();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "2");

  // Synthesize a mouse move with x = 300 and y = 200.
  WebMouseEvent mouse_move = WebMouseEventBuilder::Build(
      ::GetDesktopWindow(), WM_MOUSEMOVE, 0, MAKELPARAM(300, 200), 100,
      blink::WebPointerProperties::PointerType::Mouse);

  // The WebMouseEvent.x, WebMouseEvent.y, WebMouseEvent.windowX and
  // WebMouseEvent.windowY fields should be in pixels on return and hence
  // should be the same value as the x and y coordinates passed in to the
  // WebMouseEventBuilder::Build function.
  EXPECT_EQ(300, mouse_move.x);
  EXPECT_EQ(200, mouse_move.y);

  EXPECT_EQ(300, mouse_move.windowX);
  EXPECT_EQ(200, mouse_move.windowY);

  // WebMouseEvent.globalX and WebMouseEvent.globalY are calculated in DIPs.
  EXPECT_EQ(150, mouse_move.globalX);
  EXPECT_EQ(100, mouse_move.globalY);

  EXPECT_EQ(blink::WebPointerProperties::PointerType::Mouse,
            mouse_move.pointerType);

  command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "1");
  display::Display::ResetForceDeviceScaleFactorForTesting();
}
#endif

}  // namespace content
