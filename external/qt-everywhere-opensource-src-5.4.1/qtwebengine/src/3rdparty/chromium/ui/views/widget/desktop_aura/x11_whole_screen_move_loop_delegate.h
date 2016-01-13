// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_X11_WHOLE_SCREEN_MOVE_LOOP_DELEGATE_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_X11_WHOLE_SCREEN_MOVE_LOOP_DELEGATE_H_

#include <X11/Xlib.h>
// Get rid of a macro from Xlib.h that conflicts with Aura's RootWindow class.
#undef RootWindow

namespace views {

// Receives mouse events while the X11WholeScreenMoveLoop is tracking a drag on
// the screen.
class X11WholeScreenMoveLoopDelegate {
 public:
  // Called when we receive a motion event.
  virtual void OnMouseMovement(XMotionEvent* event) = 0;

  // Called when the mouse button is released.
  virtual void OnMouseReleased() = 0;

  // Called when the user has released the mouse button. The move loop will
  // release the grab after this has been called.
  virtual void OnMoveLoopEnded() = 0;
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_X11_WHOLE_SCREEN_MOVE_LOOP_DELEGATE_H_
