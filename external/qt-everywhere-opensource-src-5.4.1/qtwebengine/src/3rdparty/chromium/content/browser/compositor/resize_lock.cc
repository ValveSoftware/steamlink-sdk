// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/resize_lock.h"

namespace content {

ResizeLock::ResizeLock(const gfx::Size new_size, bool defer_compositor_lock)
    : new_size_(new_size),
      defer_compositor_lock_(defer_compositor_lock) {
  if (!defer_compositor_lock_)
    LockCompositor();
}

ResizeLock::~ResizeLock() {
  UnlockCompositor();
}

bool ResizeLock::GrabDeferredLock() {
  if (!defer_compositor_lock_)
    return false;
  LockCompositor();
  return true;
}

void ResizeLock::UnlockCompositor() {
  defer_compositor_lock_ = false;
}

void ResizeLock::LockCompositor() {
  defer_compositor_lock_ = false;
}

}  // namespace content
