// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "skia/ext/analysis_canvas.h"
#include "third_party/skia/include/core/SkDraw.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/core/SkShader.h"

namespace {

const int kNoLayer = -1;

bool ActsLikeClear(SkBlendMode mode, unsigned src_alpha) {
  switch (mode) {
    case SkBlendMode::kClear:
      return true;
    case SkBlendMode::kSrc:
    case SkBlendMode::kSrcIn:
    case SkBlendMode::kDstIn:
    case SkBlendMode::kSrcOut:
    case SkBlendMode::kDstATop:
      return src_alpha == 0;
    case SkBlendMode::kDstOut:
      return src_alpha == 0xFF;
    default:
      return false;
  }
}

bool IsSolidColorPaint(const SkPaint& paint) {
  SkBlendMode blendmode = paint.getBlendMode();

  // Paint is solid color if the following holds:
  // - Alpha is 1.0, style is fill, and there are no special effects
  // - Xfer mode is either kSrc or kSrcOver (kSrcOver is equivalent
  //   to kSrc if source alpha is 1.0, which is already checked).
  return (
      paint.getAlpha() == 255 && !paint.getShader() && !paint.getLooper() &&
      !paint.getMaskFilter() && !paint.getColorFilter() &&
      !paint.getImageFilter() && paint.getStyle() == SkPaint::kFill_Style &&
      (blendmode == SkBlendMode::kSrc || blendmode == SkBlendMode::kSrcOver));
}

// Returns true if the specified drawn_rect will cover the entire canvas, and
// that the canvas is not clipped (i.e. it covers ALL of the canvas).
bool IsFullQuad(SkCanvas* canvas, const SkRect& drawn_rect) {
  if (!canvas->isClipRect())
    return false;

  SkIRect clip_irect;
  if (!canvas->getClipDeviceBounds(&clip_irect))
    return false;

  // if the clip is smaller than the canvas, we're partly clipped, so abort.
  if (!clip_irect.contains(SkIRect::MakeSize(canvas->getBaseLayerSize())))
    return false;

  const SkMatrix& matrix = canvas->getTotalMatrix();
  // If the transform results in a non-axis aligned
  // rect, then be conservative and return false.
  if (!matrix.rectStaysRect())
    return false;

  SkRect device_rect;
  matrix.mapRect(&device_rect, drawn_rect);
  SkRect clip_rect;
  clip_rect.set(clip_irect);
  return device_rect.contains(clip_rect);
}

} // namespace

namespace skia {

void AnalysisCanvas::SetForceNotSolid(bool flag) {
  is_forced_not_solid_ = flag;
  if (is_forced_not_solid_)
    is_solid_color_ = false;
}

void AnalysisCanvas::SetForceNotTransparent(bool flag) {
  is_forced_not_transparent_ = flag;
  if (is_forced_not_transparent_)
    is_transparent_ = false;
}

void AnalysisCanvas::onDrawPaint(const SkPaint& paint) {
  TRACE_EVENT0("disabled-by-default-skia", "AnalysisCanvas::onDrawPaint");
  SkRect rect;
  if (getClipBounds(&rect))
    drawRect(rect, paint);
}

void AnalysisCanvas::onDrawPoints(SkCanvas::PointMode mode,
                                  size_t count,
                                  const SkPoint points[],
                                  const SkPaint& paint) {
  TRACE_EVENT0("disabled-by-default-skia", "AnalysisCanvas::onDrawPoints");
  is_solid_color_ = false;
  is_transparent_ = false;
  ++draw_op_count_;
}

void AnalysisCanvas::onDrawRect(const SkRect& rect, const SkPaint& paint) {
  TRACE_EVENT0("disabled-by-default-skia", "AnalysisCanvas::onDrawRect");
  // This recreates the early-exit logic in SkCanvas.cpp.
  SkRect scratch;
  if (paint.canComputeFastBounds() &&
      quickReject(paint.computeFastBounds(rect, &scratch))) {
    TRACE_EVENT_INSTANT0("disabled-by-default-skia", "Quick reject.",
                         TRACE_EVENT_SCOPE_THREAD);
    ++rejected_op_count_;
    return;
  }

  // An extra no-op check SkCanvas.cpp doesn't do.
  if (paint.nothingToDraw()) {
    TRACE_EVENT_INSTANT0("disabled-by-default-skia", "Nothing to draw.",
                         TRACE_EVENT_SCOPE_THREAD);
    return;
  }

  bool does_cover_canvas = IsFullQuad(this, rect);

  SkBlendMode blendmode = paint.getBlendMode();

  // This canvas will become transparent if the following holds:
  // - The quad is a full tile quad
  // - We're not in "forced not transparent" mode
  // - Transfer mode is clear (0 color, 0 alpha)
  //
  // If the paint alpha is not 0, or if the transfrer mode is
  // not src, then this canvas will not be transparent.
  //
  // In all other cases, we keep the current transparent value
  if (does_cover_canvas && !is_forced_not_transparent_ &&
      ActsLikeClear(blendmode, paint.getAlpha())) {
    is_transparent_ = true;
  } else if (paint.getAlpha() != 0 || blendmode != SkBlendMode::kSrc) {
    is_transparent_ = false;
  }

  // This bitmap is solid if and only if the following holds.
  // Note that this might be overly conservative:
  // - We're not in "forced not solid" mode
  // - Paint is solid color
  // - The quad is a full tile quad
  if (!is_forced_not_solid_ && IsSolidColorPaint(paint) && does_cover_canvas) {
    is_solid_color_ = true;
    color_ = paint.getColor();
  } else {
    is_solid_color_ = false;
  }
  ++draw_op_count_;
}

void AnalysisCanvas::onDrawOval(const SkRect& oval, const SkPaint& paint) {
  TRACE_EVENT0("disabled-by-default-skia", "AnalysisCanvas::onDrawOval");
  is_solid_color_ = false;
  is_transparent_ = false;
  ++draw_op_count_;
}

void AnalysisCanvas::onDrawArc(const SkRect& oval,
                               SkScalar startAngle,
                               SkScalar sweepAngle,
                               bool useCenter,
                               const SkPaint& paint) {
  TRACE_EVENT0("disabled-by-default-skia", "AnalysisCanvas::onDrawArc");
  is_solid_color_ = false;
  is_transparent_ = false;
  ++draw_op_count_;
}

void AnalysisCanvas::onDrawRRect(const SkRRect& rr, const SkPaint& paint) {
  TRACE_EVENT0("disabled-by-default-skia", "AnalysisCanvas::onDrawRRect");
  // This should add the SkRRect to an SkPath, and call
  // drawPath, but since drawPath ignores the SkPath, just
  // do the same work here.
  is_solid_color_ = false;
  is_transparent_ = false;
  ++draw_op_count_;
}

void AnalysisCanvas::onDrawPath(const SkPath& path, const SkPaint& paint) {
  TRACE_EVENT0("disabled-by-default-skia", "AnalysisCanvas::onDrawPath");
  is_solid_color_ = false;
  is_transparent_ = false;
  ++draw_op_count_;
}

void AnalysisCanvas::onDrawBitmap(const SkBitmap& bitmap,
                                  SkScalar left,
                                  SkScalar top,
                                  const SkPaint*) {
  TRACE_EVENT0("disabled-by-default-skia", "AnalysisCanvas::onDrawBitmap");
  is_solid_color_ = false;
  is_transparent_ = false;
  ++draw_op_count_;
}

void AnalysisCanvas::onDrawBitmapRect(const SkBitmap&,
                                      const SkRect* src,
                                      const SkRect& dst,
                                      const SkPaint* paint,
                                      SrcRectConstraint) {
  TRACE_EVENT0("disabled-by-default-skia", "AnalysisCanvas::onDrawBitmapRect");
  // Call drawRect to determine transparency,
  // but reset solid color to false.
  SkPaint tmpPaint;
  if (!paint)
    paint = &tmpPaint;
  drawRect(dst, *paint);
  is_solid_color_ = false;
  ++draw_op_count_;
}

void AnalysisCanvas::onDrawBitmapNine(const SkBitmap& bitmap,
                                      const SkIRect& center,
                                      const SkRect& dst,
                                      const SkPaint* paint) {
  TRACE_EVENT0("disabled-by-default-skia", "AnalysisCanvas::onDrawBitmapNine");
  is_solid_color_ = false;
  is_transparent_ = false;
  ++draw_op_count_;
}

void AnalysisCanvas::onDrawImage(const SkImage*,
                                 SkScalar left,
                                 SkScalar top,
                                 const SkPaint*) {
  TRACE_EVENT0("disabled-by-default-skia", "AnalysisCanvas::onDrawImage");
  is_solid_color_ = false;
  is_transparent_ = false;
  ++draw_op_count_;
}

void AnalysisCanvas::onDrawImageRect(const SkImage*,
                                     const SkRect* src,
                                     const SkRect& dst,
                                     const SkPaint* paint,
                                     SrcRectConstraint) {
  TRACE_EVENT0("disabled-by-default-skia", "AnalysisCanvas::onDrawImageRect");
  // Call drawRect to determine transparency,
  // but reset solid color to false.
  SkPaint tmpPaint;
  if (!paint)
    paint = &tmpPaint;
  drawRect(dst, *paint);
  is_solid_color_ = false;
  ++draw_op_count_;
}

void AnalysisCanvas::onDrawText(const void* text,
                                size_t len,
                                SkScalar x,
                                SkScalar y,
                                const SkPaint& paint) {
  TRACE_EVENT0("disabled-by-default-skia", "AnalysisCanvas::onDrawText");
  is_solid_color_ = false;
  is_transparent_ = false;
  ++draw_op_count_;
}

void AnalysisCanvas::onDrawPosText(const void* text,
                                   size_t byteLength,
                                   const SkPoint pos[],
                                   const SkPaint& paint) {
  TRACE_EVENT0("disabled-by-default-skia", "AnalysisCanvas::onDrawPosText");
  is_solid_color_ = false;
  is_transparent_ = false;
  ++draw_op_count_;
}

void AnalysisCanvas::onDrawPosTextH(const void* text,
                                    size_t byteLength,
                                    const SkScalar xpos[],
                                    SkScalar constY,
                                    const SkPaint& paint) {
  TRACE_EVENT0("disabled-by-default-skia", "AnalysisCanvas::onDrawPosTextH");
  is_solid_color_ = false;
  is_transparent_ = false;
  ++draw_op_count_;
}

void AnalysisCanvas::onDrawTextOnPath(const void* text,
                                      size_t len,
                                      const SkPath& path,
                                      const SkMatrix* matrix,
                                      const SkPaint& paint) {
  TRACE_EVENT0("disabled-by-default-skia", "AnalysisCanvas::onDrawTextOnPath");
  is_solid_color_ = false;
  is_transparent_ = false;
  ++draw_op_count_;
}

void AnalysisCanvas::onDrawTextBlob(const SkTextBlob* blob,
                                    SkScalar x,
                                    SkScalar y,
                                    const SkPaint &paint) {
  TRACE_EVENT0("disabled-by-default-skia", "AnalysisCanvas::onDrawTextBlob");
  is_solid_color_ = false;
  is_transparent_ = false;
  ++draw_op_count_;
}

void AnalysisCanvas::onDrawDRRect(const SkRRect& outer,
                                  const SkRRect& inner,
                                  const SkPaint& paint) {
  TRACE_EVENT0("disabled-by-default-skia", "AnalysisCanvas::onDrawDRRect");
  is_solid_color_ = false;
  is_transparent_ = false;
  ++draw_op_count_;
}

void AnalysisCanvas::onDrawVertices(SkCanvas::VertexMode,
                                    int vertex_count,
                                    const SkPoint verts[],
                                    const SkPoint texs[],
                                    const SkColor colors[],
                                    SkBlendMode bmode,
                                    const uint16_t indices[],
                                    int index_count,
                                    const SkPaint& paint) {
  TRACE_EVENT0("disabled-by-default-skia", "AnalysisCanvas::onDrawVertices");
  is_solid_color_ = false;
  is_transparent_ = false;
  ++draw_op_count_;
}

// Needed for now, since SkCanvas requires a bitmap, even if it is not backed
// by any pixels
static SkBitmap MakeEmptyBitmap(int width, int height) {
  SkBitmap bitmap;
  bitmap.setInfo(SkImageInfo::MakeUnknown(width, height));
  return bitmap;
}

AnalysisCanvas::AnalysisCanvas(int width, int height)
    : INHERITED(MakeEmptyBitmap(width, height)),
      saved_stack_size_(0),
      force_not_solid_stack_level_(kNoLayer),
      force_not_transparent_stack_level_(kNoLayer),
      is_forced_not_solid_(false),
      is_forced_not_transparent_(false),
      is_solid_color_(true),
      color_(SK_ColorTRANSPARENT),
      is_transparent_(true),
      draw_op_count_(0),
      rejected_op_count_(0) {
}

AnalysisCanvas::~AnalysisCanvas() {}

bool AnalysisCanvas::GetColorIfSolid(SkColor* color) const {
  if (is_transparent_) {
    *color = SK_ColorTRANSPARENT;
    return true;
  }
  if (is_solid_color_) {
    *color = color_;
    return true;
  }
  return false;
}

bool AnalysisCanvas::abort() {
  // Early out as soon as we have more than 1 draw op or 5 rejected ops.
  // TODO(vmpstr): Investigate if 1 and 5 are the correct metrics here. We need
  // to balance the amount of time we spend analyzing vs how many tiles would be
  // solid if the numbers were higher.
  if (draw_op_count_ > 1 || rejected_op_count_ > 5) {
    TRACE_EVENT0("disabled-by-default-skia",
                 "AnalysisCanvas::abort() -- aborting");
    // We have to reset solid/transparent state to false since we don't
    // know whether consequent operations will make this false.
    is_solid_color_ = false;
    is_transparent_ = false;
    return true;
  }
  return false;
}

void AnalysisCanvas::OnComplexClip() {
  // complex clips can make our calls to IsFullQuad invalid (ie have false
  // positives). As a precaution, force the setting to be non-solid
  // and non-transparent until we pop this
  if (force_not_solid_stack_level_ == kNoLayer) {
    force_not_solid_stack_level_ = saved_stack_size_;
    SetForceNotSolid(true);
  }
  if (force_not_transparent_stack_level_ == kNoLayer) {
    force_not_transparent_stack_level_ = saved_stack_size_;
    SetForceNotTransparent(true);
  }
}

void AnalysisCanvas::onClipRect(const SkRect& rect,
                                SkRegion::Op op,
                                ClipEdgeStyle edge_style) {
  INHERITED::onClipRect(rect, op, edge_style);
}

void AnalysisCanvas::onClipPath(const SkPath& path,
                                SkRegion::Op op,
                                ClipEdgeStyle edge_style) {
  OnComplexClip();
  INHERITED::onClipRect(path.getBounds(), op, edge_style);
}

bool doesCoverCanvas(const SkRRect& rr,
                     const SkMatrix& total_matrix,
                     const SkIRect& clip_device_bounds) {
  const SkRect& bounding_rect = rr.rect();

  // We cannot handle non axis aligned rectangles at the moment.
  if (!total_matrix.isScaleTranslate()) {
    return false;
  }

  SkMatrix inverse;
  if (!total_matrix.invert(&inverse)) {
    return false;
  }

  SkRect clip_rect = SkRect::Make(clip_device_bounds);
  inverse.mapRectScaleTranslate(&clip_rect, clip_rect);
  return rr.contains(clip_rect);
}

void AnalysisCanvas::onClipRRect(const SkRRect& rrect,
                                 SkRegion::Op op,
                                 ClipEdgeStyle edge_style) {
  SkIRect clip_device_bounds;
  if (getClipDeviceBounds(&clip_device_bounds) &&
      doesCoverCanvas(rrect, getTotalMatrix(), clip_device_bounds)) {
    // If the canvas is fully contained within the clip, it is as if we weren't
    // clipped at all, so bail early.
    return;
  }

  OnComplexClip();
  INHERITED::onClipRect(rrect.getBounds(), op, edge_style);
}

void AnalysisCanvas::onClipRegion(const SkRegion& deviceRgn, SkRegion::Op op) {
  const ClipEdgeStyle edge_style = kHard_ClipEdgeStyle;
  if (deviceRgn.isRect()) {
    onClipRect(SkRect::MakeFromIRect(deviceRgn.getBounds()), op, edge_style);
    return;
  }
  OnComplexClip();
  INHERITED::onClipRect(
      SkRect::MakeFromIRect(deviceRgn.getBounds()), op, edge_style);
}

void AnalysisCanvas::willSave() {
  ++saved_stack_size_;
  INHERITED::willSave();
}

SkCanvas::SaveLayerStrategy AnalysisCanvas::getSaveLayerStrategy(
    const SaveLayerRec& rec) {
  const SkPaint* paint = rec.fPaint;

  ++saved_stack_size_;

  SkIRect canvas_ibounds = SkIRect::MakeSize(this->getBaseLayerSize());
  SkRect canvas_bounds;
  canvas_bounds.set(canvas_ibounds);

  // If after we draw to the saved layer, we have to blend with the current
  // layer, then we can conservatively say that the canvas will not be of
  // solid color.
  if ((paint && !IsSolidColorPaint(*paint)) ||
      (rec.fBounds && !rec.fBounds->contains(canvas_bounds))) {
    if (force_not_solid_stack_level_ == kNoLayer) {
      force_not_solid_stack_level_ = saved_stack_size_;
      SetForceNotSolid(true);
    }
  }

  // If after we draw to the save layer, we have to blend with the current
  // layer using any part of the current layer's alpha, then we can
  // conservatively say that the canvas will not be transparent.
  SkBlendMode blendmode = SkBlendMode::kSrc;
  if (paint)
    blendmode = paint->getBlendMode();
  if (blendmode != SkBlendMode::kDst) {
    if (force_not_transparent_stack_level_ == kNoLayer) {
      force_not_transparent_stack_level_ = saved_stack_size_;
      SetForceNotTransparent(true);
    }
  }

  INHERITED::getSaveLayerStrategy(rec);
  // Actually saving a layer here could cause a new bitmap to be created
  // and real rendering to occur.
  return kNoLayer_SaveLayerStrategy;
}

void AnalysisCanvas::willRestore() {
  DCHECK(saved_stack_size_);
  if (saved_stack_size_) {
    --saved_stack_size_;
    if (saved_stack_size_ < force_not_solid_stack_level_) {
      SetForceNotSolid(false);
      force_not_solid_stack_level_ = kNoLayer;
    }
    if (saved_stack_size_ < force_not_transparent_stack_level_) {
      SetForceNotTransparent(false);
      force_not_transparent_stack_level_ = kNoLayer;
    }
  }

  INHERITED::willRestore();
}

}  // namespace skia


