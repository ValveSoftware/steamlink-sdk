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
#include "storage/browser/blob/blob_storage_registry.h"
#include "storage/browser/blob/internal_blob_data.h"
#include "storage/browser/storage_browser_export.h"
#include "storage/common/blob_storage/blob_storage_constants.h"
#include "storage/common/data_element.h"

class GURL;

namespace base {
class FilePath;
class Time;
}

namespace content {
class BlobDispatcherHost;
class BlobDispatcherHostTest;
}

namespace storage {

class BlobDataBuilder;
class BlobDataItem;
class BlobDataSnapshot;
class ShareableBlobDataItem;

// This class handles the logistics of blob Storage within the browser process,
// and maintains a mapping from blob uuid to the data. The class is single
// threaded and should only be used on the IO thread.
// In chromium, there is one instance per profile.
class STORAGE_EXPORT BlobStorageContext
    : public base::SupportsWeakPtr<BlobStorageContext> {
 public:
  BlobStorageContext();
  ~BlobStorageContext();

  std::unique_ptr<BlobDataHandle> GetBlobDataFromUUID(const std::string& uuid);
  std::unique_ptr<BlobDataHandle> GetBlobDataFromPublicURL(const GURL& url);

  // Useful for coining blobs from within the browser process. If the
  // blob cannot be added due to memory consumption, returns NULL.
  // A builder should not be used twice to create blobs, as the internal
  // resources are refcounted instead of copied for memory efficiency.
  // To cleanly use a builder multiple times, please call Clone() on the
  // builder, or even better for memory savings, clear the builder and append
  // the previously constructed blob.
  std::unique_ptr<BlobDataHandle> AddFinishedBlob(
      const BlobDataBuilder& builder);

  // Deprecated, use const ref version above.
  std::unique_ptr<BlobDataHandle> AddFinishedBlob(
      const BlobDataBuilder* builder);

  // Useful for coining blob urls from within the browser process.
  bool RegisterPublicBlobURL(const GURL& url, const std::string& uuid);
  void RevokePublicBlobURL(const GURL& url);

  size_t memory_usage() const { return memory_usage_; }
  size_t blob_count() const { return registry_.blob_count(); }
  size_t memory_available() const {
    return kBlobStorageMaxMemoryUsage - memory_usage_;
  }

  const BlobStorageRegistry& registry() { return registry_; }

 private:
  using BlobRegistryEntry = BlobStorageRegistry::Entry;
  using BlobConstructedCallback = BlobStorageRegistry::BlobConstructedCallback;
  friend class content::BlobDispatcherHost;
  friend class BlobAsyncBuilderHost;
  friend class BlobAsyncBuilderHostTest;
  friend class BlobDataHandle;
  friend class BlobDataHandle::BlobDataHandleShared;
  friend class BlobReaderTest;
  FRIEND_TEST_ALL_PREFIXES(BlobReaderTest, HandleBeforeAsyncCancel);
  FRIEND_TEST_ALL_PREFIXES(BlobReaderTest, ReadFromIncompleteBlob);
  friend class BlobStorageContextTest;
  FRIEND_TEST_ALL_PREFIXES(BlobStorageContextTest, IncrementDecrementRef);
  FRIEND_TEST_ALL_PREFIXES(BlobStorageContextTest, OnCancelBuildingBlob);
  FRIEND_TEST_ALL_PREFIXES(BlobStorageContextTest, PublicBlobUrls);
  FRIEND_TEST_ALL_PREFIXES(BlobStorageContextTest,
                           TestUnknownBrokenAndBuildingBlobReference);
  friend class ViewBlobInternalsJob;

  // CompletePendingBlob or CancelPendingBlob should be called after this.
  void CreatePendingBlob(const std::string& uuid,
                         const std::string& content_type,
                         const std::string& content_disposition);

  // This includes resolving blob references in the builder. This will run the
  // callbacks given in RunOnConstructionComplete.
  void CompletePendingBlob(const BlobDataBuilder& external_builder);

  // This will run the callbacks given in RunOnConstructionComplete.
  void CancelPendingBlob(const std::string& uuid,
                         IPCBlobCreationCancelCode reason);

  void IncrementBlobRefCount(const std::string& uuid);
  void DecrementBlobRefCount(const std::string& uuid);

  // Methods called by BlobDataHandle:
  // This will return an empty snapshot until the blob is complete.
  // TODO(dmurph): After we make the snapshot method in BlobHandle private, then
  // make this DCHECK on the blob not being complete.
  std::unique_ptr<BlobDataSnapshot> CreateSnapshot(const std::string& uuid);
  bool IsBroken(const std::string& uuid) const;
  bool IsBeingBuilt(const std::string& uuid) const;
  // Runs |done| when construction completes, with true if it was successful,
  // and false if there was an error, which is reported in the second argument
  // of the callback.
  void RunOnConstructionComplete(const std::string& uuid,
                                 const BlobConstructedCallback& done);

  // Appends the given blob item to the blob builder.  The new blob
  // retains ownership of data_item if applicable, and returns false if there
  // was an error and pouplates the error_code.  We can either have an error of:
  // OUT_OF_MEMORY: We are out of memory to store this blob.
  // REFERENCED_BLOB_BROKEN: One of the referenced blobs is broken.
  bool AppendAllocatedBlobItem(const std::string& target_blob_uuid,
                               scoped_refptr<BlobDataItem> data_item,
                               InternalBlobData::Builder* target_blob_data,
                               IPCBlobCreationCancelCode* error_code);

  // Allocates a shareable blob data item, with blob references resolved.  If
  // there isn't enough memory, then a nullptr is returned.
  scoped_refptr<ShareableBlobDataItem> AllocateShareableBlobDataItem(
      const std::string& target_blob_uuid,
      scoped_refptr<BlobDataItem> data_item);

  // Deconstructs the blob and appends it's contents to the target blob.  Items
  // are shared if possible, and copied if the given offset and length
  // have to split an item.
  bool AppendBlob(const std::string& target_blob_uuid,
                  const InternalBlobData& blob,
                  uint64_t offset,
                  uint64_t length,
                  InternalBlobData::Builder* target_blob_data);

  BlobStorageRegistry registry_;

  // Used to keep track of how much memory is being utilized for blob data,
  // we count only the items of TYPE_DATA which are held in memory and not
  // items of TYPE_FILE.
  size_t memory_usage_;

  DISALLOW_COPY_AND_ASSIGN(BlobStorageContext);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_BLOB_BLOB_STORAGE_CONTEXT_H_
