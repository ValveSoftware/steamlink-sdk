// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/offscreen_canvas_surface_impl.h"

#include "base/bind_helpers.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/renderer_host/offscreen_canvas_surface_manager.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {

OffscreenCanvasSurfaceImpl::OffscreenCanvasSurfaceImpl()
    : id_allocator_(new cc::SurfaceIdAllocator()) {}

OffscreenCanvasSurfaceImpl::~OffscreenCanvasSurfaceImpl() {
  if (frame_sink_id_.is_valid()) {
    OffscreenCanvasSurfaceManager::GetInstance()
        ->UnregisterOffscreenCanvasSurfaceInstance(frame_sink_id_);
  }
}

// static
void OffscreenCanvasSurfaceImpl::Create(
    mojo::InterfaceRequest<blink::mojom::OffscreenCanvasSurface> request) {
  mojo::MakeStrongBinding(base::MakeUnique<OffscreenCanvasSurfaceImpl>(),
                          std::move(request));
}

void OffscreenCanvasSurfaceImpl::GetSurfaceId(
    const GetSurfaceIdCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (frame_sink_id_.is_valid()) {
    // This IPC should be only called once for each HTMLCanvasElement. In this
    // case, frame_sink_id_ is still unset.
    // As the browser makes no assumption of correct behavior of renderer, in
    // an unwanted situation when this function is invoked twice, we need to
    // unregister the instance from manager.
    OffscreenCanvasSurfaceManager::GetInstance()
        ->UnregisterOffscreenCanvasSurfaceInstance(frame_sink_id_);
    mojo::ReportBadMessage(
        "The same OffscreenCanvasSurfaceImpl is registered to "
        "OffscreenCanvasSurfaceManager twice.");
  }

  frame_sink_id_ = AllocateFrameSinkId();
  cc::SurfaceId surface_id =
      cc::SurfaceId(frame_sink_id_, id_allocator_->GenerateId());

  OffscreenCanvasSurfaceManager::GetInstance()
      ->RegisterOffscreenCanvasSurfaceInstance(frame_sink_id_, this);

  callback.Run(surface_id);
}

void OffscreenCanvasSurfaceImpl::Require(const cc::SurfaceId& surface_id,
                                         const cc::SurfaceSequence& sequence) {
  cc::SurfaceManager* manager = GetSurfaceManager();
  cc::Surface* surface = manager->GetSurfaceForId(surface_id);
  if (!surface) {
    DLOG(ERROR) << "Attempting to require callback on nonexistent surface";
    return;
  }
  surface->AddDestructionDependency(sequence);
}

void OffscreenCanvasSurfaceImpl::Satisfy(const cc::SurfaceSequence& sequence) {
  std::vector<uint32_t> sequences;
  sequences.push_back(sequence.sequence);
  cc::SurfaceManager* manager = GetSurfaceManager();
  manager->DidSatisfySequences(sequence.frame_sink_id, &sequences);
}

}  // namespace content
