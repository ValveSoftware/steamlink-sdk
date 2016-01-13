// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/cursor_manager.h"

#include "ui/aura/client/cursor_client_observer.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/wm/core/native_cursor_manager.h"

namespace {

class TestingCursorManager : public wm::NativeCursorManager {
 public:
  // Overridden from wm::NativeCursorManager:
  virtual void SetDisplay(
      const gfx::Display& display,
      wm::NativeCursorManagerDelegate* delegate) OVERRIDE {}

  virtual void SetCursor(
      gfx::NativeCursor cursor,
      wm::NativeCursorManagerDelegate* delegate) OVERRIDE {
    delegate->CommitCursor(cursor);
  }

  virtual void SetVisibility(
      bool visible,
      wm::NativeCursorManagerDelegate* delegate) OVERRIDE {
    delegate->CommitVisibility(visible);
  }

  virtual void SetMouseEventsEnabled(
      bool enabled,
      wm::NativeCursorManagerDelegate* delegate) OVERRIDE {
    delegate->CommitMouseEventsEnabled(enabled);
  }

  virtual void SetCursorSet(
      ui::CursorSetType cursor_set,
      wm::NativeCursorManagerDelegate* delegate) OVERRIDE {
    delegate->CommitCursorSet(cursor_set);
  }
};

}  // namespace

class CursorManagerTest : public aura::test::AuraTestBase {
 protected:
  CursorManagerTest()
      : delegate_(new TestingCursorManager),
        cursor_manager_(scoped_ptr<wm::NativeCursorManager>(
            delegate_)) {
  }

  TestingCursorManager* delegate_;
  wm::CursorManager cursor_manager_;
};

class TestingCursorClientObserver : public aura::client::CursorClientObserver {
 public:
  TestingCursorClientObserver()
      : cursor_visibility_(false),
        did_visibility_change_(false) {}
  void reset() { cursor_visibility_ = did_visibility_change_ = false; }
  bool is_cursor_visible() const { return cursor_visibility_; }
  bool did_visibility_change() const { return did_visibility_change_; }

  // Overridden from aura::client::CursorClientObserver:
  virtual void OnCursorVisibilityChanged(bool is_visible) OVERRIDE {
    cursor_visibility_ = is_visible;
    did_visibility_change_ = true;
  }

 private:
  bool cursor_visibility_;
  bool did_visibility_change_;

  DISALLOW_COPY_AND_ASSIGN(TestingCursorClientObserver);
};

TEST_F(CursorManagerTest, ShowHideCursor) {
  cursor_manager_.SetCursor(ui::kCursorCopy);
  EXPECT_EQ(ui::kCursorCopy, cursor_manager_.GetCursor().native_type());

  cursor_manager_.ShowCursor();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
  cursor_manager_.HideCursor();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  // The current cursor does not change even when the cursor is not shown.
  EXPECT_EQ(ui::kCursorCopy, cursor_manager_.GetCursor().native_type());

  // Check if cursor visibility is locked.
  cursor_manager_.LockCursor();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  cursor_manager_.ShowCursor();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  cursor_manager_.UnlockCursor();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());

  cursor_manager_.LockCursor();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
  cursor_manager_.HideCursor();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
  cursor_manager_.UnlockCursor();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());

  // Checks setting visiblity while cursor is locked does not affect the
  // subsequent uses of UnlockCursor.
  cursor_manager_.LockCursor();
  cursor_manager_.HideCursor();
  cursor_manager_.UnlockCursor();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());

  cursor_manager_.ShowCursor();
  cursor_manager_.LockCursor();
  cursor_manager_.UnlockCursor();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());

  cursor_manager_.LockCursor();
  cursor_manager_.ShowCursor();
  cursor_manager_.UnlockCursor();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());

  cursor_manager_.HideCursor();
  cursor_manager_.LockCursor();
  cursor_manager_.UnlockCursor();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
}

// Verifies that LockCursor/UnlockCursor work correctly with
// EnableMouseEvents and DisableMouseEvents
TEST_F(CursorManagerTest, EnableDisableMouseEvents) {
  cursor_manager_.SetCursor(ui::kCursorCopy);
  EXPECT_EQ(ui::kCursorCopy, cursor_manager_.GetCursor().native_type());

  cursor_manager_.EnableMouseEvents();
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.DisableMouseEvents();
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());
  // The current cursor does not change even when the cursor is not shown.
  EXPECT_EQ(ui::kCursorCopy, cursor_manager_.GetCursor().native_type());

  // Check if cursor enable state is locked.
  cursor_manager_.LockCursor();
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.EnableMouseEvents();
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.UnlockCursor();
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());

  cursor_manager_.LockCursor();
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.DisableMouseEvents();
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.UnlockCursor();
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());

  // Checks enabling cursor while cursor is locked does not affect the
  // subsequent uses of UnlockCursor.
  cursor_manager_.LockCursor();
  cursor_manager_.DisableMouseEvents();
  cursor_manager_.UnlockCursor();
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());

  cursor_manager_.EnableMouseEvents();
  cursor_manager_.LockCursor();
  cursor_manager_.UnlockCursor();
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());

  cursor_manager_.LockCursor();
  cursor_manager_.EnableMouseEvents();
  cursor_manager_.UnlockCursor();
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());

  cursor_manager_.DisableMouseEvents();
  cursor_manager_.LockCursor();
  cursor_manager_.UnlockCursor();
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());
}

TEST_F(CursorManagerTest, SetCursorSet) {
  EXPECT_EQ(ui::CURSOR_SET_NORMAL, cursor_manager_.GetCursorSet());

  cursor_manager_.SetCursorSet(ui::CURSOR_SET_NORMAL);
  EXPECT_EQ(ui::CURSOR_SET_NORMAL, cursor_manager_.GetCursorSet());

  cursor_manager_.SetCursorSet(ui::CURSOR_SET_LARGE);
  EXPECT_EQ(ui::CURSOR_SET_LARGE, cursor_manager_.GetCursorSet());

  cursor_manager_.SetCursorSet(ui::CURSOR_SET_NORMAL);
  EXPECT_EQ(ui::CURSOR_SET_NORMAL, cursor_manager_.GetCursorSet());
}

TEST_F(CursorManagerTest, IsMouseEventsEnabled) {
  cursor_manager_.EnableMouseEvents();
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.DisableMouseEvents();
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());
}

// Verifies that the mouse events enable state changes correctly when
// ShowCursor/HideCursor and EnableMouseEvents/DisableMouseEvents are used
// together.
TEST_F(CursorManagerTest, ShowAndEnable) {
  // Changing the visibility of the cursor does not affect the enable state.
  cursor_manager_.EnableMouseEvents();
  cursor_manager_.ShowCursor();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.HideCursor();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.ShowCursor();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());

  // When mouse events are disabled, it also gets invisible.
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
  cursor_manager_.DisableMouseEvents();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());

  // When mouse events are enabled, it restores the visibility state.
  cursor_manager_.EnableMouseEvents();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());

  cursor_manager_.ShowCursor();
  cursor_manager_.DisableMouseEvents();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.EnableMouseEvents();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());

  cursor_manager_.HideCursor();
  cursor_manager_.DisableMouseEvents();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.EnableMouseEvents();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());

  // When mouse events are disabled, ShowCursor is ignored.
  cursor_manager_.DisableMouseEvents();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.ShowCursor();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.DisableMouseEvents();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());
}

// Verifies that calling DisableMouseEvents multiple times in a row makes no
// difference compared with calling it once.
// This is a regression test for http://crbug.com/169404.
TEST_F(CursorManagerTest, MultipleDisableMouseEvents) {
  cursor_manager_.DisableMouseEvents();
  cursor_manager_.DisableMouseEvents();
  cursor_manager_.EnableMouseEvents();
  cursor_manager_.LockCursor();
  cursor_manager_.UnlockCursor();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
}

// Verifies that calling EnableMouseEvents multiple times in a row makes no
// difference compared with calling it once.
TEST_F(CursorManagerTest, MultipleEnableMouseEvents) {
  cursor_manager_.DisableMouseEvents();
  cursor_manager_.EnableMouseEvents();
  cursor_manager_.EnableMouseEvents();
  cursor_manager_.LockCursor();
  cursor_manager_.UnlockCursor();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
}

TEST_F(CursorManagerTest, TestCursorClientObserver) {
  // Add two observers. Both should have OnCursorVisibilityChanged()
  // invoked when the visibility of the cursor changes.
  TestingCursorClientObserver observer_a;
  TestingCursorClientObserver observer_b;
  cursor_manager_.AddObserver(&observer_a);
  cursor_manager_.AddObserver(&observer_b);

  // Initial state before any events have been sent.
  observer_a.reset();
  observer_b.reset();
  EXPECT_FALSE(observer_a.did_visibility_change());
  EXPECT_FALSE(observer_b.did_visibility_change());
  EXPECT_FALSE(observer_a.is_cursor_visible());
  EXPECT_FALSE(observer_b.is_cursor_visible());

  // Hide the cursor using HideCursor().
  cursor_manager_.HideCursor();
  EXPECT_TRUE(observer_a.did_visibility_change());
  EXPECT_TRUE(observer_b.did_visibility_change());
  EXPECT_FALSE(observer_a.is_cursor_visible());
  EXPECT_FALSE(observer_b.is_cursor_visible());

  // Show the cursor using ShowCursor().
  observer_a.reset();
  observer_b.reset();
  cursor_manager_.ShowCursor();
  EXPECT_TRUE(observer_a.did_visibility_change());
  EXPECT_TRUE(observer_b.did_visibility_change());
  EXPECT_TRUE(observer_a.is_cursor_visible());
  EXPECT_TRUE(observer_b.is_cursor_visible());

  // Remove observer_b. Its OnCursorVisibilityChanged() should
  // not be invoked past this point.
  cursor_manager_.RemoveObserver(&observer_b);

  // Hide the cursor using HideCursor().
  observer_a.reset();
  observer_b.reset();
  cursor_manager_.HideCursor();
  EXPECT_TRUE(observer_a.did_visibility_change());
  EXPECT_FALSE(observer_b.did_visibility_change());
  EXPECT_FALSE(observer_a.is_cursor_visible());

  // Show the cursor using ShowCursor().
  observer_a.reset();
  observer_b.reset();
  cursor_manager_.ShowCursor();
  EXPECT_TRUE(observer_a.did_visibility_change());
  EXPECT_FALSE(observer_b.did_visibility_change());
  EXPECT_TRUE(observer_a.is_cursor_visible());
}
