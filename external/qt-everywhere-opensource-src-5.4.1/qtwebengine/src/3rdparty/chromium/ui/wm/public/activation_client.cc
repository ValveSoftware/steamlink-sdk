// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/public/activation_client.h"

#include "ui/aura/window.h"
#include "ui/aura/window_property.h"

DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(AURA_EXPORT, aura::Window*)
DECLARE_WINDOW_PROPERTY_TYPE(aura::client::ActivationClient*)

namespace aura {
namespace client {

DEFINE_WINDOW_PROPERTY_KEY(
    ActivationClient*, kRootWindowActivationClientKey, NULL);
DEFINE_WINDOW_PROPERTY_KEY(bool, kHideOnDeactivate, false);

void SetActivationClient(Window* root_window, ActivationClient* client) {
  root_window->SetProperty(kRootWindowActivationClientKey, client);
}

ActivationClient* GetActivationClient(Window* root_window) {
  return root_window ?
      root_window->GetProperty(kRootWindowActivationClientKey) : NULL;
}

void SetHideOnDeactivate(Window* window, bool hide_on_deactivate) {
  window->SetProperty(kHideOnDeactivate, hide_on_deactivate);
}

bool GetHideOnDeactivate(Window* window) {
  return window->GetProperty(kHideOnDeactivate);
}

}  // namespace client
}  // namespace aura
