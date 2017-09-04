// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_SURFACES_DIRECT_OUTPUT_SURFACE_H_
#define SERVICES_UI_SURFACES_DIRECT_OUTPUT_SURFACE_H_

#include <memory>

#include "cc/output/output_surface.h"
#include "services/ui/surfaces/surfaces_context_provider.h"
#include "services/ui/surfaces/surfaces_context_provider_delegate.h"

namespace cc {
class CompositorFrame;
class SyntheticBeginFrameSource;
}

namespace ui {

// An OutputSurface implementation that directly draws and
// swaps to an actual GL surface.
class DirectOutputSurface : public cc::OutputSurface,
                            public SurfacesContextProviderDelegate {
 public:
  DirectOutputSurface(
      scoped_refptr<SurfacesContextProvider> context_provider,
      cc::SyntheticBeginFrameSource* synthetic_begin_frame_source);
  ~DirectOutputSurface() override;

  // cc::OutputSurface implementation
  void BindToClient(cc::OutputSurfaceClient* client) override;
  void EnsureBackbuffer() override;
  void DiscardBackbuffer() override;
  void BindFramebuffer() override;
  void Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha) override;
  void SwapBuffers(cc::OutputSurfaceFrame frame) override;
  uint32_t GetFramebufferCopyTextureFormat() override;
  cc::OverlayCandidateValidator* GetOverlayCandidateValidator() const override;
  bool IsDisplayedAsOverlayPlane() const override;
  unsigned GetOverlayTextureId() const override;
  bool SurfaceIsSuspendForRecycle() const override;
  bool HasExternalStencilTest() const override;
  void ApplyExternalStencil() override;

  // SurfacesContextProviderDelegate implementation
  void OnVSyncParametersUpdated(const base::TimeTicks& timebase,
                                const base::TimeDelta& interval) override;

 private:
  void OnSwapBuffersComplete();

  cc::OutputSurfaceClient* client_ = nullptr;
  cc::SyntheticBeginFrameSource* const synthetic_begin_frame_source_;
  base::WeakPtrFactory<DirectOutputSurface> weak_ptr_factory_;
};

}  // namespace ui

#endif  // SERVICES_UI_SURFACES_DIRECT_OUTPUT_SURFACE_H_
