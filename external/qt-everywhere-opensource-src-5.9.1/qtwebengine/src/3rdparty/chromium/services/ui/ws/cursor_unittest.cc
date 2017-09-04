// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "services/ui/common/types.h"
#include "services/ui/common/util.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"
#include "services/ui/surfaces/display_compositor.h"
#include "services/ui/ws/display_manager.h"
#include "services/ui/ws/ids.h"
#include "services/ui/ws/platform_display.h"
#include "services/ui/ws/platform_display_factory.h"
#include "services/ui/ws/server_window.h"
#include "services/ui/ws/server_window_compositor_frame_sink_manager_test_api.h"
#include "services/ui/ws/test_utils.h"
#include "services/ui/ws/window_manager_display_root.h"
#include "services/ui/ws/window_manager_state.h"
#include "services/ui/ws/window_server.h"
#include "services/ui/ws/window_server_delegate.h"
#include "services/ui/ws/window_tree.h"
#include "services/ui/ws/window_tree_binding.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
namespace ws {
namespace test {

const UserId kTestId1 = "20";

class CursorTest : public testing::Test {
 public:
  CursorTest() {}
  ~CursorTest() override {}

  WindowServer* window_server() { return ws_test_helper_.window_server(); }
  TestWindowServerDelegate* window_server_delegate() {
    return ws_test_helper_.window_server_delegate();
  }
  mojom::Cursor cursor() const { return ws_test_helper_.cursor(); }

 protected:
  // testing::Test:
  void SetUp() override {
    window_server_delegate()->CreateDisplays(1);

    // As a side effect, this allocates Displays.
    AddWindowManager(window_server(), kTestId1);
    window_server()->user_id_tracker()->AddUserId(kTestId1);
    window_server()->user_id_tracker()->SetActiveUserId(kTestId1);
  }

  ServerWindow* GetRoot() {
    DisplayManager* display_manager = window_server()->display_manager();
    //    ASSERT_EQ(1u, display_manager->displays().size());
    Display* display = *display_manager->displays().begin();
    return display->GetWindowManagerDisplayRootForUser(kTestId1)->root();
  }

  // Create a 30x30 window where the outer 10 pixels is non-client.
  ServerWindow* BuildServerWindow() {
    DisplayManager* display_manager = window_server()->display_manager();
    Display* display = *display_manager->displays().begin();
    WindowManagerDisplayRoot* active_display_root =
        display->GetActiveWindowManagerDisplayRoot();
    WindowTree* tree =
        active_display_root->window_manager_state()->window_tree();
    ClientWindowId child_window_id;
    if (!NewWindowInTree(tree, &child_window_id))
      return nullptr;

    ServerWindow* w = tree->GetWindowByClientId(child_window_id);
    w->SetBounds(gfx::Rect(10, 10, 30, 30));
    w->SetClientArea(gfx::Insets(10, 10), std::vector<gfx::Rect>());
    w->SetVisible(true);

    ServerWindowCompositorFrameSinkManagerTestApi test_api(
        w->GetOrCreateCompositorFrameSinkManager());
    test_api.CreateEmptyDefaultCompositorFrameSink();

    return w;
  }

  void MoveCursorTo(const gfx::Point& p) {
    DisplayManager* display_manager = window_server()->display_manager();
    ASSERT_EQ(1u, display_manager->displays().size());
    Display* display = *display_manager->displays().begin();
    WindowManagerDisplayRoot* active_display_root =
        display->GetActiveWindowManagerDisplayRoot();
    ASSERT_TRUE(active_display_root);
    static_cast<PlatformDisplayDelegate*>(display)->OnEvent(ui::PointerEvent(
        ui::MouseEvent(ui::ET_MOUSE_MOVED, p, p, base::TimeTicks(), 0, 0)));
    WindowManagerState* wms = active_display_root->window_manager_state();
    wms->OnEventAck(wms->window_tree(), mojom::EventResult::HANDLED);
  }

 private:
  WindowServerTestHelper ws_test_helper_;
  DISALLOW_COPY_AND_ASSIGN(CursorTest);
};

TEST_F(CursorTest, ChangeByMouseMove) {
  ServerWindow* win = BuildServerWindow();
  win->SetPredefinedCursor(mojom::Cursor::IBEAM);
  EXPECT_EQ(mojom::Cursor::IBEAM, win->cursor());
  win->SetNonClientCursor(mojom::Cursor::EAST_RESIZE);
  EXPECT_EQ(mojom::Cursor::EAST_RESIZE, win->non_client_cursor());

  // Non client area
  MoveCursorTo(gfx::Point(15, 15));
  EXPECT_EQ(mojom::Cursor::EAST_RESIZE, cursor());

  // Client area
  MoveCursorTo(gfx::Point(25, 25));
  EXPECT_EQ(mojom::Cursor::IBEAM, cursor());
}

TEST_F(CursorTest, ChangeByClientAreaChange) {
  ServerWindow* win = BuildServerWindow();
  win->SetPredefinedCursor(mojom::Cursor::IBEAM);
  EXPECT_EQ(mojom::Cursor::IBEAM, mojom::Cursor(win->cursor()));
  win->SetNonClientCursor(mojom::Cursor::EAST_RESIZE);
  EXPECT_EQ(mojom::Cursor::EAST_RESIZE, win->non_client_cursor());

  // Non client area before we move.
  MoveCursorTo(gfx::Point(15, 15));
  EXPECT_EQ(mojom::Cursor::EAST_RESIZE, cursor());

  // Changing the client area should cause a change.
  win->SetClientArea(gfx::Insets(1, 1), std::vector<gfx::Rect>());
  EXPECT_EQ(mojom::Cursor::IBEAM, cursor());
}

TEST_F(CursorTest, NonClientCursorChange) {
  ServerWindow* win = BuildServerWindow();
  win->SetPredefinedCursor(mojom::Cursor::IBEAM);
  EXPECT_EQ(mojom::Cursor::IBEAM, win->cursor());
  win->SetNonClientCursor(mojom::Cursor::EAST_RESIZE);
  EXPECT_EQ(mojom::Cursor::EAST_RESIZE, win->non_client_cursor());

  MoveCursorTo(gfx::Point(15, 15));
  EXPECT_EQ(mojom::Cursor::EAST_RESIZE, cursor());

  win->SetNonClientCursor(mojom::Cursor::WEST_RESIZE);
  EXPECT_EQ(mojom::Cursor::WEST_RESIZE, cursor());
}

TEST_F(CursorTest, IgnoreClientCursorChangeInNonClientArea) {
  ServerWindow* win = BuildServerWindow();
  win->SetPredefinedCursor(mojom::Cursor::IBEAM);
  EXPECT_EQ(mojom::Cursor::IBEAM, win->cursor());
  win->SetNonClientCursor(mojom::Cursor::EAST_RESIZE);
  EXPECT_EQ(mojom::Cursor::EAST_RESIZE, win->non_client_cursor());

  MoveCursorTo(gfx::Point(15, 15));
  EXPECT_EQ(mojom::Cursor::EAST_RESIZE, cursor());

  win->SetPredefinedCursor(mojom::Cursor::HELP);
  EXPECT_EQ(mojom::Cursor::EAST_RESIZE, cursor());
}

TEST_F(CursorTest, NonClientToClientByBoundsChange) {
  ServerWindow* win = BuildServerWindow();
  win->SetPredefinedCursor(mojom::Cursor::IBEAM);
  EXPECT_EQ(mojom::Cursor::IBEAM, win->cursor());
  win->SetNonClientCursor(mojom::Cursor::EAST_RESIZE);
  EXPECT_EQ(mojom::Cursor::EAST_RESIZE, win->non_client_cursor());

  // Non client area before we move.
  MoveCursorTo(gfx::Point(15, 15));
  EXPECT_EQ(mojom::Cursor::EAST_RESIZE, cursor());

  win->SetBounds(gfx::Rect(0, 0, 30, 30));
  EXPECT_EQ(mojom::Cursor::IBEAM, cursor());
}

}  // namespace test
}  // namespace ws
}  // namespace ui
