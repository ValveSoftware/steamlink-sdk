// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface_factory.h"

#include <utility>

#include "base/trace_event/trace_event.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/copy_output_request.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_factory_client.h"
#include "cc/surfaces/surface_manager.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
SurfaceFactory::SurfaceFactory(SurfaceManager* manager,
                               SurfaceFactoryClient* client)
    : manager_(manager),
      client_(client),
      holder_(client),
      needs_sync_points_(true) {
}

SurfaceFactory::~SurfaceFactory() {
  if (!surface_map_.empty()) {
    LOG(ERROR) << "SurfaceFactory has " << surface_map_.size()
               << " entries in map on destruction.";
  }
  DestroyAll();
}

void SurfaceFactory::DestroyAll() {
  if (manager_) {
    for (auto& pair : surface_map_)
      manager_->Destroy(std::move(pair.second));
  }
  surface_map_.clear();
}

void SurfaceFactory::Create(SurfaceId surface_id) {
  std::unique_ptr<Surface> surface(new Surface(surface_id, this));
  manager_->RegisterSurface(surface.get());
  DCHECK(!surface_map_.count(surface_id));
  surface_map_[surface_id] = std::move(surface);
}

void SurfaceFactory::Destroy(SurfaceId surface_id) {
  OwningSurfaceMap::iterator it = surface_map_.find(surface_id);
  DCHECK(it != surface_map_.end());
  DCHECK(it->second->factory().get() == this);
  std::unique_ptr<Surface> surface(std::move(it->second));
  surface_map_.erase(it);
  if (manager_)
    manager_->Destroy(std::move(surface));
}

void SurfaceFactory::SetPreviousFrameSurface(SurfaceId new_id,
                                             SurfaceId old_id) {
  OwningSurfaceMap::iterator it = surface_map_.find(new_id);
  DCHECK(it != surface_map_.end());
  Surface* old_surface = manager_->GetSurfaceForId(old_id);
  if (old_surface) {
    it->second->SetPreviousFrameSurface(old_surface);
  }
}

void SurfaceFactory::SubmitCompositorFrame(SurfaceId surface_id,
                                           CompositorFrame frame,
                                           const DrawCallback& callback) {
  TRACE_EVENT0("cc", "SurfaceFactory::SubmitCompositorFrame");
  OwningSurfaceMap::iterator it = surface_map_.find(surface_id);
  DCHECK(it != surface_map_.end());
  DCHECK(it->second->factory().get() == this);
  it->second->QueueFrame(std::move(frame), callback);
  if (!manager_->SurfaceModified(surface_id)) {
    TRACE_EVENT_INSTANT0("cc", "Damage not visible.", TRACE_EVENT_SCOPE_THREAD);
    it->second->RunDrawCallbacks(SurfaceDrawStatus::DRAW_SKIPPED);
  }
}

void SurfaceFactory::RequestCopyOfSurface(
    SurfaceId surface_id,
    std::unique_ptr<CopyOutputRequest> copy_request) {
  OwningSurfaceMap::iterator it = surface_map_.find(surface_id);
  if (it == surface_map_.end()) {
    copy_request->SendEmptyResult();
    return;
  }
  DCHECK(it->second->factory().get() == this);
  it->second->RequestCopyOfOutput(std::move(copy_request));
  manager_->SurfaceModified(surface_id);
}

void SurfaceFactory::WillDrawSurface(SurfaceId id,
                                     const gfx::Rect& damage_rect) {
  client_->WillDrawSurface(id, damage_rect);
}

void SurfaceFactory::ReceiveFromChild(
    const TransferableResourceArray& resources) {
  holder_.ReceiveFromChild(resources);
}

void SurfaceFactory::RefResources(const TransferableResourceArray& resources) {
  holder_.RefResources(resources);
}

void SurfaceFactory::UnrefResources(const ReturnedResourceArray& resources) {
  holder_.UnrefResources(resources);
}

}  // namespace cc
