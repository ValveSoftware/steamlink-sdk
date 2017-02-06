// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_GPU_DISPLAY_COMPOSITOR_COMPOSITOR_FRAME_SINK_FACTORY_IMPL_H_
#define COMPONENTS_MUS_GPU_DISPLAY_COMPOSITOR_COMPOSITOR_FRAME_SINK_FACTORY_IMPL_H_

#include "cc/surfaces/surface_id_allocator.h"
#include "components/mus/gpu/display_compositor/compositor_frame_sink_delegate.h"
#include "components/mus/public/interfaces/gpu/display_compositor.mojom.h"
#include "components/mus/surfaces/surfaces_state.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace mus {
namespace gpu {

class CompositorFrameSinkImpl;

class CompositorFrameSinkFactoryImpl : public mojom::CompositorFrameSinkFactory,
                                       public CompositorFrameSinkDelegate {
 public:
  CompositorFrameSinkFactoryImpl(
      uint32_t client_id,
      mojo::InterfaceRequest<mojom::CompositorFrameSinkFactory> request,
      const scoped_refptr<SurfacesState>& surfaces_state);
  ~CompositorFrameSinkFactoryImpl() override;

  uint32_t client_id() const { return client_id_; }

  void CompositorFrameSinkConnectionLost(int local_id) override;
  cc::SurfaceId GenerateSurfaceId() override;

  // mojom::CompositorFrameSinkFactory implementation.
  void CreateCompositorFrameSink(
      uint32_t local_id,
      uint64_t nonce,
      mojo::InterfaceRequest<mojom::CompositorFrameSink> sink,
      mojom::CompositorFrameSinkClientPtr client) override;

 private:
  const uint32_t client_id_;
  scoped_refptr<SurfacesState> surfaces_state_;
  cc::SurfaceIdAllocator allocator_;
  using CompositorFrameSinkMap =
      std::map<uint32_t, std::unique_ptr<CompositorFrameSinkImpl>>;
  CompositorFrameSinkMap sinks_;
  mojo::StrongBinding<mojom::CompositorFrameSinkFactory> binding_;

  DISALLOW_COPY_AND_ASSIGN(CompositorFrameSinkFactoryImpl);
};

}  // namespace gpu
}  // namespace mus

#endif  // COMPONENTS_MUS_GPU_DISPLAY_COMPOSITOR_COMPOSITOR_FRAME_SINK_FACTORY_IMPL_H_
