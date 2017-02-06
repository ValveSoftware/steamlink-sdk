// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/gpu/display_compositor/compositor_frame_sink_factory_impl.h"

#include "base/memory/ptr_util.h"
#include "cc/surfaces/surface_id.h"
#include "components/mus/gpu/display_compositor/compositor_frame_sink_impl.h"

namespace mus {
namespace gpu {

CompositorFrameSinkFactoryImpl::CompositorFrameSinkFactoryImpl(
    uint32_t client_id,
    mojo::InterfaceRequest<mojom::CompositorFrameSinkFactory> request,
    const scoped_refptr<SurfacesState>& surfaces_state)
    : client_id_(client_id),
      surfaces_state_(surfaces_state),
      allocator_(client_id),
      binding_(this, std::move(request)) {}

CompositorFrameSinkFactoryImpl::~CompositorFrameSinkFactoryImpl() {}

void CompositorFrameSinkFactoryImpl::CompositorFrameSinkConnectionLost(
    int local_id) {
  sinks_.erase(local_id);
}

cc::SurfaceId CompositorFrameSinkFactoryImpl::GenerateSurfaceId() {
  return allocator_.GenerateId();
}

void CompositorFrameSinkFactoryImpl::CreateCompositorFrameSink(
    uint32_t local_id,
    uint64_t nonce,
    mojo::InterfaceRequest<mojom::CompositorFrameSink> sink,
    mojom::CompositorFrameSinkClientPtr client) {
  // TODO(fsamuel): Use nonce once patch lands:
  // https://codereview.chromium.org/1996783002/
  sinks_[local_id] = base::WrapUnique(new CompositorFrameSinkImpl(
      this, local_id, surfaces_state_, std::move(sink), std::move(client)));
}

}  // namespace gpu
}  // namespace mus
