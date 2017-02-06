// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_GPU_OUTPUT_SURFACE_MAC_H_
#define CONTENT_BROWSER_COMPOSITOR_GPU_OUTPUT_SURFACE_MAC_H_

#include "content/browser/compositor/gpu_surfaceless_browser_compositor_output_surface.h"

namespace content {

class GpuOutputSurfaceMac
    : public GpuSurfacelessBrowserCompositorOutputSurface {
 public:
  GpuOutputSurfaceMac(
      scoped_refptr<ContextProviderCommandBuffer> context,
      gpu::SurfaceHandle surface_handle,
      scoped_refptr<ui::CompositorVSyncManager> vsync_manager,
      cc::SyntheticBeginFrameSource* begin_frame_source,
      std::unique_ptr<display_compositor::CompositorOverlayCandidateValidator>
          overlay_candidate_validator,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager);
  ~GpuOutputSurfaceMac() override;

  // cc::OutputSurface implementation.
  void SwapBuffers(cc::CompositorFrame frame) override;
  bool SurfaceIsSuspendForRecycle() const override;

  // BrowserCompositorOutputSurface implementation.
  void OnGpuSwapBuffersCompleted(
      const std::vector<ui::LatencyInfo>& latency_info,
      gfx::SwapResult result,
      const gpu::GpuProcessHostedCALayerTreeParamsMac* params_mac) override;
  void SetSurfaceSuspendedForRecycle(bool suspended) override;

 private:
  // Store remote layers in a separate structure, so that non-Objective-C files
  // may include this header.
  struct RemoteLayers;
  std::unique_ptr<RemoteLayers> remote_layers_;

  enum ShouldShowFramesState {
    // Frames that come from the GPU process should appear on-screen.
    SHOULD_SHOW_FRAMES,
    // The compositor has been suspended. Any frames that come from the GPU
    // process are for the pre-suspend content and should not be displayed.
    SHOULD_NOT_SHOW_FRAMES_SUSPENDED,
    // The compositor has been un-suspended, but has not yet issued a swap
    // since being un-suspended, so any frames that come from the GPU process
    // are for pre-suspend content and should not be displayed.
    SHOULD_NOT_SHOW_FRAMES_NO_SWAP_AFTER_SUSPENDED,
  };
  ShouldShowFramesState should_show_frames_state_ = SHOULD_SHOW_FRAMES;

  DISALLOW_COPY_AND_ASSIGN(GpuOutputSurfaceMac);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_GPU_OUTPUT_SURFACE_MAC_H_
