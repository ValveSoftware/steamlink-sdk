// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_APP_UI_BLIMP_LAYOUT_MANAGER_H_
#define BLIMP_ENGINE_APP_UI_BLIMP_LAYOUT_MANAGER_H_

#include "base/macros.h"
#include "ui/aura/layout_manager.h"

namespace blimp {
namespace engine {

// A layout manager that sets the child window to occupy the full area of the
// root window.
class BlimpLayoutManager : public aura::LayoutManager {
 public:
  // Caller retains the ownership of |root_window|.
  explicit BlimpLayoutManager(aura::Window* root_window);
  ~BlimpLayoutManager() override;

 private:
  // aura::LayoutManager implementation.
  void OnWindowResized() override;
  void OnWindowAddedToLayout(aura::Window* child) override;
  void OnWillRemoveWindowFromLayout(aura::Window* child) override;
  void OnWindowRemovedFromLayout(aura::Window* child) override;
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override;
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override;

  aura::Window* root_window_;

  DISALLOW_COPY_AND_ASSIGN(BlimpLayoutManager);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_APP_UI_BLIMP_LAYOUT_MANAGER_H_
