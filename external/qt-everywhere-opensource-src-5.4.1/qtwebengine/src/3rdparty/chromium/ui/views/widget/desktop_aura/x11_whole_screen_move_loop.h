// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_X11_WHOLE_SCREEN_MOVE_LOOP_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_X11_WHOLE_SCREEN_MOVE_LOOP_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/vector2d_f.h"
#include "ui/views/widget/desktop_aura/x11_whole_screen_move_loop_delegate.h"

typedef struct _XDisplay XDisplay;

namespace aura {
class Window;
}

namespace ui {
class ScopedEventDispatcher;
}

namespace views {

class Widget;

// Runs a nested message loop and grabs the mouse. This is used to implement
// dragging.
class X11WholeScreenMoveLoop : public ui::PlatformEventDispatcher {
 public:
  explicit X11WholeScreenMoveLoop(X11WholeScreenMoveLoopDelegate* delegate);
  virtual ~X11WholeScreenMoveLoop();

  // ui:::PlatformEventDispatcher:
  virtual bool CanDispatchEvent(const ui::PlatformEvent& event) OVERRIDE;
  virtual uint32_t DispatchEvent(const ui::PlatformEvent& event) OVERRIDE;

  // Runs the nested message loop. While the mouse is grabbed, use |cursor| as
  // the mouse cursor. Returns true if the move-loop is completed successfully.
  // If the pointer-grab fails, or the move-loop is canceled by the user (e.g.
  // by pressing escape), then returns false.
  bool RunMoveLoop(aura::Window* window, gfx::NativeCursor cursor);

  // Updates the cursor while the move loop is running.
  void UpdateCursor(gfx::NativeCursor cursor);

  // Ends the RunMoveLoop() that's currently in progress.
  void EndMoveLoop();

  // Sets an image to be used during the drag.
  void SetDragImage(const gfx::ImageSkia& image, gfx::Vector2dF offset);

 private:
  // Grabs the pointer and keyboard, setting the mouse cursor to |cursor|.
  // Returns true if the grab was successful.
  bool GrabPointerAndKeyboard(gfx::NativeCursor cursor);

  // Creates an input-only window to be used during the drag.
  Window CreateDragInputWindow(XDisplay* display);

  // Creates a window to show the drag image during the drag.
  void CreateDragImageWindow();

  // Checks to see if |in_image| is an image that has any visible regions
  // (defined as having a pixel with alpha > 32). If so, return true.
  bool CheckIfIconValid();

  // Dispatch mouse movement event to |delegate_| in a posted task.
  void DispatchMouseMovement();

  X11WholeScreenMoveLoopDelegate* delegate_;

  // Are we running a nested message loop from RunMoveLoop()?
  bool in_move_loop_;
  scoped_ptr<ui::ScopedEventDispatcher> nested_dispatcher_;

  bool should_reset_mouse_flags_;

  // An invisible InputOnly window . We create this window so we can track the
  // cursor wherever it goes on screen during a drag, since normal windows
  // don't receive pointer motion events outside of their bounds.
  ::Window grab_input_window_;

  base::Closure quit_closure_;

  // Keeps track of whether the move-loop is cancled by the user (e.g. by
  // pressing escape).
  bool canceled_;

  // Keeps track of whether we still have a pointer grab at the end of the loop.
  bool has_grab_;

  // A Widget is created during the drag if there is an image available to be
  // used during the drag.
  scoped_ptr<Widget> drag_widget_;
  gfx::ImageSkia drag_image_;
  gfx::Vector2dF drag_offset_;
  XMotionEvent last_xmotion_;
  base::WeakPtrFactory<X11WholeScreenMoveLoop> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(X11WholeScreenMoveLoop);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_X11_WHOLE_SCREEN_MOVE_LOOP_H_
