// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/playback/drawing_display_item.h"

#include <stddef.h>

#include <string>

#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "base/values.h"
#include "cc/blimp/client_picture_cache.h"
#include "cc/blimp/image_serialization_processor.h"
#include "cc/debug/picture_debug_util.h"
#include "cc/proto/display_item.pb.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/utils/SkPictureUtils.h"
#include "ui/gfx/skia_util.h"

namespace cc {

DrawingDisplayItem::DrawingDisplayItem() {}

DrawingDisplayItem::DrawingDisplayItem(sk_sp<const SkPicture> picture) {
  SetNew(std::move(picture));
}

DrawingDisplayItem::DrawingDisplayItem(
    const proto::DisplayItem& proto,
    ClientPictureCache* client_picture_cache,
    std::vector<uint32_t>* used_engine_picture_ids) {
  DCHECK_EQ(proto::DisplayItem::Type_Drawing, proto.type());
  DCHECK(client_picture_cache);

  const proto::DrawingDisplayItem& details = proto.drawing_item();
  DCHECK(details.has_id());
  const proto::SkPictureID& sk_picture_id = details.id();
  DCHECK(sk_picture_id.has_unique_id());

  uint32_t unique_id = sk_picture_id.unique_id();
  sk_sp<const SkPicture> picture = client_picture_cache->GetPicture(unique_id);
  DCHECK(picture);

  used_engine_picture_ids->push_back(unique_id);
  SetNew(std::move(picture));
}

DrawingDisplayItem::DrawingDisplayItem(const DrawingDisplayItem& item) {
  item.CloneTo(this);
}

DrawingDisplayItem::~DrawingDisplayItem() {
}

void DrawingDisplayItem::SetNew(sk_sp<const SkPicture> picture) {
  picture_ = std::move(picture);
}

void DrawingDisplayItem::ToProtobuf(proto::DisplayItem* proto) const {
  TRACE_EVENT0("cc.remote", "DrawingDisplayItem::ToProtobuf");
  proto->set_type(proto::DisplayItem::Type_Drawing);

  if (!picture_)
    return;

  proto->mutable_drawing_item()->mutable_id()->set_unique_id(
      picture_->uniqueID());
}

sk_sp<const SkPicture> DrawingDisplayItem::GetPicture() const {
  return picture_;
}

void DrawingDisplayItem::Raster(SkCanvas* canvas,
                                SkPicture::AbortCallback* callback) const {
  if (canvas->quickReject(picture_->cullRect()))
    return;

  // SkPicture always does a wrapping save/restore on the canvas, so it is not
  // necessary here.
  if (callback)
    picture_->playback(canvas, callback);
  else
    canvas->drawPicture(picture_.get());
}

void DrawingDisplayItem::AsValueInto(
    const gfx::Rect& visual_rect,
    base::trace_event::TracedValue* array) const {
  array->BeginDictionary();
  array->SetString("name", "DrawingDisplayItem");

  array->BeginArray("visualRect");
  array->AppendInteger(visual_rect.x());
  array->AppendInteger(visual_rect.y());
  array->AppendInteger(visual_rect.width());
  array->AppendInteger(visual_rect.height());
  array->EndArray();

  array->BeginArray("cullRect");
  array->AppendInteger(picture_->cullRect().x());
  array->AppendInteger(picture_->cullRect().y());
  array->AppendInteger(picture_->cullRect().width());
  array->AppendInteger(picture_->cullRect().height());
  array->EndArray();

  std::string b64_picture;
  PictureDebugUtil::SerializeAsBase64(picture_.get(), &b64_picture);
  array->SetString("skp64", b64_picture);
  array->EndDictionary();
}

void DrawingDisplayItem::CloneTo(DrawingDisplayItem* item) const {
  item->SetNew(picture_);
}

size_t DrawingDisplayItem::ExternalMemoryUsage() const {
  return SkPictureUtils::ApproximateBytesUsed(picture_.get());
}

int DrawingDisplayItem::ApproximateOpCount() const {
  return picture_->approximateOpCount();
}

}  // namespace cc
