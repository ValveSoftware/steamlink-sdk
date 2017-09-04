// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/keyboard_layout_manager.h"

#include "ui/compositor/layer_animator.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_util.h"

namespace keyboard {

// Overridden from aura::LayoutManager
void KeyboardLayoutManager::OnWindowResized() {
  if (keyboard_) {
    // Container window is the top level window of the virtual keyboard window.
    // To support window.moveTo for the virtual keyboard window, as it actually
    // moves the top level window, the container window should be set to the
    // desired bounds before changing the bounds of the virtual keyboard window.
    gfx::Rect container_bounds = controller_->GetContainerWindow()->bounds();
    // Always align container window and keyboard window.
    if (controller_->keyboard_mode() == FULL_WIDTH) {
      SetChildBounds(keyboard_, gfx::Rect(container_bounds.size()));
    } else {
      SetChildBoundsDirect(keyboard_, gfx::Rect(container_bounds.size()));
    }
  }
}

void KeyboardLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  DCHECK(!keyboard_);
  keyboard_ = child;
  if (controller_->keyboard_mode() == FULL_WIDTH) {
    controller_->GetContainerWindow()->SetBounds(gfx::Rect());
  } else if (controller_->keyboard_mode() == FLOATING) {
    controller_->GetContainerWindow()->SetBounds(child->bounds());
    SetChildBoundsDirect(keyboard_, gfx::Rect(child->bounds().size()));
  }
}

void KeyboardLayoutManager::SetChildBounds(aura::Window* child,
                                           const gfx::Rect& requested_bounds) {
  DCHECK(child == keyboard_);

  // Request to change the bounds of child window (AKA the virtual keyboard
  // window) should change the container window first. Then the child window is
  // resized and covers the container window. Note the child's bound is only set
  // in OnWindowResized.
  gfx::Rect new_bounds = requested_bounds;
  if (controller_->keyboard_mode() == FULL_WIDTH) {
    const gfx::Rect& window_bounds =
        controller_->GetContainerWindow()->GetRootWindow()->bounds();
    new_bounds.set_y(window_bounds.height() - new_bounds.height());
    // If shelf is positioned on the left side of screen, x is not 0. In
    // FULL_WIDTH mode, the virtual keyboard should always align with the left
    // edge of the screen. So manually set x to 0 here.
    new_bounds.set_x(0);
    new_bounds.set_width(window_bounds.width());
  }
  // Keyboard bounds should only be reset when it actually changes. Otherwise
  // it interrupts the initial animation of showing the keyboard. Described in
  // crbug.com/356753.
  gfx::Rect old_bounds = keyboard_->GetTargetBounds();
  aura::Window::ConvertRectToTarget(keyboard_,
                                    keyboard_->GetRootWindow(),
                                    &old_bounds);
  if (new_bounds == old_bounds)
    return;

  ui::LayerAnimator* animator =
      controller_->GetContainerWindow()->layer()->GetAnimator();
  // Stops previous animation if a window resize is requested during animation.
  if (animator->is_animating())
    animator->StopAnimating();

  controller_->GetContainerWindow()->SetBounds(new_bounds);
  SetChildBoundsDirect(keyboard_, gfx::Rect(new_bounds.size()));

  if (old_bounds.height() == 0 && child->bounds().height() != 0 &&
      controller_->show_on_resize()) {
    // The window height is set to 0 initially or before switch to an IME in a
    // different extension. Virtual keyboard window may wait for this bounds
    // change to correctly animate in.
    controller_->ShowKeyboard(false);
  } else {
    if (controller_->keyboard_mode() == FULL_WIDTH) {
      // We need to send out this notification only if keyboard is visible since
      // keyboard window is resized even if keyboard is hidden.
      if (controller_->keyboard_visible())
        controller_->NotifyKeyboardBoundsChanging(requested_bounds);
    } else if (controller_->keyboard_mode() == FLOATING) {
      controller_->NotifyKeyboardBoundsChanging(gfx::Rect());
    }
  }
}

}  // namespace keyboard
