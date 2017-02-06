// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_DATA_ITEM_H_
#define STORAGE_BROWSER_BLOB_BLOB_DATA_ITEM_H_

#include <stdint.h>

#include <memory>
#include <ostream>
#include <string>

#include "base/memory/ref_counted.h"
#include "storage/browser/storage_browser_export.h"
#include "storage/common/data_element.h"

namespace disk_cache {
class Entry;
}

namespace storage {
class BlobDataBuilder;
class BlobStorageContext;

// Ref counted blob item. This class owns the backing data of the blob item. The
// backing data is immutable, and cannot change after creation. The purpose of
// this class is to allow the resource to stick around in the snapshot even
// after the resource was swapped in the blob (either to disk or to memory) by
// the BlobStorageContext.
class STORAGE_EXPORT BlobDataItem : public base::RefCounted<BlobDataItem> {
 public:
  // The DataHandle class is used to persist resources that are needed for
  // reading this BlobDataItem. This object will stay around while any reads are
  // pending. If all blobs with this item are deleted or the item is swapped for
  // a different backend version (mem-to-disk or the reverse), then the item
  // will be destructed after all pending reads are complete.
  class STORAGE_EXPORT DataHandle : public base::RefCounted<DataHandle> {
   protected:
    virtual ~DataHandle() = 0;

   private:
    friend class base::RefCounted<DataHandle>;
  };

  DataElement::Type type() const { return item_->type(); }
  const char* bytes() const { return item_->bytes(); }
  const base::FilePath& path() const { return item_->path(); }
  const GURL& filesystem_url() const { return item_->filesystem_url(); }
  const std::string& blob_uuid() const { return item_->blob_uuid(); }
  uint64_t offset() const { return item_->offset(); }
  uint64_t length() const { return item_->length(); }
  const base::Time& expected_modification_time() const {
    return item_->expected_modification_time();
  }
  const DataElement& data_element() const { return *item_; }
  const DataElement* data_element_ptr() const { return item_.get(); }
  DataElement* data_element_ptr() { return item_.get(); }

  disk_cache::Entry* disk_cache_entry() const { return disk_cache_entry_; }
  int disk_cache_stream_index() const { return disk_cache_stream_index_; }
  int disk_cache_side_stream_index() const {
    return disk_cache_side_stream_index_;
  }

 private:
  friend class BlobDataBuilder;
  friend class BlobStorageContext;
  friend class base::RefCounted<BlobDataItem>;
  friend STORAGE_EXPORT void PrintTo(const BlobDataItem& x, ::std::ostream* os);

  explicit BlobDataItem(std::unique_ptr<DataElement> item);
  BlobDataItem(std::unique_ptr<DataElement> item,
               const scoped_refptr<DataHandle>& data_handle);
  BlobDataItem(std::unique_ptr<DataElement> item,
               const scoped_refptr<DataHandle>& data_handle,
               disk_cache::Entry* entry,
               int disk_cache_stream_index,
               int disk_cache_side_stream_index);
  virtual ~BlobDataItem();

  std::unique_ptr<DataElement> item_;
  scoped_refptr<DataHandle> data_handle_;

  // This naked pointer is safe because the scope is protected by the DataHandle
  // instance for disk cache entries during the lifetime of this BlobDataItem.
  disk_cache::Entry* disk_cache_entry_;
  int disk_cache_stream_index_;  // For TYPE_DISK_CACHE_ENTRY.
  int disk_cache_side_stream_index_;  // For TYPE_DISK_CACHE_ENTRY.
};

STORAGE_EXPORT bool operator==(const BlobDataItem& a, const BlobDataItem& b);
STORAGE_EXPORT bool operator!=(const BlobDataItem& a, const BlobDataItem& b);

}  // namespace storage

#endif  // STORAGE_BROWSER_BLOB_BLOB_DATA_ITEM_H_
