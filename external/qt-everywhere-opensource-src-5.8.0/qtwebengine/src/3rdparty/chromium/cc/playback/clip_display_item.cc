// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/playback/clip_display_item.h"

#include <stddef.h>

#include <string>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/proto/display_item.pb.h"
#include "cc/proto/gfx_conversions.h"
#include "cc/proto/skia_conversions.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/skia_util.h"

namespace cc {
class ImageSerializationProcessor;

ClipDisplayItem::ClipDisplayItem(const gfx::Rect& clip_rect,
                                 const std::vector<SkRRect>& rounded_clip_rects,
                                 bool antialias) {
  SetNew(clip_rect, rounded_clip_rects, antialias);
}

ClipDisplayItem::ClipDisplayItem(const proto::DisplayItem& proto) {
  DCHECK_EQ(proto::DisplayItem::Type_Clip, proto.type());

  const proto::ClipDisplayItem& details = proto.clip_item();
  gfx::Rect clip_rect = ProtoToRect(details.clip_rect());
  std::vector<SkRRect> rounded_clip_rects;
  rounded_clip_rects.reserve(details.rounded_rects_size());
  for (int i = 0; i < details.rounded_rects_size(); i++) {
    rounded_clip_rects.push_back(ProtoToSkRRect(details.rounded_rects(i)));
  }
  bool antialias = details.antialias();
  SetNew(clip_rect, rounded_clip_rects, antialias);
}

void ClipDisplayItem::SetNew(const gfx::Rect& clip_rect,
                             const std::vector<SkRRect>& rounded_clip_rects,
                             bool antialias) {
  clip_rect_ = clip_rect;
  rounded_clip_rects_ = rounded_clip_rects;
  antialias_ = antialias;
}

ClipDisplayItem::~ClipDisplayItem() {}

void ClipDisplayItem::ToProtobuf(proto::DisplayItem* proto) const {
  proto->set_type(proto::DisplayItem::Type_Clip);

  proto::ClipDisplayItem* details = proto->mutable_clip_item();
  RectToProto(clip_rect_, details->mutable_clip_rect());
  DCHECK_EQ(0, details->rounded_rects_size());
  for (const auto& rrect : rounded_clip_rects_) {
    SkRRectToProto(rrect, details->add_rounded_rects());
  }
  details->set_antialias(antialias_);
}

void ClipDisplayItem::Raster(SkCanvas* canvas,
                             SkPicture::AbortCallback* callback) const {
  canvas->save();
  canvas->clipRect(SkRect::MakeXYWH(clip_rect_.x(), clip_rect_.y(),
                                    clip_rect_.width(), clip_rect_.height()),
                   SkRegion::kIntersect_Op, antialias_);
  for (size_t i = 0; i < rounded_clip_rects_.size(); ++i) {
    if (rounded_clip_rects_[i].isRect()) {
      canvas->clipRect(rounded_clip_rects_[i].rect(), SkRegion::kIntersect_Op,
                       antialias_);
    } else {
      canvas->clipRRect(rounded_clip_rects_[i], SkRegion::kIntersect_Op,
                        antialias_);
    }
  }
}

void ClipDisplayItem::AsValueInto(const gfx::Rect& visual_rect,
                                  base::trace_event::TracedValue* array) const {
  std::string value = base::StringPrintf(
      "ClipDisplayItem rect: [%s] visualRect: [%s]",
      clip_rect_.ToString().c_str(), visual_rect.ToString().c_str());
  for (const SkRRect& rounded_rect : rounded_clip_rects_) {
    base::StringAppendF(
        &value, " rounded_rect: [rect: [%s]",
        gfx::SkRectToRectF(rounded_rect.rect()).ToString().c_str());
    base::StringAppendF(&value, " radii: [");
    SkVector upper_left_radius = rounded_rect.radii(SkRRect::kUpperLeft_Corner);
    base::StringAppendF(&value, "[%f,%f],", upper_left_radius.x(),
                        upper_left_radius.y());
    SkVector upper_right_radius =
        rounded_rect.radii(SkRRect::kUpperRight_Corner);
    base::StringAppendF(&value, " [%f,%f],", upper_right_radius.x(),
                        upper_right_radius.y());
    SkVector lower_right_radius =
        rounded_rect.radii(SkRRect::kLowerRight_Corner);
    base::StringAppendF(&value, " [%f,%f],", lower_right_radius.x(),
                        lower_right_radius.y());
    SkVector lower_left_radius = rounded_rect.radii(SkRRect::kLowerLeft_Corner);
    base::StringAppendF(&value, " [%f,%f]]", lower_left_radius.x(),
                        lower_left_radius.y());
  }
  array->AppendString(value);
}

size_t ClipDisplayItem::ExternalMemoryUsage() const {
  return rounded_clip_rects_.capacity() * sizeof(rounded_clip_rects_[0]);
}

EndClipDisplayItem::EndClipDisplayItem() {}

EndClipDisplayItem::EndClipDisplayItem(const proto::DisplayItem& proto) {
  DCHECK_EQ(proto::DisplayItem::Type_EndClip, proto.type());
}

EndClipDisplayItem::~EndClipDisplayItem() {
}

void EndClipDisplayItem::ToProtobuf(proto::DisplayItem* proto) const {
  proto->set_type(proto::DisplayItem::Type_EndClip);
}

void EndClipDisplayItem::Raster(SkCanvas* canvas,
                                SkPicture::AbortCallback* callback) const {
  canvas->restore();
}

void EndClipDisplayItem::AsValueInto(
    const gfx::Rect& visual_rect,
    base::trace_event::TracedValue* array) const {
  array->AppendString(base::StringPrintf("EndClipDisplayItem visualRect: [%s]",
                                         visual_rect.ToString().c_str()));
}

size_t EndClipDisplayItem::ExternalMemoryUsage() const {
  return 0;
}

}  // namespace cc
