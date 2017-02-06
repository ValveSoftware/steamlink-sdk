// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/playback/raster_source.h"

#include <stddef.h>

#include "base/trace_event/trace_event.h"
#include "cc/base/math_util.h"
#include "cc/base/region.h"
#include "cc/debug/debug_colors.h"
#include "cc/playback/display_item_list.h"
#include "cc/playback/image_hijack_canvas.h"
#include "cc/playback/skip_image_canvas.h"
#include "skia/ext/analysis_canvas.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkClipStack.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace cc {

scoped_refptr<RasterSource> RasterSource::CreateFromRecordingSource(
    const RecordingSource* other,
    bool can_use_lcd_text) {
  return make_scoped_refptr(new RasterSource(other, can_use_lcd_text));
}

RasterSource::RasterSource(const RecordingSource* other, bool can_use_lcd_text)
    : display_list_(other->display_list_),
      painter_reported_memory_usage_(other->painter_reported_memory_usage_),
      background_color_(other->background_color_),
      requires_clear_(other->requires_clear_),
      can_use_lcd_text_(can_use_lcd_text),
      is_solid_color_(other->is_solid_color_),
      solid_color_(other->solid_color_),
      recorded_viewport_(other->recorded_viewport_),
      size_(other->size_),
      clear_canvas_with_debug_color_(other->clear_canvas_with_debug_color_),
      slow_down_raster_scale_factor_for_debug_(
          other->slow_down_raster_scale_factor_for_debug_),
      should_attempt_to_use_distance_field_text_(false),
      image_decode_controller_(nullptr) {
}

RasterSource::RasterSource(const RasterSource* other, bool can_use_lcd_text)
    : display_list_(other->display_list_),
      painter_reported_memory_usage_(other->painter_reported_memory_usage_),
      background_color_(other->background_color_),
      requires_clear_(other->requires_clear_),
      can_use_lcd_text_(can_use_lcd_text),
      is_solid_color_(other->is_solid_color_),
      solid_color_(other->solid_color_),
      recorded_viewport_(other->recorded_viewport_),
      size_(other->size_),
      clear_canvas_with_debug_color_(other->clear_canvas_with_debug_color_),
      slow_down_raster_scale_factor_for_debug_(
          other->slow_down_raster_scale_factor_for_debug_),
      should_attempt_to_use_distance_field_text_(
          other->should_attempt_to_use_distance_field_text_),
      image_decode_controller_(other->image_decode_controller_) {
}

RasterSource::~RasterSource() {
}

void RasterSource::PlaybackToCanvas(SkCanvas* raster_canvas,
                                    const gfx::Rect& canvas_bitmap_rect,
                                    const gfx::Rect& canvas_playback_rect,
                                    float contents_scale,
                                    const PlaybackSettings& settings) const {
  SkIRect raster_bounds = gfx::RectToSkIRect(canvas_bitmap_rect);
  if (!canvas_playback_rect.IsEmpty() &&
      !raster_bounds.intersect(gfx::RectToSkIRect(canvas_playback_rect)))
    return;
  // Treat all subnormal values as zero for performance.
  ScopedSubnormalFloatDisabler disabler;

  raster_canvas->save();
  raster_canvas->translate(-canvas_bitmap_rect.x(), -canvas_bitmap_rect.y());
  raster_canvas->clipRect(SkRect::MakeFromIRect(raster_bounds));
  raster_canvas->scale(contents_scale, contents_scale);
  PlaybackToCanvas(raster_canvas, settings);
  raster_canvas->restore();
}

void RasterSource::PlaybackToCanvas(SkCanvas* raster_canvas,
                                    const PlaybackSettings& settings) const {
  if (!settings.playback_to_shared_canvas)
    PrepareForPlaybackToCanvas(raster_canvas);

  if (settings.skip_images) {
    SkipImageCanvas canvas(raster_canvas);
    RasterCommon(&canvas, nullptr);
  } else if (settings.use_image_hijack_canvas) {
    const SkImageInfo& info = raster_canvas->imageInfo();

    ImageHijackCanvas canvas(info.width(), info.height(),
                             image_decode_controller_);
    // Before adding the canvas, make sure that the ImageHijackCanvas is aware
    // of the current transform and clip, which may affect the clip bounds.
    // Since we query the clip bounds of the current canvas to get the list of
    // draw commands to process, this is important to produce correct content.
    SkIRect raster_bounds;
    raster_canvas->getClipDeviceBounds(&raster_bounds);
    canvas.clipRect(SkRect::MakeFromIRect(raster_bounds));
    canvas.setMatrix(raster_canvas->getTotalMatrix());
    canvas.addCanvas(raster_canvas);

    RasterCommon(&canvas, nullptr);
  } else {
    RasterCommon(raster_canvas, nullptr);
  }
}

void RasterSource::PrepareForPlaybackToCanvas(SkCanvas* canvas) const {
  // TODO(hendrikw): See if we can split this up into separate functions.

  if (canvas->getClipStack()->quickContains(
          SkRect::MakeFromIRect(canvas->imageInfo().bounds()))) {
    canvas->discard();
  }

  // If this raster source has opaque contents, it is guaranteeing that it will
  // draw an opaque rect the size of the layer.  If it is not, then we must
  // clear this canvas ourselves.
  if (requires_clear_) {
    canvas->clear(SK_ColorTRANSPARENT);
    return;
  }

  if (clear_canvas_with_debug_color_)
    canvas->clear(DebugColors::NonPaintedFillColor());

  // If the canvas wants us to raster with complex transform, it is hard to
  // determine the exact region we must clear. Just clear everything.
  // TODO(trchen): Optimize the common case that transformed content bounds
  //               covers the whole clip region.
  if (!canvas->getTotalMatrix().rectStaysRect()) {
    canvas->clear(SK_ColorTRANSPARENT);
    return;
  }

  SkRect content_device_rect;
  canvas->getTotalMatrix().mapRect(
      &content_device_rect, SkRect::MakeWH(size_.width(), size_.height()));

  // The final texel of content may only be partially covered by a
  // rasterization; this rect represents the content rect that is fully
  // covered by content.
  SkIRect opaque_rect;
  content_device_rect.roundIn(&opaque_rect);

  SkIRect raster_bounds;
  canvas->getClipDeviceBounds(&raster_bounds);

  if (opaque_rect.contains(raster_bounds))
    return;

  // Even if completely covered, for rasterizations that touch the edge of the
  // layer, we also need to raster the background color underneath the last
  // texel (since the recording won't cover it) and outside the last texel
  // (due to linear filtering when using this texture).
  SkIRect interest_rect;
  content_device_rect.roundOut(&interest_rect);
  interest_rect.outset(1, 1);

  if (clear_canvas_with_debug_color_) {
    // Any non-painted areas outside of the content bounds are left in
    // this color.  If this is seen then it means that cc neglected to
    // rerasterize a tile that used to intersect with the content rect
    // after the content bounds grew.
    canvas->save();
    // Use clipRegion to bypass CTM because the rects are device rects.
    SkRegion interest_region;
    interest_region.setRect(interest_rect);
    canvas->clipRegion(interest_region, SkRegion::kDifference_Op);
    canvas->clear(DebugColors::MissingResizeInvalidations());
    canvas->restore();
  }

  // Drawing at most 2 x 2 x (canvas width + canvas height) texels is 2-3X
  // faster than clearing, so special case this.
  canvas->save();
  // Use clipRegion to bypass CTM because the rects are device rects.
  SkRegion interest_region;
  interest_region.setRect(interest_rect);
  interest_region.op(opaque_rect, SkRegion::kDifference_Op);
  canvas->clipRegion(interest_region);
  canvas->clear(background_color_);
  canvas->restore();
}

void RasterSource::RasterCommon(SkCanvas* canvas,
                                SkPicture::AbortCallback* callback) const {
  DCHECK(display_list_.get());
  int repeat_count = std::max(1, slow_down_raster_scale_factor_for_debug_);
  for (int i = 0; i < repeat_count; ++i)
    display_list_->Raster(canvas, callback);
}

sk_sp<SkPicture> RasterSource::GetFlattenedPicture() {
  TRACE_EVENT0("cc", "RasterSource::GetFlattenedPicture");

  SkPictureRecorder recorder;
  SkCanvas* canvas = recorder.beginRecording(size_.width(), size_.height());
  if (!size_.IsEmpty()) {
    PrepareForPlaybackToCanvas(canvas);
    RasterCommon(canvas, nullptr);
  }

  return recorder.finishRecordingAsPicture();
}

size_t RasterSource::GetPictureMemoryUsage() const {
  if (!display_list_)
    return 0;
  return display_list_->ApproximateMemoryUsage() +
         painter_reported_memory_usage_;
}

bool RasterSource::PerformSolidColorAnalysis(const gfx::Rect& content_rect,
                                             float contents_scale,
                                             SkColor* color) const {
  TRACE_EVENT0("cc", "RasterSource::PerformSolidColorAnalysis");

  gfx::Rect layer_rect =
      gfx::ScaleToEnclosingRect(content_rect, 1.0f / contents_scale);

  layer_rect.Intersect(gfx::Rect(size_));
  skia::AnalysisCanvas canvas(layer_rect.width(), layer_rect.height());
  canvas.translate(-layer_rect.x(), -layer_rect.y());
  RasterCommon(&canvas, &canvas);
  return canvas.GetColorIfSolid(color);
}

void RasterSource::GetDiscardableImagesInRect(
    const gfx::Rect& layer_rect,
    float raster_scale,
    std::vector<DrawImage>* images) const {
  DCHECK_EQ(0u, images->size());
  display_list_->GetDiscardableImagesInRect(layer_rect, raster_scale, images);
}

bool RasterSource::CoversRect(const gfx::Rect& layer_rect) const {
  if (size_.IsEmpty())
    return false;
  gfx::Rect bounded_rect = layer_rect;
  bounded_rect.Intersect(gfx::Rect(size_));
  return recorded_viewport_.Contains(bounded_rect);
}

gfx::Size RasterSource::GetSize() const {
  return size_;
}

bool RasterSource::IsSolidColor() const {
  return is_solid_color_;
}

SkColor RasterSource::GetSolidColor() const {
  DCHECK(IsSolidColor());
  return solid_color_;
}

bool RasterSource::HasRecordings() const {
  return !!display_list_.get();
}

gfx::Rect RasterSource::RecordedViewport() const {
  return recorded_viewport_;
}

void RasterSource::SetShouldAttemptToUseDistanceFieldText() {
  should_attempt_to_use_distance_field_text_ = true;
}

bool RasterSource::ShouldAttemptToUseDistanceFieldText() const {
  return should_attempt_to_use_distance_field_text_;
}

void RasterSource::AsValueInto(base::trace_event::TracedValue* array) const {
  if (display_list_.get())
    TracedValue::AppendIDRef(display_list_.get(), array);
}

void RasterSource::DidBeginTracing() {
  if (display_list_.get())
    display_list_->EmitTraceSnapshot();
}

bool RasterSource::CanUseLCDText() const {
  return can_use_lcd_text_;
}

scoped_refptr<RasterSource> RasterSource::CreateCloneWithoutLCDText() const {
  bool can_use_lcd_text = false;
  return scoped_refptr<RasterSource>(new RasterSource(this, can_use_lcd_text));
}

RasterSource::PlaybackSettings::PlaybackSettings()
    : playback_to_shared_canvas(false),
      skip_images(false),
      use_image_hijack_canvas(true) {}

}  // namespace cc
