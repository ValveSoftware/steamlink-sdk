// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "components/mus/common/types.h"
#include "components/mus/common/util.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "components/mus/surfaces/surfaces_state.h"
#include "components/mus/ws/display_manager.h"
#include "components/mus/ws/ids.h"
#include "components/mus/ws/platform_display.h"
#include "components/mus/ws/platform_display_factory.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/test_utils.h"
#include "components/mus/ws/window_manager_display_root.h"
#include "components/mus/ws/window_manager_state.h"
#include "components/mus/ws/window_server.h"
#include "components/mus/ws/window_server_delegate.h"
#include "components/mus/ws/window_tree.h"
#include "components/mus/ws/window_tree_binding.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/rect.h"

namespace mus {
namespace ws {
namespace test {
namespace {

const UserId kTestId1 = "2";
const UserId kTestId2 = "21";

ClientWindowId ClientWindowIdForFirstRoot(WindowTree* tree) {
  if (tree->roots().empty())
    return ClientWindowId();
  return ClientWindowIdForWindow(tree, *tree->roots().begin());
}

WindowManagerState* GetWindowManagerStateForUser(Display* display,
                                                 const UserId& user_id) {
  WindowManagerDisplayRoot* display_root =
      display->GetWindowManagerDisplayRootForUser(user_id);
  return display_root ? display_root->window_manager_state() : nullptr;
}

}  // namespace

// -----------------------------------------------------------------------------

class DisplayTest : public testing::Test {
 public:
  DisplayTest() : cursor_id_(0), platform_display_factory_(&cursor_id_) {}
  ~DisplayTest() override {}

 protected:
  // testing::Test:
  void SetUp() override {
    PlatformDisplay::set_factory_for_testing(&platform_display_factory_);
    window_server_.reset(new WindowServer(&window_server_delegate_,
                                          scoped_refptr<SurfacesState>()));
    window_server_delegate_.set_window_server(window_server_.get());
    window_server_->user_id_tracker()->AddUserId(kTestId1);
    window_server_->user_id_tracker()->AddUserId(kTestId2);
  }

 protected:
  int32_t cursor_id_;
  TestPlatformDisplayFactory platform_display_factory_;
  TestWindowServerDelegate window_server_delegate_;
  std::unique_ptr<WindowServer> window_server_;
  base::MessageLoop message_loop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DisplayTest);
};

TEST_F(DisplayTest, CallsCreateDefaultDisplays) {
  const int kNumHostsToCreate = 2;
  window_server_delegate_.set_num_displays_to_create(kNumHostsToCreate);

  DisplayManager* display_manager = window_server_->display_manager();
  WindowManagerWindowTreeFactorySetTestApi(
      window_server_->window_manager_window_tree_factory_set())
      .Add(kTestId1);
  // The first register should trigger creation of the default
  // Displays. There should be kNumHostsToCreate Displays.
  EXPECT_EQ(static_cast<size_t>(kNumHostsToCreate),
            display_manager->displays().size());

  // Each host should have a WindowManagerState for kTestId1.
  for (Display* display : display_manager->displays()) {
    EXPECT_EQ(1u, display->num_window_manger_states());
    EXPECT_TRUE(GetWindowManagerStateForUser(display, kTestId1));
    EXPECT_FALSE(GetWindowManagerStateForUser(display, kTestId2));
  }

  // Add another registry, should trigger creation of another wm.
  WindowManagerWindowTreeFactorySetTestApi(
      window_server_->window_manager_window_tree_factory_set())
      .Add(kTestId2);
  for (Display* display : display_manager->displays()) {
    ASSERT_EQ(2u, display->num_window_manger_states());
    WindowManagerDisplayRoot* root1 =
        display->GetWindowManagerDisplayRootForUser(kTestId1);
    ASSERT_TRUE(root1);
    WindowManagerDisplayRoot* root2 =
        display->GetWindowManagerDisplayRootForUser(kTestId2);
    ASSERT_TRUE(root2);
    // Verify the two states have different roots.
    EXPECT_NE(root1, root2);
    EXPECT_NE(root1->root(), root2->root());
  }
}

TEST_F(DisplayTest, Destruction) {
  window_server_delegate_.set_num_displays_to_create(1);

  WindowManagerWindowTreeFactorySetTestApi(
      window_server_->window_manager_window_tree_factory_set())
      .Add(kTestId1);

  // Add another registry, should trigger creation of another wm.
  DisplayManager* display_manager = window_server_->display_manager();
  WindowManagerWindowTreeFactorySetTestApi(
      window_server_->window_manager_window_tree_factory_set())
      .Add(kTestId2);
  ASSERT_EQ(1u, display_manager->displays().size());
  Display* display = *display_manager->displays().begin();
  ASSERT_EQ(2u, display->num_window_manger_states());
  // There should be two trees, one for each windowmanager.
  EXPECT_EQ(2u, window_server_->num_trees());

  {
    WindowManagerState* state = GetWindowManagerStateForUser(display, kTestId1);
    // Destroy the tree associated with |state|. Should result in deleting
    // |state|.
    window_server_->DestroyTree(state->window_tree());
    ASSERT_EQ(1u, display->num_window_manger_states());
    EXPECT_FALSE(GetWindowManagerStateForUser(display, kTestId1));
    EXPECT_EQ(1u, display_manager->displays().size());
    EXPECT_EQ(1u, window_server_->num_trees());
  }

  EXPECT_FALSE(window_server_delegate_.got_on_no_more_displays());
  window_server_->display_manager()->DestroyDisplay(display);
  // There is still one tree left.
  EXPECT_EQ(1u, window_server_->num_trees());
  EXPECT_TRUE(window_server_delegate_.got_on_no_more_displays());
}

TEST_F(DisplayTest, EventStateResetOnUserSwitch) {
  window_server_delegate_.set_num_displays_to_create(1);

  WindowManagerWindowTreeFactorySetTestApi(
      window_server_->window_manager_window_tree_factory_set())
      .Add(kTestId1);
  WindowManagerWindowTreeFactorySetTestApi(
      window_server_->window_manager_window_tree_factory_set())
      .Add(kTestId2);

  window_server_->user_id_tracker()->SetActiveUserId(kTestId1);

  DisplayManager* display_manager = window_server_->display_manager();
  ASSERT_EQ(1u, display_manager->displays().size());
  Display* display = *display_manager->displays().begin();
  WindowManagerState* active_wms =
      display->GetActiveWindowManagerDisplayRoot()->window_manager_state();
  ASSERT_TRUE(active_wms);
  EXPECT_EQ(kTestId1, active_wms->user_id());

  static_cast<PlatformDisplayDelegate*>(display)->OnEvent(ui::PointerEvent(
      ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(20, 25),
                     gfx::Point(20, 25), base::TimeTicks(),
                     ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON)));

  EXPECT_TRUE(EventDispatcherTestApi(active_wms->event_dispatcher())
                  .AreAnyPointersDown());
  EXPECT_EQ(gfx::Point(20, 25),
            active_wms->event_dispatcher()->mouse_pointer_last_location());

  // Switch the user. Should trigger resetting state in old event dispatcher
  // and update state in new event dispatcher.
  window_server_->user_id_tracker()->SetActiveUserId(kTestId2);
  EXPECT_NE(
      active_wms,
      display->GetActiveWindowManagerDisplayRoot()->window_manager_state());
  EXPECT_FALSE(EventDispatcherTestApi(active_wms->event_dispatcher())
                   .AreAnyPointersDown());
  active_wms =
      display->GetActiveWindowManagerDisplayRoot()->window_manager_state();
  EXPECT_EQ(kTestId2, active_wms->user_id());
  EXPECT_EQ(gfx::Point(20, 25),
            active_wms->event_dispatcher()->mouse_pointer_last_location());
  EXPECT_FALSE(EventDispatcherTestApi(active_wms->event_dispatcher())
                   .AreAnyPointersDown());
}

// Verifies capture fails when wm is inactive and succeeds when wm is active.
TEST_F(DisplayTest, SetCaptureFromWindowManager) {
  window_server_delegate_.set_num_displays_to_create(1);
  WindowManagerWindowTreeFactorySetTestApi(
      window_server_->window_manager_window_tree_factory_set())
      .Add(kTestId1);
  WindowManagerWindowTreeFactorySetTestApi(
      window_server_->window_manager_window_tree_factory_set())
      .Add(kTestId2);
  window_server_->user_id_tracker()->SetActiveUserId(kTestId1);
  DisplayManager* display_manager = window_server_->display_manager();
  ASSERT_EQ(1u, display_manager->displays().size());
  Display* display = *display_manager->displays().begin();
  WindowManagerState* wms_for_id2 =
      GetWindowManagerStateForUser(display, kTestId2);
  ASSERT_TRUE(wms_for_id2);
  EXPECT_FALSE(wms_for_id2->IsActive());

  // Create a child of the root that we can set capture on.
  WindowTree* tree = wms_for_id2->window_tree();
  ClientWindowId child_window_id;
  ASSERT_TRUE(NewWindowInTree(tree, &child_window_id));

  WindowTreeTestApi(tree).EnableCapture();

  // SetCapture() should fail for user id2 as it is inactive.
  EXPECT_FALSE(tree->SetCapture(child_window_id));

  // Make the second user active and verify capture works.
  window_server_->user_id_tracker()->SetActiveUserId(kTestId2);
  EXPECT_TRUE(wms_for_id2->IsActive());
  EXPECT_TRUE(tree->SetCapture(child_window_id));
}

TEST_F(DisplayTest, FocusFailsForInactiveUser) {
  window_server_delegate_.set_num_displays_to_create(1);
  WindowManagerWindowTreeFactorySetTestApi(
      window_server_->window_manager_window_tree_factory_set())
      .Add(kTestId1);
  TestWindowTreeClient* window_tree_client1 =
      window_server_delegate_.last_client();
  ASSERT_TRUE(window_tree_client1);
  WindowManagerWindowTreeFactorySetTestApi(
      window_server_->window_manager_window_tree_factory_set())
      .Add(kTestId2);
  window_server_->user_id_tracker()->SetActiveUserId(kTestId1);
  DisplayManager* display_manager = window_server_->display_manager();
  ASSERT_EQ(1u, display_manager->displays().size());
  Display* display = *display_manager->displays().begin();
  WindowManagerState* wms_for_id2 =
      GetWindowManagerStateForUser(display, kTestId2);
  wms_for_id2->window_tree()->AddActivationParent(
      ClientWindowIdForFirstRoot(wms_for_id2->window_tree()));
  ASSERT_TRUE(wms_for_id2);
  EXPECT_FALSE(wms_for_id2->IsActive());
  ClientWindowId child2_id;
  NewWindowInTree(wms_for_id2->window_tree(), &child2_id);

  // Focus should fail for windows in inactive window managers.
  EXPECT_FALSE(wms_for_id2->window_tree()->SetFocus(child2_id));

  // Focus should succeed for the active window manager.
  WindowManagerState* wms_for_id1 =
      GetWindowManagerStateForUser(display, kTestId1);
  ASSERT_TRUE(wms_for_id1);
  wms_for_id1->window_tree()->AddActivationParent(
      ClientWindowIdForFirstRoot(wms_for_id1->window_tree()));
  ClientWindowId child1_id;
  NewWindowInTree(wms_for_id1->window_tree(), &child1_id);
  EXPECT_TRUE(wms_for_id1->IsActive());
  EXPECT_TRUE(wms_for_id1->window_tree()->SetFocus(child1_id));
}

// Verifies a single tree is used for multiple displays.
TEST_F(DisplayTest, MultipleDisplays) {
  window_server_delegate_.set_num_displays_to_create(2);
  WindowManagerWindowTreeFactorySetTestApi(
      window_server_->window_manager_window_tree_factory_set())
      .Add(kTestId1);
  window_server_->user_id_tracker()->SetActiveUserId(kTestId1);
  ASSERT_EQ(1u, window_server_delegate_.bindings()->size());
  TestWindowTreeBinding* window_tree_binding =
      (*window_server_delegate_.bindings())[0];
  WindowTree* tree = window_tree_binding->tree();
  ASSERT_EQ(2u, tree->roots().size());
  std::set<const ServerWindow*> roots = tree->roots();
  auto it = roots.begin();
  ServerWindow* root1 = tree->GetWindow((*it)->id());
  ++it;
  ServerWindow* root2 = tree->GetWindow((*it)->id());
  ASSERT_NE(root1, root2);
  Display* display1 = tree->GetDisplay(root1);
  WindowManagerState* display1_wms =
      display1->GetWindowManagerDisplayRootForUser(kTestId1)
          ->window_manager_state();
  Display* display2 = tree->GetDisplay(root2);
  WindowManagerState* display2_wms =
      display2->GetWindowManagerDisplayRootForUser(kTestId1)
          ->window_manager_state();
  EXPECT_EQ(display1_wms->window_tree(), display2_wms->window_tree());
}

}  // namespace test
}  // namespace ws
}  // namespace mus
