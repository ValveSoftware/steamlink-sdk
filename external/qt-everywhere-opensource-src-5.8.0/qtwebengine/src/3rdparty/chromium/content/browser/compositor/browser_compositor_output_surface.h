// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
#define CONTENT_BROWSER_COMPOSITOR_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_

#include "base/macros.h"
#include "base/threading/non_thread_safe.h"
#include "build/build_config.h"
#include "cc/output/output_surface.h"
#include "content/common/content_export.h"
#include "ui/compositor/compositor_vsync_manager.h"

namespace cc {
class SoftwareOutputDevice;
class SyntheticBeginFrameSource;
}

namespace display_compositor {
class CompositorOverlayCandidateValidator;
}

namespace gfx {
enum class SwapResult;
}

namespace gpu {
struct GpuProcessHostedCALayerTreeParamsMac;
}

namespace content {
class ContextProviderCommandBuffer;
class ReflectorImpl;
class WebGraphicsContext3DCommandBufferImpl;

class CONTENT_EXPORT BrowserCompositorOutputSurface
    : public cc::OutputSurface,
      public ui::CompositorVSyncManager::Observer {
 public:
  ~BrowserCompositorOutputSurface() override;

  // cc::OutputSurface implementation.
  bool BindToClient(cc::OutputSurfaceClient* client) override;
  cc::OverlayCandidateValidator* GetOverlayCandidateValidator() const override;

  // ui::CompositorVSyncManager::Observer implementation.
  void OnUpdateVSyncParameters(base::TimeTicks timebase,
                               base::TimeDelta interval) override;

  void OnUpdateVSyncParametersFromGpu(base::TimeTicks timebase,
                                      base::TimeDelta interval);

  void SetReflector(ReflectorImpl* reflector);

  // Called when |reflector_| was updated.
  virtual void OnReflectorChanged();

  // Returns a callback that will be called when all mirroring
  // compositors have started composition.
  virtual base::Closure CreateCompositionStartedCallback();

  // Called when a swap completion is sent from the GPU process.
  // The argument |params_mac| is used to communicate parameters needed on Mac
  // to display the CALayer for the swap in the browser process.
  // TODO(ccameron): Remove |params_mac| when the CALayer tree is hosted in the
  // browser process.
  virtual void OnGpuSwapBuffersCompleted(
      const std::vector<ui::LatencyInfo>& latency_info,
      gfx::SwapResult result,
      const gpu::GpuProcessHostedCALayerTreeParamsMac* params_mac) = 0;

#if defined(OS_MACOSX)
  virtual void SetSurfaceSuspendedForRecycle(bool suspended) = 0;
#endif

 protected:
  // Constructor used by the accelerated implementation.
  BrowserCompositorOutputSurface(
      scoped_refptr<cc::ContextProvider> context,
      scoped_refptr<ui::CompositorVSyncManager> vsync_manager,
      cc::SyntheticBeginFrameSource* begin_frame_source,
      std::unique_ptr<display_compositor::CompositorOverlayCandidateValidator>
          overlay_candidate_validator);

  // Constructor used by the software implementation.
  BrowserCompositorOutputSurface(
      std::unique_ptr<cc::SoftwareOutputDevice> software_device,
      const scoped_refptr<ui::CompositorVSyncManager>& vsync_manager,
      cc::SyntheticBeginFrameSource* begin_frame_source);

  // Constructor used by the Vulkan implementation.
  BrowserCompositorOutputSurface(
      const scoped_refptr<cc::VulkanContextProvider>& vulkan_context_provider,
      const scoped_refptr<ui::CompositorVSyncManager>& vsync_manager,
      cc::SyntheticBeginFrameSource* begin_frame_source);

  scoped_refptr<ui::CompositorVSyncManager> vsync_manager_;
  cc::SyntheticBeginFrameSource* synthetic_begin_frame_source_;
  ReflectorImpl* reflector_;

  // True when BeginFrame scheduling is enabled.
  bool use_begin_frame_scheduling_;

 private:
  void Initialize();

  void UpdateVSyncParametersInternal(base::TimeTicks timebase,
                                     base::TimeDelta interval);

  std::unique_ptr<display_compositor::CompositorOverlayCandidateValidator>
      overlay_candidate_validator_;

  DISALLOW_COPY_AND_ASSIGN(BrowserCompositorOutputSurface);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
