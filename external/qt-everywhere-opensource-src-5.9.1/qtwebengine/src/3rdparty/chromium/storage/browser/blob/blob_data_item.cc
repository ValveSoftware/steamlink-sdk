// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_data_item.h"

#include <memory>
#include <utility>

namespace storage {

BlobDataItem::DataHandle::~DataHandle() {
}

BlobDataItem::BlobDataItem(std::unique_ptr<DataElement> item)
    : item_(std::move(item)),
      disk_cache_entry_(nullptr),
      disk_cache_stream_index_(-1),
      disk_cache_side_stream_index_(-1) {}

BlobDataItem::BlobDataItem(std::unique_ptr<DataElement> item,
                           const scoped_refptr<DataHandle>& data_handle)
    : item_(std::move(item)),
      data_handle_(data_handle),
      disk_cache_entry_(nullptr),
      disk_cache_stream_index_(-1),
      disk_cache_side_stream_index_(-1) {}

BlobDataItem::BlobDataItem(std::unique_ptr<DataElement> item,
                           const scoped_refptr<DataHandle>& data_handle,
                           disk_cache::Entry* entry,
                           int disk_cache_stream_index,
                           int disk_cache_side_stream_index)
    : item_(std::move(item)),
      data_handle_(data_handle),
      disk_cache_entry_(entry),
      disk_cache_stream_index_(disk_cache_stream_index),
      disk_cache_side_stream_index_(disk_cache_side_stream_index) {}

BlobDataItem::~BlobDataItem() {}

void PrintTo(const BlobDataItem& x, ::std::ostream* os) {
  DCHECK(os);
  *os << "<BlobDataItem>{item: ";
  PrintTo(*x.item_, os);
  *os << ", has_data_handle: " << (x.data_handle_.get() ? "true" : "false")
      << ", disk_cache_entry_ptr: " << x.disk_cache_entry_
      << ", disk_cache_stream_index_: " << x.disk_cache_stream_index_ << "}";
}

bool operator==(const BlobDataItem& a, const BlobDataItem& b) {
  return a.disk_cache_entry() == b.disk_cache_entry() &&
         a.disk_cache_stream_index() == b.disk_cache_stream_index() &&
         a.disk_cache_side_stream_index() == b.disk_cache_side_stream_index() &&
         a.data_element() == b.data_element();
}

bool operator!=(const BlobDataItem& a, const BlobDataItem& b) {
  return !(a == b);
}

}  // namespace storage
