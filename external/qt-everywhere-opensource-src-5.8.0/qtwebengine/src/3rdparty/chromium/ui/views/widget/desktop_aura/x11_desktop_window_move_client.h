// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_X11_DESKTOP_WINDOW_MOVE_CLIENT_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_X11_DESKTOP_WINDOW_MOVE_CLIENT_H_

#include <X11/Xlib.h>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/desktop_aura/x11_move_loop_delegate.h"
#include "ui/views/widget/desktop_aura/x11_whole_screen_move_loop.h"
#include "ui/wm/public/window_move_client.h"

namespace aura {
class WindowTreeHost;
}

namespace views {

// When we're dragging tabs, we need to manually position our window.
class VIEWS_EXPORT X11DesktopWindowMoveClient :
      public views::X11MoveLoopDelegate,
      public aura::client::WindowMoveClient {
 public:
  X11DesktopWindowMoveClient();
  ~X11DesktopWindowMoveClient() override;

  // Overridden from X11WholeScreenMoveLoopDelegate:
  void OnMouseMovement(const gfx::Point& screen_point,
                       int flags,
                       base::TimeTicks event_time) override;
  void OnMouseReleased() override;
  void OnMoveLoopEnded() override;

  // Overridden from aura::client::WindowMoveClient:
  aura::client::WindowMoveResult RunMoveLoop(
      aura::Window* window,
      const gfx::Vector2d& drag_offset,
      aura::client::WindowMoveSource move_source) override;
  void EndMoveLoop() override;

 private:
  X11WholeScreenMoveLoop move_loop_;

  // We need to keep track of this so we can actually move it when reacting to
  // mouse events.
  aura::WindowTreeHost* host_;

  // Our cursor offset from the top left window origin when the drag
  // started. Used to calculate the window's new bounds relative to the current
  // location of the cursor.
  gfx::Vector2d window_offset_;
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_X11_DESKTOP_WINDOW_MOVE_CLIENT_H_
