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
#include "components/mus/ws/server_window_surface_manager_test_api.h"
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

const UserId kTestId1 = "20";

class CursorTest : public testing::Test {
 public:
  CursorTest() : cursor_id_(-1), platform_display_factory_(&cursor_id_) {}
  ~CursorTest() override {}

 protected:
  // testing::Test:
  void SetUp() override {
    PlatformDisplay::set_factory_for_testing(&platform_display_factory_);
    window_server_.reset(
        new WindowServer(&window_server_delegate_,
                         scoped_refptr<SurfacesState>(new SurfacesState)));
    window_server_delegate_.set_window_server(window_server_.get());

    window_server_delegate_.set_num_displays_to_create(1);

    // As a side effect, this allocates Displays.
    WindowManagerWindowTreeFactorySetTestApi(
        window_server_->window_manager_window_tree_factory_set())
        .Add(kTestId1);
    window_server_->user_id_tracker()->AddUserId(kTestId1);
    window_server_->user_id_tracker()->SetActiveUserId(kTestId1);
  }

  ServerWindow* GetRoot() {
    DisplayManager* display_manager = window_server_->display_manager();
    //    ASSERT_EQ(1u, display_manager->displays().size());
    Display* display = *display_manager->displays().begin();
    return display->GetWindowManagerDisplayRootForUser(kTestId1)->root();
  }

  // Create a 30x30 window where the outer 10 pixels is non-client.
  ServerWindow* BuildServerWindow() {
    DisplayManager* display_manager = window_server_->display_manager();
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

    ServerWindowSurfaceManagerTestApi test_api(w->GetOrCreateSurfaceManager());
    test_api.CreateEmptyDefaultSurface();

    return w;
  }

  void MoveCursorTo(const gfx::Point& p) {
    DisplayManager* display_manager = window_server_->display_manager();
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

 protected:
  int32_t cursor_id_;
  TestPlatformDisplayFactory platform_display_factory_;
  TestWindowServerDelegate window_server_delegate_;
  std::unique_ptr<WindowServer> window_server_;
  base::MessageLoop message_loop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CursorTest);
};

TEST_F(CursorTest, ChangeByMouseMove) {
  ServerWindow* win = BuildServerWindow();
  win->SetPredefinedCursor(mojom::Cursor::IBEAM);
  EXPECT_EQ(mojom::Cursor::IBEAM, mojom::Cursor(win->cursor()));
  win->SetNonClientCursor(mojom::Cursor::EAST_RESIZE);
  EXPECT_EQ(mojom::Cursor::EAST_RESIZE,
            mojom::Cursor(win->non_client_cursor()));

  // Non client area
  MoveCursorTo(gfx::Point(15, 15));
  EXPECT_EQ(mojom::Cursor::EAST_RESIZE, mojom::Cursor(cursor_id_));

  // Client area
  MoveCursorTo(gfx::Point(25, 25));
  EXPECT_EQ(mojom::Cursor::IBEAM, mojom::Cursor(cursor_id_));
}

TEST_F(CursorTest, ChangeByClientAreaChange) {
  ServerWindow* win = BuildServerWindow();
  win->SetPredefinedCursor(mojom::Cursor::IBEAM);
  EXPECT_EQ(mojom::Cursor::IBEAM, mojom::Cursor(win->cursor()));
  win->SetNonClientCursor(mojom::Cursor::EAST_RESIZE);
  EXPECT_EQ(mojom::Cursor::EAST_RESIZE,
            mojom::Cursor(win->non_client_cursor()));

  // Non client area before we move.
  MoveCursorTo(gfx::Point(15, 15));
  EXPECT_EQ(mojom::Cursor::EAST_RESIZE, mojom::Cursor(cursor_id_));

  // Changing the client area should cause a change.
  win->SetClientArea(gfx::Insets(1, 1), std::vector<gfx::Rect>());
  EXPECT_EQ(mojom::Cursor::IBEAM, mojom::Cursor(cursor_id_));
}

TEST_F(CursorTest, NonClientCursorChange) {
  ServerWindow* win = BuildServerWindow();
  win->SetPredefinedCursor(mojom::Cursor::IBEAM);
  EXPECT_EQ(mojom::Cursor::IBEAM, mojom::Cursor(win->cursor()));
  win->SetNonClientCursor(mojom::Cursor::EAST_RESIZE);
  EXPECT_EQ(mojom::Cursor::EAST_RESIZE,
            mojom::Cursor(win->non_client_cursor()));

  MoveCursorTo(gfx::Point(15, 15));
  EXPECT_EQ(mojom::Cursor::EAST_RESIZE, mojom::Cursor(cursor_id_));

  win->SetNonClientCursor(mojom::Cursor::WEST_RESIZE);
  EXPECT_EQ(mojom::Cursor::WEST_RESIZE, mojom::Cursor(cursor_id_));
}

TEST_F(CursorTest, IgnoreClientCursorChangeInNonClientArea) {
  ServerWindow* win = BuildServerWindow();
  win->SetPredefinedCursor(mojom::Cursor::IBEAM);
  EXPECT_EQ(mojom::Cursor::IBEAM, mojom::Cursor(win->cursor()));
  win->SetNonClientCursor(mojom::Cursor::EAST_RESIZE);
  EXPECT_EQ(mojom::Cursor::EAST_RESIZE,
            mojom::Cursor(win->non_client_cursor()));

  MoveCursorTo(gfx::Point(15, 15));
  EXPECT_EQ(mojom::Cursor::EAST_RESIZE, mojom::Cursor(cursor_id_));

  win->SetPredefinedCursor(mojom::Cursor::HELP);
  EXPECT_EQ(mojom::Cursor::EAST_RESIZE, mojom::Cursor(cursor_id_));
}

TEST_F(CursorTest, NonClientToClientByBoundsChange) {
  ServerWindow* win = BuildServerWindow();
  win->SetPredefinedCursor(mojom::Cursor::IBEAM);
  EXPECT_EQ(mojom::Cursor::IBEAM, mojom::Cursor(win->cursor()));
  win->SetNonClientCursor(mojom::Cursor::EAST_RESIZE);
  EXPECT_EQ(mojom::Cursor::EAST_RESIZE,
            mojom::Cursor(win->non_client_cursor()));

  // Non client area before we move.
  MoveCursorTo(gfx::Point(15, 15));
  EXPECT_EQ(mojom::Cursor::EAST_RESIZE, mojom::Cursor(cursor_id_));

  win->SetBounds(gfx::Rect(0, 0, 30, 30));
  EXPECT_EQ(mojom::Cursor::IBEAM, mojom::Cursor(cursor_id_));
}

}  // namespace test
}  // namespace ws
}  // namespace mus
