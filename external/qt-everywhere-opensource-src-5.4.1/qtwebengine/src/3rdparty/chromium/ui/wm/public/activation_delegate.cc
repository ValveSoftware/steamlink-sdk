// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/public/activation_delegate.h"

#include "ui/aura/window.h"
#include "ui/aura/window_property.h"

DECLARE_WINDOW_PROPERTY_TYPE(aura::client::ActivationDelegate*)

namespace aura {
namespace client {

DEFINE_LOCAL_WINDOW_PROPERTY_KEY(
    ActivationDelegate*, kActivationDelegateKey, NULL);

void SetActivationDelegate(Window* window, ActivationDelegate* delegate) {
  window->SetProperty(kActivationDelegateKey, delegate);
}

ActivationDelegate* GetActivationDelegate(Window* window) {
  return window->GetProperty(kActivationDelegateKey);
}

}  // namespace client
}  // namespace aura
