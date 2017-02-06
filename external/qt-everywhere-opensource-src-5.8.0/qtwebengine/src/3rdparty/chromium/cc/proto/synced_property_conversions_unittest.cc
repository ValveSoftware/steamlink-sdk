// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/proto/synced_property_conversions.h"

#include "cc/proto/synced_property.pb.h"
#include "cc/trees/property_tree.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

TEST(SyncedPropertyConversionTest, SerializeDeserializeSyncedScrollOffset) {
  scoped_refptr<SyncedScrollOffset> synced_scroll_offset =
      new SyncedScrollOffset();
  synced_scroll_offset->PushFromMainThread(gfx::ScrollOffset(1, 2));
  proto::SyncedProperty proto;
  scoped_refptr<SyncedScrollOffset> serialized_synced_scroll_offset =
      new SyncedScrollOffset();
  SyncedScrollOffsetToProto(*synced_scroll_offset.get(), &proto);
  ProtoToSyncedScrollOffset(proto, serialized_synced_scroll_offset.get());
  EXPECT_EQ(synced_scroll_offset.get()->PendingBase(),
            serialized_synced_scroll_offset.get()->PendingBase());
  EXPECT_EQ(synced_scroll_offset.get()->PendingBase(),
            serialized_synced_scroll_offset.get()->PendingBase());
}

}  // namespace
}  // namespace cc
