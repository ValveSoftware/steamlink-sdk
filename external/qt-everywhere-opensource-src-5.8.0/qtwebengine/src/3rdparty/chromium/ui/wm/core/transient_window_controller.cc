// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/transient_window_controller.h"

#include "ui/wm/core/transient_window_manager.h"

namespace wm {

TransientWindowController::TransientWindowController() {
}

TransientWindowController::~TransientWindowController() {
}

void TransientWindowController::AddTransientChild(aura::Window* parent,
                                                  aura::Window* child) {
  TransientWindowManager::Get(parent)->AddTransientChild(child);
}

void TransientWindowController::RemoveTransientChild(aura::Window* parent,
                                                     aura::Window* child) {
  TransientWindowManager::Get(parent)->RemoveTransientChild(child);
}

aura::Window* TransientWindowController::GetTransientParent(
    aura::Window* window) {
  return const_cast<aura::Window*>(GetTransientParent(
      const_cast<const aura::Window*>(window)));
}

const aura::Window* TransientWindowController::GetTransientParent(
    const aura::Window* window) {
  const TransientWindowManager* window_manager =
      TransientWindowManager::Get(window);
  return window_manager ? window_manager->transient_parent() : NULL;
}

}  // namespace wm
