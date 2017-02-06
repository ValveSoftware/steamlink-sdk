// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/vulkan_browser_compositor_output_surface.h"

#include "cc/output/output_surface_client.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "gpu/vulkan/vulkan_surface.h"

namespace content {

VulkanBrowserCompositorOutputSurface::VulkanBrowserCompositorOutputSurface(
    const scoped_refptr<cc::VulkanContextProvider>& context,
    const scoped_refptr<ui::CompositorVSyncManager>& vsync_manager,
    cc::SyntheticBeginFrameSource* begin_frame_source)
    : BrowserCompositorOutputSurface(context,
                                     vsync_manager,
                                     begin_frame_source) {}

VulkanBrowserCompositorOutputSurface::~VulkanBrowserCompositorOutputSurface() {
  Destroy();
}

bool VulkanBrowserCompositorOutputSurface::Initialize(
    gfx::AcceleratedWidget widget) {
  DCHECK(!surface_);
  std::unique_ptr<gpu::VulkanSurface> surface(
      gpu::VulkanSurface::CreateViewSurface(widget));
  if (!surface->Initialize(vulkan_context_provider()->GetDeviceQueue(),
                           gpu::VulkanSurface::DEFAULT_SURFACE_FORMAT)) {
    return false;
  }
  surface_ = std::move(surface);

  return true;
}

void VulkanBrowserCompositorOutputSurface::Destroy() {
  if (surface_) {
    surface_->Destroy();
    surface_.reset();
  }
}

void VulkanBrowserCompositorOutputSurface::OnGpuSwapBuffersCompleted(
    const std::vector<ui::LatencyInfo>& latency_info,
    gfx::SwapResult result,
    const gpu::GpuProcessHostedCALayerTreeParamsMac* params_mac) {
  RenderWidgetHostImpl::CompositorFrameDrawn(latency_info);
  OnSwapBuffersComplete();
}

void VulkanBrowserCompositorOutputSurface::SwapBuffers(
    cc::CompositorFrame* frame) {
  surface_->SwapBuffers();
  PostSwapBuffersComplete();
  client_->DidSwapBuffers();
}

}  // namespace content
