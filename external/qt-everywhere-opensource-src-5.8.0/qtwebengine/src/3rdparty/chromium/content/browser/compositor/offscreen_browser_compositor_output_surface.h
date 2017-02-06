// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_OFFSCREEN_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
#define CONTENT_BROWSER_COMPOSITOR_OFFSCREEN_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_

#include <stdint.h>

#include <memory>

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "content/browser/compositor/browser_compositor_output_surface.h"

namespace ui {
class CompositorVSyncManager;
}

namespace content {
class CommandBufferProxyImpl;
class ReflectorTexture;

class OffscreenBrowserCompositorOutputSurface
    : public BrowserCompositorOutputSurface {
 public:
  OffscreenBrowserCompositorOutputSurface(
      scoped_refptr<ContextProviderCommandBuffer> context,
      scoped_refptr<ui::CompositorVSyncManager> vsync_manager,
      cc::SyntheticBeginFrameSource* begin_frame_source,
      std::unique_ptr<display_compositor::CompositorOverlayCandidateValidator>
          overlay_candidate_validator);

  ~OffscreenBrowserCompositorOutputSurface() override;

 protected:
  // cc::OutputSurface:
  void EnsureBackbuffer() override;
  void DiscardBackbuffer() override;
  void Reshape(const gfx::Size& size,
               float scale_factor,
               const gfx::ColorSpace& color_space,
               bool alpha) override;
  void BindFramebuffer() override;
  uint32_t GetFramebufferCopyTextureFormat() override;
  void SwapBuffers(cc::CompositorFrame frame) override;

  // BrowserCompositorOutputSurface
  void OnReflectorChanged() override;
  base::Closure CreateCompositionStartedCallback() override;
  void OnGpuSwapBuffersCompleted(
      const std::vector<ui::LatencyInfo>& latency_info,
      gfx::SwapResult result,
      const gpu::GpuProcessHostedCALayerTreeParamsMac* params_mac) override{};
#if defined(OS_MACOSX)
  void SetSurfaceSuspendedForRecycle(bool suspended) override {};
#endif

  uint32_t fbo_;
  bool is_backbuffer_discarded_;
  std::unique_ptr<ReflectorTexture> reflector_texture_;

  base::WeakPtrFactory<OffscreenBrowserCompositorOutputSurface>
      weak_ptr_factory_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OffscreenBrowserCompositorOutputSurface);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_OFFSCREEN_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
