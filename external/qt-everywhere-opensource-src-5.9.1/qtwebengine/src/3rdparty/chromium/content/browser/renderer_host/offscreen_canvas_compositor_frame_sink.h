// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_COMPOSITOR_FRAME_SINK_H_
#define CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_COMPOSITOR_FRAME_SINK_H_

#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_factory_client.h"
#include "third_party/WebKit/public/platform/modules/offscreencanvas/offscreen_canvas_surface.mojom.h"

namespace content {

class OffscreenCanvasCompositorFrameSink
    : public cc::mojom::MojoCompositorFrameSink,
      public cc::SurfaceFactoryClient {
 public:
  OffscreenCanvasCompositorFrameSink(
      const cc::SurfaceId& surface_id,
      cc::mojom::MojoCompositorFrameSinkClientPtr client);
  ~OffscreenCanvasCompositorFrameSink() override;

  static void Create(const cc::SurfaceId& surface_id,
                     cc::mojom::MojoCompositorFrameSinkClientPtr client,
                     cc::mojom::MojoCompositorFrameSinkRequest request);

  // cc::mojom::MojoCompositorFrameSink implementation.
  void SubmitCompositorFrame(cc::CompositorFrame frame) override;
  void SetNeedsBeginFrame(bool needs_begin_frame) override;

  // cc::SurfaceFactoryClient implementation.
  void ReturnResources(const cc::ReturnedResourceArray& resources) override;
  void WillDrawSurface(const cc::LocalFrameId& id,
                       const gfx::Rect& damage_rect) override;
  void SetBeginFrameSource(cc::BeginFrameSource* begin_frame_source) override;

 private:
  void DidReceiveCompositorFrameAck();

  cc::SurfaceId surface_id_;
  std::unique_ptr<cc::SurfaceFactory> surface_factory_;
  cc::mojom::MojoCompositorFrameSinkClientPtr client_;
  int ack_pending_count_ = 0;
  cc::ReturnedResourceArray surface_returned_resources_;

  DISALLOW_COPY_AND_ASSIGN(OffscreenCanvasCompositorFrameSink);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_COMPOSITOR_FRAME_SINK_H_
