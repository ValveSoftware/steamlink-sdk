// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/metrics/histogram.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_data_item.h"
#include "storage/browser/blob/blob_entry.h"
#include "storage/browser/blob/shareable_blob_data_item.h"
#include "storage/common/data_element.h"

namespace storage {

BlobEntry::ItemCopyEntry::ItemCopyEntry(
    scoped_refptr<ShareableBlobDataItem> source_item,
    size_t source_item_offset,
    scoped_refptr<ShareableBlobDataItem> dest_item)
    : source_item(std::move(source_item)),
      source_item_offset(source_item_offset),
      dest_item(std::move(dest_item)) {}

BlobEntry::ItemCopyEntry::ItemCopyEntry(ItemCopyEntry&& other) = default;
BlobEntry::ItemCopyEntry& BlobEntry::ItemCopyEntry::operator=(
    BlobEntry::ItemCopyEntry&& rhs) = default;
BlobEntry::ItemCopyEntry::~ItemCopyEntry() {}

BlobEntry::BuildingState::BuildingState(
    bool transport_items_present,
    TransportAllowedCallback transport_allowed_callback,
    size_t num_building_dependent_blobs)
    : transport_items_present(transport_items_present),
      transport_allowed_callback(transport_allowed_callback),
      num_building_dependent_blobs(num_building_dependent_blobs) {}

BlobEntry::BuildingState::~BuildingState() {}

BlobEntry::BlobEntry(const std::string& content_type,
                     const std::string& content_disposition)
    : content_type_(content_type), content_disposition_(content_disposition) {}
BlobEntry::~BlobEntry() {}

void BlobEntry::AppendSharedBlobItem(
    scoped_refptr<ShareableBlobDataItem> item) {
  DCHECK(item);
  if (!items_.empty()) {
    offsets_.push_back(size_);
  }
  size_ += item->item()->length();
  items_.push_back(std::move(item));
}

const std::vector<scoped_refptr<ShareableBlobDataItem>>& BlobEntry::items()
    const {
  return items_;
}

void BlobEntry::ClearItems() {
  items_.clear();
}

void BlobEntry::ClearOffsets() {
  offsets_.clear();
}

}  // namespace storage
