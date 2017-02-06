// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/playback/filter_display_item.h"

#include <stddef.h>

#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/output/render_surface_filters.h"
#include "cc/proto/display_item.pb.h"
#include "cc/proto/gfx_conversions.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "ui/gfx/skia_util.h"

namespace cc {

FilterDisplayItem::FilterDisplayItem(const FilterOperations& filters,
                                     const gfx::RectF& bounds) {
  SetNew(filters, bounds);
}

FilterDisplayItem::FilterDisplayItem(const proto::DisplayItem& proto) {
  DCHECK_EQ(proto::DisplayItem::Type_Filter, proto.type());

  const proto::FilterDisplayItem& details = proto.filter_item();
  gfx::RectF bounds = ProtoToRectF(details.bounds());

  // TODO(dtrainor): Support deserializing FilterOperations (crbug.com/541321).
  FilterOperations filters;

  SetNew(filters, bounds);
}

FilterDisplayItem::~FilterDisplayItem() {}

void FilterDisplayItem::SetNew(const FilterOperations& filters,
                               const gfx::RectF& bounds) {
  filters_ = filters;
  bounds_ = bounds;
}

void FilterDisplayItem::ToProtobuf(proto::DisplayItem* proto) const {
  proto->set_type(proto::DisplayItem::Type_Filter);

  proto::FilterDisplayItem* details = proto->mutable_filter_item();
  RectFToProto(bounds_, details->mutable_bounds());

  // TODO(dtrainor): Support serializing FilterOperations (crbug.com/541321).
}

void FilterDisplayItem::Raster(SkCanvas* canvas,
                               SkPicture::AbortCallback* callback) const {
  canvas->save();
  canvas->translate(bounds_.x(), bounds_.y());

  sk_sp<SkImageFilter> image_filter = RenderSurfaceFilters::BuildImageFilter(
      filters_, gfx::SizeF(bounds_.width(), bounds_.height()));
  SkRect boundaries = SkRect::MakeWH(bounds_.width(), bounds_.height());

  SkPaint paint;
  paint.setXfermodeMode(SkXfermode::kSrcOver_Mode);
  paint.setImageFilter(std::move(image_filter));
  canvas->saveLayer(&boundaries, &paint);

  canvas->translate(-bounds_.x(), -bounds_.y());
}

void FilterDisplayItem::AsValueInto(
    const gfx::Rect& visual_rect,
    base::trace_event::TracedValue* array) const {
  array->AppendString(base::StringPrintf(
      "FilterDisplayItem bounds: [%s] visualRect: [%s]",
      bounds_.ToString().c_str(), visual_rect.ToString().c_str()));
}

size_t FilterDisplayItem::ExternalMemoryUsage() const {
  // FilterOperations doesn't expose its capacity, but size is probably good
  // enough.
  return filters_.size() * sizeof(filters_.at(0));
}

EndFilterDisplayItem::EndFilterDisplayItem() {}

EndFilterDisplayItem::EndFilterDisplayItem(const proto::DisplayItem& proto) {
  DCHECK_EQ(proto::DisplayItem::Type_EndFilter, proto.type());
}

EndFilterDisplayItem::~EndFilterDisplayItem() {}

void EndFilterDisplayItem::ToProtobuf(proto::DisplayItem* proto) const {
  proto->set_type(proto::DisplayItem::Type_EndFilter);
}

void EndFilterDisplayItem::Raster(SkCanvas* canvas,
                                  SkPicture::AbortCallback* callback) const {
  canvas->restore();
  canvas->restore();
}

void EndFilterDisplayItem::AsValueInto(
    const gfx::Rect& visual_rect,
    base::trace_event::TracedValue* array) const {
  array->AppendString(
      base::StringPrintf("EndFilterDisplayItem  visualRect: [%s]",
                         visual_rect.ToString().c_str()));
}

size_t EndFilterDisplayItem::ExternalMemoryUsage() const {
  return 0;
}

}  // namespace cc
