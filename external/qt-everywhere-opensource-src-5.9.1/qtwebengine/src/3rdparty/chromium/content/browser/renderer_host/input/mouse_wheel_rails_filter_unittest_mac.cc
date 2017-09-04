// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/mouse_wheel_rails_filter_mac.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::WebInputEvent;
using blink::WebMouseWheelEvent;

namespace content {
namespace {

WebMouseWheelEvent MakeEvent(WebMouseWheelEvent::Phase phase,
                             float delta_x,
                             float delta_y) {
  WebMouseWheelEvent event;
  event.phase = phase;
  event.deltaX = delta_x;
  event.deltaY = delta_y;
  return event;
}

TEST(MouseWheelRailsFilterMacTest, Functionality) {
  WebInputEvent::RailsMode mode;
  MouseWheelRailsFilterMac filter;

  // Start with a mostly-horizontal event and see that it is locked
  // horizontally and continues to be locked.
  mode =
      filter.UpdateRailsMode(MakeEvent(WebMouseWheelEvent::PhaseBegan, 2, 1));
  EXPECT_EQ(mode, WebInputEvent::RailsModeHorizontal);
  mode = filter.UpdateRailsMode(
      MakeEvent(WebMouseWheelEvent::PhaseChanged, 2, 2));
  EXPECT_EQ(mode, WebInputEvent::RailsModeHorizontal);
  mode = filter.UpdateRailsMode(
      MakeEvent(WebMouseWheelEvent::PhaseChanged, 10, -4));
  EXPECT_EQ(mode, WebInputEvent::RailsModeHorizontal);

  // Change from horizontal to vertical and back.
  mode =
      filter.UpdateRailsMode(MakeEvent(WebMouseWheelEvent::PhaseBegan, 4, 1));
  EXPECT_EQ(mode, WebInputEvent::RailsModeHorizontal);
  mode = filter.UpdateRailsMode(
      MakeEvent(WebMouseWheelEvent::PhaseChanged, 3, 4));
  EXPECT_EQ(mode, WebInputEvent::RailsModeHorizontal);
  mode = filter.UpdateRailsMode(
      MakeEvent(WebMouseWheelEvent::PhaseChanged, 1, 4));
  EXPECT_EQ(mode, WebInputEvent::RailsModeVertical);
  mode = filter.UpdateRailsMode(
      MakeEvent(WebMouseWheelEvent::PhaseChanged, 10, 0));
  EXPECT_EQ(mode, WebInputEvent::RailsModeHorizontal);

  // Make sure zeroes don't break things.
  mode = filter.UpdateRailsMode(
      MakeEvent(WebMouseWheelEvent::PhaseChanged, 0, 0));
  EXPECT_EQ(mode, WebInputEvent::RailsModeFree);
  mode =
      filter.UpdateRailsMode(MakeEvent(WebMouseWheelEvent::PhaseBegan, 0, 0));
  EXPECT_EQ(mode, WebInputEvent::RailsModeFree);
}

}  // namespace
}  // namespace content
