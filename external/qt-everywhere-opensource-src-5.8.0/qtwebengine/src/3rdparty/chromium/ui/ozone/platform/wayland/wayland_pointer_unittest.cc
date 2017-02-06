// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <linux/input.h>
#include <wayland-server.h>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/ozone/platform/wayland/fake_server.h"
#include "ui/ozone/platform/wayland/mock_platform_window_delegate.h"
#include "ui/ozone/platform/wayland/wayland_test.h"
#include "ui/ozone/platform/wayland/wayland_window.h"

using ::testing::SaveArg;
using ::testing::_;

namespace ui {

class WaylandPointerTest : public WaylandTest {
 public:
  WaylandPointerTest() {}

  void SetUp() override {
    WaylandTest::SetUp();

    wl_seat_send_capabilities(server.seat()->resource(),
                              WL_SEAT_CAPABILITY_POINTER);

    Sync();

    pointer = server.seat()->pointer.get();
    ASSERT_TRUE(pointer);
  }

 protected:
  wl::MockPointer* pointer;

 private:
  DISALLOW_COPY_AND_ASSIGN(WaylandPointerTest);
};

TEST_F(WaylandPointerTest, Leave) {
  MockPlatformWindowDelegate other_delegate;
  WaylandWindow other_window(&other_delegate, &display,
                             gfx::Rect(0, 0, 10, 10));
  gfx::AcceleratedWidget other_widget = gfx::kNullAcceleratedWidget;
  EXPECT_CALL(other_delegate, OnAcceleratedWidgetAvailable(_, _))
      .WillOnce(SaveArg<0>(&other_widget));
  ASSERT_TRUE(other_window.Initialize());
  ASSERT_NE(other_widget, gfx::kNullAcceleratedWidget);

  Sync();

  wl::MockSurface* other_surface =
      server.GetObject<wl::MockSurface>(other_widget);
  ASSERT_TRUE(other_surface);

  wl_pointer_send_enter(pointer->resource(), 1, surface->resource(), 0, 0);
  wl_pointer_send_leave(pointer->resource(), 2, surface->resource());
  wl_pointer_send_enter(pointer->resource(), 3, other_surface->resource(), 0,
                        0);
  wl_pointer_send_button(pointer->resource(), 4, 1004, BTN_LEFT,
                         WL_POINTER_BUTTON_STATE_PRESSED);
  EXPECT_CALL(delegate, DispatchEvent(_)).Times(0);

  // Do an extra Sync() here so that we process the second enter event before we
  // destroy |other_window|.
  Sync();
}

ACTION_P(CloneEvent, ptr) {
  *ptr = Event::Clone(*arg0);
}

TEST_F(WaylandPointerTest, Motion) {
  wl_pointer_send_enter(pointer->resource(), 1, surface->resource(), 0, 0);
  wl_pointer_send_motion(pointer->resource(), 1002, wl_fixed_from_double(10.75),
                         wl_fixed_from_double(20.375));

  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate, DispatchEvent(_)).WillOnce(CloneEvent(&event));

  Sync();

  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseEvent());
  auto mouse_event = static_cast<MouseEvent*>(event.get());
  EXPECT_EQ(ET_MOUSE_MOVED, mouse_event->type());
  EXPECT_EQ(0, mouse_event->button_flags());
  EXPECT_EQ(0, mouse_event->changed_button_flags());
  // TODO(forney): Once crbug.com/337827 is solved, compare with the fractional
  // coordinates sent above.
  EXPECT_EQ(gfx::PointF(10, 20), mouse_event->location_f());
  EXPECT_EQ(gfx::PointF(10, 20), mouse_event->root_location_f());
}

TEST_F(WaylandPointerTest, MotionDragged) {
  wl_pointer_send_enter(pointer->resource(), 1, surface->resource(), 0, 0);
  wl_pointer_send_button(pointer->resource(), 2, 1002, BTN_MIDDLE,
                         WL_POINTER_BUTTON_STATE_PRESSED);

  Sync();

  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate, DispatchEvent(_)).WillOnce(CloneEvent(&event));
  wl_pointer_send_motion(pointer->resource(), 1003, wl_fixed_from_int(400),
                         wl_fixed_from_int(500));

  Sync();

  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseEvent());
  auto mouse_event = static_cast<MouseEvent*>(event.get());
  EXPECT_EQ(ET_MOUSE_DRAGGED, mouse_event->type());
  EXPECT_EQ(EF_MIDDLE_MOUSE_BUTTON, mouse_event->button_flags());
  EXPECT_EQ(0, mouse_event->changed_button_flags());
  EXPECT_EQ(gfx::PointF(400, 500), mouse_event->location_f());
  EXPECT_EQ(gfx::PointF(400, 500), mouse_event->root_location_f());
}

TEST_F(WaylandPointerTest, ButtonPress) {
  wl_pointer_send_enter(pointer->resource(), 1, surface->resource(),
                        wl_fixed_from_int(200), wl_fixed_from_int(150));
  wl_pointer_send_button(pointer->resource(), 2, 1002, BTN_RIGHT,
                         WL_POINTER_BUTTON_STATE_PRESSED);

  Sync();

  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate, DispatchEvent(_)).WillOnce(CloneEvent(&event));
  wl_pointer_send_button(pointer->resource(), 3, 1003, BTN_LEFT,
                         WL_POINTER_BUTTON_STATE_PRESSED);

  Sync();

  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseEvent());
  auto mouse_event = static_cast<MouseEvent*>(event.get());
  EXPECT_EQ(ET_MOUSE_PRESSED, mouse_event->type());
  EXPECT_EQ(EF_LEFT_MOUSE_BUTTON | EF_RIGHT_MOUSE_BUTTON,
            mouse_event->button_flags());
  EXPECT_EQ(EF_LEFT_MOUSE_BUTTON, mouse_event->changed_button_flags());
  EXPECT_EQ(gfx::PointF(200, 150), mouse_event->location_f());
  EXPECT_EQ(gfx::PointF(200, 150), mouse_event->root_location_f());
}

TEST_F(WaylandPointerTest, ButtonRelease) {
  wl_pointer_send_enter(pointer->resource(), 1, surface->resource(),
                        wl_fixed_from_int(50), wl_fixed_from_int(50));
  wl_pointer_send_button(pointer->resource(), 2, 1002, BTN_BACK,
                         WL_POINTER_BUTTON_STATE_PRESSED);
  wl_pointer_send_button(pointer->resource(), 3, 1003, BTN_LEFT,
                         WL_POINTER_BUTTON_STATE_PRESSED);

  Sync();

  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate, DispatchEvent(_)).WillOnce(CloneEvent(&event));
  wl_pointer_send_button(pointer->resource(), 4, 1004, BTN_LEFT,
                         WL_POINTER_BUTTON_STATE_RELEASED);

  Sync();

  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseEvent());
  auto mouse_event = static_cast<MouseEvent*>(event.get());
  EXPECT_EQ(ET_MOUSE_RELEASED, mouse_event->type());
  EXPECT_EQ(EF_LEFT_MOUSE_BUTTON | EF_BACK_MOUSE_BUTTON,
            mouse_event->button_flags());
  EXPECT_EQ(EF_LEFT_MOUSE_BUTTON, mouse_event->changed_button_flags());
  EXPECT_EQ(gfx::PointF(50, 50), mouse_event->location_f());
  EXPECT_EQ(gfx::PointF(50, 50), mouse_event->root_location_f());
}

TEST_F(WaylandPointerTest, AxisVertical) {
  wl_pointer_send_enter(pointer->resource(), 1, surface->resource(),
                        wl_fixed_from_int(0), wl_fixed_from_int(0));
  wl_pointer_send_button(pointer->resource(), 2, 1002, BTN_RIGHT,
                         WL_POINTER_BUTTON_STATE_PRESSED);

  Sync();

  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate, DispatchEvent(_)).WillOnce(CloneEvent(&event));
  // Wayland servers typically send a value of 10 per mouse wheel click.
  wl_pointer_send_axis(pointer->resource(), 1003,
                       WL_POINTER_AXIS_VERTICAL_SCROLL, wl_fixed_from_int(20));

  Sync();

  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseWheelEvent());
  auto mouse_wheel_event = static_cast<MouseWheelEvent*>(event.get());
  EXPECT_EQ(gfx::Vector2d(0, -2 * MouseWheelEvent::kWheelDelta),
            mouse_wheel_event->offset());
  EXPECT_EQ(EF_RIGHT_MOUSE_BUTTON, mouse_wheel_event->button_flags());
  EXPECT_EQ(0, mouse_wheel_event->changed_button_flags());
  EXPECT_EQ(gfx::PointF(), mouse_wheel_event->location_f());
  EXPECT_EQ(gfx::PointF(), mouse_wheel_event->root_location_f());
}

TEST_F(WaylandPointerTest, AxisHorizontal) {
  wl_pointer_send_enter(pointer->resource(), 1, surface->resource(),
                        wl_fixed_from_int(50), wl_fixed_from_int(75));
  wl_pointer_send_button(pointer->resource(), 2, 1002, BTN_LEFT,
                         WL_POINTER_BUTTON_STATE_PRESSED);

  Sync();

  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate, DispatchEvent(_)).WillOnce(CloneEvent(&event));
  // Wayland servers typically send a value of 10 per mouse wheel click.
  wl_pointer_send_axis(pointer->resource(), 1003,
                       WL_POINTER_AXIS_HORIZONTAL_SCROLL,
                       wl_fixed_from_int(10));

  Sync();

  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseWheelEvent());
  auto mouse_wheel_event = static_cast<MouseWheelEvent*>(event.get());
  EXPECT_EQ(gfx::Vector2d(MouseWheelEvent::kWheelDelta, 0),
            mouse_wheel_event->offset());
  EXPECT_EQ(EF_LEFT_MOUSE_BUTTON, mouse_wheel_event->button_flags());
  EXPECT_EQ(0, mouse_wheel_event->changed_button_flags());
  EXPECT_EQ(gfx::PointF(50, 75), mouse_wheel_event->location_f());
  EXPECT_EQ(gfx::PointF(50, 75), mouse_wheel_event->root_location_f());
}

}  // namespace ui
