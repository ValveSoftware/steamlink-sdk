// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_GBM_SURFACELESS_H_
#define UI_OZONE_PLATFORM_DRM_GPU_GBM_SURFACELESS_H_

#include <vector>

#include "base/macros.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/drm/gpu/overlay_plane.h"
#include "ui/ozone/public/surface_ozone_egl.h"

namespace gfx {
class Size;
}

namespace ui {

class DrmWindowProxy;
class GbmSurfaceFactory;

// In surfaceless mode drawing and displaying happens directly through
// NativePixmap buffers. CC would call into SurfaceFactoryOzone to allocate the
// buffers and then call ScheduleOverlayPlane(..) to schedule the buffer for
// presentation.
class GbmSurfaceless : public SurfaceOzoneEGL {
 public:
  GbmSurfaceless(std::unique_ptr<DrmWindowProxy> window,
                 GbmSurfaceFactory* surface_manager);
  ~GbmSurfaceless() override;

  void QueueOverlayPlane(const OverlayPlane& plane);

  // SurfaceOzoneEGL:
  intptr_t GetNativeWindow() override;
  bool ResizeNativeWindow(const gfx::Size& viewport_size) override;
  bool OnSwapBuffers() override;
  void OnSwapBuffersAsync(const SwapCompletionCallback& callback) override;
  std::unique_ptr<gfx::VSyncProvider> CreateVSyncProvider() override;
  bool IsUniversalDisplayLinkDevice() override;
  void* /* EGLConfig */ GetEGLSurfaceConfig(
      const EglConfigCallbacks& egl) override;

 protected:
  std::unique_ptr<DrmWindowProxy> window_;

  GbmSurfaceFactory* surface_manager_;

  std::vector<OverlayPlane> planes_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GbmSurfaceless);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_GBM_SURFACELESS_H_
