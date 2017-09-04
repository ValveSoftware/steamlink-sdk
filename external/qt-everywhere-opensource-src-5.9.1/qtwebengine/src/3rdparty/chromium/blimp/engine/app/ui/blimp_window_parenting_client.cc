// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/app/ui/blimp_window_parenting_client.h"

#include "ui/aura/window.h"

namespace blimp {
namespace engine {

BlimpWindowParentingClient::BlimpWindowParentingClient(
    aura::Window* root_window)
    : root_window_(root_window) {
  aura::client::SetWindowParentingClient(root_window_, this);
}

BlimpWindowParentingClient::~BlimpWindowParentingClient() {
  aura::client::SetWindowParentingClient(root_window_, nullptr);
}

aura::Window* BlimpWindowParentingClient::GetDefaultParent(
    aura::Window* context,
    aura::Window* window,
    const gfx::Rect& bounds) {
  // Blimp has one root window. Always attach null-parented windows to the root
  // window.
  return root_window_;
}

}  // namespace engine
}  // namespace blimp
