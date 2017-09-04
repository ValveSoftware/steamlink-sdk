// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_MEMORY_CONTROLLER_H_
#define STORAGE_BROWSER_BLOB_BLOB_MEMORY_CONTROLLER_H_

#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/mru_cache.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "storage/browser/storage_browser_export.h"
#include "storage/common/blob_storage/blob_storage_constants.h"

namespace base {
class TaskRunner;
}

namespace storage {
class DataElement;
class ShareableBlobDataItem;
class ShareableFileReference;

// This class's main responsibility is deciding how blob data gets stored.
// This encompasses:
// * Keeping track of memory & file quota,
// * How to transport the blob data from the renderer (DetermineStrategy),
// * Allocating memory & file quota (ReserveMemoryQuota, ReserveFileQuota)
// * Paging memory quota to disk when we're nearing our memory limit, and
// * Maintaining an LRU of memory items to choose candidates to page to disk
//   (NotifyMemoryItemsUsed).
// This class can only be interacted with on the IO thread.
class STORAGE_EXPORT BlobMemoryController {
 public:
  enum class Strategy {
    // We don't have enough memory for this blob.
    TOO_LARGE,
    // There isn't any memory that needs transporting.
    NONE_NEEDED,
    // Transportation strategies.
    IPC,
    SHARED_MEMORY,
    FILE
  };

  struct STORAGE_EXPORT FileCreationInfo {
    FileCreationInfo();
    ~FileCreationInfo();
    FileCreationInfo(FileCreationInfo&& other);
    FileCreationInfo& operator=(FileCreationInfo&&);

    base::File::Error error = base::File::FILE_ERROR_FAILED;
    base::File file;
    scoped_refptr<base::TaskRunner> file_deletion_runner;
    base::FilePath path;
    scoped_refptr<ShareableFileReference> file_reference;
    base::Time last_modified;
  };

  struct MemoryAllocation {
    MemoryAllocation(base::WeakPtr<BlobMemoryController> controller,
                     uint64_t item_id,
                     size_t length);
    ~MemoryAllocation();

   private:
    base::WeakPtr<BlobMemoryController> controller;
    uint64_t item_id;
    size_t length;

    DISALLOW_COPY_AND_ASSIGN(MemoryAllocation);
  };

  class QuotaAllocationTask {
   public:
    // Operation is cancelled and the callback will NOT be called. This object
    // will be destroyed by this call.
    virtual void Cancel() = 0;

   protected:
    virtual ~QuotaAllocationTask();
  };

  // The bool argument is true if we successfully received memory quota.
  using MemoryQuotaRequestCallback = base::Callback<void(bool)>;
  // The bool argument is true if we successfully received file quota, and the
  // vector argument provides the file info.
  using FileQuotaRequestCallback =
      base::Callback<void(bool, std::vector<FileCreationInfo>)>;

  // We enable file paging if |file_runner| isn't a nullptr.
  BlobMemoryController(const base::FilePath& storage_directory,
                       scoped_refptr<base::TaskRunner> file_runner);
  ~BlobMemoryController();

  // Disables file paging. This cancels all pending file creations and paging
  // operations. Reason is recorded in UMA.
  void DisableFilePaging(base::File::Error reason);

  bool file_paging_enabled() const { return file_paging_enabled_; }

  // Returns the strategy the transportation layer should use to transport the
  // given memory. |preemptive_transported_bytes| are the number of transport
  // bytes that are already populated for us, so we don't haved to request them
  // from the renderer.
  Strategy DetermineStrategy(size_t preemptive_transported_bytes,
                             uint64_t total_transportation_bytes) const;

  // Checks to see if we can reserve quota (disk or memory) for the given size.
  bool CanReserveQuota(uint64_t size) const;

  // Reserves quota for the given |unreserved_memory_items|. The items must be
  // bytes items in QUOTA_NEEDED state which we change to QUOTA_REQUESTED.
  // After we reserve memory quota we change their state to QUOTA_GRANTED, set
  // the 'memory_allocation' on the item, and  call |done_callback|. This can
  // happen synchronously.
  // Returns a task handle if the request is asynchronous for cancellation.
  // NOTE: We don't inspect quota limits and assume the user checked
  //       CanReserveQuota before calling this.
  base::WeakPtr<QuotaAllocationTask> ReserveMemoryQuota(
      std::vector<scoped_refptr<ShareableBlobDataItem>> unreserved_memory_items,
      const MemoryQuotaRequestCallback& done_callback);

  // Reserves quota for the given |unreserved_file_items|. The items must be
  // temporary file items (BlobDataBuilder::IsTemporaryFileItem returns true) in
  // QUOTA_NEEDED state, which we change to QUOTA_REQUESTED. After we reserve
  // file quota we change their state to QUOTA_GRANTED and call
  // |done_callback|.
  // Returns a task handle for cancellation.
  // NOTE: We don't inspect quota limits and assume the user checked
  //       CanReserveQuota before calling this.
  base::WeakPtr<QuotaAllocationTask> ReserveFileQuota(
      std::vector<scoped_refptr<ShareableBlobDataItem>> unreserved_file_items,
      const FileQuotaRequestCallback& done_callback);

  // Called when initially populated or upon later access.
  void NotifyMemoryItemsUsed(
      const std::vector<scoped_refptr<ShareableBlobDataItem>>& items);

  size_t memory_usage() const { return blob_memory_used_; }
  uint64_t disk_usage() const { return disk_used_; }

  const BlobStorageLimits& limits() const { return limits_; }
  void set_limits_for_testing(const BlobStorageLimits& limits) {
    limits_ = limits;
  }

 private:
  class FileQuotaAllocationTask;
  class MemoryQuotaAllocationTask;

  using PendingMemoryQuotaTaskList =
      std::list<std::unique_ptr<MemoryQuotaAllocationTask>>;
  using PendingFileQuotaTaskList =
      std::list<std::unique_ptr<FileQuotaAllocationTask>>;

  base::WeakPtr<QuotaAllocationTask> AppendMemoryTask(
      uint64_t total_bytes_needed,
      std::vector<scoped_refptr<ShareableBlobDataItem>> unreserved_memory_items,
      const MemoryQuotaRequestCallback& done_callback);

  void MaybeGrantPendingMemoryRequests();

  size_t CollectItemsForEviction(
      std::vector<scoped_refptr<ShareableBlobDataItem>>* output);

  // Schedule paging until our memory usage is below our memory limit.
  void MaybeScheduleEvictionUntilSystemHealthy();

  // Called when we've completed evicting a list of items to disk. This is where
  // we swap the bytes items for file items, and update our bookkeeping.
  void OnEvictionComplete(
      scoped_refptr<ShareableFileReference> file_reference,
      std::vector<scoped_refptr<ShareableBlobDataItem>> items,
      size_t total_items_size,
      FileCreationInfo result);

  size_t GetAvailableMemoryForBlobs() const;
  uint64_t GetAvailableFileSpaceForBlobs() const;

  void GrantMemoryAllocations(
      std::vector<scoped_refptr<ShareableBlobDataItem>>* items,
      size_t total_bytes);
  void RevokeMemoryAllocation(uint64_t item_id, size_t length);

  // This is registered as a callback for file deletions on the file reference
  // of our paging files. We decrement the disk space used.
  void OnBlobFileDelete(uint64_t size, const base::FilePath& path);

  base::FilePath GenerateNextPageFileName();

  // This records diagnostic counters of our memory quotas. Called when usage
  // changes.
  void RecordTracingCounters() const;

  BlobStorageLimits limits_;

  // Memory bookkeeping. These numbers are all disjoint.
  // This is the amount of memory we're using for blobs in RAM, including the
  // in_flight_memory_used_.
  size_t blob_memory_used_ = 0;
  // This is memory we're temporarily using while we try to write blob items to
  // disk.
  size_t in_flight_memory_used_ = 0;
  // This is the amount of memory we're using on disk.
  uint64_t disk_used_ = 0;

  // State for GenerateNextPageFileName.
  uint64_t current_file_num_ = 0;

  size_t pending_memory_quota_total_size_ = 0;
  PendingMemoryQuotaTaskList pending_memory_quota_tasks_;
  PendingFileQuotaTaskList pending_file_quota_tasks_;

  int pending_evictions_ = 0;

  bool file_paging_enabled_ = false;
  base::FilePath blob_storage_dir_;
  scoped_refptr<base::TaskRunner> file_runner_;

  // Lifetime of the ShareableBlobDataItem objects is handled externally in the
  // BlobStorageContext class.
  base::MRUCache<uint64_t, ShareableBlobDataItem*> populated_memory_items_;
  size_t populated_memory_items_bytes_ = 0;
  // We need to keep track of items currently being paged to disk so that if
  // another blob successfully grabs a ref, we can prevent it from adding the
  // item to the recent_item_cache_ above.
  std::unordered_set<uint64_t> items_paging_to_file_;

  base::WeakPtrFactory<BlobMemoryController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlobMemoryController);
};
}  // namespace storage
#endif  // STORAGE_BROWSER_BLOB_BLOB_MEMORY_CONTROLLER_H_
