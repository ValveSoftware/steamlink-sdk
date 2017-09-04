// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_MASKED_WINDOW_TARGETER_H_
#define UI_WM_CORE_MASKED_WINDOW_TARGETER_H_

#include "base/macros.h"
#include "ui/aura/window_targeter.h"
#include "ui/wm/wm_export.h"

namespace gfx {
class Path;
}

namespace wm {

class WM_EXPORT MaskedWindowTargeter : public aura::WindowTargeter {
 public:
  explicit MaskedWindowTargeter(aura::Window* masked_window);
  ~MaskedWindowTargeter() override;

 protected:
  // Sets the hit-test mask for |window| in |mask| (in |window|'s local
  // coordinate system). Returns whether a valid mask has been set in |mask|.
  virtual bool GetHitTestMask(aura::Window* window, gfx::Path* mask) const = 0;

  // aura::WindowTargeter:
  bool EventLocationInsideBounds(aura::Window* target,
                                 const ui::LocatedEvent& event) const override;

 private:
  aura::Window* masked_window_;

  DISALLOW_COPY_AND_ASSIGN(MaskedWindowTargeter);
};

}  // namespace wm

#endif  // UI_WM_CORE_MASKED_WINDOW_TARGETER_H_
