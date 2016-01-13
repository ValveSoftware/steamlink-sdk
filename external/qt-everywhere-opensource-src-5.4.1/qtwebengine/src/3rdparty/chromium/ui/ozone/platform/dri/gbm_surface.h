// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_GBM_SURFACE_H_
#define UI_OZONE_PLATFORM_DRI_GBM_SURFACE_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/dri/scanout_surface.h"

struct gbm_bo;
struct gbm_device;
struct gbm_surface;

namespace ui {

class DriBuffer;
class DriWrapper;

// Implement the ScanoutSurface interface on top of GBM (Generic Buffer
// Manager). GBM provides generic access to hardware accelerated surfaces which
// can be used in association with EGL to provide accelerated drawing.
class GbmSurface : public ScanoutSurface {
 public:
  GbmSurface(gbm_device* device, DriWrapper* dri, const gfx::Size& size);
  virtual ~GbmSurface();

  // ScanoutSurface:
  virtual bool Initialize() OVERRIDE;
  virtual uint32_t GetFramebufferId() const OVERRIDE;
  virtual uint32_t GetHandle() const OVERRIDE;
  virtual gfx::Size Size() const OVERRIDE;
  virtual void SwapBuffers() OVERRIDE;

  // Before scheduling the backbuffer to be scanned out we need to "lock" it.
  // When we lock it, GBM will give a pointer to a buffer representing the
  // backbuffer. It will also update its information on which buffers can not be
  // used for drawing. The buffer will be released when the page flip event
  // occurs (see SwapBuffers). This is called from GbmSurfaceFactory before
  // scheduling a page flip.
  void LockCurrentDrawable();

  gbm_surface* native_surface() { return native_surface_; };

 private:
  gbm_device* gbm_device_;

  DriWrapper* dri_;

  gfx::Size size_;

  // The native GBM surface. In EGL this represents the EGLNativeWindowType.
  gbm_surface* native_surface_;

  // Backing GBM buffers. One is the current front buffer. The other is the
  // current backbuffer that is pending scan out.
  gbm_bo* buffers_[2];

  // Index to the front buffer.
  int front_buffer_;

  // We can't lock (and get) an accelerated buffer from the GBM surface until
  // after something draws into it. But modesetting needs to happen earlier,
  // before an actual window is created and draws. So, we create a dumb buffer
  // for this purpose.
  scoped_ptr<DriBuffer> dumb_buffer_;

  DISALLOW_COPY_AND_ASSIGN(GbmSurface);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_GBM_SURFACE_H_
