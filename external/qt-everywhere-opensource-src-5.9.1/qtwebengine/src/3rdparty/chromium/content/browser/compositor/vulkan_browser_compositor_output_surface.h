// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_VULKAN_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
#define CONTENT_BROWSER_COMPOSITOR_VULKAN_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "cc/output/output_surface_frame.h"
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
      scoped_refptr<cc::VulkanContextProvider> context,
      scoped_refptr<ui::CompositorVSyncManager> vsync_manager,
      cc::SyntheticBeginFrameSource* begin_frame_source);

  ~VulkanBrowserCompositorOutputSurface() override;

  bool Initialize(gfx::AcceleratedWidget widget);
  void Destroy();

  // cc::OutputSurface implementation.
  void BindToClient(cc::OutputSurfaceClient* client) override;
  void EnsureBackbuffer() override;
  void DiscardBackbuffer() override;
  void BindFramebuffer() override;
  bool IsDisplayedAsOverlayPlane() const override;
  unsigned GetOverlayTextureId() const override;
  bool SurfaceIsSuspendForRecycle() const override;
  void Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha) override;
  uint32_t GetFramebufferCopyTextureFormat() override;
  void SwapBuffers(cc::OutputSurfaceFrame frame) override;

 private:
  void SwapBuffersAck();

  std::unique_ptr<gpu::VulkanSurface> surface_;
  cc::OutputSurfaceClient* client_ = nullptr;
  base::WeakPtrFactory<VulkanBrowserCompositorOutputSurface> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VulkanBrowserCompositorOutputSurface);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_VULKAN_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
