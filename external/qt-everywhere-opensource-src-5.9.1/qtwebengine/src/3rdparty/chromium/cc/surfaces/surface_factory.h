// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_FACTORY_H_
#define CC_SURFACES_SURFACE_FACTORY_H_

#include <memory>
#include <set>
#include <unordered_map>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "cc/output/compositor_frame.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_resource_holder.h"
#include "cc/surfaces/surface_sequence.h"
#include "cc/surfaces/surfaces_export.h"

namespace cc {
class CopyOutputRequest;
class Surface;
class SurfaceFactoryClient;
class SurfaceManager;

// A SurfaceFactory is used to create surfaces that may share resources and
// receive returned resources for frames submitted to those surfaces. Resources
// submitted to frames created by a particular factory will be returned to that
// factory's client when they are no longer being used. This is the only class
// most users of surfaces will need to directly interact with.
class CC_SURFACES_EXPORT SurfaceFactory {
 public:
  using DrawCallback = base::Callback<void()>;

  SurfaceFactory(const FrameSinkId& frame_sink_id,
                 SurfaceManager* manager,
                 SurfaceFactoryClient* client);
  ~SurfaceFactory();

  const FrameSinkId& frame_sink_id() const { return frame_sink_id_; }

  void Create(const LocalFrameId& local_frame_id);
  void Destroy(const LocalFrameId& local_frame_id);

  // Destroys all surfaces.
  void DestroyAll();

  // Destroys and disown all surfaces, and reset all resource references. This
  // is useful when resources are invalid (e.g. lost context).
  void Reset();

  // Set that the current frame on new_id is to be treated as the successor to
  // the current frame on old_id for the purposes of calculating damage.
  void SetPreviousFrameSurface(const LocalFrameId& new_id,
                               const LocalFrameId& old_id);

  // A frame can only be submitted to a surface created by this factory,
  // although the frame may reference surfaces created by other factories.
  // The callback is called the first time this frame is used to draw, or if
  // the frame is discarded.
  void SubmitCompositorFrame(const LocalFrameId& local_frame_id,
                             CompositorFrame frame,
                             const DrawCallback& callback);
  void RequestCopyOfSurface(const LocalFrameId& local_frame_id,
                            std::unique_ptr<CopyOutputRequest> copy_request);

  void WillDrawSurface(const LocalFrameId& id, const gfx::Rect& damage_rect);

  SurfaceFactoryClient* client() { return client_; }

  void ReceiveFromChild(const TransferableResourceArray& resources);
  void RefResources(const TransferableResourceArray& resources);
  void UnrefResources(const ReturnedResourceArray& resources);

  SurfaceManager* manager() { return manager_; }

  // This can be set to false if resources from this SurfaceFactory don't need
  // to have sync points set on them when returned from the Display, for
  // example if the Display shares a context with the creator.
  bool needs_sync_points() const { return needs_sync_points_; }
  void set_needs_sync_points(bool needs) { needs_sync_points_ = needs; }

  // SurfaceFactory's owner can call this when it finds out that SurfaceManager
  // is no longer alive during destruction.
  void DidDestroySurfaceManager() { manager_ = nullptr; }

 private:
  FrameSinkId frame_sink_id_;
  SurfaceManager* manager_;
  SurfaceFactoryClient* client_;
  SurfaceResourceHolder holder_;

  bool needs_sync_points_;

  using OwningSurfaceMap = std::
      unordered_map<LocalFrameId, std::unique_ptr<Surface>, LocalFrameIdHash>;
  OwningSurfaceMap surface_map_;

  base::WeakPtrFactory<SurfaceFactory> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceFactory);
};

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_FACTORY_H_
