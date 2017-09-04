// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/vulkan_browser_compositor_output_surface.h"

#include "cc/output/output_surface_client.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "gpu/vulkan/vulkan_surface.h"

namespace content {

VulkanBrowserCompositorOutputSurface::VulkanBrowserCompositorOutputSurface(
    scoped_refptr<cc::VulkanContextProvider> context,
    scoped_refptr<ui::CompositorVSyncManager> vsync_manager,
    cc::SyntheticBeginFrameSource* begin_frame_source)
    : BrowserCompositorOutputSurface(std::move(context),
                                     std::move(vsync_manager),
                                     begin_frame_source),
      weak_ptr_factory_(this) {}

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

void VulkanBrowserCompositorOutputSurface::BindToClient(
    cc::OutputSurfaceClient* client) {
  DCHECK(client);
  DCHECK(!client_);
  client_ = client;
}

void VulkanBrowserCompositorOutputSurface::EnsureBackbuffer() {
  NOTIMPLEMENTED();
}

void VulkanBrowserCompositorOutputSurface::DiscardBackbuffer() {
  NOTIMPLEMENTED();
}

void VulkanBrowserCompositorOutputSurface::BindFramebuffer() {
  NOTIMPLEMENTED();
}

bool VulkanBrowserCompositorOutputSurface::IsDisplayedAsOverlayPlane() const {
  NOTIMPLEMENTED();
  return false;
}

unsigned VulkanBrowserCompositorOutputSurface::GetOverlayTextureId() const {
  NOTIMPLEMENTED();
  return 0;
}

bool VulkanBrowserCompositorOutputSurface::SurfaceIsSuspendForRecycle() const {
  NOTIMPLEMENTED();
  return false;
}

void VulkanBrowserCompositorOutputSurface::Reshape(
    const gfx::Size& size,
    float device_scale_factor,
    const gfx::ColorSpace& color_space,
    bool has_alpha) {
  NOTIMPLEMENTED();
}

uint32_t
VulkanBrowserCompositorOutputSurface::GetFramebufferCopyTextureFormat() {
  NOTIMPLEMENTED();
  return 0;
}

void VulkanBrowserCompositorOutputSurface::SwapBuffers(
    cc::OutputSurfaceFrame frame) {
  surface_->SwapBuffers();

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&VulkanBrowserCompositorOutputSurface::SwapBuffersAck,
                 weak_ptr_factory_.GetWeakPtr()));
}

void VulkanBrowserCompositorOutputSurface::SwapBuffersAck() {
  DCHECK(client_);
  client_->DidReceiveSwapBuffersAck();
}

}  // namespace content
