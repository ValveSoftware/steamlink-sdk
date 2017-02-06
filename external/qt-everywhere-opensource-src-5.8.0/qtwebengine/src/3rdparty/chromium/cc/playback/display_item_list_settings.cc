// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/playback/display_item_list_settings.h"
#include "cc/proto/display_item.pb.h"

namespace cc {

DisplayItemListSettings::DisplayItemListSettings()
    : use_cached_picture(false) {}

DisplayItemListSettings::DisplayItemListSettings(
    const proto::DisplayItemListSettings& proto)
    : use_cached_picture(proto.use_cached_picture()) {}

DisplayItemListSettings::~DisplayItemListSettings() {
}

void DisplayItemListSettings::ToProtobuf(
    proto::DisplayItemListSettings* proto) const {
  proto->set_use_cached_picture(use_cached_picture);
}

}  // namespace cc
