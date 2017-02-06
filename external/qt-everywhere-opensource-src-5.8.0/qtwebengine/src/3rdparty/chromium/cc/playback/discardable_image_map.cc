// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/playback/discardable_image_map.h"

#include <stddef.h>

#include <algorithm>
#include <limits>

#include "base/containers/adapters.h"
#include "cc/base/math_util.h"
#include "cc/playback/display_item_list.h"
#include "third_party/skia/include/utils/SkNWayCanvas.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/skia_util.h"

namespace cc {

SkRect MapRect(const SkMatrix& matrix, const SkRect& src) {
  SkRect dst;
  matrix.mapRect(&dst, src);
  return dst;
}

namespace {

// We're using an NWay canvas with no added canvases, so in effect
// non-overridden functions are no-ops.
class DiscardableImagesMetadataCanvas : public SkNWayCanvas {
 public:
  DiscardableImagesMetadataCanvas(
      int width,
      int height,
      std::vector<std::pair<DrawImage, gfx::Rect>>* image_set)
      : SkNWayCanvas(width, height),
        image_set_(image_set),
        canvas_bounds_(SkRect::MakeIWH(width, height)) {}

 protected:
  // we need to "undo" the behavior of SkNWayCanvas, which will try to forward
  // it.
  void onDrawPicture(const SkPicture* picture,
                     const SkMatrix* matrix,
                     const SkPaint* paint) override {
    SkCanvas::onDrawPicture(picture, matrix, paint);
  }

  void onDrawImage(const SkImage* image,
                   SkScalar x,
                   SkScalar y,
                   const SkPaint* paint) override {
    const SkMatrix& ctm = getTotalMatrix();
    AddImage(
        sk_ref_sp(image), SkRect::MakeIWH(image->width(), image->height()),
        MapRect(ctm, SkRect::MakeXYWH(x, y, image->width(), image->height())),
        ctm, paint);
  }

  void onDrawImageRect(const SkImage* image,
                       const SkRect* src,
                       const SkRect& dst,
                       const SkPaint* paint,
                       SrcRectConstraint) override {
    const SkMatrix& ctm = getTotalMatrix();
    SkRect src_storage;
    if (!src) {
      src_storage = SkRect::MakeIWH(image->width(), image->height());
      src = &src_storage;
    }
    SkMatrix matrix;
    matrix.setRectToRect(*src, dst, SkMatrix::kFill_ScaleToFit);
    matrix.postConcat(ctm);
    AddImage(sk_ref_sp(image), *src, MapRect(ctm, dst), matrix, paint);
  }

  void onDrawImageNine(const SkImage* image,
                       const SkIRect& center,
                       const SkRect& dst,
                       const SkPaint* paint) override {
    // No cc embedder issues image nine calls.
    NOTREACHED();
  }

  SaveLayerStrategy getSaveLayerStrategy(const SaveLayerRec& rec) override {
    saved_paints_.push_back(*rec.fPaint);
    return SkNWayCanvas::getSaveLayerStrategy(rec);
  }

  void willSave() override {
    saved_paints_.push_back(SkPaint());
    return SkNWayCanvas::willSave();
  }

  void willRestore() override {
    DCHECK_GT(saved_paints_.size(), 0u);
    saved_paints_.pop_back();
    SkNWayCanvas::willRestore();
  }

 private:
  bool ComputePaintBounds(const SkRect& rect,
                          const SkPaint* current_paint,
                          SkRect* paint_bounds) {
    *paint_bounds = rect;
    if (current_paint) {
      if (!current_paint->canComputeFastBounds())
        return false;
      *paint_bounds =
          current_paint->computeFastBounds(*paint_bounds, paint_bounds);
    }

    for (const auto& paint : base::Reversed(saved_paints_)) {
      if (!paint.canComputeFastBounds())
        return false;
      *paint_bounds = paint.computeFastBounds(*paint_bounds, paint_bounds);
    }
    return true;
  }

  void AddImage(sk_sp<const SkImage> image,
                const SkRect& src_rect,
                const SkRect& rect,
                const SkMatrix& matrix,
                const SkPaint* paint) {
    if (!image->isLazyGenerated())
      return;

    SkRect paint_rect;
    bool computed_paint_bounds = ComputePaintBounds(rect, paint, &paint_rect);
    if (!computed_paint_bounds) {
      // TODO(vmpstr): UMA this case.
      paint_rect = canvas_bounds_;
    }

    if (!paint_rect.intersects(canvas_bounds_))
      return;

    SkFilterQuality filter_quality = kNone_SkFilterQuality;
    if (paint) {
      filter_quality = paint->getFilterQuality();
    }

    SkIRect src_irect;
    src_rect.roundOut(&src_irect);
    image_set_->push_back(std::make_pair(
        DrawImage(std::move(image), src_irect, filter_quality, matrix),
        gfx::ToEnclosingRect(gfx::SkRectToRectF(paint_rect))));
  }

  std::vector<std::pair<DrawImage, gfx::Rect>>* image_set_;
  const SkRect canvas_bounds_;
  std::vector<SkPaint> saved_paints_;
};

}  // namespace

DiscardableImageMap::DiscardableImageMap() {}

DiscardableImageMap::~DiscardableImageMap() {}

sk_sp<SkCanvas> DiscardableImageMap::BeginGeneratingMetadata(
    const gfx::Size& bounds) {
  DCHECK(all_images_.empty());
  return sk_make_sp<DiscardableImagesMetadataCanvas>(
      bounds.width(), bounds.height(), &all_images_);
}

void DiscardableImageMap::EndGeneratingMetadata() {
  images_rtree_.Build(all_images_,
                      [](const std::pair<DrawImage, gfx::Rect>& image) {
                        return image.second;
                      });
}

void DiscardableImageMap::GetDiscardableImagesInRect(
    const gfx::Rect& rect,
    float raster_scale,
    std::vector<DrawImage>* images) const {
  std::vector<size_t> indices;
  images_rtree_.Search(rect, &indices);
  for (size_t index : indices)
    images->push_back(all_images_[index].first.ApplyScale(raster_scale));
}

DiscardableImageMap::ScopedMetadataGenerator::ScopedMetadataGenerator(
    DiscardableImageMap* image_map,
    const gfx::Size& bounds)
    : image_map_(image_map),
      metadata_canvas_(image_map->BeginGeneratingMetadata(bounds)) {}

DiscardableImageMap::ScopedMetadataGenerator::~ScopedMetadataGenerator() {
  image_map_->EndGeneratingMetadata();
}

}  // namespace cc
