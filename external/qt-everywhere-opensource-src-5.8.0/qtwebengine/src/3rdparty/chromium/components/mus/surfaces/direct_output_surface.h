// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_SURFACES_DIRECT_OUTPUT_SURFACE_H_
#define COMPONENTS_MUS_SURFACES_DIRECT_OUTPUT_SURFACE_H_

#include <memory>

#include "cc/output/output_surface.h"
#include "components/mus/surfaces/surfaces_context_provider.h"
#include "components/mus/surfaces/surfaces_context_provider_delegate.h"

namespace cc {
class CompositorFrame;
class SyntheticBeginFrameSource;
}

namespace mus {

// An OutputSurface implementation that directly draws and
// swaps to an actual GL surface.
class DirectOutputSurface : public cc::OutputSurface,
                            public SurfacesContextProviderDelegate {
 public:
  explicit DirectOutputSurface(
      scoped_refptr<SurfacesContextProvider> context_provider,
      cc::SyntheticBeginFrameSource* synthetic_begin_frame_source);
  ~DirectOutputSurface() override;

  // cc::OutputSurface implementation
  bool BindToClient(cc::OutputSurfaceClient* client) override;
  void SwapBuffers(cc::CompositorFrame frame) override;
  uint32_t GetFramebufferCopyTextureFormat() override;

  // SurfacesContextProviderDelegate implementation
  void OnVSyncParametersUpdated(const base::TimeTicks& timebase,
                                const base::TimeDelta& interval) override;

 private:
  cc::SyntheticBeginFrameSource* const synthetic_begin_frame_source_;
  base::WeakPtrFactory<DirectOutputSurface> weak_ptr_factory_;
};

}  // namespace mus

#endif  // COMPONENTS_MUS_SURFACES_DIRECT_OUTPUT_SURFACE_H_
