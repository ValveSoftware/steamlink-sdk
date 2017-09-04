// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/app/ui/blimp_layout_manager.h"

#include "ui/aura/window.h"

namespace blimp {
namespace engine {

BlimpLayoutManager::BlimpLayoutManager(aura::Window* root_window)
    : root_window_(root_window) {}

BlimpLayoutManager::~BlimpLayoutManager() {}

void BlimpLayoutManager::OnWindowResized() {}

void BlimpLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  child->SetBounds(root_window_->bounds());
}

void BlimpLayoutManager::OnWillRemoveWindowFromLayout(aura::Window* child) {}

void BlimpLayoutManager::OnWindowRemovedFromLayout(aura::Window* child) {}

void BlimpLayoutManager::OnChildWindowVisibilityChanged(aura::Window* child,
                                                        bool visible) {}

void BlimpLayoutManager::SetChildBounds(aura::Window* child,
                                        const gfx::Rect& requested_bounds) {
  SetChildBoundsDirect(child, root_window_->bounds());
}

}  // namespace engine
}  // namespace blimp
