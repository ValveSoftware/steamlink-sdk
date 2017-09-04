// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/offscreen_canvas_compositor_frame_sink_provider_impl.h"

#include "content/browser/renderer_host/offscreen_canvas_compositor_frame_sink.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {

OffscreenCanvasCompositorFrameSinkProviderImpl::
    OffscreenCanvasCompositorFrameSinkProviderImpl() {}

OffscreenCanvasCompositorFrameSinkProviderImpl::
    ~OffscreenCanvasCompositorFrameSinkProviderImpl() {}

// static
void OffscreenCanvasCompositorFrameSinkProviderImpl::Create(
    blink::mojom::OffscreenCanvasCompositorFrameSinkProviderRequest request) {
  mojo::MakeStrongBinding(
      base::MakeUnique<OffscreenCanvasCompositorFrameSinkProviderImpl>(),
      std::move(request));
}

void OffscreenCanvasCompositorFrameSinkProviderImpl::CreateCompositorFrameSink(
    const cc::SurfaceId& surface_id,
    cc::mojom::MojoCompositorFrameSinkClientPtr client,
    cc::mojom::MojoCompositorFrameSinkRequest request) {
  OffscreenCanvasCompositorFrameSink::Create(surface_id, std::move(client),
                                             std::move(request));
}

}  // namespace content
