// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_MESSAGE_LOOP_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_MESSAGE_LOOP_H_

#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Point;
}

namespace ui {
class LocatedEvent;
}

namespace views {

class MenuController;

// Interface used by MenuController to run a nested message loop while
// showing a menu, allowing for platform specific implementations.
class MenuMessageLoop {
 public:
  virtual ~MenuMessageLoop() {}

  // Create a platform specific instance.
  static MenuMessageLoop* Create();

  // Repost |event| to |window|.
  // |screen_loc| is the event's location in screen coordinates.
  static void RepostEventToWindow(const ui::LocatedEvent* event,
                                  gfx::NativeWindow window,
                                  const gfx::Point& screen_loc);

  // Runs the platform specific bits of the message loop.
  virtual void Run() = 0;

  // Quit an earlier call to Run().
  virtual void QuitNow() = 0;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_MESSAGE_LOOP_H_
