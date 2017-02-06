// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_DELEGATING_RENDERER_H_
#define CC_OUTPUT_DELEGATING_RENDERER_H_

#include <memory>

#include "base/macros.h"
#include "cc/base/cc_export.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/renderer.h"

namespace cc {

class OutputSurface;
class ResourceProvider;

class CC_EXPORT DelegatingRenderer : public Renderer {
 public:
  static std::unique_ptr<DelegatingRenderer> Create(
      RendererClient* client,
      const RendererSettings* settings,
      OutputSurface* output_surface,
      ResourceProvider* resource_provider);
  ~DelegatingRenderer() override;

  const RendererCapabilitiesImpl& Capabilities() const override;

  void DrawFrame(RenderPassList* render_passes_in_draw_order,
                 float device_scale_factor,
                 const gfx::ColorSpace& device_color_space,
                 const gfx::Rect& device_viewport_rect,
                 const gfx::Rect& device_clip_rect,
                 bool disable_picture_quad_image_filtering) override;

  void Finish() override {}

  void SwapBuffers(CompositorFrameMetadata metadata) override;
  void ReceiveSwapBuffersAck(const CompositorFrameAck&) override;

 private:
  DelegatingRenderer(RendererClient* client,
                     const RendererSettings* settings,
                     OutputSurface* output_surface,
                     ResourceProvider* resource_provider);

  void DidChangeVisibility() override;

  OutputSurface* output_surface_;
  ResourceProvider* resource_provider_;
  RendererCapabilitiesImpl capabilities_;
  std::unique_ptr<DelegatedFrameData> delegated_frame_data_;

  DISALLOW_COPY_AND_ASSIGN(DelegatingRenderer);
};

}  // namespace cc

#endif  // CC_OUTPUT_DELEGATING_RENDERER_H_
