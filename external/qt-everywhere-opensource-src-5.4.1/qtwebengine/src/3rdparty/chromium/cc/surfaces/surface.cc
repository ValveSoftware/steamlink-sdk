// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface.h"

#include "cc/output/compositor_frame.h"
#include "cc/surfaces/surface_manager.h"

namespace cc {

Surface::Surface(SurfaceManager* manager,
                 SurfaceClient* client,
                 const gfx::Size& size)
    : manager_(manager),
      client_(client),
      size_(size) {
  surface_id_ = manager_->RegisterAndAllocateIdForSurface(this);
}

Surface::~Surface() {
  manager_->DeregisterSurface(surface_id_);
}

void Surface::QueueFrame(scoped_ptr<CompositorFrame> frame) {
  scoped_ptr<CompositorFrame> previous_frame = current_frame_.Pass();
  current_frame_ = frame.Pass();
  ReceiveResourcesFromClient(
      current_frame_->delegated_frame_data->resource_list);
  if (previous_frame) {
    ReturnedResourceArray previous_resources;
    TransferableResource::ReturnResources(
        previous_frame->delegated_frame_data->resource_list,
        &previous_resources);
    UnrefResources(previous_resources);
  }
}

CompositorFrame* Surface::GetEligibleFrame() { return current_frame_.get(); }

void Surface::ReturnUnusedResourcesToClient() {
  client_->ReturnResources(resources_available_to_return_);
  resources_available_to_return_.clear();
}

void Surface::RefCurrentFrameResources() {
  if (!current_frame_)
    return;
  const TransferableResourceArray& current_frame_resources =
      current_frame_->delegated_frame_data->resource_list;
  for (size_t i = 0; i < current_frame_resources.size(); ++i) {
    const TransferableResource& resource = current_frame_resources[i];
    ResourceIdCountMap::iterator it =
        resource_id_use_count_map_.find(resource.id);
    DCHECK(it != resource_id_use_count_map_.end());
    it->second.refs_holding_resource_alive++;
  }
}

Surface::ResourceRefs::ResourceRefs()
    : refs_received_from_child(0), refs_holding_resource_alive(0) {
}

void Surface::ReceiveResourcesFromClient(
    const TransferableResourceArray& resources) {
  for (TransferableResourceArray::const_iterator it = resources.begin();
       it != resources.end();
       ++it) {
    ResourceRefs& ref = resource_id_use_count_map_[it->id];
    ref.refs_holding_resource_alive++;
    ref.refs_received_from_child++;
  }
}

void Surface::UnrefResources(const ReturnedResourceArray& resources) {
  for (ReturnedResourceArray::const_iterator it = resources.begin();
       it != resources.end();
       ++it) {
    ResourceProvider::ResourceId id = it->id;
    ResourceIdCountMap::iterator count_it = resource_id_use_count_map_.find(id);
    DCHECK(count_it != resource_id_use_count_map_.end());
    count_it->second.refs_holding_resource_alive -= it->count;
    if (count_it->second.refs_holding_resource_alive == 0) {
      resources_available_to_return_.push_back(*it);
      resources_available_to_return_.back().count =
          count_it->second.refs_received_from_child;
      resource_id_use_count_map_.erase(count_it);
    }
  }
}

}  // namespace cc
