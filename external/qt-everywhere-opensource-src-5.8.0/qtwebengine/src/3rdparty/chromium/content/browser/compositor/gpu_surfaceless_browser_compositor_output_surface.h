// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_GPU_SURFACELESS_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
#define CONTENT_BROWSER_COMPOSITOR_GPU_SURFACELESS_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_

#include <memory>

#include "content/browser/compositor/gpu_browser_compositor_output_surface.h"
#include "gpu/ipc/common/surface_handle.h"

namespace gpu {
class GpuMemoryBufferManager;
}

namespace display_compositor {
class BufferQueue;
class GLHelper;
}

namespace content {

class GpuSurfacelessBrowserCompositorOutputSurface
    : public GpuBrowserCompositorOutputSurface {
 public:
  GpuSurfacelessBrowserCompositorOutputSurface(
      scoped_refptr<ContextProviderCommandBuffer> context,
      gpu::SurfaceHandle surface_handle,
      scoped_refptr<ui::CompositorVSyncManager> vsync_manager,
      cc::SyntheticBeginFrameSource* begin_frame_source,
      std::unique_ptr<display_compositor::CompositorOverlayCandidateValidator>
          overlay_candidate_validator,
      unsigned int target,
      unsigned int internalformat,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager);
  ~GpuSurfacelessBrowserCompositorOutputSurface() override;

  // cc::OutputSurface implementation.
  void SwapBuffers(cc::CompositorFrame frame) override;
  void OnSwapBuffersComplete() override;
  void BindFramebuffer() override;
  uint32_t GetFramebufferCopyTextureFormat() override;
  void Reshape(const gfx::Size& size,
               float scale_factor,
               const gfx::ColorSpace& color_space,
               bool alpha) override;
  bool IsDisplayedAsOverlayPlane() const override;
  unsigned GetOverlayTextureId() const override;

  // BrowserCompositorOutputSurface implementation.
  void OnGpuSwapBuffersCompleted(
      const std::vector<ui::LatencyInfo>& latency_info,
      gfx::SwapResult result,
      const gpu::GpuProcessHostedCALayerTreeParamsMac* params_mac) override;

 private:
  const unsigned int internalformat_;
  std::unique_ptr<display_compositor::GLHelper> gl_helper_;
  std::unique_ptr<display_compositor::BufferQueue> buffer_queue_;
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_GPU_SURFACELESS_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
