// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/software_renderer.h"

#include "base/debug/trace_event.h"
#include "cc/base/math_util.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "cc/output/compositor_frame_metadata.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/output_surface.h"
#include "cc/output/render_surface_filters.h"
#include "cc/output/software_output_device.h"
#include "cc/quads/checkerboard_draw_quad.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/picture_draw_quad.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/quads/tile_draw_quad.h"
#include "cc/resources/raster_worker_pool.h"
#include "skia/ext/opacity_draw_filter.h"
#include "third_party/skia/include/core/SkBitmapDevice.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkShader.h"
#include "third_party/skia/include/effects/SkLayerRasterizer.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/transform.h"

namespace cc {

namespace {

class OnDemandRasterTaskImpl : public Task {
 public:
  OnDemandRasterTaskImpl(PicturePileImpl* picture_pile,
                         SkCanvas* canvas,
                         gfx::Rect content_rect,
                         float contents_scale)
      : picture_pile_(picture_pile),
        canvas_(canvas),
        content_rect_(content_rect),
        contents_scale_(contents_scale) {
    DCHECK(picture_pile_);
    DCHECK(canvas_);
  }

  // Overridden from Task:
  virtual void RunOnWorkerThread() OVERRIDE {
    TRACE_EVENT0("cc", "OnDemandRasterTaskImpl::RunOnWorkerThread");

    PicturePileImpl* picture_pile = picture_pile_->GetCloneForDrawingOnThread(
        RasterWorkerPool::GetPictureCloneIndexForCurrentThread());
    DCHECK(picture_pile);

    picture_pile->RasterDirect(canvas_, content_rect_, contents_scale_, NULL);
  }

 protected:
  virtual ~OnDemandRasterTaskImpl() {}

 private:
  PicturePileImpl* picture_pile_;
  SkCanvas* canvas_;
  const gfx::Rect content_rect_;
  const float contents_scale_;

  DISALLOW_COPY_AND_ASSIGN(OnDemandRasterTaskImpl);
};

static inline bool IsScalarNearlyInteger(SkScalar scalar) {
  return SkScalarNearlyZero(scalar - SkScalarRoundToScalar(scalar));
}

bool IsScaleAndIntegerTranslate(const SkMatrix& matrix) {
  return IsScalarNearlyInteger(matrix[SkMatrix::kMTransX]) &&
         IsScalarNearlyInteger(matrix[SkMatrix::kMTransY]) &&
         SkScalarNearlyZero(matrix[SkMatrix::kMSkewX]) &&
         SkScalarNearlyZero(matrix[SkMatrix::kMSkewY]) &&
         SkScalarNearlyZero(matrix[SkMatrix::kMPersp0]) &&
         SkScalarNearlyZero(matrix[SkMatrix::kMPersp1]) &&
         SkScalarNearlyZero(matrix[SkMatrix::kMPersp2] - 1.0f);
}

static SkShader::TileMode WrapModeToTileMode(GLint wrap_mode) {
  switch (wrap_mode) {
    case GL_REPEAT:
      return SkShader::kRepeat_TileMode;
    case GL_CLAMP_TO_EDGE:
      return SkShader::kClamp_TileMode;
  }
  NOTREACHED();
  return SkShader::kClamp_TileMode;
}

}  // anonymous namespace

scoped_ptr<SoftwareRenderer> SoftwareRenderer::Create(
    RendererClient* client,
    const LayerTreeSettings* settings,
    OutputSurface* output_surface,
    ResourceProvider* resource_provider) {
  return make_scoped_ptr(new SoftwareRenderer(
      client, settings, output_surface, resource_provider));
}

SoftwareRenderer::SoftwareRenderer(RendererClient* client,
                                   const LayerTreeSettings* settings,
                                   OutputSurface* output_surface,
                                   ResourceProvider* resource_provider)
    : DirectRenderer(client, settings, output_surface, resource_provider),
      is_scissor_enabled_(false),
      is_backbuffer_discarded_(false),
      output_device_(output_surface->software_device()),
      current_canvas_(NULL) {
  if (resource_provider_) {
    capabilities_.max_texture_size = resource_provider_->max_texture_size();
    capabilities_.best_texture_format =
        resource_provider_->best_texture_format();
  }
  // The updater can access bitmaps while the SoftwareRenderer is using them.
  capabilities_.allow_partial_texture_updates = true;
  capabilities_.using_partial_swap = true;

  capabilities_.using_map_image = true;
  capabilities_.using_shared_memory_resources = true;

  capabilities_.allow_rasterize_on_demand = true;
}

SoftwareRenderer::~SoftwareRenderer() {}

const RendererCapabilitiesImpl& SoftwareRenderer::Capabilities() const {
  return capabilities_;
}

void SoftwareRenderer::BeginDrawingFrame(DrawingFrame* frame) {
  TRACE_EVENT0("cc", "SoftwareRenderer::BeginDrawingFrame");
  root_canvas_ = output_device_->BeginPaint(
      gfx::ToEnclosingRect(frame->root_damage_rect));
}

void SoftwareRenderer::FinishDrawingFrame(DrawingFrame* frame) {
  TRACE_EVENT0("cc", "SoftwareRenderer::FinishDrawingFrame");
  current_framebuffer_lock_.reset();
  current_canvas_ = NULL;
  root_canvas_ = NULL;

  current_frame_data_.reset(new SoftwareFrameData);
  output_device_->EndPaint(current_frame_data_.get());
}

void SoftwareRenderer::SwapBuffers(const CompositorFrameMetadata& metadata) {
  TRACE_EVENT0("cc,benchmark", "SoftwareRenderer::SwapBuffers");
  CompositorFrame compositor_frame;
  compositor_frame.metadata = metadata;
  compositor_frame.software_frame_data = current_frame_data_.Pass();
  output_surface_->SwapBuffers(&compositor_frame);
}

void SoftwareRenderer::ReceiveSwapBuffersAck(const CompositorFrameAck& ack) {
  output_device_->ReclaimSoftwareFrame(ack.last_software_frame_id);
}

bool SoftwareRenderer::FlippedFramebuffer() const {
  return false;
}

void SoftwareRenderer::EnsureScissorTestEnabled() {
  is_scissor_enabled_ = true;
  SetClipRect(scissor_rect_);
}

void SoftwareRenderer::EnsureScissorTestDisabled() {
  // There is no explicit notion of enabling/disabling scissoring in software
  // rendering, but the underlying effect we want is to clear any existing
  // clipRect on the current SkCanvas. This is done by setting clipRect to
  // the viewport's dimensions.
  is_scissor_enabled_ = false;
  SkISize size = current_canvas_->getDeviceSize();
  SetClipRect(gfx::Rect(size.width(), size.height()));
}

void SoftwareRenderer::Finish() {}

void SoftwareRenderer::BindFramebufferToOutputSurface(DrawingFrame* frame) {
  DCHECK(!output_surface_->HasExternalStencilTest());
  current_framebuffer_lock_.reset();
  current_canvas_ = root_canvas_;
}

bool SoftwareRenderer::BindFramebufferToTexture(
    DrawingFrame* frame,
    const ScopedResource* texture,
    const gfx::Rect& target_rect) {
  current_framebuffer_lock_.reset();
  current_framebuffer_lock_ = make_scoped_ptr(
      new ResourceProvider::ScopedWriteLockSoftware(
          resource_provider_, texture->id()));
  current_canvas_ = current_framebuffer_lock_->sk_canvas();
  InitializeViewport(frame,
                     target_rect,
                     gfx::Rect(target_rect.size()),
                     target_rect.size());
  return true;
}

void SoftwareRenderer::SetScissorTestRect(const gfx::Rect& scissor_rect) {
  is_scissor_enabled_ = true;
  scissor_rect_ = scissor_rect;
  SetClipRect(scissor_rect);
}

void SoftwareRenderer::SetClipRect(const gfx::Rect& rect) {
  // Skia applies the current matrix to clip rects so we reset it temporary.
  SkMatrix current_matrix = current_canvas_->getTotalMatrix();
  current_canvas_->resetMatrix();
  current_canvas_->clipRect(gfx::RectToSkRect(rect), SkRegion::kReplace_Op);
  current_canvas_->setMatrix(current_matrix);
}

void SoftwareRenderer::ClearCanvas(SkColor color) {
  // SkCanvas::clear doesn't respect the current clipping region
  // so we SkCanvas::drawColor instead if scissoring is active.
  if (is_scissor_enabled_)
    current_canvas_->drawColor(color, SkXfermode::kSrc_Mode);
  else
    current_canvas_->clear(color);
}

void SoftwareRenderer::DiscardPixels(bool has_external_stencil_test,
                                     bool draw_rect_covers_full_surface) {}

void SoftwareRenderer::ClearFramebuffer(DrawingFrame* frame,
                                        bool has_external_stencil_test) {
  if (frame->current_render_pass->has_transparent_background) {
    ClearCanvas(SkColorSetARGB(0, 0, 0, 0));
  } else {
#ifndef NDEBUG
    // On DEBUG builds, opaque render passes are cleared to blue
    // to easily see regions that were not drawn on the screen.
    ClearCanvas(SkColorSetARGB(255, 0, 0, 255));
#endif
  }
}

void SoftwareRenderer::SetDrawViewport(
    const gfx::Rect& window_space_viewport) {}

bool SoftwareRenderer::IsSoftwareResource(
    ResourceProvider::ResourceId resource_id) const {
  switch (resource_provider_->GetResourceType(resource_id)) {
    case ResourceProvider::GLTexture:
      return false;
    case ResourceProvider::Bitmap:
      return true;
    case ResourceProvider::InvalidType:
      break;
  }

  LOG(FATAL) << "Invalid resource type.";
  return false;
}

void SoftwareRenderer::DoDrawQuad(DrawingFrame* frame, const DrawQuad* quad) {
  TRACE_EVENT0("cc", "SoftwareRenderer::DoDrawQuad");
  gfx::Transform quad_rect_matrix;
  QuadRectTransform(&quad_rect_matrix, quad->quadTransform(), quad->rect);
  gfx::Transform contents_device_transform =
      frame->window_matrix * frame->projection_matrix * quad_rect_matrix;
  contents_device_transform.FlattenTo2d();
  SkMatrix sk_device_matrix;
  gfx::TransformToFlattenedSkMatrix(contents_device_transform,
                                    &sk_device_matrix);
  current_canvas_->setMatrix(sk_device_matrix);

  current_paint_.reset();
  if (!IsScaleAndIntegerTranslate(sk_device_matrix)) {
    // TODO(danakj): Until we can enable AA only on exterior edges of the
    // layer, disable AA if any interior edges are present. crbug.com/248175
    bool all_four_edges_are_exterior = quad->IsTopEdge() &&
                                       quad->IsLeftEdge() &&
                                       quad->IsBottomEdge() &&
                                       quad->IsRightEdge();
    if (settings_->allow_antialiasing && all_four_edges_are_exterior)
      current_paint_.setAntiAlias(true);
    current_paint_.setFilterLevel(SkPaint::kLow_FilterLevel);
  }

  if (quad->ShouldDrawWithBlending()) {
    current_paint_.setAlpha(quad->opacity() * 255);
    current_paint_.setXfermodeMode(SkXfermode::kSrcOver_Mode);
  } else {
    current_paint_.setXfermodeMode(SkXfermode::kSrc_Mode);
  }

  switch (quad->material) {
    case DrawQuad::CHECKERBOARD:
      DrawCheckerboardQuad(frame, CheckerboardDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::DEBUG_BORDER:
      DrawDebugBorderQuad(frame, DebugBorderDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::PICTURE_CONTENT:
      DrawPictureQuad(frame, PictureDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::RENDER_PASS:
      DrawRenderPassQuad(frame, RenderPassDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::SOLID_COLOR:
      DrawSolidColorQuad(frame, SolidColorDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::TEXTURE_CONTENT:
      DrawTextureQuad(frame, TextureDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::TILED_CONTENT:
      DrawTileQuad(frame, TileDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::SURFACE_CONTENT:
      // Surface content should be fully resolved to other quad types before
      // reaching a direct renderer.
      NOTREACHED();
      break;
    case DrawQuad::INVALID:
    case DrawQuad::IO_SURFACE_CONTENT:
    case DrawQuad::YUV_VIDEO_CONTENT:
    case DrawQuad::STREAM_VIDEO_CONTENT:
      DrawUnsupportedQuad(frame, quad);
      NOTREACHED();
      break;
  }

  current_canvas_->resetMatrix();
}

void SoftwareRenderer::DrawCheckerboardQuad(const DrawingFrame* frame,
                                            const CheckerboardDrawQuad* quad) {
  gfx::RectF visible_quad_vertex_rect = MathUtil::ScaleRectProportional(
      QuadVertexRect(), quad->rect, quad->visible_rect);
  current_paint_.setColor(quad->color);
  current_paint_.setAlpha(quad->opacity() * SkColorGetA(quad->color));
  current_canvas_->drawRect(gfx::RectFToSkRect(visible_quad_vertex_rect),
                            current_paint_);
}

void SoftwareRenderer::DrawDebugBorderQuad(const DrawingFrame* frame,
                                           const DebugBorderDrawQuad* quad) {
  // We need to apply the matrix manually to have pixel-sized stroke width.
  SkPoint vertices[4];
  gfx::RectFToSkRect(QuadVertexRect()).toQuad(vertices);
  SkPoint transformed_vertices[4];
  current_canvas_->getTotalMatrix().mapPoints(transformed_vertices,
                                              vertices,
                                              4);
  current_canvas_->resetMatrix();

  current_paint_.setColor(quad->color);
  current_paint_.setAlpha(quad->opacity() * SkColorGetA(quad->color));
  current_paint_.setStyle(SkPaint::kStroke_Style);
  current_paint_.setStrokeWidth(quad->width);
  current_canvas_->drawPoints(SkCanvas::kPolygon_PointMode,
                              4, transformed_vertices, current_paint_);
}

void SoftwareRenderer::DrawPictureQuad(const DrawingFrame* frame,
                                       const PictureDrawQuad* quad) {
  SkMatrix content_matrix;
  content_matrix.setRectToRect(
      gfx::RectFToSkRect(quad->tex_coord_rect),
      gfx::RectFToSkRect(QuadVertexRect()),
      SkMatrix::kFill_ScaleToFit);
  current_canvas_->concat(content_matrix);

  // TODO(aelias): This isn't correct in all cases. We should detect these
  // cases and fall back to a persistent bitmap backing
  // (http://crbug.com/280374).
  skia::RefPtr<SkDrawFilter> opacity_filter =
      skia::AdoptRef(new skia::OpacityDrawFilter(
          quad->opacity(), frame->disable_picture_quad_image_filtering));
  DCHECK(!current_canvas_->getDrawFilter());
  current_canvas_->setDrawFilter(opacity_filter.get());

  TRACE_EVENT0("cc",
               "SoftwareRenderer::DrawPictureQuad");

  // Create and run on-demand raster task for tile.
  scoped_refptr<Task> on_demand_raster_task(
      new OnDemandRasterTaskImpl(quad->picture_pile,
                                 current_canvas_,
                                 quad->content_rect,
                                 quad->contents_scale));
  client_->RunOnDemandRasterTask(on_demand_raster_task.get());

  current_canvas_->setDrawFilter(NULL);
}

void SoftwareRenderer::DrawSolidColorQuad(const DrawingFrame* frame,
                                          const SolidColorDrawQuad* quad) {
  gfx::RectF visible_quad_vertex_rect = MathUtil::ScaleRectProportional(
      QuadVertexRect(), quad->rect, quad->visible_rect);
  current_paint_.setColor(quad->color);
  current_paint_.setAlpha(quad->opacity() * SkColorGetA(quad->color));
  current_canvas_->drawRect(gfx::RectFToSkRect(visible_quad_vertex_rect),
                            current_paint_);
}

void SoftwareRenderer::DrawTextureQuad(const DrawingFrame* frame,
                                       const TextureDrawQuad* quad) {
  if (!IsSoftwareResource(quad->resource_id)) {
    DrawUnsupportedQuad(frame, quad);
    return;
  }

  // TODO(skaslev): Add support for non-premultiplied alpha.
  ResourceProvider::ScopedReadLockSoftware lock(resource_provider_,
                                                quad->resource_id);
  if (!lock.valid())
    return;
  const SkBitmap* bitmap = lock.sk_bitmap();
  gfx::RectF uv_rect = gfx::ScaleRect(gfx::BoundingRect(quad->uv_top_left,
                                                        quad->uv_bottom_right),
                                      bitmap->width(),
                                      bitmap->height());
  gfx::RectF visible_uv_rect =
      MathUtil::ScaleRectProportional(uv_rect, quad->rect, quad->visible_rect);
  SkRect sk_uv_rect = gfx::RectFToSkRect(visible_uv_rect);
  gfx::RectF visible_quad_vertex_rect = MathUtil::ScaleRectProportional(
      QuadVertexRect(), quad->rect, quad->visible_rect);
  SkRect quad_rect = gfx::RectFToSkRect(visible_quad_vertex_rect);

  if (quad->flipped)
    current_canvas_->scale(1, -1);

  bool blend_background = quad->background_color != SK_ColorTRANSPARENT &&
                          !bitmap->isOpaque();
  bool needs_layer = blend_background && (current_paint_.getAlpha() != 0xFF);
  if (needs_layer) {
    current_canvas_->saveLayerAlpha(&quad_rect, current_paint_.getAlpha());
    current_paint_.setAlpha(0xFF);
  }
  if (blend_background) {
    SkPaint background_paint;
    background_paint.setColor(quad->background_color);
    current_canvas_->drawRect(quad_rect, background_paint);
  }
  SkShader::TileMode tile_mode = WrapModeToTileMode(lock.wrap_mode());
  if (tile_mode != SkShader::kClamp_TileMode) {
    SkMatrix matrix;
    matrix.setRectToRect(sk_uv_rect, quad_rect, SkMatrix::kFill_ScaleToFit);
    skia::RefPtr<SkShader> shader = skia::AdoptRef(
        SkShader::CreateBitmapShader(*bitmap, tile_mode, tile_mode, &matrix));
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setShader(shader.get());
    current_canvas_->drawRect(quad_rect, paint);
  } else {
    current_canvas_->drawBitmapRectToRect(*bitmap,
                                          &sk_uv_rect,
                                          quad_rect,
                                          &current_paint_);
  }

  if (needs_layer)
    current_canvas_->restore();
}

void SoftwareRenderer::DrawTileQuad(const DrawingFrame* frame,
                                    const TileDrawQuad* quad) {
  DCHECK(!output_surface_->ForcedDrawToSoftwareDevice());
  DCHECK(IsSoftwareResource(quad->resource_id));

  ResourceProvider::ScopedReadLockSoftware lock(resource_provider_,
                                                quad->resource_id);
  if (!lock.valid())
    return;
  DCHECK_EQ(GL_CLAMP_TO_EDGE, lock.wrap_mode());

  gfx::RectF visible_tex_coord_rect = MathUtil::ScaleRectProportional(
      quad->tex_coord_rect, quad->rect, quad->visible_rect);
  gfx::RectF visible_quad_vertex_rect = MathUtil::ScaleRectProportional(
      QuadVertexRect(), quad->rect, quad->visible_rect);

  SkRect uv_rect = gfx::RectFToSkRect(visible_tex_coord_rect);
  current_paint_.setFilterLevel(SkPaint::kLow_FilterLevel);
  current_canvas_->drawBitmapRectToRect(
      *lock.sk_bitmap(),
      &uv_rect,
      gfx::RectFToSkRect(visible_quad_vertex_rect),
      &current_paint_);
}

void SoftwareRenderer::DrawRenderPassQuad(const DrawingFrame* frame,
                                          const RenderPassDrawQuad* quad) {
  ScopedResource* content_texture =
      render_pass_textures_.get(quad->render_pass_id);
  if (!content_texture || !content_texture->id())
    return;

  DCHECK(IsSoftwareResource(content_texture->id()));
  ResourceProvider::ScopedReadLockSoftware lock(resource_provider_,
                                                content_texture->id());
  if (!lock.valid())
    return;
  SkShader::TileMode content_tile_mode = WrapModeToTileMode(lock.wrap_mode());

  SkRect dest_rect = gfx::RectFToSkRect(QuadVertexRect());
  SkRect dest_visible_rect = gfx::RectFToSkRect(MathUtil::ScaleRectProportional(
      QuadVertexRect(), quad->rect, quad->visible_rect));
  SkRect content_rect = SkRect::MakeWH(quad->rect.width(), quad->rect.height());

  SkMatrix content_mat;
  content_mat.setRectToRect(content_rect, dest_rect,
                            SkMatrix::kFill_ScaleToFit);

  const SkBitmap* content = lock.sk_bitmap();

  SkBitmap filter_bitmap;
  if (!quad->filters.IsEmpty()) {
    skia::RefPtr<SkImageFilter> filter = RenderSurfaceFilters::BuildImageFilter(
        quad->filters, content_texture->size());
    // TODO(ajuma): In addition origin translation, the canvas should also be
    // scaled to accomodate device pixel ratio and pinch zoom. See
    // crbug.com/281516 and crbug.com/281518.
    // TODO(ajuma): Apply the filter in the same pass as the content where
    // possible (e.g. when there's no origin offset). See crbug.com/308201.
    if (filter) {
      SkImageInfo info = SkImageInfo::MakeN32Premul(
          content_texture->size().width(), content_texture->size().height());
      if (filter_bitmap.allocPixels(info)) {
        SkCanvas canvas(filter_bitmap);
        SkPaint paint;
        paint.setImageFilter(filter.get());
        canvas.clear(SK_ColorTRANSPARENT);
        canvas.translate(SkIntToScalar(-quad->rect.origin().x()),
                         SkIntToScalar(-quad->rect.origin().y()));
        canvas.drawSprite(*content, 0, 0, &paint);
      }
    }
  }

  skia::RefPtr<SkShader> shader;
  if (filter_bitmap.isNull()) {
    shader = skia::AdoptRef(SkShader::CreateBitmapShader(
        *content, content_tile_mode, content_tile_mode, &content_mat));
  } else {
    shader = skia::AdoptRef(SkShader::CreateBitmapShader(
        filter_bitmap, content_tile_mode, content_tile_mode, &content_mat));
  }
  current_paint_.setShader(shader.get());

  if (quad->mask_resource_id) {
    ResourceProvider::ScopedReadLockSoftware mask_lock(resource_provider_,
                                                       quad->mask_resource_id);
    if (!lock.valid())
      return;
    SkShader::TileMode mask_tile_mode = WrapModeToTileMode(
        mask_lock.wrap_mode());

    const SkBitmap* mask = mask_lock.sk_bitmap();

    SkRect mask_rect = SkRect::MakeXYWH(
        quad->mask_uv_rect.x() * mask->width(),
        quad->mask_uv_rect.y() * mask->height(),
        quad->mask_uv_rect.width() * mask->width(),
        quad->mask_uv_rect.height() * mask->height());

    SkMatrix mask_mat;
    mask_mat.setRectToRect(mask_rect, dest_rect, SkMatrix::kFill_ScaleToFit);

    skia::RefPtr<SkShader> mask_shader =
        skia::AdoptRef(SkShader::CreateBitmapShader(
            *mask, mask_tile_mode, mask_tile_mode, &mask_mat));

    SkPaint mask_paint;
    mask_paint.setShader(mask_shader.get());

    SkLayerRasterizer::Builder builder;
    builder.addLayer(mask_paint);

    skia::RefPtr<SkLayerRasterizer> mask_rasterizer =
        skia::AdoptRef(builder.detachRasterizer());

    current_paint_.setRasterizer(mask_rasterizer.get());
    current_canvas_->drawRect(dest_visible_rect, current_paint_);
  } else {
    // TODO(skaslev): Apply background filters and blend with content
    current_canvas_->drawRect(dest_visible_rect, current_paint_);
  }
}

void SoftwareRenderer::DrawUnsupportedQuad(const DrawingFrame* frame,
                                           const DrawQuad* quad) {
#ifdef NDEBUG
  current_paint_.setColor(SK_ColorWHITE);
#else
  current_paint_.setColor(SK_ColorMAGENTA);
#endif
  current_paint_.setAlpha(quad->opacity() * 255);
  current_canvas_->drawRect(gfx::RectFToSkRect(QuadVertexRect()),
                            current_paint_);
}

void SoftwareRenderer::CopyCurrentRenderPassToBitmap(
    DrawingFrame* frame,
    scoped_ptr<CopyOutputRequest> request) {
  gfx::Rect copy_rect = frame->current_render_pass->output_rect;
  if (request->has_area())
    copy_rect.Intersect(request->area());
  gfx::Rect window_copy_rect = MoveFromDrawToWindowSpace(copy_rect);

  scoped_ptr<SkBitmap> bitmap(new SkBitmap);
  bitmap->setConfig(SkBitmap::kARGB_8888_Config,
                    window_copy_rect.width(),
                    window_copy_rect.height());
  current_canvas_->readPixels(
      bitmap.get(), window_copy_rect.x(), window_copy_rect.y());

  request->SendBitmapResult(bitmap.Pass());
}

void SoftwareRenderer::DiscardBackbuffer() {
  if (is_backbuffer_discarded_)
    return;

  output_surface_->DiscardBackbuffer();

  is_backbuffer_discarded_ = true;

  // Damage tracker needs a full reset every time framebuffer is discarded.
  client_->SetFullRootLayerDamage();
}

void SoftwareRenderer::EnsureBackbuffer() {
  if (!is_backbuffer_discarded_)
    return;

  output_surface_->EnsureBackbuffer();
  is_backbuffer_discarded_ = false;
}

void SoftwareRenderer::DidChangeVisibility() {
  if (visible())
    EnsureBackbuffer();
  else
    DiscardBackbuffer();
}

}  // namespace cc
