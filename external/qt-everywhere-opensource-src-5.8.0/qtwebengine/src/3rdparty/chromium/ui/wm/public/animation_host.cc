// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/public/animation_host.h"

#include "ui/aura/window.h"
#include "ui/aura/window_property.h"

DECLARE_WINDOW_PROPERTY_TYPE(aura::client::AnimationHost*)

namespace aura {
namespace client {

DEFINE_WINDOW_PROPERTY_KEY(AnimationHost*, kRootWindowAnimationHostKey, NULL);

void SetAnimationHost(Window* window, AnimationHost* animation_host) {
  DCHECK(window);
  window->SetProperty(kRootWindowAnimationHostKey, animation_host);
}

AnimationHost* GetAnimationHost(Window* window) {
  DCHECK(window);
  return window->GetProperty(kRootWindowAnimationHostKey);
}

}  // namespace client
}  // namespace aura
