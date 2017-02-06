// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_PUBLIC_WINDOW_MOVE_CLIENT_H_
#define UI_WM_PUBLIC_WINDOW_MOVE_CLIENT_H_

#include "ui/aura/aura_export.h"
#include "ui/gfx/geometry/vector2d.h"

namespace gfx {
class Point;
}

namespace aura {
class Window;
namespace client {

enum WindowMoveResult {
  MOVE_SUCCESSFUL,  // Moving window was successful.
  MOVE_CANCELED    // Moving window was canceled.
};

enum WindowMoveSource {
  WINDOW_MOVE_SOURCE_MOUSE,
  WINDOW_MOVE_SOURCE_TOUCH,
};

// An interface implemented by an object that manages programatically keyed
// window moving.
class AURA_EXPORT WindowMoveClient {
 public:
  // Starts a nested message loop for moving the window. |drag_offset| is the
  // offset from the window origin to the cursor when the drag was started.
  // Returns MOVE_SUCCESSFUL if the move has completed successfully, or
  // MOVE_CANCELED otherwise.
  virtual WindowMoveResult RunMoveLoop(Window* window,
                                       const gfx::Vector2d& drag_offset,
                                       WindowMoveSource source) = 0;

  // Ends a previously started move loop.
  virtual void EndMoveLoop() = 0;

 protected:
  virtual ~WindowMoveClient() {}
};

// Sets/Gets the activation client for the specified window.
AURA_EXPORT void SetWindowMoveClient(Window* window,
                                     WindowMoveClient* client);
AURA_EXPORT WindowMoveClient* GetWindowMoveClient(Window* window);

}  // namespace client
}  // namespace aura

#endif  // UI_WM_PUBLIC_WINDOW_MOVE_CLIENT_H_
