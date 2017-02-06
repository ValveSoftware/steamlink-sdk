// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/window_manager_connection.h"

#include <memory>

#include "base/message_loop/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/views/pointer_watcher.h"
#include "ui/views/test/scoped_views_test_helper.h"

namespace views {
namespace {

class TestPointerWatcher : public PointerWatcher {
 public:
  TestPointerWatcher() {}
  ~TestPointerWatcher() override {}

  bool mouse_pressed() const { return mouse_pressed_; }
  bool touch_pressed() const { return touch_pressed_; }

  void Reset() {
    mouse_pressed_ = false;
    touch_pressed_ = false;
  }

  // PointerWatcher:
  void OnMousePressed(const ui::MouseEvent& event,
                      const gfx::Point& location_in_screen,
                      Widget* target) override {
    mouse_pressed_ = true;
  }
  void OnTouchPressed(const ui::TouchEvent& event,
                      const gfx::Point& location_in_screen,
                      Widget* target) override {
    touch_pressed_ = true;
  }

 private:
  bool mouse_pressed_ = false;
  bool touch_pressed_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestPointerWatcher);
};

}  // namespace

class WindowManagerConnectionTest : public testing::Test {
 public:
  WindowManagerConnectionTest() {}
  ~WindowManagerConnectionTest() override {}

  void OnEventObserved(const ui::Event& event) {
    WindowManagerConnection::Get()->OnEventObserved(event, nullptr);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowManagerConnectionTest);
};

TEST_F(WindowManagerConnectionTest, PointerWatcher) {
  base::MessageLoop message_loop(base::MessageLoop::TYPE_UI);
  ScopedViewsTestHelper helper;
  WindowManagerConnection* connection = WindowManagerConnection::Get();
  ASSERT_TRUE(connection);
  ui::MouseEvent mouse_pressed(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                               base::TimeTicks(), ui::EF_NONE, 0);
  ui::TouchEvent touch_pressed(ui::ET_TOUCH_PRESSED, gfx::Point(), 1,
                               base::TimeTicks());
  ui::KeyEvent key_pressed(ui::ET_KEY_PRESSED, ui::VKEY_A, 0);

  // PointerWatchers receive mouse events.
  TestPointerWatcher watcher1;
  connection->AddPointerWatcher(&watcher1);
  OnEventObserved(mouse_pressed);
  EXPECT_TRUE(watcher1.mouse_pressed());
  watcher1.Reset();

  // PointerWatchers receive touch events.
  OnEventObserved(touch_pressed);
  EXPECT_TRUE(watcher1.touch_pressed());
  watcher1.Reset();

  // PointerWatchers do not trigger for key events.
  OnEventObserved(key_pressed);
  EXPECT_FALSE(watcher1.mouse_pressed());
  EXPECT_FALSE(watcher1.touch_pressed());
  watcher1.Reset();

  // Two PointerWatchers can both receive a single observed event.
  TestPointerWatcher watcher2;
  connection->AddPointerWatcher(&watcher2);
  OnEventObserved(mouse_pressed);
  EXPECT_TRUE(watcher1.mouse_pressed());
  EXPECT_TRUE(watcher2.mouse_pressed());
  watcher1.Reset();
  watcher2.Reset();

  // Removing the first PointerWatcher stops sending events to it.
  connection->RemovePointerWatcher(&watcher1);
  OnEventObserved(mouse_pressed);
  EXPECT_FALSE(watcher1.mouse_pressed());
  EXPECT_TRUE(watcher2.mouse_pressed());
  watcher1.Reset();
  watcher2.Reset();

  // Removing the last PointerWatcher stops sending events to it.
  connection->RemovePointerWatcher(&watcher2);
  OnEventObserved(mouse_pressed);
  EXPECT_FALSE(watcher1.mouse_pressed());
  EXPECT_FALSE(watcher1.touch_pressed());
}

}  // namespace views
