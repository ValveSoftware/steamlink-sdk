// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_ASYNC_BUILDER_HOST_H_
#define STORAGE_BROWSER_BLOB_BLOB_ASYNC_BUILDER_HOST_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory_handle.h"
#include "base/memory/weak_ptr.h"
#include "storage/browser/blob/blob_async_transport_request_builder.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_transport_result.h"
#include "storage/browser/storage_browser_export.h"
#include "storage/common/blob_storage/blob_item_bytes_request.h"
#include "storage/common/blob_storage/blob_item_bytes_response.h"
#include "storage/common/blob_storage/blob_storage_constants.h"
#include "storage/common/data_element.h"

namespace base {
class SharedMemory;
}

namespace storage {
class BlobDataHandle;
class BlobStorageContext;

// This class
// * holds all blobs that are currently being built asynchronously for a child
//   process,
// * sends memory requests through the given callback in |StartBuildingBlob|,
//   and uses the BlobTransportResult return value to signify other results,
// * includes all logic for deciding which async transport strategy to use, and
// * handles all blob construction communication with the BlobStorageContext.
// The method |CancelAll| must be called by the consumer, it is not called on
// destruction.
class STORAGE_EXPORT BlobAsyncBuilderHost {
 public:
  using RequestMemoryCallback = base::Callback<void(
      std::unique_ptr<std::vector<storage::BlobItemBytesRequest>>,
      std::unique_ptr<std::vector<base::SharedMemoryHandle>>,
      std::unique_ptr<std::vector<base::File>>)>;
  BlobAsyncBuilderHost();
  ~BlobAsyncBuilderHost();

  // This registers the given blob internally and adds it to the storage.
  // Calling this method also guarentees that the referenced blobs are kept
  // alive for the duration of the construction of this blob.
  // We return
  // * BAD_IPC if we already have the blob registered or if we reference ourself
  //   in the referenced_blob_uuids.
  // * CANCEL_REFERENCED_BLOB_BROKEN if one of the referenced blobs is broken or
  //   doesn't exist. We store the blob in the context as broken  with code
  //   REFERENCED_BLOB_BROKEN.
  // * DONE if we successfully registered the blob.
  BlobTransportResult RegisterBlobUUID(
      const std::string& uuid,
      const std::string& content_type,
      const std::string& content_disposition,
      const std::set<std::string>& referenced_blob_uuids,
      BlobStorageContext* context);

  // This method begins the construction of the blob given the descriptions. The
  // blob uuid MUST be building in this object.
  // When we return:
  // * DONE: The blob is finished transfering right away, and is now
  //   successfully saved in the context.
  // * PENDING_RESPONSES: The async builder host is waiting for responses from
  //   the renderer. It has called |request_memory| for these responses.
  // * CANCEL_*: We have to cancel the blob construction. This function clears
  //   the blob's internal state and marks the blob as broken in the context
  //   before returning.
  // * BAD_IPC: The arguments were invalid/bad. This marks the blob as broken in
  //   the context before returning.
  BlobTransportResult StartBuildingBlob(
      const std::string& uuid,
      const std::vector<DataElement>& elements,
      size_t memory_available,
      BlobStorageContext* context,
      const RequestMemoryCallback& request_memory);

  // This is called when we have responses from the Renderer to our calls to
  // the request_memory callback above. See above for return value meaning.
  BlobTransportResult OnMemoryResponses(
      const std::string& uuid,
      const std::vector<BlobItemBytesResponse>& responses,
      BlobStorageContext* context);

  // This removes the BlobBuildingState from our map and flags the blob as
  // broken in the context. This can be called both from our own logic to cancel
  // the blob, or from the DispatcherHost (Renderer). The blob MUST be being
  // built in this builder.
  // Note: if the blob isn't in the context (renderer dereferenced it before we
  // finished constructing), then we don't bother touching the context.
  void CancelBuildingBlob(const std::string& uuid,
                          IPCBlobCreationCancelCode code,
                          BlobStorageContext* context);

  // This clears this object of pending construction. It also handles marking
  // blobs that haven't been fully constructed as broken in the context if there
  // are any references being held by anyone. We know that they're being used
  // by someone else if they still exist in the context.
  void CancelAll(BlobStorageContext* context);

  bool IsEmpty() const { return async_blob_map_.empty(); }

  size_t blob_building_count() const { return async_blob_map_.size(); }

  bool IsBeingBuilt(const std::string& key) const {
    return async_blob_map_.find(key) != async_blob_map_.end();
  }

  // For testing use only.  Must be called before StartBuildingBlob.
  void SetMemoryConstantsForTesting(size_t max_ipc_memory_size,
                                    size_t max_shared_memory_size,
                                    uint64_t max_file_size) {
    max_ipc_memory_size_ = max_ipc_memory_size;
    max_shared_memory_size_ = max_shared_memory_size;
    max_file_size_ = max_file_size;
  }

 private:
  struct BlobBuildingState {
    // |refernced_blob_handles| should be all handles generated from the set
    // of |refernced_blob_uuids|.
    BlobBuildingState(
        const std::string& uuid,
        std::set<std::string> referenced_blob_uuids,
        std::vector<std::unique_ptr<BlobDataHandle>>* referenced_blob_handles);
    ~BlobBuildingState();

    BlobAsyncTransportRequestBuilder request_builder;
    BlobDataBuilder data_builder;
    std::vector<bool> request_received;
    size_t next_request = 0;
    size_t num_fulfilled_requests = 0;
    std::unique_ptr<base::SharedMemory> shared_memory_block;
    // This is the number of requests that have been sent to populate the above
    // shared data. We won't ask for more data in shared memory until all
    // requests have been responded to.
    size_t num_shared_memory_requests = 0;
    // Only relevant if num_shared_memory_requests is > 0
    size_t current_shared_memory_handle_index = 0;

    // We save these to double check that the RegisterBlob and StartBuildingBlob
    // messages are in sync.
    std::set<std::string> referenced_blob_uuids;
    // These are the blobs that are referenced in the newly constructed blob.
    // We use these to make sure they stay alive while we create the new blob,
    // and to wait until any blobs that are not done building are fully
    // constructed.
    std::vector<std::unique_ptr<BlobDataHandle>> referenced_blob_handles;

    // These are the number of blobs we're waiting for before we can start
    // building.
    size_t num_referenced_blobs_building = 0;

    BlobAsyncBuilderHost::RequestMemoryCallback request_memory_callback;
  };

  typedef std::map<std::string, std::unique_ptr<BlobBuildingState>>
      AsyncBlobMap;

  // This is the 'main loop' of our memory requests to the renderer.
  BlobTransportResult ContinueBlobMemoryRequests(const std::string& uuid,
                                                 BlobStorageContext* context);

  // This is our callback for when we want to finish the blob and we're waiting
  // for blobs we reference to be built. When the last callback occurs, we
  // complete the blob and erase our internal state.
  void ReferencedBlobFinished(const std::string& uuid,
                              base::WeakPtr<BlobStorageContext> context,
                              bool construction_success,
                              IPCBlobCreationCancelCode reason);

  // This finishes creating the blob in the context, decrements blob references
  // that we were holding during construction, and erases our state.
  void FinishBuildingBlob(BlobBuildingState* state,
                          BlobStorageContext* context);

  AsyncBlobMap async_blob_map_;

  // Here for testing.
  size_t max_ipc_memory_size_ = kBlobStorageIPCThresholdBytes;
  size_t max_shared_memory_size_ = kBlobStorageMaxSharedMemoryBytes;
  uint64_t max_file_size_ = kBlobStorageMaxFileSizeBytes;

  base::WeakPtrFactory<BlobAsyncBuilderHost> ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlobAsyncBuilderHost);
};

}  // namespace storage
#endif  // STORAGE_BROWSER_BLOB_BLOB_ASYNC_BUILDER_HOST_H_
