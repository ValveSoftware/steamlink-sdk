// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_KEYBOARD_LAYOUT_MANAGER_H_
#define UI_KEYBOARD_KEYBOARD_LAYOUT_MANAGER_H_

#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"

namespace keyboard {

class KeyboardController;

// LayoutManager for the virtual keyboard container. Manages a single window
// (the virtual keyboard) and keeps it positioned at the bottom of the
// owner window.
class KeyboardLayoutManager : public aura::LayoutManager {
 public:
  explicit KeyboardLayoutManager(KeyboardController* controller)
      : controller_(controller), keyboard_(NULL) {
  }

  // Overridden from aura::LayoutManager
  virtual void OnWindowResized() OVERRIDE;
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE;
  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE {}
  virtual void OnWindowRemovedFromLayout(aura::Window* child) OVERRIDE {}
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visible) OVERRIDE {}
  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE;

 private:
  KeyboardController* controller_;
  aura::Window* keyboard_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardLayoutManager);
};

}  // namespace keyboard

#endif  // UI_KEYBOARD_KEYBOARD_LAYOUT_MANAGER_H_
