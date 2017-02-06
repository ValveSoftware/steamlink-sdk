// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_CAST_SURFACE_OZONE_EGL_CAST_H_
#define UI_OZONE_PLATFORM_CAST_SURFACE_OZONE_EGL_CAST_H_

#include "base/macros.h"
#include "ui/ozone/public/surface_ozone_egl.h"

namespace ui {

class SurfaceFactoryCast;

// EGL surface wrapper for OzonePlatformCast.
class SurfaceOzoneEglCast : public SurfaceOzoneEGL {
 public:
  explicit SurfaceOzoneEglCast(SurfaceFactoryCast* parent) : parent_(parent) {}
  ~SurfaceOzoneEglCast() override;

  // SurfaceOzoneEGL implementation:
  intptr_t GetNativeWindow() override;
  bool OnSwapBuffers() override;
  void OnSwapBuffersAsync(const SwapCompletionCallback& callback) override;
  bool ResizeNativeWindow(const gfx::Size& viewport_size) override;
  std::unique_ptr<gfx::VSyncProvider> CreateVSyncProvider() override;
  void* /* EGLConfig */ GetEGLSurfaceConfig(
      const EglConfigCallbacks& egl) override;

 private:
  SurfaceFactoryCast* parent_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceOzoneEglCast);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_CAST_SURFACE_OZONE_EGL_CAST_H_
