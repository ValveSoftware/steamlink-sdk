// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_STORAGE_CONTEXT_H_
#define STORAGE_BROWSER_BLOB_BLOB_STORAGE_CONTEXT_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_entry.h"
#include "storage/browser/blob/blob_memory_controller.h"
#include "storage/browser/blob/blob_storage_registry.h"
#include "storage/browser/storage_browser_export.h"
#include "storage/common/blob_storage/blob_storage_constants.h"

class GURL;

namespace base {
class FilePath;
class Time;
class TaskRunner;
}

namespace content {
class BlobDispatcherHost;
class BlobDispatcherHostTest;
}

namespace storage {
class BlobDataBuilder;
class BlobDataHandle;
class BlobDataItem;
class BlobDataSnapshot;
class ShareableBlobDataItem;

// This class handles the logistics of blob storage within the browser process.
// We are single threaded and should only be used on the IO thread. In Chromium
// there is one instance per profile.
class STORAGE_EXPORT BlobStorageContext {
 public:
  using TransportAllowedCallback = BlobEntry::TransportAllowedCallback;

  // Initializes the context without disk support.
  BlobStorageContext();
  ~BlobStorageContext();

  std::unique_ptr<BlobDataHandle> GetBlobDataFromUUID(const std::string& uuid);
  std::unique_ptr<BlobDataHandle> GetBlobDataFromPublicURL(const GURL& url);

  // Always returns a handle to a blob. Use BlobStatus::GetBlobStatus() and
  // BlobStatus::RunOnConstructionComplete(callback) to determine construction
  // completion and possible errors.
  std::unique_ptr<BlobDataHandle> AddFinishedBlob(
      const BlobDataBuilder& builder);

  // Deprecated, use const ref version above or BuildBlob below.
  std::unique_ptr<BlobDataHandle> AddFinishedBlob(
      const BlobDataBuilder* builder);

  std::unique_ptr<BlobDataHandle> AddBrokenBlob(
      const std::string& uuid,
      const std::string& content_type,
      const std::string& content_disposition,
      BlobStatus reason);

  // Useful for coining blob urls from within the browser process.
  bool RegisterPublicBlobURL(const GURL& url, const std::string& uuid);
  void RevokePublicBlobURL(const GURL& url);

  size_t blob_count() const { return registry_.blob_count(); }

  const BlobStorageRegistry& registry() { return registry_; }

  // This builds a blob with the given |input_builder| and returns a handle to
  // the constructed Blob. Blob metadata and data should be accessed through
  // this handle.
  // If there is data present that needs further population then we will call
  // |transport_allowed_callback| when we're ready for the user data to be
  // populated with the PENDING_DATA_POPULATION status. This can happen
  // synchronously or asynchronously. Otherwise |transport_allowed_callback|
  // should be null. In the further population case, the caller must call either
  // NotifyTransportComplete or CancelBuildingBlob after
  // |transport_allowed_callback| is called to signify the data is finished
  // populating or an error occurred (respectively).
  // If the returned handle is broken, then the possible error cases are:
  // * OUT_OF_MEMORY if we don't have enough memory to store the blob,
  // * REFERENCED_BLOB_BROKEN if a referenced blob is broken or we're
  //   referencing ourself.
  std::unique_ptr<BlobDataHandle> BuildBlob(
      const BlobDataBuilder& input_builder,
      const TransportAllowedCallback& transport_allowed_callback);

  // This breaks a blob that is currently being built by using the BuildBlob
  // method above. Any callbacks waiting on this blob, including the
  // |transport_allowed_callback| callback given to BuildBlob, will be called
  // with this status code.
  void CancelBuildingBlob(const std::string& uuid, BlobStatus code);

  // After calling BuildBlob above, the caller should call this method to
  // notify the construction system that the unpopulated data in the given blob
  // has been. populated. Caller must have all pending items populated in the
  // original builder |input_builder| given in BuildBlob or we'll check-fail.
  // If there is no pending data in the |input_builder| for the BuildBlob call,
  // then this method doesn't need to be called.
  void NotifyTransportComplete(const std::string& uuid);

  const BlobMemoryController& memory_controller() { return memory_controller_; }

  base::WeakPtr<BlobStorageContext> AsWeakPtr() {
    return ptr_factory_.GetWeakPtr();
  }

 protected:
  friend class content::BlobDispatcherHost;
  friend class content::BlobDispatcherHostTest;
  friend class BlobTransportHost;
  friend class BlobTransportHostTest;
  friend class BlobDataHandle;
  friend class BlobDataHandle::BlobDataHandleShared;
  friend class BlobFlattenerTest;
  friend class BlobSliceTest;
  friend class BlobStorageContextTest;

  // Transforms a BlobDataBuilder into a BlobEntry with no blob references.
  // BlobSlice is used to flatten out these references. Records the total size
  // and items for memory and file quota requests.
  // Exposed in the header file for testing.
  struct STORAGE_EXPORT BlobFlattener {
    BlobFlattener(const BlobDataBuilder& input_builder,
                  BlobEntry* output_blob,
                  BlobStorageRegistry* registry);
    ~BlobFlattener();

    // This can be:
    // * PENDING_QUOTA if we need memory quota, if we're populated and don't
    // need quota.
    // * PENDING_INTERNALS if we're waiting on dependent blobs or we're done.
    // * INVALID_CONSTRUCTION_ARGUMENTS if we have invalid input.
    // * REFERENCED_BLOB_BROKEN if one of the referenced blobs is broken or we
    //   reference ourself.
    BlobStatus status = BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;

    // This is the total size of the blob, including all memory, files, etc.
    uint64_t total_size = 0;
    // Total memory size of the blob (not including files, etc).
    uint64_t total_memory_size = 0;

    std::vector<std::pair<std::string, BlobEntry*>> dependent_blobs;

    uint64_t memory_quota_needed = 0;
    std::vector<scoped_refptr<ShareableBlobDataItem>> pending_memory_items;

    std::vector<ShareableBlobDataItem*> transport_items;

    // These record all future copies we'll need to do from referenced blobs.
    // This
    // happens when we do a partial slice from a pending data or file item.
    std::vector<BlobEntry::ItemCopyEntry> copies;

   private:
    DISALLOW_COPY_AND_ASSIGN(BlobFlattener);
  };

  // Used when a blob reference has a size and offset. Records the source items
  // and memory we need to copy if either side of slice intersects an item.
  // Exposed in the header file for testing.
  struct STORAGE_EXPORT BlobSlice {
    BlobSlice(const BlobEntry& source,
              uint64_t slice_offset,
              uint64_t slice_size);
    ~BlobSlice();

    // Size of memory copying from the source blob.
    base::CheckedNumeric<size_t> copying_memory_size = 0;
    // Size of all memory for UMA stats.
    base::CheckedNumeric<size_t> total_memory_size = 0;

    size_t first_item_slice_offset = 0;
    // Populated if our first slice item is a temporary item that we'll copy to
    // later from this |first_source_item|, at offset |first_item_slice_offset|.
    scoped_refptr<ShareableBlobDataItem> first_source_item;
    // Populated if our last slice item is a temporary item that we'll copy to
    // later from this |last_source_item|.
    scoped_refptr<ShareableBlobDataItem> last_source_item;

    std::vector<scoped_refptr<ShareableBlobDataItem>> dest_items;

   private:
    DISALLOW_COPY_AND_ASSIGN(BlobSlice);
  };

  void IncrementBlobRefCount(const std::string& uuid);
  void DecrementBlobRefCount(const std::string& uuid);

  // This will return an empty snapshot until the blob is complete.
  // TODO(dmurph): After we make the snapshot method in BlobHandle private, then
  // make this DCHECK on the blob not being complete.
  std::unique_ptr<BlobDataSnapshot> CreateSnapshot(const std::string& uuid);

  BlobStatus GetBlobStatus(const std::string& uuid) const;

  // Runs |done| when construction completes with the final status of the blob.
  void RunOnConstructionComplete(const std::string& uuid,
                                 const BlobStatusCallback& done_callback);

  BlobStorageRegistry* mutable_registry() { return &registry_; }

  BlobMemoryController* mutable_memory_controller() {
    return &memory_controller_;
  }

 private:
  std::unique_ptr<BlobDataHandle> CreateHandle(const std::string& uuid,
                                               BlobEntry* entry);

  void NotifyTransportCompleteInternal(BlobEntry* entry);

  void CancelBuildingBlobInternal(BlobEntry* entry, BlobStatus reason);

  void FinishBuilding(BlobEntry* entry);

  void RequestTransport(
      BlobEntry* entry,
      std::vector<BlobMemoryController::FileCreationInfo> files);

  void OnEnoughSizeForMemory(const std::string& uuid, bool can_fit);

  void OnDependentBlobFinished(const std::string& owning_blob_uuid,
                               BlobStatus reason);

  void ClearAndFreeMemory(BlobEntry* entry);

  BlobStorageRegistry registry_;
  BlobMemoryController memory_controller_;
  base::WeakPtrFactory<BlobStorageContext> ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlobStorageContext);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_BLOB_BLOB_STORAGE_CONTEXT_H_
