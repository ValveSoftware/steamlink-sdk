// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_window_tree_client.h"

#include "ui/aura/window.h"

namespace headless {

HeadlessWindowTreeClient::HeadlessWindowTreeClient(aura::Window* root_window)
    : root_window_(root_window) {
  aura::client::SetWindowTreeClient(root_window_, this);
}

HeadlessWindowTreeClient::~HeadlessWindowTreeClient() {
  aura::client::SetWindowTreeClient(root_window_, nullptr);
}

aura::Window* HeadlessWindowTreeClient::GetDefaultParent(
    aura::Window* context,
    aura::Window* window,
    const gfx::Rect& bounds) {
  return root_window_;
}

}  // namespace headless
