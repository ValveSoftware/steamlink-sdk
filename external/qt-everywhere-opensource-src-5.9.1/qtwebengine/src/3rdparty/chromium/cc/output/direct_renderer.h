// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_DIRECT_RENDERER_H_
#define CC_OUTPUT_DIRECT_RENDERER_H_

#include <memory>
#include <unordered_map>

#include "base/callback.h"
#include "base/macros.h"
#include "cc/base/cc_export.h"
#include "cc/output/ca_layer_overlay.h"
#include "cc/output/overlay_processor.h"
#include "cc/quads/tile_draw_quad.h"
#include "cc/resources/resource_provider.h"
#include "gpu/command_buffer/common/texture_in_use_response.h"
#include "ui/events/latency_info.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect.h"

namespace gfx {
class ColorSpace;
}

namespace cc {
class DrawPolygon;
class OutputSurface;
class RenderPass;
class RenderPassId;
class RendererSettings;
class ResourceProvider;
class ScopedResource;

// This is the base class for code shared between the GL and software
// renderer implementations. "Direct" refers to the fact that it does not
// delegate rendering to another compositor (see historical DelegatingRenderer
// for reference).
class CC_EXPORT DirectRenderer {
 public:
  DirectRenderer(const RendererSettings* settings,
                 OutputSurface* output_surface,
                 ResourceProvider* resource_provider);
  virtual ~DirectRenderer();

  void Initialize();

  bool use_partial_swap() const { return use_partial_swap_; }

  void SetVisible(bool visible);
  void DecideRenderPassAllocationsForFrame(
      const RenderPassList& render_passes_in_draw_order);
  bool HasAllocatedResourcesForTesting(RenderPassId id) const;
  void DrawFrame(RenderPassList* render_passes_in_draw_order,
                 float device_scale_factor,
                 const gfx::ColorSpace& device_color_space,
                 const gfx::Size& device_viewport_size);

  // Public interface implemented by subclasses.
  virtual void SwapBuffers(std::vector<ui::LatencyInfo> latency_info) = 0;
  virtual void SwapBuffersComplete() {}
  virtual void DidReceiveTextureInUseResponses(
      const gpu::TextureInUseResponses& responses) {}

  // Allow tests to enlarge the texture size of non-root render passes to
  // verify cases where the texture doesn't match the render pass size.
  void SetEnlargePassTextureAmountForTesting(const gfx::Size& amount) {
    enlarge_pass_texture_amount_ = amount;
  }

  // Public for tests that poke at internals.
  struct CC_EXPORT DrawingFrame {
    DrawingFrame();
    ~DrawingFrame();

    const RenderPassList* render_passes_in_draw_order = nullptr;
    const RenderPass* root_render_pass = nullptr;
    const RenderPass* current_render_pass = nullptr;
    const ScopedResource* current_texture = nullptr;

    gfx::Rect root_damage_rect;
    gfx::Size device_viewport_size;
    gfx::ColorSpace device_color_space;

    gfx::Transform projection_matrix;
    gfx::Transform window_matrix;

    OverlayCandidateList overlay_list;
    CALayerOverlayList ca_layer_overlay_list;
  };

 protected:
  friend class BspWalkActionDrawPolygon;

  enum SurfaceInitializationMode {
    SURFACE_INITIALIZATION_MODE_PRESERVE,
    SURFACE_INITIALIZATION_MODE_SCISSORED_CLEAR,
    SURFACE_INITIALIZATION_MODE_FULL_SURFACE_CLEAR,
  };

  static gfx::RectF QuadVertexRect();
  static void QuadRectTransform(gfx::Transform* quad_rect_transform,
                                const gfx::Transform& quad_transform,
                                const gfx::RectF& quad_rect);
  void InitializeViewport(DrawingFrame* frame,
                          const gfx::Rect& draw_rect,
                          const gfx::Rect& viewport_rect,
                          const gfx::Size& surface_size);
  gfx::Rect MoveFromDrawToWindowSpace(const DrawingFrame* frame,
                                      const gfx::Rect& draw_rect) const;

  gfx::Rect DeviceViewportRectInDrawSpace(const DrawingFrame* frame) const;
  gfx::Rect OutputSurfaceRectInDrawSpace(const DrawingFrame* frame) const;
  static gfx::Rect ComputeScissorRectForRenderPass(const DrawingFrame* frame);
  void SetScissorStateForQuad(const DrawingFrame* frame,
                              const DrawQuad& quad,
                              const gfx::Rect& render_pass_scissor,
                              bool use_render_pass_scissor);
  bool ShouldSkipQuad(const DrawQuad& quad,
                      const gfx::Rect& render_pass_scissor);
  void SetScissorTestRectInDrawSpace(const DrawingFrame* frame,
                                     const gfx::Rect& draw_space_rect);

  static gfx::Size RenderPassTextureSize(const RenderPass* render_pass);

  void FlushPolygons(std::deque<std::unique_ptr<DrawPolygon>>* poly_list,
                     DrawingFrame* frame,
                     const gfx::Rect& render_pass_scissor,
                     bool use_render_pass_scissor);
  void DrawRenderPassAndExecuteCopyRequests(DrawingFrame* frame,
                                            RenderPass* render_pass);
  void DrawRenderPass(DrawingFrame* frame, const RenderPass* render_pass);
  bool UseRenderPass(DrawingFrame* frame, const RenderPass* render_pass);

  void DoDrawPolygon(const DrawPolygon& poly,
                     DrawingFrame* frame,
                     const gfx::Rect& render_pass_scissor,
                     bool use_render_pass_scissor);

  // Private interface implemented by subclasses for use by DirectRenderer.
  virtual bool CanPartialSwap() = 0;
  virtual void BindFramebufferToOutputSurface(DrawingFrame* frame) = 0;
  virtual bool BindFramebufferToTexture(DrawingFrame* frame,
                                        const ScopedResource* resource) = 0;
  virtual void SetScissorTestRect(const gfx::Rect& scissor_rect) = 0;
  virtual void PrepareSurfaceForPass(
      DrawingFrame* frame,
      SurfaceInitializationMode initialization_mode,
      const gfx::Rect& render_pass_scissor) = 0;
  // |clip_region| is a (possibly null) pointer to a quad in the same
  // space as the quad. When non-null only the area of the quad that overlaps
  // with clip_region will be drawn.
  virtual void DoDrawQuad(DrawingFrame* frame,
                          const DrawQuad* quad,
                          const gfx::QuadF* clip_region) = 0;
  virtual void BeginDrawingFrame(DrawingFrame* frame) = 0;
  virtual void FinishDrawingFrame(DrawingFrame* frame) = 0;
  // If a pass contains a single tile draw quad and can be drawn without
  // a render pass (e.g. applying a filter directly to the tile quad)
  // return that quad, otherwise return null.
  virtual const TileDrawQuad* CanPassBeDrawnDirectly(const RenderPass* pass);
  virtual void FinishDrawingQuadList() {}
  virtual bool FlippedFramebuffer(const DrawingFrame* frame) const = 0;
  virtual void EnsureScissorTestEnabled() = 0;
  virtual void EnsureScissorTestDisabled() = 0;
  virtual void DidChangeVisibility() = 0;
  virtual void CopyCurrentRenderPassToBitmap(
      DrawingFrame* frame,
      std::unique_ptr<CopyOutputRequest> request) = 0;

  gfx::Size surface_size_for_swap_buffers() const {
    return reshape_surface_size_;
  }

  const RendererSettings* const settings_;
  OutputSurface* const output_surface_;
  ResourceProvider* const resource_provider_;
  // This can be replaced by test implementations.
  std::unique_ptr<OverlayProcessor> overlay_processor_;

  // Whether it's valid to SwapBuffers with an empty rect. Trivially true when
  // using partial swap.
  bool allow_empty_swap_;
  // Whether partial swap can be used.
  bool use_partial_swap_;

  // TODO(danakj): Just use a vector of pairs here? Hash map is way overkill.
  std::unordered_map<RenderPassId,
                     std::unique_ptr<ScopedResource>,
                     RenderPassIdHash>
      render_pass_textures_;
  std::unordered_map<RenderPassId, TileDrawQuad, RenderPassIdHash>
      render_pass_bypass_quads_;

  bool visible_ = false;

  // For use in coordinate conversion, this stores the output rect, viewport
  // rect (= unflipped version of glViewport rect), the size of target
  // framebuffer, and the current window space viewport. During a draw, this
  // stores the values for the current render pass; in between draws, they
  // retain the values for the root render pass of the last draw.
  gfx::Rect current_draw_rect_;
  gfx::Rect current_viewport_rect_;
  gfx::Size current_surface_size_;
  gfx::Rect current_window_space_viewport_;

 private:
  bool initialized_ = false;
  gfx::Size enlarge_pass_texture_amount_;

  // Cached values given to Reshape().
  gfx::Size reshape_surface_size_;
  float reshape_device_scale_factor_ = 0.f;
  gfx::ColorSpace reshape_device_color_space_;
  bool reshape_has_alpha_ = false;

  DISALLOW_COPY_AND_ASSIGN(DirectRenderer);
};

}  // namespace cc

#endif  // CC_OUTPUT_DIRECT_RENDERER_H_
