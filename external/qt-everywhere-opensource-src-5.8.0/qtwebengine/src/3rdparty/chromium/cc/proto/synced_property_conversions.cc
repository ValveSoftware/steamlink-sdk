// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/proto/synced_property_conversions.h"

#include "cc/proto/gfx_conversions.h"
#include "cc/proto/synced_property.pb.h"

namespace cc {

void SyncedScrollOffsetToProto(const SyncedScrollOffset& synced_scroll_offset,
                               proto::SyncedProperty* proto) {
  proto::ScrollOffsetGroup* data = proto->mutable_scroll_offset_group();
  ScrollOffsetToProto(synced_scroll_offset.PendingBase(),
                      data->mutable_pending_base());
}

void ProtoToSyncedScrollOffset(const proto::SyncedProperty& proto,
                               SyncedScrollOffset* synced_scroll_offset) {
  const proto::ScrollOffsetGroup& data = proto.scroll_offset_group();
  synced_scroll_offset->PushFromMainThread(
      ProtoToScrollOffset(data.pending_base()));
}

}  // namespace cc
