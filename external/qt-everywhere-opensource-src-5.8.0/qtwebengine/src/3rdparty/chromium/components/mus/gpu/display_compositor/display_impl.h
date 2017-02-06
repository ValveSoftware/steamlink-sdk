// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_GPU_DISPLAY_COMPOSITOR_DISPLAY_IMPL_H_
#define COMPONENTS_MUS_GPU_DISPLAY_COMPOSITOR_DISPLAY_IMPL_H_

#include "components/mus/gpu/display_compositor/compositor_frame_sink_delegate.h"
#include "components/mus/public/interfaces/gpu/display_compositor_host.mojom.h"
#include "components/mus/surfaces/surfaces_state.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace mus {
namespace gpu {

class DisplayImpl : public mojom::Display, public CompositorFrameSinkDelegate {
 public:
  DisplayImpl(int accelerated_widget,
              mojo::InterfaceRequest<mojom::Display> display,
              mojom::DisplayHostPtr host,
              mojo::InterfaceRequest<mojom::CompositorFrameSink> sink,
              mojom::CompositorFrameSinkClientPtr client,
              const scoped_refptr<SurfacesState>& surfaces_state);
  ~DisplayImpl() override;

  // mojom::Display implementation.
  void CreateClient(
      uint32_t client_id,
      mojo::InterfaceRequest<mojom::DisplayClient> client) override;

 private:
  // CompositorFrameSinkDelegate implementation:
  void CompositorFrameSinkConnectionLost(int sink_id) override;
  cc::SurfaceId GenerateSurfaceId() override;

  std::unique_ptr<CompositorFrameSinkImpl> sink_;
  mojo::Binding<mojom::Display> binding_;
  DISALLOW_COPY_AND_ASSIGN(DisplayImpl);
};

}  // namespace gpu
}  // namespace mus

#endif  // COMPONENTS_MUS_GPU_DISPLAY_COMPOSITOR_DISPLAY_IMPL_H_
