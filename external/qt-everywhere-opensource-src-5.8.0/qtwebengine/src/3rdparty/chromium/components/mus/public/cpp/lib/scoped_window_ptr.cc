// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/scoped_window_ptr.h"

#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_observer.h"
#include "components/mus/public/cpp/window_tree_client.h"

namespace mus {

ScopedWindowPtr::ScopedWindowPtr(Window* window) : window_(window) {
  window_->AddObserver(this);
}

ScopedWindowPtr::~ScopedWindowPtr() {
  if (window_)
    DeleteWindowOrWindowManager(window_);
  DetachFromWindow();
}

// static
void ScopedWindowPtr::DeleteWindowOrWindowManager(Window* window) {
  if (window->window_tree()->GetRoots().count(window) > 0)
    delete window->window_tree();
  else
    window->Destroy();
}

void ScopedWindowPtr::DetachFromWindow() {
  if (!window_)
    return;

  window_->RemoveObserver(this);
  window_ = nullptr;
}

void ScopedWindowPtr::OnWindowDestroying(Window* window) {
  DCHECK_EQ(window_, window);
  DetachFromWindow();
}

}  // namespace mus
