// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_COMPOSITOR_RESIZE_LOCK_AURA_H_
#define CONTENT_BROWSER_RENDERER_HOST_COMPOSITOR_RESIZE_LOCK_AURA_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/compositor/resize_lock.h"

namespace aura {
class WindowTreeHost;
}
namespace ui {
class CompositorLock;
}

namespace content {

// Used to prevent further resizes while a resize is pending.
class CompositorResizeLock : public ResizeLock {
 public:
  CompositorResizeLock(aura::WindowTreeHost* host,
                       const gfx::Size new_size,
                       bool defer_compositor_lock,
                       const base::TimeDelta& timeout);
  virtual ~CompositorResizeLock();

  virtual bool GrabDeferredLock() OVERRIDE;
  virtual void UnlockCompositor() OVERRIDE;

 protected:
  virtual void LockCompositor() OVERRIDE;
  void CancelLock();

 private:
  aura::WindowTreeHost* host_;
  scoped_refptr<ui::CompositorLock> compositor_lock_;
  base::WeakPtrFactory<CompositorResizeLock> weak_ptr_factory_;
  bool cancelled_;

  DISALLOW_COPY_AND_ASSIGN(CompositorResizeLock);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_COMPOSITOR_RESIZE_LOCK_AURA_H_
