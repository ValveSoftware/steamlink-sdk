// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/shareable_blob_data_item.h"

#include "storage/browser/blob/blob_data_item.h"

namespace storage {

ShareableBlobDataItem::ShareableBlobDataItem(
    const std::string& blob_uuid,
    const scoped_refptr<BlobDataItem>& item)
    : item_(item) {
  DCHECK_NE(item_->type(), DataElement::TYPE_BLOB);
  referencing_blobs_.insert(blob_uuid);
}

ShareableBlobDataItem::~ShareableBlobDataItem() {
}

const scoped_refptr<BlobDataItem>& ShareableBlobDataItem::item() {
  return item_;
}

}  // namespace storage
