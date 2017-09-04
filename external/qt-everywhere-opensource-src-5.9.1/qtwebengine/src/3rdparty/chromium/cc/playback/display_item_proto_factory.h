// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PLAYBACK_DISPLAY_ITEM_PROTO_FACTORY_H_
#define CC_PLAYBACK_DISPLAY_ITEM_PROTO_FACTORY_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "cc/playback/display_item.h"
#include "cc/playback/display_item_list.h"

namespace cc {
class ClientPictureCache;

namespace proto {
class DisplayItem;
}

class DisplayItemProtoFactory {
 public:
  static void AllocateAndConstruct(
      const gfx::Rect& visual_rect,
      DisplayItemList* list,
      const proto::DisplayItem& proto,
      ClientPictureCache* client_picture_cache,
      std::vector<uint32_t>* used_engine_picture_ids);

 private:
  DisplayItemProtoFactory() {}
  virtual ~DisplayItemProtoFactory() {}

  DISALLOW_COPY_AND_ASSIGN(DisplayItemProtoFactory);
};

}  // namespace cc

#endif  // CC_PLAYBACK_DISPLAY_ITEM_PROTO_FACTORY_H_
