// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_H_
#define CC_SURFACES_SURFACE_H_

#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/return_callback.h"
#include "cc/surfaces/surface_client.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surfaces_export.h"
#include "ui/gfx/size.h"

namespace cc {
class CompositorFrame;
class SurfaceManager;

class CC_SURFACES_EXPORT Surface {
 public:
  Surface(SurfaceManager* manager,
          SurfaceClient* client,
          const gfx::Size& size);
  ~Surface();

  const gfx::Size& size() const { return size_; }
  SurfaceId surface_id() const { return surface_id_; }

  void QueueFrame(scoped_ptr<CompositorFrame> frame);
  // Returns the most recent frame that is eligible to be rendered.
  CompositorFrame* GetEligibleFrame();

  // Takes a reference to all of the current frame's resources for an external
  // consumer (e.g. a ResourceProvider). The caller to this should call
  // UnrefResources() when they are done with the resources.
  void RefCurrentFrameResources();
  void UnrefResources(const ReturnedResourceArray& resources);

  // Returns all resources that are currently not in use to the client.
  void ReturnUnusedResourcesToClient();

 private:
  void ReceiveResourcesFromClient(const TransferableResourceArray& resources);

  SurfaceManager* manager_;
  SurfaceClient* client_;
  gfx::Size size_;
  SurfaceId surface_id_;
  // TODO(jamesr): Support multiple frames in flight.
  scoped_ptr<CompositorFrame> current_frame_;

  struct ResourceRefs {
    ResourceRefs();

    int refs_received_from_child;
    int refs_holding_resource_alive;
  };
  // Keeps track of the number of users currently in flight for each resource
  // ID we've received from the client. When this counter hits zero for a
  // particular resource, that ID is available to return to the client.
  typedef base::hash_map<ResourceProvider::ResourceId, ResourceRefs>
      ResourceIdCountMap;
  ResourceIdCountMap resource_id_use_count_map_;

  ReturnedResourceArray resources_available_to_return_;

  DISALLOW_COPY_AND_ASSIGN(Surface);
};

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_H_
