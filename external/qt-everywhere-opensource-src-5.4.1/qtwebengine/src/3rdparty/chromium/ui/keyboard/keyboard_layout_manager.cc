// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/keyboard_layout_manager.h"

#include "ui/compositor/layer_animator.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_controller_proxy.h"
#include "ui/keyboard/keyboard_util.h"

namespace keyboard {

// Overridden from aura::LayoutManager
void KeyboardLayoutManager::OnWindowResized() {
  if (keyboard_) {
    gfx::Rect window_bounds = controller_->GetContainerWindow()->bounds();
    // Keep the same height when window resize. It usually get called when
    // screen rotate.
    int height = keyboard_->bounds().height();
    keyboard_->SetBounds(gfx::Rect(
        window_bounds.x(),
        window_bounds.bottom() - height,
        window_bounds.width(),
        height));
  }
}

void KeyboardLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  DCHECK(!keyboard_);
  keyboard_ = child;
  keyboard_->SetBounds(DefaultKeyboardBoundsFromWindowBounds(
      controller_->GetContainerWindow()->bounds()));
}

void KeyboardLayoutManager::SetChildBounds(aura::Window* child,
                                           const gfx::Rect& requested_bounds) {
  // SetChildBounds can be invoked by resizing from the container or by
  // resizing from the contents (through window.resizeTo call in JS).
  // The flag resizing_from_contents() is used to determine the source of the
  // resize.
  DCHECK(child == keyboard_);

  ui::LayerAnimator* animator =
      controller_->GetContainerWindow()->layer()->GetAnimator();
  // Stops previous animation if a window resize is requested during animation.
  if (animator->is_animating())
    animator->StopAnimating();

  gfx::Rect old_bounds = child->bounds();
  SetChildBoundsDirect(child, requested_bounds);
  if (old_bounds.height() == 0 && child->bounds().height() != 0) {
    // The window height is set to 0 initially. If the height of |old_bounds| is
    // 0 and the new bounds is not 0, it probably means window.resizeTo is
    // called to set the window height. We should try to show keyboard again in
    // case the show keyboard request is called before the height is set.
    controller_->ShowKeyboard(false);
  } else {
    controller_->NotifyKeyboardBoundsChanging(requested_bounds);
  }
}

}  // namespace keyboard
