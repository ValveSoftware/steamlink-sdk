// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/surfaces/display_compositor.h"

#include "cc/surfaces/surface.h"

namespace ui {

DisplayCompositor::DisplayCompositor(
    cc::mojom::DisplayCompositorClientPtr client)
    : client_(std::move(client)) {
  manager_.AddObserver(this);
}

void DisplayCompositor::AddSurfaceReference(
    const cc::SurfaceId& surface_id,
    const cc::SurfaceSequence& surface_sequence) {
  cc::Surface* surface = manager_.GetSurfaceForId(surface_id);
  if (!surface) {
    LOG(ERROR) << "Attempting to add dependency to nonexistent surface "
               << surface_id.ToString();
    return;
  }
  surface->AddDestructionDependency(surface_sequence);
}

void DisplayCompositor::ReturnSurfaceReferences(
    const cc::FrameSinkId& frame_sink_id,
    const std::vector<uint32_t>& sequences) {
  std::vector<uint32_t> sequences_copy(sequences);
  manager_.DidSatisfySequences(frame_sink_id, &sequences_copy);
}

DisplayCompositor::~DisplayCompositor() {
  manager_.RemoveObserver(this);
}

void DisplayCompositor::OnSurfaceCreated(const cc::SurfaceId& surface_id,
                                         const gfx::Size& frame_size,
                                         float device_scale_factor) {
  if (client_)
    client_->OnSurfaceCreated(surface_id, frame_size, device_scale_factor);
}

void DisplayCompositor::OnSurfaceDamaged(const cc::SurfaceId& surface_id,
                                         bool* changed) {}

}  // namespace ui
