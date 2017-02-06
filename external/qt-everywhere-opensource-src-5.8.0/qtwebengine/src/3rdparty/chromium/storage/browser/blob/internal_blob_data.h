// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_INTERNAL_BLOB_DATA_H_
#define STORAGE_BROWSER_BLOB_INTERNAL_BLOB_DATA_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "storage/browser/blob/shareable_blob_data_item.h"

namespace storage {
class ViewBlobInternalsJob;

// This class represents a blob in the BlobStorageContext.  It is constructed
// using the internal Builder class.
class InternalBlobData {
 public:
  ~InternalBlobData();

 protected:
  friend class BlobStorageContext;
  friend class BlobStorageRegistry;
  friend class ViewBlobInternalsJob;

  // Removes the given blob uuid from the internal ShareableBlobDataItems.
  // This is called when this blob is being destroyed.
  void RemoveBlobFromShareableItems(const std::string& blob_uuid);

  const std::vector<scoped_refptr<ShareableBlobDataItem>>& items() const;

  // Gets the memory used by this blob that is not shared by other blobs. This
  // also doesn't count duplicate items.
  size_t GetUnsharedMemoryUsage() const;

  // Gets the memory used by this blob.  Total memory includes memory of items
  // possibly shared with other blobs, or items that appear multiple times in
  // this blob. Unshared memory is memory used by this blob that is not shared
  // by other blobs.
  void GetMemoryUsage(size_t* total_memory, size_t* unshared_memory);

 private:
  friend class Builder;
  InternalBlobData();

  std::string content_type_;
  std::string content_disposition_;
  std::vector<scoped_refptr<ShareableBlobDataItem>> items_;

  class Builder {
   public:
    Builder();
    ~Builder();

    void AppendSharedBlobItem(scoped_refptr<ShareableBlobDataItem> item);

    // Gets the memory used by this builder that is not shared with other blobs.
    size_t GetNonsharedMemoryUsage() const;

    // Removes the given blob uuid from the internal ShareableBlobDataItems.
    // This is called on destruction of the blob if we're still building it.
    void RemoveBlobFromShareableItems(const std::string& blob_uuid);

    // The builder is invalid after calling this method.
    std::unique_ptr<::storage::InternalBlobData> Build();

   private:
    std::unique_ptr<::storage::InternalBlobData> data_;

    DISALLOW_COPY_AND_ASSIGN(Builder);
  };

  DISALLOW_COPY_AND_ASSIGN(InternalBlobData);
};

}  // namespace storage
#endif  // STORAGE_BROWSER_BLOB_INTERNAL_BLOB_DATA_H_
