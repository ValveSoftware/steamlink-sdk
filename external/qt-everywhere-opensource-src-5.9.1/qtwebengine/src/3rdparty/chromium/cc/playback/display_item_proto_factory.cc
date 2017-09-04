// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/playback/display_item_proto_factory.h"

#include "cc/playback/clip_display_item.h"
#include "cc/playback/clip_path_display_item.h"
#include "cc/playback/compositing_display_item.h"
#include "cc/playback/drawing_display_item.h"
#include "cc/playback/filter_display_item.h"
#include "cc/playback/float_clip_display_item.h"
#include "cc/playback/transform_display_item.h"
#include "cc/proto/display_item.pb.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {
class ClientPictureCache;

// static
void DisplayItemProtoFactory::AllocateAndConstruct(
    const gfx::Rect& visual_rect,
    DisplayItemList* list,
    const proto::DisplayItem& proto,
    ClientPictureCache* client_picture_cache,
    std::vector<uint32_t>* used_engine_picture_ids) {
  switch (proto.type()) {
    case proto::DisplayItem::Type_Clip:
      list->CreateAndAppendPairedBeginItem<ClipDisplayItem>(proto);
      return;
    case proto::DisplayItem::Type_EndClip:
      list->CreateAndAppendPairedEndItem<EndClipDisplayItem>(proto);
      return;
    case proto::DisplayItem::Type_ClipPath:
      list->CreateAndAppendPairedBeginItem<ClipPathDisplayItem>(proto);
      return;
    case proto::DisplayItem::Type_EndClipPath:
      list->CreateAndAppendPairedEndItem<EndClipPathDisplayItem>(proto);
      return;
    case proto::DisplayItem::Type_Compositing:
      list->CreateAndAppendPairedBeginItem<CompositingDisplayItem>(proto);
      return;
    case proto::DisplayItem::Type_EndCompositing:
      list->CreateAndAppendPairedEndItem<EndCompositingDisplayItem>(proto);
      return;
    case proto::DisplayItem::Type_Drawing:
      list->CreateAndAppendDrawingItem<DrawingDisplayItem>(
          visual_rect, proto, client_picture_cache, used_engine_picture_ids);
      return;
    case proto::DisplayItem::Type_Filter:
      list->CreateAndAppendPairedBeginItem<FilterDisplayItem>(proto);
      return;
    case proto::DisplayItem::Type_EndFilter:
      list->CreateAndAppendPairedEndItem<EndFilterDisplayItem>(proto);
      return;
    case proto::DisplayItem::Type_FloatClip:
      list->CreateAndAppendPairedBeginItem<FloatClipDisplayItem>(proto);
      return;
    case proto::DisplayItem::Type_EndFloatClip:
      list->CreateAndAppendPairedEndItem<EndFloatClipDisplayItem>(proto);
      return;
    case proto::DisplayItem::Type_Transform:
      list->CreateAndAppendPairedBeginItem<TransformDisplayItem>(proto);
      return;
    case proto::DisplayItem::Type_EndTransform:
      list->CreateAndAppendPairedEndItem<EndTransformDisplayItem>(proto);
      return;
  }

  NOTREACHED();
}

}  // namespace cc
