// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/window_manager_display_root.h"

#include "components/mus/ws/display_manager.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/window_server.h"

namespace mus {
namespace ws {

WindowManagerDisplayRoot::WindowManagerDisplayRoot(Display* display)
    : display_(display) {
  root_.reset(window_server()->CreateServerWindow(
      window_server()->display_manager()->GetAndAdvanceNextRootId(),
      ServerWindow::Properties()));
  // Our root is always a child of the Display's root. Do this
  // before the WindowTree has been created so that the client doesn't get
  // notified of the add, bounds change and visibility change.
  root_->SetBounds(gfx::Rect(display->root_window()->bounds().size()));
  root_->SetVisible(true);
  display->root_window()->Add(root_.get());
}

WindowManagerDisplayRoot::~WindowManagerDisplayRoot() {}

WindowServer* WindowManagerDisplayRoot::window_server() {
  return display_->window_server();
}

}  // namespace ws
}  // namespace mus
