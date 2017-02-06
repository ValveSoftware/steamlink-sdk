// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_SURFACE_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_SURFACE_IMPL_H_

#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_factory_client.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/string.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "third_party/WebKit/public/platform/modules/offscreencanvas/offscreen_canvas_surface.mojom.h"

namespace content {

class OffscreenCanvasSurfaceImpl : public blink::mojom::OffscreenCanvasSurface,
                                   public cc::SurfaceFactoryClient {
 public:
  static void Create(
      mojo::InterfaceRequest<blink::mojom::OffscreenCanvasSurface> request);

  // blink::mojom::OffscreenCanvasSurface implementation.
  void GetSurfaceId(const GetSurfaceIdCallback& callback) override;
  void RequestSurfaceCreation(const cc::SurfaceId& surface_id) override;
  void Require(const cc::SurfaceId& surface_id,
               const cc::SurfaceSequence& sequence) override;
  void Satisfy(const cc::SurfaceSequence& sequence) override;

  // cc::SurfaceFactoryClient implementation.
  void ReturnResources(const cc::ReturnedResourceArray& resources) override;
  void WillDrawSurface(const cc::SurfaceId& id,
                       const gfx::Rect& damage_rect) override;
  void SetBeginFrameSource(cc::BeginFrameSource* begin_frame_source) override;

 private:
  ~OffscreenCanvasSurfaceImpl() override;
  explicit OffscreenCanvasSurfaceImpl(
      mojo::InterfaceRequest<blink::mojom::OffscreenCanvasSurface> request);

  // Surface-related state
  std::unique_ptr<cc::SurfaceIdAllocator> id_allocator_;
  cc::SurfaceId surface_id_;
  std::unique_ptr<cc::SurfaceFactory> surface_factory_;

  mojo::StrongBinding<blink::mojom::OffscreenCanvasSurface> binding_;

  DISALLOW_COPY_AND_ASSIGN(OffscreenCanvasSurfaceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_SURFACE_IMPL_H_
