// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_SOFTWARE_RENDERER_H_
#define CC_OUTPUT_SOFTWARE_RENDERER_H_

#include "base/macros.h"
#include "cc/base/cc_export.h"
#include "cc/output/direct_renderer.h"
#include "ui/events/latency_info.h"

namespace cc {
class DebugBorderDrawQuad;
class OutputSurface;
class PictureDrawQuad;
class RenderPassDrawQuad;
class ResourceProvider;
class SoftwareOutputDevice;
class SolidColorDrawQuad;
class TextureDrawQuad;
class TileDrawQuad;

class CC_EXPORT SoftwareRenderer : public DirectRenderer {
 public:
  SoftwareRenderer(const RendererSettings* settings,
                   OutputSurface* output_surface,
                   ResourceProvider* resource_provider);

  ~SoftwareRenderer() override;

  void SwapBuffers(std::vector<ui::LatencyInfo> latency_info) override;

  void SetDisablePictureQuadImageFiltering(bool disable) {
    disable_picture_quad_image_filtering_ = disable;
  }

 protected:
  bool CanPartialSwap() override;
  void BindFramebufferToOutputSurface(DrawingFrame* frame) override;
  bool BindFramebufferToTexture(DrawingFrame* frame,
                                const ScopedResource* texture) override;
  void SetScissorTestRect(const gfx::Rect& scissor_rect) override;
  void PrepareSurfaceForPass(DrawingFrame* frame,
                             SurfaceInitializationMode initialization_mode,
                             const gfx::Rect& render_pass_scissor) override;
  void DoDrawQuad(DrawingFrame* frame,
                  const DrawQuad* quad,
                  const gfx::QuadF* draw_region) override;
  void BeginDrawingFrame(DrawingFrame* frame) override;
  void FinishDrawingFrame(DrawingFrame* frame) override;
  bool FlippedFramebuffer(const DrawingFrame* frame) const override;
  void EnsureScissorTestEnabled() override;
  void EnsureScissorTestDisabled() override;
  void CopyCurrentRenderPassToBitmap(
      DrawingFrame* frame,
      std::unique_ptr<CopyOutputRequest> request) override;
  void DidChangeVisibility() override;

 private:
  void ClearCanvas(SkColor color);
  void ClearFramebuffer(DrawingFrame* frame);
  void SetClipRect(const gfx::Rect& rect);
  bool IsSoftwareResource(ResourceId resource_id) const;

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
  bool ShouldApplyBackgroundFilters(const RenderPassDrawQuad* quad) const;
  sk_sp<SkImage> ApplyImageFilter(SkImageFilter* filter,
                                  const RenderPassDrawQuad* quad,
                                  const SkBitmap& to_filter,
                                  SkIRect* auto_bounds) const;
  gfx::Rect GetBackdropBoundingBoxForRenderPassQuad(
      const DrawingFrame* frame,
      const RenderPassDrawQuad* quad,
      const gfx::Transform& contents_device_transform,
      gfx::Rect* unclipped_rect) const;
  SkBitmap GetBackdropBitmap(const gfx::Rect& bounding_rect) const;
  sk_sp<SkShader> GetBackgroundFilterShader(
      const DrawingFrame* frame,
      const RenderPassDrawQuad* quad,
      SkShader::TileMode content_tile_mode) const;

  bool disable_picture_quad_image_filtering_ = false;

  bool is_scissor_enabled_ = false;
  gfx::Rect scissor_rect_;

  SoftwareOutputDevice* output_device_;
  SkCanvas* root_canvas_ = nullptr;
  SkCanvas* current_canvas_ = nullptr;
  SkPaint current_paint_;
  std::unique_ptr<ResourceProvider::ScopedWriteLockSoftware>
      current_framebuffer_lock_;
  sk_sp<SkCanvas> current_framebuffer_canvas_;

  DISALLOW_COPY_AND_ASSIGN(SoftwareRenderer);
};

}  // namespace cc

#endif  // CC_OUTPUT_SOFTWARE_RENDERER_H_
