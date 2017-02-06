// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_GPU_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
#define CONTENT_BROWSER_COMPOSITOR_GPU_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_

#include <memory>

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "content/browser/compositor/browser_compositor_output_surface.h"
#include "ui/gfx/swap_result.h"

namespace display_compositor {
class CompositorOverlayCandidateValidator;
}

namespace gpu {
class CommandBufferProxyImpl;
}

namespace ui {
class CompositorVSyncManager;
}

namespace content {
class ReflectorTexture;

// Adapts a WebGraphicsContext3DCommandBufferImpl into a
// cc::OutputSurface that also handles vsync parameter updates
// arriving from the GPU process.
class GpuBrowserCompositorOutputSurface
    : public BrowserCompositorOutputSurface {
 public:
  GpuBrowserCompositorOutputSurface(
      scoped_refptr<ContextProviderCommandBuffer> context,
      scoped_refptr<ui::CompositorVSyncManager> vsync_manager,
      cc::SyntheticBeginFrameSource* begin_frame_source,
      std::unique_ptr<display_compositor::CompositorOverlayCandidateValidator>
          overlay_candidate_validator);

  ~GpuBrowserCompositorOutputSurface() override;

 protected:
  // BrowserCompositorOutputSurface:
  void OnReflectorChanged() override;
  void OnGpuSwapBuffersCompleted(
      const std::vector<ui::LatencyInfo>& latency_info,
      gfx::SwapResult result,
      const gpu::GpuProcessHostedCALayerTreeParamsMac* params_mac) override;
#if defined(OS_MACOSX)
  void SetSurfaceSuspendedForRecycle(bool suspended) override;
#endif

  // cc::OutputSurface implementation.
  void SwapBuffers(cc::CompositorFrame frame) override;
  bool BindToClient(cc::OutputSurfaceClient* client) override;
  uint32_t GetFramebufferCopyTextureFormat() override;

  gpu::CommandBufferProxyImpl* GetCommandBufferProxy();

  base::CancelableCallback<void(
      const std::vector<ui::LatencyInfo>&,
      gfx::SwapResult,
      const gpu::GpuProcessHostedCALayerTreeParamsMac* params_mac)>
      swap_buffers_completion_callback_;
  base::CancelableCallback<void(base::TimeTicks timebase,
                                base::TimeDelta interval)>
      update_vsync_parameters_callback_;

  std::unique_ptr<ReflectorTexture> reflector_texture_;

  DISALLOW_COPY_AND_ASSIGN(GpuBrowserCompositorOutputSurface);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_GPU_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
