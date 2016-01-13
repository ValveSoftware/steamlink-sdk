// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_DISPLAY_H_
#define CC_SURFACES_DISPLAY_H_

#include "base/memory/scoped_ptr.h"

#include "cc/output/output_surface_client.h"
#include "cc/output/renderer.h"
#include "cc/surfaces/surface_aggregator.h"
#include "cc/surfaces/surface_client.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surfaces_export.h"

namespace gfx {
class Size;
}

namespace cc {

class DirectRenderer;
class DisplayClient;
class OutputSurface;
class ResourceProvider;
class SharedBitmapManager;
class Surface;
class SurfaceManager;

class CC_SURFACES_EXPORT Display : public SurfaceClient,
                                   public OutputSurfaceClient,
                                   public RendererClient {
 public:
  Display(DisplayClient* client,
          SurfaceManager* manager,
          SharedBitmapManager* bitmap_manager);
  virtual ~Display();

  void Resize(const gfx::Size& new_size);
  bool Draw();

  SurfaceId CurrentSurfaceId();

  // OutputSurfaceClient implementation.
  virtual void DeferredInitialize() OVERRIDE {}
  virtual void ReleaseGL() OVERRIDE {}
  virtual void CommitVSyncParameters(base::TimeTicks timebase,
                                     base::TimeDelta interval) OVERRIDE {}
  virtual void SetNeedsRedrawRect(const gfx::Rect& damage_rect) OVERRIDE {}
  virtual void BeginFrame(const BeginFrameArgs& args) OVERRIDE {}
  virtual void DidSwapBuffers() OVERRIDE {}
  virtual void DidSwapBuffersComplete() OVERRIDE {}
  virtual void ReclaimResources(const CompositorFrameAck* ack) OVERRIDE {}
  virtual void DidLoseOutputSurface() OVERRIDE {}
  virtual void SetExternalDrawConstraints(
      const gfx::Transform& transform,
      const gfx::Rect& viewport,
      const gfx::Rect& clip,
      bool valid_for_tile_management) OVERRIDE {}
  virtual void SetMemoryPolicy(const ManagedMemoryPolicy& policy) OVERRIDE {}
  virtual void SetTreeActivationCallback(
      const base::Closure& callback) OVERRIDE {}

  // RendererClient implementation.
  virtual void SetFullRootLayerDamage() OVERRIDE {}
  virtual void RunOnDemandRasterTask(Task* on_demand_raster_task) OVERRIDE {}

  // SurfaceClient implementation.
  virtual void ReturnResources(const ReturnedResourceArray& resources) OVERRIDE;

 private:
  void InitializeOutputSurface();

  DisplayClient* client_;
  SurfaceManager* manager_;
  SurfaceAggregator aggregator_;
  SharedBitmapManager* bitmap_manager_;
  LayerTreeSettings settings_;
  scoped_ptr<Surface> current_surface_;
  scoped_ptr<OutputSurface> output_surface_;
  scoped_ptr<ResourceProvider> resource_provider_;
  scoped_ptr<DirectRenderer> renderer_;
  int child_id_;

  DISALLOW_COPY_AND_ASSIGN(Display);
};

}  // namespace cc

#endif  // CC_SURFACES_DISPLAY_H_
