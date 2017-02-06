// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/offscreen_canvas_surface_impl.h"

#include "base/bind_helpers.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/public/browser/browser_thread.h"

namespace content {

// static
void OffscreenCanvasSurfaceImpl::Create(
    mojo::InterfaceRequest<blink::mojom::OffscreenCanvasSurface> request) {
  // |binding_| will take ownership of OffscreenCanvasSurfaceImpl
  new OffscreenCanvasSurfaceImpl(std::move(request));
}

OffscreenCanvasSurfaceImpl::OffscreenCanvasSurfaceImpl(
    mojo::InterfaceRequest<blink::mojom::OffscreenCanvasSurface> request)
    : id_allocator_(CreateSurfaceIdAllocator()),
      binding_(this, std::move(request)) {}

OffscreenCanvasSurfaceImpl::~OffscreenCanvasSurfaceImpl() {
  if (!GetSurfaceManager()) {
    // Inform both members that SurfaceManager's no longer alive to
    // avoid their destruction errors.
    if (surface_factory_)
        surface_factory_->DidDestroySurfaceManager();
    if (id_allocator_)
        id_allocator_->DidDestroySurfaceManager();
  }
  surface_factory_->Destroy(surface_id_);
}

void OffscreenCanvasSurfaceImpl::GetSurfaceId(
    const GetSurfaceIdCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  surface_id_ = id_allocator_->GenerateId();

  callback.Run(surface_id_);
}

void OffscreenCanvasSurfaceImpl::RequestSurfaceCreation(
    const cc::SurfaceId& surface_id) {
  cc::SurfaceManager* manager = GetSurfaceManager();
  if (!surface_factory_) {
    surface_factory_ = base::MakeUnique<cc::SurfaceFactory>(manager, this);
  }
  surface_factory_->Create(surface_id);
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
  manager->DidSatisfySequences(sequence.id_namespace, &sequences);
}

// TODO(619136): Implement cc::SurfaceFactoryClient functions for resources
// return.
void OffscreenCanvasSurfaceImpl::ReturnResources(
    const cc::ReturnedResourceArray& resources) {}

void OffscreenCanvasSurfaceImpl::WillDrawSurface(const cc::SurfaceId& id,
                                                 const gfx::Rect& damage_rect) {
}

void OffscreenCanvasSurfaceImpl::SetBeginFrameSource(
    cc::BeginFrameSource* begin_frame_source) {}

}  // namespace content
