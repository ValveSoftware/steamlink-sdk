// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/focus_client.h"

#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_property.h"

DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(AURA_EXPORT, aura::Window*)
DECLARE_WINDOW_PROPERTY_TYPE(aura::client::FocusClient*)

namespace aura {
namespace client {

DEFINE_WINDOW_PROPERTY_KEY(FocusClient*, kRootWindowFocusClientKey, NULL);

void SetFocusClient(Window* root_window, FocusClient* client) {
  DCHECK_EQ(root_window->GetRootWindow(), root_window);
  root_window->SetProperty(kRootWindowFocusClientKey, client);
}

FocusClient* GetFocusClient(Window* window) {
  return GetFocusClient(static_cast<const Window*>(window));
}

FocusClient* GetFocusClient(const Window* window) {
  const Window* root_window = window->GetRootWindow();
  return root_window ?
      root_window->GetProperty(kRootWindowFocusClientKey) : NULL;
}

}  // namespace client
}  // namespace aura
