// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/mus_util.h"

#include "ui/aura/window.h"
#include "ui/aura/window_property.h"

namespace aura {
namespace {

// This code uses Set/GetNativeWindowProperty instead of Set/GetProperty to
// avoid a dependency on mus.
const char kMusWindowKey[] = "mus";

}  // namespace

mus::Window* GetMusWindow(Window* window) {
  if (!window)
    return nullptr;
  return static_cast<mus::Window*>(
      window->GetNativeWindowProperty(kMusWindowKey));
}

void SetMusWindow(Window* window, mus::Window* mus_window) {
  window->SetNativeWindowProperty(kMusWindowKey, mus_window);
}

}  // namespace aura
