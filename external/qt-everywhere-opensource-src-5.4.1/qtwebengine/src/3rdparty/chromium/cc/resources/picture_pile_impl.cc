// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <limits>

#include "base/debug/trace_event.h"
#include "cc/base/region.h"
#include "cc/debug/debug_colors.h"
#include "cc/resources/picture_pile_impl.h"
#include "cc/resources/raster_worker_pool.h"
#include "skia/ext/analysis_canvas.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkSize.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/skia_util.h"

namespace cc {

PicturePileImpl::ClonesForDrawing::ClonesForDrawing(
    const PicturePileImpl* pile, int num_threads) {
  for (int i = 0; i < num_threads; i++) {
    scoped_refptr<PicturePileImpl> clone =
        PicturePileImpl::CreateCloneForDrawing(pile, i);
    clones_.push_back(clone);
  }
}

PicturePileImpl::ClonesForDrawing::~ClonesForDrawing() {
}

scoped_refptr<PicturePileImpl> PicturePileImpl::Create() {
  return make_scoped_refptr(new PicturePileImpl);
}

scoped_refptr<PicturePileImpl> PicturePileImpl::CreateFromOther(
    const PicturePileBase* other) {
  return make_scoped_refptr(new PicturePileImpl(other));
}

scoped_refptr<PicturePileImpl> PicturePileImpl::CreateCloneForDrawing(
    const PicturePileImpl* other, unsigned thread_index) {
  return make_scoped_refptr(new PicturePileImpl(other, thread_index));
}

PicturePileImpl::PicturePileImpl()
    : clones_for_drawing_(ClonesForDrawing(this, 0)) {
}

PicturePileImpl::PicturePileImpl(const PicturePileBase* other)
    : PicturePileBase(other),
      clones_for_drawing_(ClonesForDrawing(
                              this, RasterWorkerPool::GetNumRasterThreads())) {
}

PicturePileImpl::PicturePileImpl(
    const PicturePileImpl* other, unsigned thread_index)
    : PicturePileBase(other, thread_index),
      clones_for_drawing_(ClonesForDrawing(this, 0)) {
}

PicturePileImpl::~PicturePileImpl() {
}

PicturePileImpl* PicturePileImpl::GetCloneForDrawingOnThread(
    unsigned thread_index) const {
  CHECK_GT(clones_for_drawing_.clones_.size(), thread_index);
  return clones_for_drawing_.clones_[thread_index].get();
}

void PicturePileImpl::RasterDirect(
    SkCanvas* canvas,
    const gfx::Rect& canvas_rect,
    float contents_scale,
    RenderingStatsInstrumentation* rendering_stats_instrumentation) {
  RasterCommon(canvas,
               NULL,
               canvas_rect,
               contents_scale,
               rendering_stats_instrumentation,
               false);
}

void PicturePileImpl::RasterForAnalysis(
    skia::AnalysisCanvas* canvas,
    const gfx::Rect& canvas_rect,
    float contents_scale,
    RenderingStatsInstrumentation* stats_instrumentation) {
  RasterCommon(
      canvas, canvas, canvas_rect, contents_scale, stats_instrumentation, true);
}

void PicturePileImpl::RasterToBitmap(
    SkCanvas* canvas,
    const gfx::Rect& canvas_rect,
    float contents_scale,
    RenderingStatsInstrumentation* rendering_stats_instrumentation) {
  canvas->discard();
  if (clear_canvas_with_debug_color_) {
    // Any non-painted areas in the content bounds will be left in this color.
    canvas->clear(DebugColors::NonPaintedFillColor());
  }

  // If this picture has opaque contents, it is guaranteeing that it will
  // draw an opaque rect the size of the layer.  If it is not, then we must
  // clear this canvas ourselves.
  if (contents_opaque_ || contents_fill_bounds_completely_) {
    // Even if completely covered, for rasterizations that touch the edge of the
    // layer, we also need to raster the background color underneath the last
    // texel (since the recording won't cover it) and outside the last texel
    // (due to linear filtering when using this texture).
    gfx::Rect content_tiling_rect = gfx::ToEnclosingRect(
        gfx::ScaleRect(tiling_.tiling_rect(), contents_scale));

    // The final texel of content may only be partially covered by a
    // rasterization; this rect represents the content rect that is fully
    // covered by content.
    gfx::Rect deflated_content_tiling_rect = content_tiling_rect;
    deflated_content_tiling_rect.Inset(0, 0, 1, 1);
    if (!deflated_content_tiling_rect.Contains(canvas_rect)) {
      if (clear_canvas_with_debug_color_) {
        // Any non-painted areas outside of the content bounds are left in
        // this color.  If this is seen then it means that cc neglected to
        // rerasterize a tile that used to intersect with the content rect
        // after the content bounds grew.
        canvas->save();
        canvas->translate(-canvas_rect.x(), -canvas_rect.y());
        canvas->clipRect(gfx::RectToSkRect(content_tiling_rect),
                         SkRegion::kDifference_Op);
        canvas->drawColor(DebugColors::MissingResizeInvalidations(),
                          SkXfermode::kSrc_Mode);
        canvas->restore();
      }

      // Drawing at most 2 x 2 x (canvas width + canvas height) texels is 2-3X
      // faster than clearing, so special case this.
      canvas->save();
      canvas->translate(-canvas_rect.x(), -canvas_rect.y());
      gfx::Rect inflated_content_tiling_rect = content_tiling_rect;
      inflated_content_tiling_rect.Inset(0, 0, -1, -1);
      canvas->clipRect(gfx::RectToSkRect(inflated_content_tiling_rect),
                       SkRegion::kReplace_Op);
      canvas->clipRect(gfx::RectToSkRect(deflated_content_tiling_rect),
                       SkRegion::kDifference_Op);
      canvas->drawColor(background_color_, SkXfermode::kSrc_Mode);
      canvas->restore();
    }
  } else {
    TRACE_EVENT_INSTANT0("cc", "SkCanvas::clear", TRACE_EVENT_SCOPE_THREAD);
    // Clearing is about ~4x faster than drawing a rect even if the content
    // isn't covering a majority of the canvas.
    canvas->clear(SK_ColorTRANSPARENT);
  }

  RasterCommon(canvas,
               NULL,
               canvas_rect,
               contents_scale,
               rendering_stats_instrumentation,
               false);
}

void PicturePileImpl::CoalesceRasters(const gfx::Rect& canvas_rect,
                                      const gfx::Rect& content_rect,
                                      float contents_scale,
                                      PictureRegionMap* results) {
  DCHECK(results);
  // Rasterize the collection of relevant picture piles.
  gfx::Rect layer_rect = gfx::ScaleToEnclosingRect(
      content_rect, 1.f / contents_scale);

  // Make sure pictures don't overlap by keeping track of previous right/bottom.
  int min_content_left = -1;
  int min_content_top = -1;
  int last_row_index = -1;
  int last_col_index = -1;
  gfx::Rect last_content_rect;

  // Coalesce rasters of the same picture into different rects:
  //  - Compute the clip of each of the pile chunks,
  //  - Subtract it from the canvas rect to get difference region
  //  - Later, use the difference region to subtract each of the comprising
  //    rects from the canvas.
  // Note that in essence, we're trying to mimic clipRegion with intersect op
  // that also respects the current canvas transform and clip. In order to use
  // the canvas transform, we must stick to clipRect operations (clipRegion
  // ignores the transform). Intersect then can be written as subtracting the
  // negation of the region we're trying to intersect. Luckily, we know that all
  // of the rects will have to fit into |content_rect|, so we can start with
  // that and subtract chunk rects to get the region that we need to subtract
  // from the canvas. Then, we can use clipRect with difference op to subtract
  // each rect in the region.
  bool include_borders = true;
  for (TilingData::Iterator tile_iter(&tiling_, layer_rect, include_borders);
       tile_iter;
       ++tile_iter) {
    PictureMap::iterator map_iter = picture_map_.find(tile_iter.index());
    if (map_iter == picture_map_.end())
      continue;
    PictureInfo& info = map_iter->second;
    Picture* picture = info.GetPicture();
    if (!picture)
      continue;

    // This is intentionally *enclosed* rect, so that the clip is aligned on
    // integral post-scale content pixels and does not extend past the edges
    // of the picture chunk's layer rect.  The min_contents_scale enforces that
    // enough buffer pixels have been added such that the enclosed rect
    // encompasses all invalidated pixels at any larger scale level.
    gfx::Rect chunk_rect = PaddedRect(tile_iter.index());
    gfx::Rect content_clip =
        gfx::ScaleToEnclosedRect(chunk_rect, contents_scale);
    DCHECK(!content_clip.IsEmpty()) << "Layer rect: "
                                    << picture->LayerRect().ToString()
                                    << "Contents scale: " << contents_scale;
    content_clip.Intersect(canvas_rect);

    // Make sure iterator goes top->bottom.
    DCHECK_GE(tile_iter.index_y(), last_row_index);
    if (tile_iter.index_y() > last_row_index) {
      // First tile in a new row.
      min_content_left = content_clip.x();
      min_content_top = last_content_rect.bottom();
    } else {
      // Make sure iterator goes left->right.
      DCHECK_GT(tile_iter.index_x(), last_col_index);
      min_content_left = last_content_rect.right();
      min_content_top = last_content_rect.y();
    }

    last_col_index = tile_iter.index_x();
    last_row_index = tile_iter.index_y();

    // Only inset if the content_clip is less than then previous min.
    int inset_left = std::max(0, min_content_left - content_clip.x());
    int inset_top = std::max(0, min_content_top - content_clip.y());
    content_clip.Inset(inset_left, inset_top, 0, 0);

    PictureRegionMap::iterator it = results->find(picture);
    Region* clip_region;
    if (it == results->end()) {
      // The clip for a set of coalesced pictures starts out clipping the entire
      // canvas.  Each picture added to the set must subtract its own bounds
      // from the clip region, poking a hole so that the picture is unclipped.
      clip_region = &(*results)[picture];
      *clip_region = canvas_rect;
    } else {
      clip_region = &it->second;
    }

    DCHECK(clip_region->Contains(content_clip))
        << "Content clips should not overlap.";
    clip_region->Subtract(content_clip);
    last_content_rect = content_clip;
  }
}

void PicturePileImpl::RasterCommon(
    SkCanvas* canvas,
    SkDrawPictureCallback* callback,
    const gfx::Rect& canvas_rect,
    float contents_scale,
    RenderingStatsInstrumentation* rendering_stats_instrumentation,
    bool is_analysis) {
  DCHECK(contents_scale >= min_contents_scale_);

  canvas->translate(-canvas_rect.x(), -canvas_rect.y());
  gfx::Rect content_tiling_rect = gfx::ToEnclosingRect(
      gfx::ScaleRect(tiling_.tiling_rect(), contents_scale));
  content_tiling_rect.Intersect(canvas_rect);

  canvas->clipRect(gfx::RectToSkRect(content_tiling_rect),
                   SkRegion::kIntersect_Op);

  PictureRegionMap picture_region_map;
  CoalesceRasters(
      canvas_rect, content_tiling_rect, contents_scale, &picture_region_map);

#ifndef NDEBUG
  Region total_clip;
#endif  // NDEBUG

  // Iterate the coalesced map and use each picture's region
  // to clip the canvas.
  for (PictureRegionMap::iterator it = picture_region_map.begin();
       it != picture_region_map.end();
       ++it) {
    Picture* picture = it->first;
    Region negated_clip_region = it->second;

#ifndef NDEBUG
    Region positive_clip = content_tiling_rect;
    positive_clip.Subtract(negated_clip_region);
    // Make sure we never rasterize the same region twice.
    DCHECK(!total_clip.Intersects(positive_clip));
    total_clip.Union(positive_clip);
#endif  // NDEBUG

    base::TimeDelta best_duration = base::TimeDelta::Max();
    int repeat_count = std::max(1, slow_down_raster_scale_factor_for_debug_);
    int rasterized_pixel_count = 0;

    for (int j = 0; j < repeat_count; ++j) {
      base::TimeTicks start_time;
      if (rendering_stats_instrumentation)
        start_time = rendering_stats_instrumentation->StartRecording();

      rasterized_pixel_count = picture->Raster(
          canvas, callback, negated_clip_region, contents_scale);

      if (rendering_stats_instrumentation) {
        base::TimeDelta duration =
            rendering_stats_instrumentation->EndRecording(start_time);
        best_duration = std::min(best_duration, duration);
      }
    }

    if (rendering_stats_instrumentation) {
      if (is_analysis) {
        rendering_stats_instrumentation->AddAnalysis(best_duration,
                                                     rasterized_pixel_count);
      } else {
        rendering_stats_instrumentation->AddRaster(best_duration,
                                                   rasterized_pixel_count);
      }
    }
  }

#ifndef NDEBUG
  // Fill the clip with debug color. This allows us to
  // distinguish between non painted areas and problems with missing
  // pictures.
  SkPaint paint;
  for (Region::Iterator it(total_clip); it.has_rect(); it.next())
    canvas->clipRect(gfx::RectToSkRect(it.rect()), SkRegion::kDifference_Op);
  paint.setColor(DebugColors::MissingPictureFillColor());
  paint.setXfermodeMode(SkXfermode::kSrc_Mode);
  canvas->drawPaint(paint);
#endif  // NDEBUG
}

skia::RefPtr<SkPicture> PicturePileImpl::GetFlattenedPicture() {
  TRACE_EVENT0("cc", "PicturePileImpl::GetFlattenedPicture");

  gfx::Rect tiling_rect(tiling_.tiling_rect());
  SkPictureRecorder recorder;
  SkCanvas* canvas =
      recorder.beginRecording(tiling_rect.width(), tiling_rect.height());
  if (!tiling_rect.IsEmpty())
    RasterToBitmap(canvas, tiling_rect, 1.0, NULL);
  skia::RefPtr<SkPicture> picture = skia::AdoptRef(recorder.endRecording());

  return picture;
}

void PicturePileImpl::AnalyzeInRect(
    const gfx::Rect& content_rect,
    float contents_scale,
    PicturePileImpl::Analysis* analysis) {
  AnalyzeInRect(content_rect, contents_scale, analysis, NULL);
}

void PicturePileImpl::AnalyzeInRect(
    const gfx::Rect& content_rect,
    float contents_scale,
    PicturePileImpl::Analysis* analysis,
    RenderingStatsInstrumentation* stats_instrumentation) {
  DCHECK(analysis);
  TRACE_EVENT0("cc", "PicturePileImpl::AnalyzeInRect");

  gfx::Rect layer_rect = gfx::ScaleToEnclosingRect(
      content_rect, 1.0f / contents_scale);

  layer_rect.Intersect(tiling_.tiling_rect());

  skia::AnalysisCanvas canvas(layer_rect.width(), layer_rect.height());

  RasterForAnalysis(&canvas, layer_rect, 1.0f, stats_instrumentation);

  analysis->is_solid_color = canvas.GetColorIfSolid(&analysis->solid_color);
}

// Since there are situations when we can skip analysis, the variables have to
// be set to their safest values. That is, we have to assume that the tile is
// not solid color. As well, we have to assume that the tile has text so we
// don't early out incorrectly.
PicturePileImpl::Analysis::Analysis() : is_solid_color(false) {
}

PicturePileImpl::Analysis::~Analysis() {
}

PicturePileImpl::PixelRefIterator::PixelRefIterator(
    const gfx::Rect& content_rect,
    float contents_scale,
    const PicturePileImpl* picture_pile)
    : picture_pile_(picture_pile),
      layer_rect_(
          gfx::ScaleToEnclosingRect(content_rect, 1.f / contents_scale)),
      tile_iterator_(&picture_pile_->tiling_,
                     layer_rect_,
                     false /* include_borders */) {
  // Early out if there isn't a single tile.
  if (!tile_iterator_)
    return;

  AdvanceToTilePictureWithPixelRefs();
}

PicturePileImpl::PixelRefIterator::~PixelRefIterator() {
}

PicturePileImpl::PixelRefIterator&
    PicturePileImpl::PixelRefIterator::operator++() {
  ++pixel_ref_iterator_;
  if (pixel_ref_iterator_)
    return *this;

  ++tile_iterator_;
  AdvanceToTilePictureWithPixelRefs();
  return *this;
}

void PicturePileImpl::PixelRefIterator::AdvanceToTilePictureWithPixelRefs() {
  for (; tile_iterator_; ++tile_iterator_) {
    PictureMap::const_iterator it =
        picture_pile_->picture_map_.find(tile_iterator_.index());
    if (it == picture_pile_->picture_map_.end())
      continue;

    const Picture* picture = it->second.GetPicture();
    if (!picture || (processed_pictures_.count(picture) != 0) ||
        !picture->WillPlayBackBitmaps())
      continue;

    processed_pictures_.insert(picture);
    pixel_ref_iterator_ = Picture::PixelRefIterator(layer_rect_, picture);
    if (pixel_ref_iterator_)
      break;
  }
}

void PicturePileImpl::DidBeginTracing() {
  std::set<void*> processed_pictures;
  for (PictureMap::iterator it = picture_map_.begin();
       it != picture_map_.end();
       ++it) {
    Picture* picture = it->second.GetPicture();
    if (picture && (processed_pictures.count(picture) == 0)) {
      picture->EmitTraceSnapshot();
      processed_pictures.insert(picture);
    }
  }
}

}  // namespace cc
