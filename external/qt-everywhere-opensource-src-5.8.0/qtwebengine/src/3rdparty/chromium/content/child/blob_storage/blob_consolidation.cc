// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/blob_storage/blob_consolidation.h"

#include <algorithm>
#include <limits>
#include <string>

#include "base/bind.h"
#include "base/callback.h"

using storage::DataElement;
using blink::WebThreadSafeData;

namespace content {
namespace {
bool WriteMemory(void* memory_out,
                 size_t* total_read,
                 const char* memory,
                 size_t size) {
  memcpy(static_cast<char*>(memory_out) + *total_read, memory, size);
  *total_read += size;
  return true;
}
}  // namespace

using ReadStatus = BlobConsolidation::ReadStatus;

BlobConsolidation::ConsolidatedItem::ConsolidatedItem()
    : type(DataElement::TYPE_UNKNOWN),
      offset(0),
      length(std::numeric_limits<uint64_t>::max()),
      expected_modification_time(0) {}

BlobConsolidation::ConsolidatedItem::~ConsolidatedItem() {
}

BlobConsolidation::ConsolidatedItem::ConsolidatedItem(DataElement::Type type,
                                                      uint64_t offset,
                                                      uint64_t length)
    : type(type),
      offset(offset),
      length(length),
      expected_modification_time(0) {
}

BlobConsolidation::ConsolidatedItem::ConsolidatedItem(
    const ConsolidatedItem& other) = default;

BlobConsolidation::BlobConsolidation() : total_memory_(0) {
}

BlobConsolidation::~BlobConsolidation() {
}

void BlobConsolidation::AddDataItem(const WebThreadSafeData& data) {
  if (data.size() == 0)
    return;
  if (consolidated_items_.empty() ||
      consolidated_items_.back().type != DataElement::TYPE_BYTES) {
    consolidated_items_.push_back(
        ConsolidatedItem(DataElement::TYPE_BYTES, 0, 0));
  }
  ConsolidatedItem& item = consolidated_items_.back();
  if (!item.data.empty()) {
    item.offsets.push_back(static_cast<size_t>(item.length));
  }
  item.length += data.size();
  total_memory_ += data.size();
  item.data.push_back(data);
}

void BlobConsolidation::AddFileItem(const base::FilePath& path,
                                    uint64_t offset,
                                    uint64_t length,
                                    double expected_modification_time) {
  if (length == 0)
    return;
  consolidated_items_.push_back(
      ConsolidatedItem(DataElement::TYPE_FILE, offset, length));
  ConsolidatedItem& item = consolidated_items_.back();
  item.path = path;
  item.expected_modification_time = expected_modification_time;
}

void BlobConsolidation::AddBlobItem(const std::string& uuid,
                                    uint64_t offset,
                                    uint64_t length) {
  if (length == 0)
    return;
  consolidated_items_.push_back(
      ConsolidatedItem(DataElement::TYPE_BLOB, offset, length));
  ConsolidatedItem& item = consolidated_items_.back();
  item.blob_uuid = uuid;
  referenced_blobs_.insert(uuid);
}

void BlobConsolidation::AddFileSystemItem(const GURL& url,
                                          uint64_t offset,
                                          uint64_t length,
                                          double expected_modification_time) {
  if (length == 0)
    return;
  consolidated_items_.push_back(
      ConsolidatedItem(DataElement::TYPE_FILE_FILESYSTEM, offset, length));
  ConsolidatedItem& item = consolidated_items_.back();
  item.filesystem_url = url;
  item.expected_modification_time = expected_modification_time;
}

ReadStatus BlobConsolidation::VisitMemory(size_t consolidated_item_index,
                                          size_t consolidated_offset,
                                          size_t consolidated_size,
                                          const MemoryVisitor& visitor) const {
  if (consolidated_item_index >= consolidated_items_.size())
    return ReadStatus::ERROR_OUT_OF_BOUNDS;

  const ConsolidatedItem& item = consolidated_items_[consolidated_item_index];
  if (item.type != DataElement::TYPE_BYTES)
    return ReadStatus::ERROR_WRONG_TYPE;

  if (consolidated_size + consolidated_offset > item.length) {
    return ReadStatus::ERROR_OUT_OF_BOUNDS;
  }

  // We do a binary search to find the correct data to start with in the data
  // elements.  This is slightly customized due to our unique storage and
  // constraints.
  size_t mid = 0;
  size_t offset_from_mid = consolidated_offset;
  size_t num_items = item.data.size();
  if (!item.offsets.empty()) {
    size_t low = 0;
    size_t high = num_items - 1;
    while (true) {
      mid = (high + low) / 2;
      // Note: we don't include the implicit '0' for the first item in offsets.
      size_t item_offset = (mid == 0 ? 0 : item.offsets[mid - 1]);
      offset_from_mid = consolidated_offset - item_offset;
      size_t next_item_offset = (mid + 1 == num_items ? 0 : item.offsets[mid]);
      if (item_offset == consolidated_offset) {
        // found exact match.
        break;
      } else if (item_offset > consolidated_offset) {
        high = mid - 1;
      } else if (mid + 1 == num_items ||
                 next_item_offset > consolidated_offset) {
        // We are at the last item, or the next offset is greater than the one
        // we want, so the current item wins.
        break;
      } else {
        low = mid + 1;
      }
    }
  }

  DCHECK_LT(offset_from_mid, item.data[mid].size());
  // Read starting from 'mid' and 'offset_from_mid'.
  for (size_t memory_read = 0;
       mid < num_items && memory_read < consolidated_size; mid++) {
    size_t read_size = std::min(item.data[mid].size() - offset_from_mid,
                                consolidated_size - memory_read);
    bool continu =
        visitor.Run(item.data[mid].data() + offset_from_mid, read_size);
    if (!continu)
      return ReadStatus::CANCELLED_BY_VISITOR;
    offset_from_mid = 0;
    memory_read += read_size;
  }
  return ReadStatus::OK;
}

ReadStatus BlobConsolidation::ReadMemory(size_t consolidated_item_index,
                                         size_t consolidated_offset,
                                         size_t consolidated_size,
                                         void* memory_out) const {
  size_t total_read = 0;
  return VisitMemory(consolidated_item_index, consolidated_offset,
                     consolidated_size,
                     base::Bind(&WriteMemory, memory_out, &total_read));
}

}  // namespace content
