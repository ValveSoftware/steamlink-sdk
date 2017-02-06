// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_WINDOW_MANAGER_DISPLAY_ROOT_H_
#define COMPONENTS_MUS_WS_WINDOW_MANAGER_DISPLAY_ROOT_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"

namespace mus {
namespace ws {

class Display;
class ServerWindow;
class WindowManagerState;
class WindowServer;

namespace test {
class WindowManagerDisplayRootTestApi;
}

// Owns the root window of a window manager for one display. Each window manager
// has one WindowManagerDisplayRoot for each Display. The root window is
// parented to the root of a Display.
class WindowManagerDisplayRoot {
 public:
  explicit WindowManagerDisplayRoot(Display* display);
  ~WindowManagerDisplayRoot();

  ServerWindow* root() { return root_.get(); }
  const ServerWindow* root() const { return root_.get(); }

  Display* display() { return display_; }
  const Display* display() const { return display_; }

  WindowManagerState* window_manager_state() { return window_manager_state_; }
  const WindowManagerState* window_manager_state() const {
    return window_manager_state_;
  }

 private:
  friend class Display;

  WindowServer* window_server();

  Display* display_;
  // Root ServerWindow of this WindowManagerDisplayRoot. |root_| has a parent,
  // the root ServerWindow of the Display.
  std::unique_ptr<ServerWindow> root_;
  WindowManagerState* window_manager_state_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerDisplayRoot);
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_WINDOW_MANAGER_DISPLAY_ROOT_H_
