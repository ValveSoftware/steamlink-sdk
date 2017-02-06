// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/playback/float_clip_display_item.h"

#include <stddef.h>

#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/proto/display_item.pb.h"
#include "cc/proto/gfx_conversions.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/skia_util.h"

namespace cc {

FloatClipDisplayItem::FloatClipDisplayItem(const gfx::RectF& clip_rect) {
  SetNew(clip_rect);
}

FloatClipDisplayItem::FloatClipDisplayItem(const proto::DisplayItem& proto) {
  DCHECK_EQ(proto::DisplayItem::Type_FloatClip, proto.type());

  const proto::FloatClipDisplayItem& details = proto.float_clip_item();
  gfx::RectF clip_rect = ProtoToRectF(details.clip_rect());

  SetNew(clip_rect);
}

FloatClipDisplayItem::~FloatClipDisplayItem() {
}

void FloatClipDisplayItem::SetNew(const gfx::RectF& clip_rect) {
  clip_rect_ = clip_rect;
}

void FloatClipDisplayItem::ToProtobuf(proto::DisplayItem* proto) const {
  proto->set_type(proto::DisplayItem::Type_FloatClip);

  proto::FloatClipDisplayItem* details = proto->mutable_float_clip_item();
  RectFToProto(clip_rect_, details->mutable_clip_rect());
}

void FloatClipDisplayItem::Raster(SkCanvas* canvas,
                                  SkPicture::AbortCallback* callback) const {
  canvas->save();
  canvas->clipRect(gfx::RectFToSkRect(clip_rect_));
}

void FloatClipDisplayItem::AsValueInto(
    const gfx::Rect& visual_rect,
    base::trace_event::TracedValue* array) const {
  array->AppendString(base::StringPrintf(
      "FloatClipDisplayItem rect: [%s] visualRect: [%s]",
      clip_rect_.ToString().c_str(), visual_rect.ToString().c_str()));
}

size_t FloatClipDisplayItem::ExternalMemoryUsage() const {
  return 0;
}

EndFloatClipDisplayItem::EndFloatClipDisplayItem() {}

EndFloatClipDisplayItem::EndFloatClipDisplayItem(
    const proto::DisplayItem& proto) {
  DCHECK_EQ(proto::DisplayItem::Type_EndFloatClip, proto.type());
}

EndFloatClipDisplayItem::~EndFloatClipDisplayItem() {
}

void EndFloatClipDisplayItem::ToProtobuf(proto::DisplayItem* proto) const {
  proto->set_type(proto::DisplayItem::Type_EndFloatClip);
}

void EndFloatClipDisplayItem::Raster(
    SkCanvas* canvas,
    SkPicture::AbortCallback* callback) const {
  canvas->restore();
}

void EndFloatClipDisplayItem::AsValueInto(
    const gfx::Rect& visual_rect,
    base::trace_event::TracedValue* array) const {
  array->AppendString(
      base::StringPrintf("EndFloatClipDisplayItem visualRect: [%s]",
                         visual_rect.ToString().c_str()));
}

size_t EndFloatClipDisplayItem::ExternalMemoryUsage() const {
  return 0;
}

}  // namespace cc
