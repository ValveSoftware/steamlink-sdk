// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_SHAREABLE_BLOB_DATA_ITEM_H_
#define STORAGE_BROWSER_BLOB_SHAREABLE_BLOB_DATA_ITEM_H_

#include <string>

#include "base/containers/hash_tables.h"
#include "base/hash.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "storage/common/data_element.h"

namespace storage {
class BlobDataItem;
class InternalBlobData;

// This class allows blob items to be shared between blobs, and is only used by
// BlobStorageContext. This class contains both the blob data item and the uuids
// of all the blobs using this item.
// The data in this class (the item) is immutable, but the item itself can be
// swapped out with an item with the same data but a different backing (think
// RAM vs file backed).
class ShareableBlobDataItem : public base::RefCounted<ShareableBlobDataItem> {
 public:
  ShareableBlobDataItem(const std::string& blob_uuid,
                        const scoped_refptr<BlobDataItem>& item);

  const scoped_refptr<BlobDataItem>& item();

  base::hash_set<std::string>& referencing_blobs() {
    return referencing_blobs_;
  }

 private:
  friend class base::RefCounted<ShareableBlobDataItem>;
  friend class InternalBlobData;
  ~ShareableBlobDataItem();

  scoped_refptr<BlobDataItem> item_;

  base::hash_set<std::string> referencing_blobs_;

  DISALLOW_COPY_AND_ASSIGN(ShareableBlobDataItem);
};

}  // namespace storage
#endif  // STORAGE_BROWSER_BLOB_SHAREABLE_BLOB_DATA_ITEM_H_
