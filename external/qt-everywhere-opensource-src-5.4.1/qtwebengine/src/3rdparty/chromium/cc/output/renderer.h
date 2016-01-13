// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_RENDERER_H_
#define CC_OUTPUT_RENDERER_H_

#include "base/basictypes.h"
#include "cc/base/cc_export.h"
#include "cc/quads/render_pass.h"
#include "cc/trees/layer_tree_host.h"

namespace cc {

class CompositorFrameAck;
class CompositorFrameMetadata;
class ScopedResource;
class Task;

struct RendererCapabilitiesImpl {
  RendererCapabilitiesImpl();
  ~RendererCapabilitiesImpl();

  // Capabilities copied to main thread.
  ResourceFormat best_texture_format;
  bool allow_partial_texture_updates;
  int max_texture_size;
  bool using_shared_memory_resources;

  // Capabilities used on compositor thread only.
  bool using_partial_swap;
  bool using_egl_image;
  bool avoid_pow2_textures;
  bool using_map_image;
  bool using_discard_framebuffer;
  bool allow_rasterize_on_demand;

  RendererCapabilities MainThreadCapabilities() const;
};

class CC_EXPORT RendererClient {
 public:
  virtual void SetFullRootLayerDamage() = 0;
  virtual void RunOnDemandRasterTask(Task* on_demand_raster_task) = 0;
};

class CC_EXPORT Renderer {
 public:
  virtual ~Renderer() {}

  virtual const RendererCapabilitiesImpl& Capabilities() const = 0;

  virtual void DecideRenderPassAllocationsForFrame(
      const RenderPassList& render_passes_in_draw_order) {}
  virtual bool HasAllocatedResourcesForTesting(RenderPass::Id id) const;

  // This passes ownership of the render passes to the renderer. It should
  // consume them, and empty the list. The parameters here may change from frame
  // to frame and should not be cached.
  // The |device_viewport_rect| and |device_clip_rect| are in non-y-flipped
  // window space.
  virtual void DrawFrame(RenderPassList* render_passes_in_draw_order,
                         float device_scale_factor,
                         const gfx::Rect& device_viewport_rect,
                         const gfx::Rect& device_clip_rect,
                         bool disable_picture_quad_image_filtering) = 0;

  // Waits for rendering to finish.
  virtual void Finish() = 0;

  virtual void DoNoOp() {}

  // Puts backbuffer onscreen.
  virtual void SwapBuffers(const CompositorFrameMetadata& metadata) = 0;
  virtual void ReceiveSwapBuffersAck(const CompositorFrameAck& ack) {}

  virtual bool IsContextLost();

  bool visible() const { return visible_; }
  void SetVisible(bool visible);

 protected:
  explicit Renderer(RendererClient* client, const LayerTreeSettings* settings)
      : client_(client), settings_(settings), visible_(true) {}

  virtual void DidChangeVisibility() = 0;

  RendererClient* client_;
  const LayerTreeSettings* settings_;
  bool visible_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Renderer);
};

}  // namespace cc

#endif  // CC_OUTPUT_RENDERER_H_
