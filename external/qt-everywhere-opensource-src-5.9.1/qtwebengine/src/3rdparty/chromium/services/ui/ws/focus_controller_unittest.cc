// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/focus_controller.h"

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "services/ui/ws/focus_controller_delegate.h"
#include "services/ui/ws/focus_controller_observer.h"
#include "services/ui/ws/server_window.h"
#include "services/ui/ws/test_server_window_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

namespace ws {
namespace {

const char kDisallowActiveChildren[] = "disallow-active-children";

class TestFocusControllerObserver : public FocusControllerObserver,
                                    public FocusControllerDelegate {
 public:
  TestFocusControllerObserver()
      : ignore_explicit_(true),
        focus_change_count_(0u),
        old_focused_window_(nullptr),
        new_focused_window_(nullptr),
        old_active_window_(nullptr),
        new_active_window_(nullptr) {}

  void ClearAll() {
    focus_change_count_ = 0u;
    old_focused_window_ = nullptr;
    new_focused_window_ = nullptr;
    old_active_window_ = nullptr;
    new_active_window_ = nullptr;
  }
  size_t focus_change_count() const { return focus_change_count_; }
  ServerWindow* old_focused_window() { return old_focused_window_; }
  ServerWindow* new_focused_window() { return new_focused_window_; }

  ServerWindow* old_active_window() { return old_active_window_; }
  ServerWindow* new_active_window() { return new_active_window_; }

  void set_ignore_explicit(bool ignore) { ignore_explicit_ = ignore; }

 private:
  // FocusControllerDelegate:
  bool CanHaveActiveChildren(ServerWindow* window) const override {
    return !window || window->properties().count(kDisallowActiveChildren) == 0;
  }
  // FocusControllerObserver:
  void OnActivationChanged(ServerWindow* old_active_window,
                           ServerWindow* new_active_window) override {
    old_active_window_ = old_active_window;
    new_active_window_ = new_active_window;
  }
  void OnFocusChanged(FocusControllerChangeSource source,
                      ServerWindow* old_focused_window,
                      ServerWindow* new_focused_window) override {
    if (ignore_explicit_ && source == FocusControllerChangeSource::EXPLICIT)
      return;

    focus_change_count_++;
    old_focused_window_ = old_focused_window;
    new_focused_window_ = new_focused_window;
  }

  bool ignore_explicit_;
  size_t focus_change_count_;
  ServerWindow* old_focused_window_;
  ServerWindow* new_focused_window_;
  ServerWindow* old_active_window_;
  ServerWindow* new_active_window_;

  DISALLOW_COPY_AND_ASSIGN(TestFocusControllerObserver);
};

}  // namespace

TEST(FocusControllerTest, Basic) {
  TestServerWindowDelegate server_window_delegate;
  ServerWindow root(&server_window_delegate, WindowId());
  server_window_delegate.set_root_window(&root);
  root.SetVisible(true);
  ServerWindow child(&server_window_delegate, WindowId());
  child.SetVisible(true);
  root.Add(&child);
  ServerWindow child_child(&server_window_delegate, WindowId());
  child_child.SetVisible(true);
  child.Add(&child_child);

  TestFocusControllerObserver focus_observer;
  FocusController focus_controller(&focus_observer, &root);
  focus_controller.AddObserver(&focus_observer);

  focus_controller.SetFocusedWindow(&child_child);
  EXPECT_EQ(0u, focus_observer.focus_change_count());

  // Remove the ancestor of the focused window, focus should go to the |root|.
  root.Remove(&child);
  EXPECT_EQ(1u, focus_observer.focus_change_count());
  EXPECT_EQ(&root, focus_observer.new_focused_window());
  EXPECT_EQ(&child_child, focus_observer.old_focused_window());
  focus_observer.ClearAll();

  // Make the focused window invisible. Focus is lost in this case (as no one
  // to give focus to).
  root.SetVisible(false);
  EXPECT_EQ(1u, focus_observer.focus_change_count());
  EXPECT_EQ(nullptr, focus_observer.new_focused_window());
  EXPECT_EQ(&root, focus_observer.old_focused_window());
  focus_observer.ClearAll();

  // Go back to initial state and focus |child_child|.
  root.SetVisible(true);
  root.Add(&child);
  focus_controller.SetFocusedWindow(&child_child);
  EXPECT_EQ(0u, focus_observer.focus_change_count());

  // Hide the focused window, focus should go to parent.
  child_child.SetVisible(false);
  EXPECT_EQ(1u, focus_observer.focus_change_count());
  EXPECT_EQ(&child, focus_observer.new_focused_window());
  EXPECT_EQ(&child_child, focus_observer.old_focused_window());
  focus_observer.ClearAll();

  child_child.SetVisible(true);
  focus_controller.SetFocusedWindow(&child_child);
  EXPECT_EQ(0u, focus_observer.focus_change_count());

  // Hide the parent of the focused window.
  child.SetVisible(false);
  EXPECT_EQ(1u, focus_observer.focus_change_count());
  EXPECT_EQ(&root, focus_observer.new_focused_window());
  EXPECT_EQ(&child_child, focus_observer.old_focused_window());
  focus_observer.ClearAll();
  focus_controller.RemoveObserver(&focus_observer);
}

// Tests that focus shifts correctly if the focused window is destroyed.
TEST(FocusControllerTest, FocusShiftsOnDestroy) {
  TestServerWindowDelegate server_window_delegate;
  ServerWindow parent(&server_window_delegate, WindowId());
  server_window_delegate.set_root_window(&parent);
  parent.SetVisible(true);
  ServerWindow child_first(&server_window_delegate, WindowId());
  child_first.SetVisible(true);
  parent.Add(&child_first);
  std::unique_ptr<ServerWindow> child_second(
      new ServerWindow(&server_window_delegate, WindowId()));
  child_second->SetVisible(true);
  parent.Add(child_second.get());
  std::vector<uint8_t> dummy;
  // Allow only |parent| to be activated.
  parent.SetProperty(kDisallowActiveChildren, &dummy);

  TestFocusControllerObserver focus_observer;
  focus_observer.set_ignore_explicit(false);
  FocusController focus_controller(&focus_observer, &parent);
  focus_controller.AddObserver(&focus_observer);

  focus_controller.ActivateNextWindow();
  EXPECT_EQ(nullptr, focus_observer.old_active_window());
  EXPECT_EQ(&parent, focus_observer.new_active_window());
  EXPECT_EQ(nullptr, focus_observer.old_focused_window());
  EXPECT_EQ(child_second.get(), focus_observer.new_focused_window());
  focus_observer.ClearAll();

  // Destroying |child_second| should move focus to |child_first|.
  child_second.reset();
  EXPECT_NE(nullptr, focus_observer.old_focused_window());
  EXPECT_EQ(&child_first, focus_observer.new_focused_window());

  focus_controller.RemoveObserver(&focus_observer);
}

TEST(FocusControllerTest, ActivationSkipsOverHiddenWindow) {
  TestServerWindowDelegate server_window_delegate;
  ServerWindow parent(&server_window_delegate, WindowId());
  server_window_delegate.set_root_window(&parent);
  parent.SetVisible(true);

  ServerWindow child_first(&server_window_delegate, WindowId());
  ServerWindow child_second(&server_window_delegate, WindowId());
  ServerWindow child_third(&server_window_delegate, WindowId());

  parent.Add(&child_first);
  parent.Add(&child_second);
  parent.Add(&child_third);

  child_first.SetVisible(true);
  child_second.SetVisible(false);
  child_third.SetVisible(true);

  TestFocusControllerObserver focus_observer;
  focus_observer.set_ignore_explicit(false);
  FocusController focus_controller(&focus_observer, &parent);
  focus_controller.AddObserver(&focus_observer);

  // Since |child_second| is invisible, activation should cycle from
  // |child_third|, to |child_first|, to |parent|, back to |child_third|.
  focus_controller.ActivateNextWindow();
  EXPECT_EQ(nullptr, focus_observer.old_active_window());
  EXPECT_EQ(&child_third, focus_observer.new_active_window());
  focus_observer.ClearAll();

  focus_controller.ActivateNextWindow();
  EXPECT_EQ(&child_third, focus_observer.old_active_window());
  EXPECT_EQ(&child_first, focus_observer.new_active_window());
  focus_observer.ClearAll();

  focus_controller.ActivateNextWindow();
  EXPECT_EQ(&child_first, focus_observer.old_active_window());
  EXPECT_EQ(&parent, focus_observer.new_active_window());
  focus_observer.ClearAll();

  focus_controller.ActivateNextWindow();
  EXPECT_EQ(&parent, focus_observer.old_active_window());
  EXPECT_EQ(&child_third, focus_observer.new_active_window());
  focus_observer.ClearAll();

  // Once |child_second| is made visible, activation should go from
  // |child_third| to |child_second|.
  child_second.SetVisible(true);
  focus_controller.ActivateNextWindow();
  EXPECT_EQ(&child_third, focus_observer.old_active_window());
  EXPECT_EQ(&child_second, focus_observer.new_active_window());
}

TEST(FocusControllerTest, ActivationSkipsOverHiddenContainers) {
  TestServerWindowDelegate server_window_delegate;
  ServerWindow parent(&server_window_delegate, WindowId());
  server_window_delegate.set_root_window(&parent);
  parent.SetVisible(true);

  ServerWindow child1(&server_window_delegate, WindowId());
  ServerWindow child2(&server_window_delegate, WindowId());

  parent.Add(&child1);
  parent.Add(&child2);

  child1.SetVisible(true);
  child2.SetVisible(true);

  ServerWindow child11(&server_window_delegate, WindowId());
  ServerWindow child12(&server_window_delegate, WindowId());
  ServerWindow child21(&server_window_delegate, WindowId());
  ServerWindow child22(&server_window_delegate, WindowId());

  child1.Add(&child11);
  child1.Add(&child12);
  child2.Add(&child21);
  child2.Add(&child22);

  child11.SetVisible(true);
  child12.SetVisible(true);
  child21.SetVisible(true);
  child22.SetVisible(true);

  TestFocusControllerObserver focus_observer;
  FocusController focus_controller(&focus_observer, &parent);
  focus_controller.AddObserver(&focus_observer);

  focus_controller.ActivateNextWindow();
  EXPECT_EQ(nullptr, focus_observer.old_active_window());
  EXPECT_EQ(&child22, focus_observer.new_active_window());
  focus_observer.ClearAll();

  child2.SetVisible(false);
  EXPECT_EQ(&child22, focus_observer.old_active_window());
  EXPECT_EQ(&child12, focus_observer.new_active_window());
}

TEST(FocusControllerTest, NonFocusableWindowNotActivated) {
  TestServerWindowDelegate server_window_delegate;
  ServerWindow parent(&server_window_delegate, WindowId());
  server_window_delegate.set_root_window(&parent);
  parent.SetVisible(true);

  ServerWindow child_first(&server_window_delegate, WindowId());
  ServerWindow child_second(&server_window_delegate, WindowId());
  parent.Add(&child_first);
  parent.Add(&child_second);
  child_first.SetVisible(true);
  child_second.SetVisible(true);

  TestFocusControllerObserver focus_observer;
  focus_observer.set_ignore_explicit(false);
  FocusController focus_controller(&focus_observer, &parent);
  focus_controller.AddObserver(&focus_observer);

  child_first.set_can_focus(false);

  // |child_second| is activated first, but then activation skips over
  // |child_first| and goes to |parent| instead.
  focus_controller.ActivateNextWindow();
  EXPECT_EQ(nullptr, focus_observer.old_active_window());
  EXPECT_EQ(&child_second, focus_observer.new_active_window());
  focus_observer.ClearAll();

  focus_controller.ActivateNextWindow();
  EXPECT_EQ(&child_second, focus_observer.old_active_window());
  EXPECT_EQ(&parent, focus_observer.new_active_window());
}

}  // namespace ws
}  // namespace ui
