// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/scoped_window_targeter.h"

#include "ui/aura/window.h"

namespace aura {

ScopedWindowTargeter::ScopedWindowTargeter(
    Window* window,
    scoped_ptr<ui::EventTargeter> new_targeter)
    : window_(window),
      old_targeter_(window->SetEventTargeter(new_targeter.Pass())) {
}

ScopedWindowTargeter::~ScopedWindowTargeter() {
  if (window_)
    window_->SetEventTargeter(old_targeter_.Pass());
}

void ScopedWindowTargeter::OnWindowDestroyed(Window* window) {
  CHECK_EQ(window_, window);
  window_ = NULL;
  old_targeter_.reset();
}

}  // namespace aura
