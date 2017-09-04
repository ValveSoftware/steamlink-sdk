// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/offscreen_canvas_compositor_frame_sink.h"

#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {

OffscreenCanvasCompositorFrameSink::OffscreenCanvasCompositorFrameSink(
    const cc::SurfaceId& surface_id,
    cc::mojom::MojoCompositorFrameSinkClientPtr client)
    : surface_id_(surface_id), client_(std::move(client)) {
  cc::SurfaceManager* manager = GetSurfaceManager();
  surface_factory_ = base::MakeUnique<cc::SurfaceFactory>(
      surface_id_.frame_sink_id(), manager, this);
  manager->RegisterFrameSinkId(surface_id_.frame_sink_id());
  surface_factory_->Create(surface_id_.local_frame_id());
}

OffscreenCanvasCompositorFrameSink::~OffscreenCanvasCompositorFrameSink() {
  cc::SurfaceManager* manager = GetSurfaceManager();
  if (!manager) {
    // Inform SurfaceFactory that SurfaceManager's no longer alive to
    // avoid its destruction error.
    surface_factory_->DidDestroySurfaceManager();
  } else {
    manager->InvalidateFrameSinkId(surface_id_.frame_sink_id());
  }
  surface_factory_->Destroy(surface_id_.local_frame_id());
}

// static
void OffscreenCanvasCompositorFrameSink::Create(
    const cc::SurfaceId& surface_id,
    cc::mojom::MojoCompositorFrameSinkClientPtr client,
    cc::mojom::MojoCompositorFrameSinkRequest request) {
  mojo::MakeStrongBinding(base::MakeUnique<OffscreenCanvasCompositorFrameSink>(
                              surface_id, std::move(client)),
                          std::move(request));
}

void OffscreenCanvasCompositorFrameSink::SubmitCompositorFrame(
    cc::CompositorFrame frame) {
  ++ack_pending_count_;
  surface_factory_->SubmitCompositorFrame(
      surface_id_.local_frame_id(), std::move(frame),
      base::Bind(
          &OffscreenCanvasCompositorFrameSink::DidReceiveCompositorFrameAck,
          base::Unretained(this)));
}

void OffscreenCanvasCompositorFrameSink::SetNeedsBeginFrame(
    bool needs_begin_frame) {
  NOTIMPLEMENTED();
}

void OffscreenCanvasCompositorFrameSink::ReturnResources(
    const cc::ReturnedResourceArray& resources) {
  if (resources.empty())
    return;

  if (!ack_pending_count_ && client_) {
    client_->ReclaimResources(resources);
    return;
  }

  std::copy(resources.begin(), resources.end(),
            std::back_inserter(surface_returned_resources_));
}

void OffscreenCanvasCompositorFrameSink::WillDrawSurface(
    const cc::LocalFrameId& id,
    const gfx::Rect& damage_rect) {}

void OffscreenCanvasCompositorFrameSink::SetBeginFrameSource(
    cc::BeginFrameSource* begin_frame_source) {}

void OffscreenCanvasCompositorFrameSink::DidReceiveCompositorFrameAck() {
  if (!client_)
    return;
  client_->DidReceiveCompositorFrameAck();
  DCHECK_GT(ack_pending_count_, 0);
  if (!surface_returned_resources_.empty()) {
    client_->ReclaimResources(surface_returned_resources_);
    surface_returned_resources_.clear();
  }
  ack_pending_count_--;
}

}  // namespace content
