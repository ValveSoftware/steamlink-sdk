// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_SURFACES_DIRECT_OUTPUT_SURFACE_OZONE_H_
#define COMPONENTS_MUS_SURFACES_DIRECT_OUTPUT_SURFACE_OZONE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "cc/output/context_provider.h"
#include "cc/output/output_surface.h"
#include "components/display_compositor/gl_helper.h"
#include "components/mus/surfaces/ozone_gpu_memory_buffer_manager.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/swap_result.h"
#include "ui/gl/gl_surface.h"

namespace display_compositor {
class BufferQueue;
}

namespace ui {
class LatencyInfo;
}  // namespace ui

namespace cc {
class CompositorFrame;
class SyntheticBeginFrameSource;
}  // namespace cc

namespace mus {

class SurfacesContextProvider;

// An OutputSurface implementation that directly draws and swap to a GL
// "surfaceless" surface (aka one backed by a buffer managed explicitly in
// mus/ozone. This class is adapted from
// GpuSurfacelessBrowserCompositorOutputSurface.
class DirectOutputSurfaceOzone : public cc::OutputSurface {
 public:
  DirectOutputSurfaceOzone(
      scoped_refptr<SurfacesContextProvider> context_provider,
      gfx::AcceleratedWidget widget,
      cc::SyntheticBeginFrameSource* synthetic_begin_frame_source,
      uint32_t target,
      uint32_t internalformat);

  ~DirectOutputSurfaceOzone() override;

  // TODO(rjkroege): Implement the equivalent of Reflector.

 private:
  // cc::OutputSurface implementation.
  void SwapBuffers(cc::CompositorFrame frame) override;
  void BindFramebuffer() override;
  uint32_t GetFramebufferCopyTextureFormat() override;
  void Reshape(const gfx::Size& size,
               float scale_factor,
               const gfx::ColorSpace& color_space,
               bool alpha) override;
  bool IsDisplayedAsOverlayPlane() const override;
  unsigned GetOverlayTextureId() const override;
  bool BindToClient(cc::OutputSurfaceClient* client) override;

  // Taken from BrowserCompositor specific API.
  void OnUpdateVSyncParametersFromGpu(base::TimeTicks timebase,
                                      base::TimeDelta interval);

  // Called when a swap completion is sent from the GPU process.
  void OnGpuSwapBuffersCompleted(gfx::SwapResult result);

  display_compositor::GLHelper gl_helper_;
  std::unique_ptr<OzoneGpuMemoryBufferManager> ozone_gpu_memory_buffer_manager_;
  std::unique_ptr<display_compositor::BufferQueue> buffer_queue_;
  cc::SyntheticBeginFrameSource* const synthetic_begin_frame_source_;

  base::WeakPtrFactory<DirectOutputSurfaceOzone> weak_ptr_factory_;
};

}  // namespace mus

#endif  // COMPONENTS_MUS_SURFACES_DIRECT_OUTPUT_SURFACE_OZONE_H_
