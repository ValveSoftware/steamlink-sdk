// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <wayland-server-core.h>
#include <xdg-shell-unstable-v5-server-protocol.h>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/ozone/platform/wayland/fake_server.h"
#include "ui/ozone/platform/wayland/mock_platform_window_delegate.h"
#include "ui/ozone/platform/wayland/wayland_test.h"
#include "ui/ozone/platform/wayland/wayland_window.h"

using ::testing::Eq;
using ::testing::Mock;
using ::testing::SaveArg;
using ::testing::StrEq;
using ::testing::_;

namespace ui {

class WaylandWindowTest : public WaylandTest {
 public:
  WaylandWindowTest()
      : test_mouse_event(ET_MOUSE_PRESSED,
                         gfx::Point(10, 15),
                         gfx::Point(10, 15),
                         ui::EventTimeStampFromSeconds(123456),
                         EF_LEFT_MOUSE_BUTTON | EF_RIGHT_MOUSE_BUTTON,
                         EF_LEFT_MOUSE_BUTTON) {}

  void SetUp() override {
    WaylandTest::SetUp();

    xdg_surface = surface->xdg_surface.get();
    ASSERT_TRUE(xdg_surface);
  }

 protected:
  wl::MockXdgSurface* xdg_surface;

  MouseEvent test_mouse_event;

 private:
  DISALLOW_COPY_AND_ASSIGN(WaylandWindowTest);
};

TEST_F(WaylandWindowTest, SetTitle) {
  EXPECT_CALL(*xdg_surface, SetTitle(StrEq("hello")));
  window.SetTitle(base::ASCIIToUTF16("hello"));
}

TEST_F(WaylandWindowTest, Maximize) {
  EXPECT_CALL(*xdg_surface, SetMaximized());
  window.Maximize();
}

TEST_F(WaylandWindowTest, Minimize) {
  EXPECT_CALL(*xdg_surface, SetMinimized());
  window.Minimize();
}

TEST_F(WaylandWindowTest, Restore) {
  EXPECT_CALL(*xdg_surface, UnsetMaximized());
  window.Restore();
}

TEST_F(WaylandWindowTest, CanDispatchMouseEventDefault) {
  EXPECT_FALSE(window.CanDispatchEvent(&test_mouse_event));
}

TEST_F(WaylandWindowTest, CanDispatchMouseEventFocus) {
  window.set_pointer_focus(true);
  EXPECT_TRUE(window.CanDispatchEvent(&test_mouse_event));
}

TEST_F(WaylandWindowTest, CanDispatchMouseEventUnfocus) {
  window.set_pointer_focus(false);
  EXPECT_FALSE(window.CanDispatchEvent(&test_mouse_event));
}

ACTION_P(CloneEvent, ptr) {
  *ptr = Event::Clone(*arg0);
}

TEST_F(WaylandWindowTest, DispatchEvent) {
  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate, DispatchEvent(_)).WillOnce(CloneEvent(&event));
  window.DispatchEvent(&test_mouse_event);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseEvent());
  auto mouse_event = static_cast<MouseEvent*>(event.get());
  EXPECT_EQ(mouse_event->location_f(), test_mouse_event.location_f());
  EXPECT_EQ(mouse_event->root_location_f(), test_mouse_event.root_location_f());
  EXPECT_EQ(mouse_event->time_stamp(), test_mouse_event.time_stamp());
  EXPECT_EQ(mouse_event->button_flags(), test_mouse_event.button_flags());
  EXPECT_EQ(mouse_event->changed_button_flags(),
            test_mouse_event.changed_button_flags());
}

TEST_F(WaylandWindowTest, ConfigureEvent) {
  wl_array states;
  wl_array_init(&states);
  xdg_surface_send_configure(xdg_surface->resource(), 1000, 1000, &states, 12);
  xdg_surface_send_configure(xdg_surface->resource(), 1500, 1000, &states, 13);

  // Make sure that the implementation does not call OnBoundsChanged for each
  // configure event if it receives multiple in a row.
  EXPECT_CALL(delegate, OnBoundsChanged(Eq(gfx::Rect(0, 0, 1500, 1000))));
  EXPECT_CALL(*xdg_surface, AckConfigure(13));
}

}  // namespace ui
