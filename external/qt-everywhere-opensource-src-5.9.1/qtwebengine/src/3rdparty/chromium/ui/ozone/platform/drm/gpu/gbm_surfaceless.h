// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_GBM_SURFACELESS_H_
#define UI_OZONE_PLATFORM_DRM_GPU_GBM_SURFACELESS_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_surface_overlay.h"
#include "ui/gl/scoped_binders.h"
#include "ui/ozone/platform/drm/gpu/overlay_plane.h"

namespace ui {

class DrmWindowProxy;
class GbmSurfaceFactory;

// A GLSurface for GBM Ozone platform that uses surfaceless drawing. Drawing and
// displaying happens directly through NativePixmap buffers. CC would call into
// SurfaceFactoryOzone to allocate the buffers and then call
// ScheduleOverlayPlane(..) to schedule the buffer for presentation.
class GbmSurfaceless : public gl::SurfacelessEGL {
 public:
  GbmSurfaceless(GbmSurfaceFactory* surface_factory,
                 std::unique_ptr<DrmWindowProxy> window,
                 gfx::AcceleratedWidget widget);

  void QueueOverlayPlane(const OverlayPlane& plane);

  // gl::GLSurface:
  bool Initialize(GLSurface::Format format) override;
  gfx::SwapResult SwapBuffers() override;
  bool ScheduleOverlayPlane(int z_order,
                            gfx::OverlayTransform transform,
                            gl::GLImage* image,
                            const gfx::Rect& bounds_rect,
                            const gfx::RectF& crop_rect) override;
  bool IsOffscreen() override;
  gfx::VSyncProvider* GetVSyncProvider() override;
  bool SupportsAsyncSwap() override;
  bool SupportsPostSubBuffer() override;
  gfx::SwapResult PostSubBuffer(int x, int y, int width, int height) override;
  void SwapBuffersAsync(const SwapCompletionCallback& callback) override;
  void PostSubBufferAsync(int x,
                          int y,
                          int width,
                          int height,
                          const SwapCompletionCallback& callback) override;
  EGLConfig GetConfig() override;

 protected:
  ~GbmSurfaceless() override;

  gfx::AcceleratedWidget widget() { return widget_; }
  GbmSurfaceFactory* surface_factory() { return surface_factory_; }

 private:
  struct PendingFrame {
    PendingFrame();
    ~PendingFrame();

    bool ScheduleOverlayPlanes(gfx::AcceleratedWidget widget);
    void Flush();

    bool ready = false;
    std::vector<gl::GLSurfaceOverlay> overlays;
    SwapCompletionCallback callback;
  };

  void SubmitFrame();

  EGLSyncKHR InsertFence(bool implicit);
  void FenceRetired(EGLSyncKHR fence, PendingFrame* frame);

  void SwapCompleted(const SwapCompletionCallback& callback,
                     gfx::SwapResult result);

  bool IsUniversalDisplayLinkDevice();

  GbmSurfaceFactory* surface_factory_;
  std::unique_ptr<DrmWindowProxy> window_;
  std::vector<OverlayPlane> planes_;

  // The native surface. Deleting this is allowed to free the EGLNativeWindow.
  gfx::AcceleratedWidget widget_;
  std::unique_ptr<gfx::VSyncProvider> vsync_provider_;
  ScopedVector<PendingFrame> unsubmitted_frames_;
  bool has_implicit_external_sync_;
  bool has_image_flush_external_;
  bool last_swap_buffers_result_ = true;
  bool swap_buffers_pending_ = false;

  base::WeakPtrFactory<GbmSurfaceless> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GbmSurfaceless);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_GBM_SURFACELESS_H_
