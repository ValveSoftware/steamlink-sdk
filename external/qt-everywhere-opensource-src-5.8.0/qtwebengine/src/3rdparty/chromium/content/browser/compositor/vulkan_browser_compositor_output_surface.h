// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_VULKAN_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
#define CONTENT_BROWSER_COMPOSITOR_VULKAN_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_

#include <memory>

#include "base/macros.h"
#include "content/browser/compositor/browser_compositor_output_surface.h"
#include "ui/gfx/native_widget_types.h"

namespace gpu {
class VulkanSurface;
}

namespace content {

class VulkanBrowserCompositorOutputSurface
    : public BrowserCompositorOutputSurface {
 public:
  VulkanBrowserCompositorOutputSurface(
      const scoped_refptr<cc::VulkanContextProvider>& context,
      const scoped_refptr<ui::CompositorVSyncManager>& vsync_manager,
      cc::SyntheticBeginFrameSource* begin_frame_source);

  ~VulkanBrowserCompositorOutputSurface() override;

  bool Initialize(gfx::AcceleratedWidget widget);
  void Destroy();

  // BrowserCompositorOutputSurface implementation.
  void OnGpuSwapBuffersCompleted(
      const std::vector<ui::LatencyInfo>& latency_info,
      gfx::SwapResult result,
      const gpu::GpuProcessHostedCALayerTreeParamsMac* params_mac) override;

 protected:
  // cc::OutputSurface implementation.
  void SwapBuffers(cc::CompositorFrame* frame) override;

 private:
  std::unique_ptr<gpu::VulkanSurface> surface_;

  DISALLOW_COPY_AND_ASSIGN(VulkanBrowserCompositorOutputSurface);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_VULKAN_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
