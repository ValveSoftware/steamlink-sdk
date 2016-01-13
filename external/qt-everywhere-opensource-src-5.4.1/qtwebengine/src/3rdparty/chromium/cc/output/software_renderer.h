// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_SOFTWARE_RENDERER_H_
#define CC_OUTPUT_SOFTWARE_RENDERER_H_

#include "base/basictypes.h"
#include "cc/base/cc_export.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/direct_renderer.h"

namespace cc {

class OutputSurface;
class RendererClient;
class ResourceProvider;
class SoftwareOutputDevice;

class CheckerboardDrawQuad;
class DebugBorderDrawQuad;
class PictureDrawQuad;
class RenderPassDrawQuad;
class SolidColorDrawQuad;
class TextureDrawQuad;
class TileDrawQuad;

class CC_EXPORT SoftwareRenderer : public DirectRenderer {
 public:
  static scoped_ptr<SoftwareRenderer> Create(
      RendererClient* client,
      const LayerTreeSettings* settings,
      OutputSurface* output_surface,
      ResourceProvider* resource_provider);

  virtual ~SoftwareRenderer();
  virtual const RendererCapabilitiesImpl& Capabilities() const OVERRIDE;
  virtual void Finish() OVERRIDE;
  virtual void SwapBuffers(const CompositorFrameMetadata& metadata) OVERRIDE;
  virtual void ReceiveSwapBuffersAck(
      const CompositorFrameAck& ack) OVERRIDE;
  virtual void DiscardBackbuffer() OVERRIDE;
  virtual void EnsureBackbuffer() OVERRIDE;

 protected:
  virtual void BindFramebufferToOutputSurface(DrawingFrame* frame) OVERRIDE;
  virtual bool BindFramebufferToTexture(
      DrawingFrame* frame,
      const ScopedResource* texture,
      const gfx::Rect& target_rect) OVERRIDE;
  virtual void SetDrawViewport(const gfx::Rect& window_space_viewport) OVERRIDE;
  virtual void SetScissorTestRect(const gfx::Rect& scissor_rect) OVERRIDE;
  virtual void DiscardPixels(bool has_external_stencil_test,
                             bool draw_rect_covers_full_surface) OVERRIDE;
  virtual void ClearFramebuffer(DrawingFrame* frame,
                                bool has_external_stencil_test) OVERRIDE;
  virtual void DoDrawQuad(DrawingFrame* frame, const DrawQuad* quad) OVERRIDE;
  virtual void BeginDrawingFrame(DrawingFrame* frame) OVERRIDE;
  virtual void FinishDrawingFrame(DrawingFrame* frame) OVERRIDE;
  virtual bool FlippedFramebuffer() const OVERRIDE;
  virtual void EnsureScissorTestEnabled() OVERRIDE;
  virtual void EnsureScissorTestDisabled() OVERRIDE;
  virtual void CopyCurrentRenderPassToBitmap(
      DrawingFrame* frame,
      scoped_ptr<CopyOutputRequest> request) OVERRIDE;

  SoftwareRenderer(RendererClient* client,
                   const LayerTreeSettings* settings,
                   OutputSurface* output_surface,
                   ResourceProvider* resource_provider);

  virtual void DidChangeVisibility() OVERRIDE;

 private:
  void ClearCanvas(SkColor color);
  void SetClipRect(const gfx::Rect& rect);
  bool IsSoftwareResource(ResourceProvider::ResourceId resource_id) const;

  void DrawCheckerboardQuad(const DrawingFrame* frame,
                            const CheckerboardDrawQuad* quad);
  void DrawDebugBorderQuad(const DrawingFrame* frame,
                           const DebugBorderDrawQuad* quad);
  void DrawPictureQuad(const DrawingFrame* frame,
                       const PictureDrawQuad* quad);
  void DrawRenderPassQuad(const DrawingFrame* frame,
                          const RenderPassDrawQuad* quad);
  void DrawSolidColorQuad(const DrawingFrame* frame,
                          const SolidColorDrawQuad* quad);
  void DrawTextureQuad(const DrawingFrame* frame,
                       const TextureDrawQuad* quad);
  void DrawTileQuad(const DrawingFrame* frame,
                    const TileDrawQuad* quad);
  void DrawUnsupportedQuad(const DrawingFrame* frame,
                           const DrawQuad* quad);

  RendererCapabilitiesImpl capabilities_;
  bool is_scissor_enabled_;
  bool is_backbuffer_discarded_;
  gfx::Rect scissor_rect_;

  SoftwareOutputDevice* output_device_;
  SkCanvas* root_canvas_;
  SkCanvas* current_canvas_;
  SkPaint current_paint_;
  scoped_ptr<ResourceProvider::ScopedWriteLockSoftware>
      current_framebuffer_lock_;
  scoped_ptr<SoftwareFrameData> current_frame_data_;

  DISALLOW_COPY_AND_ASSIGN(SoftwareRenderer);
};

}  // namespace cc

#endif  // CC_OUTPUT_SOFTWARE_RENDERER_H_
