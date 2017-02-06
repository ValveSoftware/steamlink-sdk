// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PROTO_SYNCED_PROPERTY_CONVERSIONS_H_
#define CC_PROTO_SYNCED_PROPERTY_CONVERSIONS_H_

#include "cc/base/cc_export.h"
#include "cc/trees/property_tree.h"

namespace cc {

namespace proto {
class SyncedProperty;
}  // namespace proto

CC_EXPORT void SyncedScrollOffsetToProto(
    const SyncedScrollOffset& synced_scroll_offset,
    proto::SyncedProperty* proto);
CC_EXPORT void ProtoToSyncedScrollOffset(
    const proto::SyncedProperty& proto,
    SyncedScrollOffset* synced_scroll_offset);

}  // namespace cc

#endif  // CC_PROTO_SYNCED_PROPERTY_CONVERSIONS_H_
