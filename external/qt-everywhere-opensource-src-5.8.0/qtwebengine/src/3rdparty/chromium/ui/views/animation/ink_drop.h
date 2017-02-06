// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_H_
#define UI_VIEWS_ANIMATION_INK_DROP_H_

#include <memory>

#include "base/macros.h"
#include "base/time/time.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/views_export.h"

namespace ui {
class Layer;
}
namespace views {

// Pure virtual base class that manages the lifetime and state of an ink drop
// ripple as well as visual hover state feedback.
class VIEWS_EXPORT InkDrop {
 public:
  virtual ~InkDrop() {}

  // Gets the target state of the ink drop.
  virtual InkDropState GetTargetInkDropState() const = 0;

  // Animates from the current InkDropState to |ink_drop_state|.
  virtual void AnimateToState(InkDropState ink_drop_state) = 0;

  // Immediately snaps the InkDropState to ACTIVATED. This more specific
  // implementation of the non-existent SnapToState(InkDropState) function is
  // the only one available because it was the only InkDropState that clients
  // needed to skip animations for.
  virtual void SnapToActivated() = 0;

  // Enables or disables the hover state.
  virtual void SetHovered(bool is_hovered) = 0;

  // Enables or disables the focus state.
  virtual void SetFocused(bool is_focused) = 0;

 protected:
  InkDrop() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(InkDrop);
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_H_
