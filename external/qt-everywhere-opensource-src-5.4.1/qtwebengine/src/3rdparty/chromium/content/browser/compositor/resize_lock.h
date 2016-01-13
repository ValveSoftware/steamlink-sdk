// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_RESIZE_LOCK_H_
#define CONTENT_BROWSER_COMPOSITOR_RESIZE_LOCK_H_

#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "ui/gfx/size.h"

namespace content {

class CONTENT_EXPORT ResizeLock {
 public:
  virtual ~ResizeLock();

  virtual bool GrabDeferredLock();
  virtual void UnlockCompositor();

  const gfx::Size& expected_size() const { return new_size_; }

 protected:
  ResizeLock(const gfx::Size new_size, bool defer_compositor_lock);

  virtual void LockCompositor();

 private:
  gfx::Size new_size_;
  bool defer_compositor_lock_;

  DISALLOW_COPY_AND_ASSIGN(ResizeLock);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_RESIZE_LOCK_H_
