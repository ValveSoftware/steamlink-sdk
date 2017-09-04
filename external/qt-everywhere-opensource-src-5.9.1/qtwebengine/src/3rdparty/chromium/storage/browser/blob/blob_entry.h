// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_ENTRY_H_
#define STORAGE_BROWSER_BLOB_BLOB_ENTRY_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "storage/browser/blob/blob_memory_controller.h"
#include "storage/browser/storage_browser_export.h"

namespace storage {
class BlobDataHandle;
class ShareableBlobDataItem;
class ViewBlobInternalsJob;

// This class represents a blob in BlobStorageRegistry. We export this only for
// unit tests.
class STORAGE_EXPORT BlobEntry {
 public:
  using TransportAllowedCallback =
      base::Callback<void(BlobStatus,
                          std::vector<BlobMemoryController::FileCreationInfo>)>;

  // This records a copy from a referenced blob. When we finish building our
  // blob we perform all of these copies.
  struct STORAGE_EXPORT ItemCopyEntry {
    ItemCopyEntry(scoped_refptr<ShareableBlobDataItem> source_item,
                  size_t source_item_offset,
                  scoped_refptr<ShareableBlobDataItem> dest_item);
    ~ItemCopyEntry();
    ItemCopyEntry(ItemCopyEntry&& other);
    BlobEntry::ItemCopyEntry& operator=(BlobEntry::ItemCopyEntry&& rhs);

    scoped_refptr<ShareableBlobDataItem> source_item;
    size_t source_item_offset = 0;
    scoped_refptr<ShareableBlobDataItem> dest_item;

   private:
    DISALLOW_COPY_AND_ASSIGN(ItemCopyEntry);
  };

  // This keeps track of our building state for our blob. While building, four
  // things can be happening mostly simultaneously:
  // 1. Waiting for quota to be reserved for memory needed (PENDING_QUOTA)
  // 2. Waiting for user population of data after quota (PENDING_TRANSPORT)
  // 3. Waiting for blobs we reference to complete (PENDING_INTERNALS)
  struct STORAGE_EXPORT BuildingState {
    // |transport_allowed_callback| is not null when data needs population. See
    // BlobStorageContext::BuildBlob for when the callback is called.
    BuildingState(bool transport_items_present,
                  TransportAllowedCallback transport_allowed_callback,
                  size_t num_building_dependent_blobs);
    ~BuildingState();

    const bool transport_items_present;
    // We can have trasnport data that's either populated or unpopulated. If we
    // need population, this is populated.
    TransportAllowedCallback transport_allowed_callback;
    std::vector<ShareableBlobDataItem*> transport_items;

    // Stores all blobs that we're depending on for building. This keeps the
    // blobs alive while we build our blob.
    std::vector<std::unique_ptr<BlobDataHandle>> dependent_blobs;
    size_t num_building_dependent_blobs;

    base::WeakPtr<BlobMemoryController::QuotaAllocationTask>
        memory_quota_request;

    // These are copies from a referenced blob item to our blob items. Some of
    // these entries may have changed from bytes to files if they were paged.
    std::vector<ItemCopyEntry> copies;

    // When our blob finishes building these callbacks are called.
    std::vector<BlobStatusCallback> build_completion_callbacks;

   private:
    DISALLOW_COPY_AND_ASSIGN(BuildingState);
  };

  BlobEntry(const std::string& content_type,
            const std::string& content_disposition);
  ~BlobEntry();

  // Appends the given shared blob data item to this object.
  void AppendSharedBlobItem(scoped_refptr<ShareableBlobDataItem> item);

  // Returns if we're a pending blob that can finish building.
  bool CanFinishBuilding() const {
    return status_ == BlobStatus::PENDING_INTERNALS &&
           building_state_->num_building_dependent_blobs == 0;
  }

  BlobStatus status() const { return status_; }

  size_t refcount() const { return refcount_; }

  const std::string& content_type() const { return content_type_; }

  const std::string& content_disposition() const {
    return content_disposition_;
  }

  // Total size of this blob in bytes.
  uint64_t total_size() const { return size_; };

  // We record the cumulative offsets of each blob item (except for the first,
  // which would always be 0) to enable binary searching for an item at a
  // specific byte offset.
  const std::vector<uint64_t>& offsets() const { return offsets_; }

  const std::vector<scoped_refptr<ShareableBlobDataItem>>& items() const;

 protected:
  friend class BlobStorageContext;

  void IncrementRefCount() { ++refcount_; }
  void DecrementRefCount() { --refcount_; }

  void set_status(BlobStatus status) { status_ = status; }
  void set_size(uint64_t size) { size_ = size; }

  void ClearItems();
  void ClearOffsets();

  void set_building_state(std::unique_ptr<BuildingState> building_state) {
    building_state_ = std::move(building_state);
  }

 private:
  BlobStatus status_ = BlobStatus::PENDING_QUOTA;
  size_t refcount_ = 0;

  // Metadata.
  std::string content_type_;
  std::string content_disposition_;

  std::vector<scoped_refptr<ShareableBlobDataItem>> items_;

  // Size in bytes. IFF we're a single file then this can be uint64_max.
  uint64_t size_ = 0;

  // Only populated if len(items_) > 1.  Used for binary search.
  // Since the offset of the first item is always 0, we exclude this.
  std::vector<uint64_t> offsets_;

  // Only populated if our status is PENDING_*.
  std::unique_ptr<BuildingState> building_state_;

  DISALLOW_COPY_AND_ASSIGN(BlobEntry);
};

}  // namespace storage
#endif  // STORAGE_BROWSER_BLOB_BLOB_ENTRY_H_
