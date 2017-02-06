// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_SOFTWARE_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
#define CONTENT_BROWSER_COMPOSITOR_SOFTWARE_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "content/browser/compositor/browser_compositor_output_surface.h"
#include "content/common/content_export.h"

namespace cc {
class SoftwareOutputDevice;
}

namespace ui {
class CompositorVSyncManager;
}

namespace content {

class CONTENT_EXPORT SoftwareBrowserCompositorOutputSurface
    : public BrowserCompositorOutputSurface {
 public:
  SoftwareBrowserCompositorOutputSurface(
      std::unique_ptr<cc::SoftwareOutputDevice> software_device,
      const scoped_refptr<ui::CompositorVSyncManager>& vsync_manager,
      cc::SyntheticBeginFrameSource* begin_frame_source);

  ~SoftwareBrowserCompositorOutputSurface() override;

  // OutputSurface implementation.
  void SwapBuffers(cc::CompositorFrame frame) override;
  void BindFramebuffer() override;
  uint32_t GetFramebufferCopyTextureFormat() override;

 private:
  // BrowserCompositorOutputSurface implementation.
  void OnGpuSwapBuffersCompleted(
      const std::vector<ui::LatencyInfo>& latency_info,
      gfx::SwapResult result,
      const gpu::GpuProcessHostedCALayerTreeParamsMac* params_mac) override;

#if defined(OS_MACOSX)
  void SetSurfaceSuspendedForRecycle(bool suspended) override;
#endif

  base::WeakPtrFactory<SoftwareBrowserCompositorOutputSurface> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SoftwareBrowserCompositorOutputSurface);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_SOFTWARE_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
