// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/public/window_move_client.h"

#include "ui/aura/window.h"
#include "ui/aura/window_property.h"

DECLARE_WINDOW_PROPERTY_TYPE(aura::client::WindowMoveClient*)

namespace aura {
namespace client {

// A property key to store a client that handles window moves.
DEFINE_LOCAL_WINDOW_PROPERTY_KEY(
    WindowMoveClient*, kWindowMoveClientKey, NULL);

void SetWindowMoveClient(Window* window, WindowMoveClient* client) {
  window->SetProperty(kWindowMoveClientKey, client);
}

WindowMoveClient* GetWindowMoveClient(Window* window) {
  return window->GetProperty(kWindowMoveClientKey);
}

}  // namespace client
}  // namespace aura
