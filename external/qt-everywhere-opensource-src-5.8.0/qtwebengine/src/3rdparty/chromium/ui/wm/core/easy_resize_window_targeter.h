// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_EASY_RESIZE_WINDOW_TARGETER_H_
#define UI_WM_CORE_EASY_RESIZE_WINDOW_TARGETER_H_

#include "base/macros.h"
#include "ui/aura/window_targeter.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/wm/wm_export.h"

namespace wm {

// An EventTargeter for a container window that uses a slightly larger
// hit-target region for easier resize.
class WM_EXPORT EasyResizeWindowTargeter : public aura::WindowTargeter {
 public:
  // |container| window is the owner of this targeter.
  EasyResizeWindowTargeter(aura::Window* container,
                           const gfx::Insets& mouse_extend,
                           const gfx::Insets& touch_extend);

  ~EasyResizeWindowTargeter() override;

 protected:
  void set_mouse_extend(const gfx::Insets& mouse_extend) {
    mouse_extend_ = mouse_extend;
  }

  void set_touch_extend(const gfx::Insets& touch_extend) {
    touch_extend_ = touch_extend;
  }

  // aura::WindowTargeter:
  bool EventLocationInsideBounds(aura::Window* window,
                                 const ui::LocatedEvent& event) const override;

 private:
  // Returns true if the hit testing (EventLocationInsideBounds()) should use
  // the extended bounds.
  bool ShouldUseExtendedBounds(const aura::Window* window) const;

  aura::Window* container_;
  gfx::Insets mouse_extend_;
  gfx::Insets touch_extend_;

  DISALLOW_COPY_AND_ASSIGN(EasyResizeWindowTargeter);
};

}  // namespace wm

#endif  // UI_WM_CORE_EASY_RESIZE_WINDOW_TARGETER_H_
