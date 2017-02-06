// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/x11_desktop_window_move_client.h"

#include <X11/Xlib.h>

#include "base/debug/stack_trace.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/event.h"

namespace views {

X11DesktopWindowMoveClient::X11DesktopWindowMoveClient()
    : move_loop_(this),
      host_(NULL) {
}

X11DesktopWindowMoveClient::~X11DesktopWindowMoveClient() {}

void X11DesktopWindowMoveClient::OnMouseMovement(const gfx::Point& screen_point,
                                                 int flags,
                                                 base::TimeTicks event_time) {
  gfx::Point system_loc = screen_point - window_offset_;
  host_->SetBounds(gfx::Rect(system_loc, host_->GetBounds().size()));
}

void X11DesktopWindowMoveClient::OnMouseReleased() {
  EndMoveLoop();
}

void X11DesktopWindowMoveClient::OnMoveLoopEnded() {
  host_ = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostLinux, aura::client::WindowMoveClient implementation:

aura::client::WindowMoveResult X11DesktopWindowMoveClient::RunMoveLoop(
    aura::Window* source,
    const gfx::Vector2d& drag_offset,
    aura::client::WindowMoveSource move_source) {
  window_offset_ = drag_offset;
  host_ = source->GetHost();

  source->SetCapture();
  bool success = move_loop_.RunMoveLoop(source, host_->last_cursor());
  return success ? aura::client::MOVE_SUCCESSFUL : aura::client::MOVE_CANCELED;
}

void X11DesktopWindowMoveClient::EndMoveLoop() {
  move_loop_.EndMoveLoop();
}

}  // namespace views
