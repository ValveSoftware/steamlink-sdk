// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_GL_RENDERER_H_
#define CC_OUTPUT_GL_RENDERER_H_

#include "base/cancelable_callback.h"
#include "cc/base/cc_export.h"
#include "cc/base/scoped_ptr_deque.h"
#include "cc/base/scoped_ptr_vector.h"
#include "cc/output/direct_renderer.h"
#include "cc/output/gl_renderer_draw_cache.h"
#include "cc/output/program_binding.h"
#include "cc/output/renderer.h"
#include "cc/quads/checkerboard_draw_quad.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/io_surface_draw_quad.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/tile_draw_quad.h"
#include "cc/quads/yuv_video_draw_quad.h"
#include "ui/gfx/quad_f.h"

class SkBitmap;

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace cc {

class GLRendererShaderTest;
class OutputSurface;
class PictureDrawQuad;
class ScopedResource;
class StreamVideoDrawQuad;
class TextureDrawQuad;
class TextureMailboxDeleter;
class GeometryBinding;
class ScopedEnsureFramebufferAllocation;

// Class that handles drawing of composited render layers using GL.
class CC_EXPORT GLRenderer : public DirectRenderer {
 public:
  class ScopedUseGrContext;

  static scoped_ptr<GLRenderer> Create(
      RendererClient* client,
      const LayerTreeSettings* settings,
      OutputSurface* output_surface,
      ResourceProvider* resource_provider,
      TextureMailboxDeleter* texture_mailbox_deleter,
      int highp_threshold_min);

  virtual ~GLRenderer();

  virtual const RendererCapabilitiesImpl& Capabilities() const OVERRIDE;

  // Waits for rendering to finish.
  virtual void Finish() OVERRIDE;

  virtual void DoNoOp() OVERRIDE;
  virtual void SwapBuffers(const CompositorFrameMetadata& metadata) OVERRIDE;

  virtual bool IsContextLost() OVERRIDE;

  static void DebugGLCall(gpu::gles2::GLES2Interface* gl,
                          const char* command,
                          const char* file,
                          int line);

 protected:
  GLRenderer(RendererClient* client,
             const LayerTreeSettings* settings,
             OutputSurface* output_surface,
             ResourceProvider* resource_provider,
             TextureMailboxDeleter* texture_mailbox_deleter,
             int highp_threshold_min);

  virtual void DidChangeVisibility() OVERRIDE;

  bool IsBackbufferDiscarded() const { return is_backbuffer_discarded_; }

  const gfx::QuadF& SharedGeometryQuad() const { return shared_geometry_quad_; }
  const GeometryBinding* SharedGeometry() const {
    return shared_geometry_.get();
  }

  void GetFramebufferPixelsAsync(const gfx::Rect& rect,
                                 scoped_ptr<CopyOutputRequest> request);
  void GetFramebufferTexture(unsigned texture_id,
                             ResourceFormat texture_format,
                             const gfx::Rect& device_rect);
  void ReleaseRenderPassTextures();

  void SetStencilEnabled(bool enabled);
  bool stencil_enabled() const { return stencil_shadow_; }
  void SetBlendEnabled(bool enabled);
  bool blend_enabled() const { return blend_shadow_; }

  virtual void BindFramebufferToOutputSurface(DrawingFrame* frame) OVERRIDE;
  virtual bool BindFramebufferToTexture(DrawingFrame* frame,
                                        const ScopedResource* resource,
                                        const gfx::Rect& target_rect) OVERRIDE;
  virtual void SetDrawViewport(const gfx::Rect& window_space_viewport) OVERRIDE;
  virtual void SetScissorTestRect(const gfx::Rect& scissor_rect) OVERRIDE;
  virtual void DiscardPixels(bool has_external_stencil_test,
                             bool draw_rect_covers_full_surface) OVERRIDE;
  virtual void ClearFramebuffer(DrawingFrame* frame,
                                bool has_external_stencil_test) OVERRIDE;
  virtual void DoDrawQuad(DrawingFrame* frame, const class DrawQuad*) OVERRIDE;
  virtual void BeginDrawingFrame(DrawingFrame* frame) OVERRIDE;
  virtual void FinishDrawingFrame(DrawingFrame* frame) OVERRIDE;
  virtual bool FlippedFramebuffer() const OVERRIDE;
  virtual void EnsureScissorTestEnabled() OVERRIDE;
  virtual void EnsureScissorTestDisabled() OVERRIDE;
  virtual void CopyCurrentRenderPassToBitmap(
      DrawingFrame* frame,
      scoped_ptr<CopyOutputRequest> request) OVERRIDE;
  virtual void FinishDrawingQuadList() OVERRIDE;

  // Check if quad needs antialiasing and if so, inflate the quad and
  // fill edge array for fragment shader.  local_quad is set to
  // inflated quad if antialiasing is required, otherwise it is left
  // unchanged.  edge array is filled with inflated quad's edge data
  // if antialiasing is required, otherwise it is left unchanged.
  // Returns true if quad requires antialiasing and false otherwise.
  static bool SetupQuadForAntialiasing(const gfx::Transform& device_transform,
                                       const DrawQuad* quad,
                                       gfx::QuadF* local_quad,
                                       float edge[24]);

 private:
  friend class GLRendererShaderPixelTest;
  friend class GLRendererShaderTest;

  static void ToGLMatrix(float* gl_matrix, const gfx::Transform& transform);

  void DrawCheckerboardQuad(const DrawingFrame* frame,
                            const CheckerboardDrawQuad* quad);
  void DrawDebugBorderQuad(const DrawingFrame* frame,
                           const DebugBorderDrawQuad* quad);
  scoped_ptr<ScopedResource> GetBackgroundWithFilters(
      DrawingFrame* frame,
      const RenderPassDrawQuad* quad,
      const gfx::Transform& contents_device_transform,
      const gfx::Transform& contents_device_transformInverse,
      bool* background_changed);
  void DrawRenderPassQuad(DrawingFrame* frame, const RenderPassDrawQuad* quad);
  void DrawSolidColorQuad(const DrawingFrame* frame,
                          const SolidColorDrawQuad* quad);
  void DrawStreamVideoQuad(const DrawingFrame* frame,
                           const StreamVideoDrawQuad* quad);
  void EnqueueTextureQuad(const DrawingFrame* frame,
                          const TextureDrawQuad* quad);
  void FlushTextureQuadCache();
  void DrawIOSurfaceQuad(const DrawingFrame* frame,
                         const IOSurfaceDrawQuad* quad);
  void DrawTileQuad(const DrawingFrame* frame, const TileDrawQuad* quad);
  void DrawContentQuad(const DrawingFrame* frame,
                       const ContentDrawQuadBase* quad,
                       ResourceProvider::ResourceId resource_id);
  void DrawYUVVideoQuad(const DrawingFrame* frame,
                        const YUVVideoDrawQuad* quad);
  void DrawPictureQuad(const DrawingFrame* frame,
                       const PictureDrawQuad* quad);

  void SetShaderOpacity(float opacity, int alpha_location);
  void SetShaderQuadF(const gfx::QuadF& quad, int quad_location);
  void DrawQuadGeometry(const DrawingFrame* frame,
                        const gfx::Transform& draw_transform,
                        const gfx::RectF& quad_rect,
                        int matrix_location);
  void SetUseProgram(unsigned program);

  void CopyTextureToFramebuffer(const DrawingFrame* frame,
                                int texture_id,
                                const gfx::Rect& rect,
                                const gfx::Transform& draw_matrix,
                                bool flip_vertically);

  bool UseScopedTexture(DrawingFrame* frame,
                        const ScopedResource* resource,
                        const gfx::Rect& viewport_rect);

  bool MakeContextCurrent();

  void InitializeSharedObjects();
  void CleanupSharedObjects();

  typedef base::Callback<void(scoped_ptr<CopyOutputRequest> copy_request,
                              bool success)>
      AsyncGetFramebufferPixelsCleanupCallback;
  void FinishedReadback(unsigned source_buffer,
                        unsigned query,
                        const gfx::Size& size);

  void ReinitializeGLState();
  void RestoreGLState();
  void RestoreFramebuffer(DrawingFrame* frame);

  virtual void DiscardBackbuffer() OVERRIDE;
  virtual void EnsureBackbuffer() OVERRIDE;
  void EnforceMemoryPolicy();

  void ScheduleOverlays(DrawingFrame* frame);

  typedef ScopedPtrVector<ResourceProvider::ScopedReadLockGL>
      OverlayResourceLockList;
  OverlayResourceLockList pending_overlay_resources_;
  OverlayResourceLockList in_use_overlay_resources_;

  RendererCapabilitiesImpl capabilities_;

  unsigned offscreen_framebuffer_id_;

  scoped_ptr<GeometryBinding> shared_geometry_;
  gfx::QuadF shared_geometry_quad_;

  // This block of bindings defines all of the programs used by the compositor
  // itself.  Add any new programs here to GLRendererShaderTest.

  // Tiled layer shaders.
  typedef ProgramBinding<VertexShaderTile, FragmentShaderRGBATexAlpha>
      TileProgram;
  typedef ProgramBinding<VertexShaderTileAA, FragmentShaderRGBATexClampAlphaAA>
      TileProgramAA;
  typedef ProgramBinding<VertexShaderTileAA,
                         FragmentShaderRGBATexClampSwizzleAlphaAA>
      TileProgramSwizzleAA;
  typedef ProgramBinding<VertexShaderTile, FragmentShaderRGBATexOpaque>
      TileProgramOpaque;
  typedef ProgramBinding<VertexShaderTile, FragmentShaderRGBATexSwizzleAlpha>
      TileProgramSwizzle;
  typedef ProgramBinding<VertexShaderTile, FragmentShaderRGBATexSwizzleOpaque>
      TileProgramSwizzleOpaque;
  typedef ProgramBinding<VertexShaderPosTex, FragmentShaderCheckerboard>
      TileCheckerboardProgram;

  // Texture shaders.
  typedef ProgramBinding<VertexShaderPosTexTransform,
                         FragmentShaderRGBATexVaryingAlpha> TextureProgram;
  typedef ProgramBinding<VertexShaderPosTexTransform,
                         FragmentShaderRGBATexPremultiplyAlpha>
      NonPremultipliedTextureProgram;
  typedef ProgramBinding<VertexShaderPosTexTransform,
                         FragmentShaderTexBackgroundVaryingAlpha>
      TextureBackgroundProgram;
  typedef ProgramBinding<VertexShaderPosTexTransform,
                         FragmentShaderTexBackgroundPremultiplyAlpha>
      NonPremultipliedTextureBackgroundProgram;

  // Render surface shaders.
  typedef ProgramBinding<VertexShaderPosTexTransform,
                         FragmentShaderRGBATexAlpha> RenderPassProgram;
  typedef ProgramBinding<VertexShaderPosTexTransform,
                         FragmentShaderRGBATexAlphaMask> RenderPassMaskProgram;
  typedef ProgramBinding<VertexShaderQuadTexTransformAA,
                         FragmentShaderRGBATexAlphaAA> RenderPassProgramAA;
  typedef ProgramBinding<VertexShaderQuadTexTransformAA,
                         FragmentShaderRGBATexAlphaMaskAA>
      RenderPassMaskProgramAA;
  typedef ProgramBinding<VertexShaderPosTexTransform,
                         FragmentShaderRGBATexColorMatrixAlpha>
      RenderPassColorMatrixProgram;
  typedef ProgramBinding<VertexShaderQuadTexTransformAA,
                         FragmentShaderRGBATexAlphaMaskColorMatrixAA>
      RenderPassMaskColorMatrixProgramAA;
  typedef ProgramBinding<VertexShaderQuadTexTransformAA,
                         FragmentShaderRGBATexAlphaColorMatrixAA>
      RenderPassColorMatrixProgramAA;
  typedef ProgramBinding<VertexShaderPosTexTransform,
                         FragmentShaderRGBATexAlphaMaskColorMatrix>
      RenderPassMaskColorMatrixProgram;

  // Video shaders.
  typedef ProgramBinding<VertexShaderVideoTransform, FragmentShaderRGBATex>
      VideoStreamTextureProgram;
  typedef ProgramBinding<VertexShaderPosTexYUVStretchOffset,
                         FragmentShaderYUVVideo> VideoYUVProgram;
  typedef ProgramBinding<VertexShaderPosTexYUVStretchOffset,
                         FragmentShaderYUVAVideo> VideoYUVAProgram;

  // Special purpose / effects shaders.
  typedef ProgramBinding<VertexShaderPos, FragmentShaderColor>
      DebugBorderProgram;
  typedef ProgramBinding<VertexShaderQuad, FragmentShaderColor>
      SolidColorProgram;
  typedef ProgramBinding<VertexShaderQuadAA, FragmentShaderColorAA>
      SolidColorProgramAA;

  const TileProgram* GetTileProgram(
      TexCoordPrecision precision, SamplerType sampler);
  const TileProgramOpaque* GetTileProgramOpaque(
      TexCoordPrecision precision, SamplerType sampler);
  const TileProgramAA* GetTileProgramAA(
      TexCoordPrecision precision, SamplerType sampler);
  const TileProgramSwizzle* GetTileProgramSwizzle(
      TexCoordPrecision precision, SamplerType sampler);
  const TileProgramSwizzleOpaque* GetTileProgramSwizzleOpaque(
      TexCoordPrecision precision, SamplerType sampler);
  const TileProgramSwizzleAA* GetTileProgramSwizzleAA(
      TexCoordPrecision precision, SamplerType sampler);

  const TileCheckerboardProgram* GetTileCheckerboardProgram();

  const RenderPassProgram* GetRenderPassProgram(
      TexCoordPrecision precision);
  const RenderPassProgramAA* GetRenderPassProgramAA(
      TexCoordPrecision precision);
  const RenderPassMaskProgram* GetRenderPassMaskProgram(
      TexCoordPrecision precision);
  const RenderPassMaskProgramAA* GetRenderPassMaskProgramAA(
      TexCoordPrecision precision);
  const RenderPassColorMatrixProgram* GetRenderPassColorMatrixProgram(
      TexCoordPrecision precision);
  const RenderPassColorMatrixProgramAA* GetRenderPassColorMatrixProgramAA(
      TexCoordPrecision precision);
  const RenderPassMaskColorMatrixProgram* GetRenderPassMaskColorMatrixProgram(
      TexCoordPrecision precision);
  const RenderPassMaskColorMatrixProgramAA*
      GetRenderPassMaskColorMatrixProgramAA(TexCoordPrecision precision);

  const TextureProgram* GetTextureProgram(
      TexCoordPrecision precision);
  const NonPremultipliedTextureProgram* GetNonPremultipliedTextureProgram(
      TexCoordPrecision precision);
  const TextureBackgroundProgram* GetTextureBackgroundProgram(
      TexCoordPrecision precision);
  const NonPremultipliedTextureBackgroundProgram*
      GetNonPremultipliedTextureBackgroundProgram(TexCoordPrecision precision);
  const TextureProgram* GetTextureIOSurfaceProgram(
      TexCoordPrecision precision);

  const VideoYUVProgram* GetVideoYUVProgram(
      TexCoordPrecision precision);
  const VideoYUVAProgram* GetVideoYUVAProgram(
      TexCoordPrecision precision);
  const VideoStreamTextureProgram* GetVideoStreamTextureProgram(
      TexCoordPrecision precision);

  const DebugBorderProgram* GetDebugBorderProgram();
  const SolidColorProgram* GetSolidColorProgram();
  const SolidColorProgramAA* GetSolidColorProgramAA();

  TileProgram tile_program_[NumTexCoordPrecisions][NumSamplerTypes];
  TileProgramOpaque
      tile_program_opaque_[NumTexCoordPrecisions][NumSamplerTypes];
  TileProgramAA tile_program_aa_[NumTexCoordPrecisions][NumSamplerTypes];
  TileProgramSwizzle
      tile_program_swizzle_[NumTexCoordPrecisions][NumSamplerTypes];
  TileProgramSwizzleOpaque
      tile_program_swizzle_opaque_[NumTexCoordPrecisions][NumSamplerTypes];
  TileProgramSwizzleAA
      tile_program_swizzle_aa_[NumTexCoordPrecisions][NumSamplerTypes];

  TileCheckerboardProgram tile_checkerboard_program_;

  TextureProgram texture_program_[NumTexCoordPrecisions];
  NonPremultipliedTextureProgram
      nonpremultiplied_texture_program_[NumTexCoordPrecisions];
  TextureBackgroundProgram texture_background_program_[NumTexCoordPrecisions];
  NonPremultipliedTextureBackgroundProgram
      nonpremultiplied_texture_background_program_[NumTexCoordPrecisions];
  TextureProgram texture_io_surface_program_[NumTexCoordPrecisions];

  RenderPassProgram render_pass_program_[NumTexCoordPrecisions];
  RenderPassProgramAA render_pass_program_aa_[NumTexCoordPrecisions];
  RenderPassMaskProgram render_pass_mask_program_[NumTexCoordPrecisions];
  RenderPassMaskProgramAA render_pass_mask_program_aa_[NumTexCoordPrecisions];
  RenderPassColorMatrixProgram
      render_pass_color_matrix_program_[NumTexCoordPrecisions];
  RenderPassColorMatrixProgramAA
      render_pass_color_matrix_program_aa_[NumTexCoordPrecisions];
  RenderPassMaskColorMatrixProgram
      render_pass_mask_color_matrix_program_[NumTexCoordPrecisions];
  RenderPassMaskColorMatrixProgramAA
      render_pass_mask_color_matrix_program_aa_[NumTexCoordPrecisions];

  VideoYUVProgram video_yuv_program_[NumTexCoordPrecisions];
  VideoYUVAProgram video_yuva_program_[NumTexCoordPrecisions];
  VideoStreamTextureProgram
      video_stream_texture_program_[NumTexCoordPrecisions];

  DebugBorderProgram debug_border_program_;
  SolidColorProgram solid_color_program_;
  SolidColorProgramAA solid_color_program_aa_;

  gpu::gles2::GLES2Interface* gl_;
  gpu::ContextSupport* context_support_;

  skia::RefPtr<GrContext> gr_context_;
  skia::RefPtr<SkCanvas> sk_canvas_;

  TextureMailboxDeleter* texture_mailbox_deleter_;

  gfx::Rect swap_buffer_rect_;
  gfx::Rect scissor_rect_;
  gfx::Rect viewport_;
  bool is_backbuffer_discarded_;
  bool is_using_bind_uniform_;
  bool is_scissor_enabled_;
  bool scissor_rect_needs_reset_;
  bool stencil_shadow_;
  bool blend_shadow_;
  unsigned program_shadow_;
  TexturedQuadDrawCache draw_cache_;
  int highp_threshold_min_;
  int highp_threshold_cache_;

  struct PendingAsyncReadPixels;
  ScopedPtrVector<PendingAsyncReadPixels> pending_async_read_pixels_;

  scoped_ptr<ResourceProvider::ScopedWriteLockGL> current_framebuffer_lock_;

  class SyncQuery;
  ScopedPtrDeque<SyncQuery> pending_sync_queries_;
  ScopedPtrDeque<SyncQuery> available_sync_queries_;
  scoped_ptr<SyncQuery> current_sync_query_;
  bool use_sync_query_;

  SkBitmap on_demand_tile_raster_bitmap_;
  ResourceProvider::ResourceId on_demand_tile_raster_resource_id_;

  DISALLOW_COPY_AND_ASSIGN(GLRenderer);
};

// Setting DEBUG_GL_CALLS to 1 will call glGetError() after almost every GL
// call made by the compositor. Useful for debugging rendering issues but
// will significantly degrade performance.
#define DEBUG_GL_CALLS 0

#if DEBUG_GL_CALLS && !defined(NDEBUG)
#define GLC(context, x)                                                        \
  (x, GLRenderer::DebugGLCall(&* context, #x, __FILE__, __LINE__))
#else
#define GLC(context, x) (x)
#endif

}  // namespace cc

#endif  // CC_OUTPUT_GL_RENDERER_H_
