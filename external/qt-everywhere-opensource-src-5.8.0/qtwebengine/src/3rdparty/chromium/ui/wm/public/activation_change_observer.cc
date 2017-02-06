// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/public/activation_change_observer.h"

#include "ui/aura/window.h"
#include "ui/aura/window_property.h"

DECLARE_WINDOW_PROPERTY_TYPE(aura::client::ActivationChangeObserver*)

namespace aura {
namespace client {

DEFINE_LOCAL_WINDOW_PROPERTY_KEY(
    ActivationChangeObserver*, kActivationChangeObserverKey, NULL);

void SetActivationChangeObserver(
    Window* window,
    ActivationChangeObserver* observer) {
  window->SetProperty(kActivationChangeObserverKey, observer);
}

ActivationChangeObserver* GetActivationChangeObserver(Window* window) {
  return window ? window->GetProperty(kActivationChangeObserverKey) : NULL;
}

}  // namespace client
}  // namespace aura
