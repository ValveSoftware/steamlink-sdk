// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/gpu/display_compositor/display_impl.h"

#include "components/mus/gpu/display_compositor/compositor_frame_sink_impl.h"

namespace mus {
namespace gpu {

DisplayImpl::DisplayImpl(
    int accelerated_widget,
    mojo::InterfaceRequest<mojom::Display> display,
    mojom::DisplayHostPtr host,
    mojo::InterfaceRequest<mojom::CompositorFrameSink> sink,
    mojom::CompositorFrameSinkClientPtr client,
    const scoped_refptr<SurfacesState>& surfaces_state)
    : binding_(this, std::move(display)) {
  const uint32_t client_id = 1;
  sink_.reset(new CompositorFrameSinkImpl(this, client_id, surfaces_state,
                                          std::move(sink), std::move(client)));
}

DisplayImpl::~DisplayImpl() {}

void DisplayImpl::CreateClient(
    uint32_t client_id,
    mojo::InterfaceRequest<mojom::DisplayClient> client) {
  NOTIMPLEMENTED();
}

void DisplayImpl::CompositorFrameSinkConnectionLost(int sink_id) {
  NOTIMPLEMENTED();
}

cc::SurfaceId DisplayImpl::GenerateSurfaceId() {
  NOTIMPLEMENTED();
  return cc::SurfaceId();
}

}  // namespace gpu
}  // namespace mus
