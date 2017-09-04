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
class ContextProviderCommandBuffer;
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

 private:
  // cc::OutputSurface implementation.
  void BindToClient(cc::OutputSurfaceClient* client) override;
  void EnsureBackbuffer() override;
  void DiscardBackbuffer() override;
  void Reshape(const gfx::Size& size,
               float scale_factor,
               const gfx::ColorSpace& color_space,
               bool alpha) override;
  void BindFramebuffer() override;
  void SwapBuffers(cc::OutputSurfaceFrame frame) override;
  bool IsDisplayedAsOverlayPlane() const override;
  unsigned GetOverlayTextureId() const override;
  bool SurfaceIsSuspendForRecycle() const override;
  uint32_t GetFramebufferCopyTextureFormat() override;

  // BrowserCompositorOutputSurface implementation.
  void OnReflectorChanged() override;
#if defined(OS_MACOSX)
  void SetSurfaceSuspendedForRecycle(bool suspended) override {};
#endif

  void OnSwapBuffersComplete();

  cc::OutputSurfaceClient* client_ = nullptr;
  gfx::Size reshape_size_;
  uint32_t fbo_ = 0;
  bool reflector_changed_ = false;
  std::unique_ptr<ReflectorTexture> reflector_texture_;
  base::WeakPtrFactory<OffscreenBrowserCompositorOutputSurface>
      weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OffscreenBrowserCompositorOutputSurface);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_OFFSCREEN_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
