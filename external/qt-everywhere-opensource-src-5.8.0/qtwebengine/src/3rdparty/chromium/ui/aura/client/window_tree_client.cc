// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/window_tree_client.h"

#include "ui/aura/env.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_property.h"

DECLARE_WINDOW_PROPERTY_TYPE(aura::client::WindowTreeClient*)

namespace aura {
namespace client {

DEFINE_WINDOW_PROPERTY_KEY(
    WindowTreeClient*, kRootWindowWindowTreeClientKey, NULL);

void SetWindowTreeClient(Window* window, WindowTreeClient* window_tree_client) {
  DCHECK(window);

  Window* root_window = window->GetRootWindow();
  DCHECK(root_window);
  root_window->SetProperty(kRootWindowWindowTreeClientKey, window_tree_client);
}

WindowTreeClient* GetWindowTreeClient(Window* window) {
  DCHECK(window);
  Window* root_window = window->GetRootWindow();
  DCHECK(root_window);
  WindowTreeClient* client =
      root_window->GetProperty(kRootWindowWindowTreeClientKey);
  DCHECK(client);
  return client;
}

void ParentWindowWithContext(Window* window,
                             Window* context,
                             const gfx::Rect& screen_bounds) {
  DCHECK(context);

  // |context| must be attached to a hierarchy with a WindowTreeClient.
  WindowTreeClient* client = GetWindowTreeClient(context);
  DCHECK(client);
  Window* default_parent =
      client->GetDefaultParent(context, window, screen_bounds);
  default_parent->AddChild(window);
}

}  // namespace client
}  // namespace aura
