// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/window_manager_connection.h"

#include "base/message_loop/message_loop.h"
#include "services/ui/public/cpp/window.h"
#include "services/ui/public/cpp/window_private.h"
#include "services/ui/public/cpp/window_tree_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/mus/screen_mus.h"
#include "ui/views/mus/test_utils.h"
#include "ui/views/test/scoped_views_test_helper.h"

namespace views {
namespace {

class WindowManagerConnectionTest : public testing::Test {
 public:
  WindowManagerConnectionTest() : message_loop_(base::MessageLoop::TYPE_UI) {}
  ~WindowManagerConnectionTest() override {}

  WindowManagerConnection* connection() {
    return WindowManagerConnection::Get();
  }

  ScreenMusDelegate* screen_mus_delegate() { return connection(); }

  ui::Window* GetWindowAtScreenPoint(const gfx::Point& point) {
    return test::WindowManagerConnectionTestApi(connection())
        .GetUiWindowAtScreenPoint(point);
  }

 private:
  base::MessageLoop message_loop_;
  ScopedViewsTestHelper helper_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerConnectionTest);
};

TEST_F(WindowManagerConnectionTest, GetWindowAtScreenPointRecurse) {
  ui::Window* one = connection()->client()->NewTopLevelWindow(nullptr);
  one->SetBounds(gfx::Rect(0, 0, 100, 100));
  one->SetVisible(true);

  std::vector<display::Display> displays =
      display::Screen::GetScreen()->GetAllDisplays();
  ASSERT_GE(displays.size(), 1u);
  ui::WindowPrivate(one).LocalSetDisplay(displays[0].id());

  ui::Window* two = connection()->client()->NewWindow();
  one->AddChild(two);
  two->SetBounds(gfx::Rect(10, 10, 80, 80));
  two->SetVisible(true);

  // Ensure that we recurse down to the second window.
  EXPECT_EQ(two, GetWindowAtScreenPoint(gfx::Point(50, 50)));
}

TEST_F(WindowManagerConnectionTest, GetWindowAtScreenPointRecurseButIgnore) {
  ui::Window* one = connection()->client()->NewTopLevelWindow(nullptr);
  one->SetBounds(gfx::Rect(0, 0, 100, 100));
  one->SetVisible(true);

  std::vector<display::Display> displays =
      display::Screen::GetScreen()->GetAllDisplays();
  ASSERT_GE(displays.size(), 1u);
  ui::WindowPrivate(one).LocalSetDisplay(displays[0].id());

  ui::Window* two = connection()->client()->NewWindow();
  one->AddChild(two);
  two->SetBounds(gfx::Rect(5, 5, 10, 10));
  two->SetVisible(true);

  // We'll recurse down, but we'll use the parent anyway because the children
  // don't match the bounds.
  EXPECT_EQ(one, GetWindowAtScreenPoint(gfx::Point(50, 50)));
}

TEST_F(WindowManagerConnectionTest, GetWindowAtScreenPointDisplayOffset) {
  ui::Window* one = connection()->client()->NewTopLevelWindow(nullptr);
  one->SetBounds(gfx::Rect(5, 5, 5, 5));
  one->SetVisible(true);

  std::vector<display::Display> displays =
      display::Screen::GetScreen()->GetAllDisplays();
  ASSERT_GE(displays.size(), 1u);
  ui::WindowPrivate(one).LocalSetDisplay(displays[0].id());

  // Make the first display offset by 50.
  test::WindowManagerConnectionTestApi api(connection());
  display::Display display(
      *api.screen()->display_list().FindDisplayById(displays[0].id()));
  display.set_bounds(gfx::Rect(44, 44, 50, 50));
  api.screen()->display_list().UpdateDisplay(display);

  EXPECT_EQ(one, GetWindowAtScreenPoint(gfx::Point(50, 50)));
}

TEST_F(WindowManagerConnectionTest, IgnoresHiddenWindows) {
  // Hide the one window.
  ui::Window* one = connection()->client()->NewTopLevelWindow(nullptr);
  one->SetBounds(gfx::Rect(0, 0, 100, 100));
  one->SetVisible(false);

  std::vector<display::Display> displays =
      display::Screen::GetScreen()->GetAllDisplays();
  ASSERT_GE(displays.size(), 1u);
  ui::WindowPrivate(one).LocalSetDisplay(displays[0].id());

  EXPECT_EQ(nullptr, GetWindowAtScreenPoint(gfx::Point(50, 50)));
}

}  // namespace
}  // namespace views
