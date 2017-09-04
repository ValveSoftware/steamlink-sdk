// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_COMPOSITOR_FRAME_SINK_PROVIDER_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_COMPOSITOR_FRAME_SINK_PROVIDER_IMPL_H_

#include "third_party/WebKit/public/platform/modules/offscreencanvas/offscreen_canvas_surface.mojom.h"

namespace content {

class OffscreenCanvasCompositorFrameSinkProviderImpl
    : public blink::mojom::OffscreenCanvasCompositorFrameSinkProvider {
 public:
  OffscreenCanvasCompositorFrameSinkProviderImpl();
  ~OffscreenCanvasCompositorFrameSinkProviderImpl() override;

  static void Create(
      blink::mojom::OffscreenCanvasCompositorFrameSinkProviderRequest request);

  // blink::mojom::OffscreenCanvasCompositorFrameSinkProvider implementation.
  void CreateCompositorFrameSink(
      const cc::SurfaceId& surface_id,
      cc::mojom::MojoCompositorFrameSinkClientPtr client,
      cc::mojom::MojoCompositorFrameSinkRequest request) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(OffscreenCanvasCompositorFrameSinkProviderImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_COMPOSITOR_FRAME_SINK_PROVIDER_IMPL_H_
