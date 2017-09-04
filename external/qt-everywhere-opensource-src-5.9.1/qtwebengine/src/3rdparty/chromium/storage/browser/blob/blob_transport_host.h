// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_TRANSPORT_HOST_H_
#define STORAGE_BROWSER_BLOB_BLOB_TRANSPORT_HOST_H_

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
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_memory_controller.h"
#include "storage/browser/blob/blob_transport_request_builder.h"
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

// This class facilitates moving memory from the renderer to the browser.
class STORAGE_EXPORT BlobTransportHost {
 public:
  // One is expected to use std::move when calling this callback.
  using RequestMemoryCallback =
      base::Callback<void(std::vector<storage::BlobItemBytesRequest>,
                          std::vector<base::SharedMemoryHandle>,
                          std::vector<base::File>)>;

  BlobTransportHost();
  ~BlobTransportHost();

  // This registers the given blob internally and adds it to the storage with a
  // refcount of 1. |completion_callback| is called synchronously or
  // asynchronously with:
  // * INVALID_CONSTRUCTION_ARGUMENTS if we have invalid input arguments/data.
  // * REFERENCED_BLOB_BROKEN if one of the referenced blobs is broken or
  //   doesn't exist.
  // * DONE if we don't need any more data transported and we can clean up.
  // Returns a blob handle that is never null.
  std::unique_ptr<BlobDataHandle> StartBuildingBlob(
      const std::string& uuid,
      const std::string& content_type,
      const std::string& content_disposition,
      const std::vector<DataElement>& elements,
      BlobStorageContext* context,
      const RequestMemoryCallback& request_memory,
      const BlobStatusCallback& completion_callback);

  // This is called when we have responses from the Renderer to our calls to
  // the request_memory callback above. The callbacks given in StartBuildingBlob
  // will be used to request for more memory or signal completion.
  // Note: The uuid must be being built in this host (IsBeingBuilt).
  void OnMemoryResponses(const std::string& uuid,
                         const std::vector<BlobItemBytesResponse>& responses,
                         BlobStorageContext* context);

  // This removes the TransportState from our map and flags the blob as broken
  // in the context. This can be called both from our own logic to cancel the
  // blob, or from the DispatcherHost (Renderer). The blob MUST be being built
  // in this builder. This also calls the |completion_callback|.
  // Note: if the blob isn't in the context (renderer dereferenced it before we
  // finished constructing), then we don't bother touching the context.
  void CancelBuildingBlob(const std::string& uuid,
                          BlobStatus code,
                          BlobStorageContext* context);

  // This clears this object of pending construction and does NOT call transport
  // complete callbacks.
  void CancelAll(BlobStorageContext* context);

  bool IsEmpty() const { return async_blob_map_.empty(); }

  size_t blob_building_count() const { return async_blob_map_.size(); }

  bool IsBeingBuilt(const std::string& key) const {
    return async_blob_map_.find(key) != async_blob_map_.end();
  }

 private:
  struct TransportState {
    TransportState(const std::string& uuid,
                   const std::string& content_type,
                   const std::string& content_disposition,
                   RequestMemoryCallback request_memory_callback,
                   BlobStatusCallback completion_callback);
    TransportState(TransportState&&);
    TransportState& operator=(TransportState&&);
    DISALLOW_COPY_AND_ASSIGN(TransportState);
    ~TransportState();

    IPCBlobItemRequestStrategy strategy = IPCBlobItemRequestStrategy::UNKNOWN;
    BlobTransportRequestBuilder request_builder;
    BlobDataBuilder data_builder;
    std::vector<bool> request_received;
    size_t num_fulfilled_requests = 0;

    RequestMemoryCallback request_memory_callback;
    BlobStatusCallback completion_callback;

    // Used by shared memory strategy.
    size_t next_request = 0;
    std::unique_ptr<base::SharedMemory> shared_memory_block;
    // This is the number of requests that have been sent to populate the above
    // shared data. We won't ask for more data in shared memory until all
    // requests have been responded to.
    size_t num_shared_memory_requests = 0;
    // Only relevant if num_shared_memory_requests is > 0
    size_t current_shared_memory_handle_index = 0;

    // Used by file strategy.
    std::vector<scoped_refptr<ShareableFileReference>> files;
  };

  typedef std::unordered_map<std::string, TransportState> AsyncBlobMap;

  void StartRequests(
      const std::string& uuid,
      TransportState* state,
      BlobStorageContext* context,
      std::vector<BlobMemoryController::FileCreationInfo> file_infos);

  void OnReadyForTransport(
      const std::string& uuid,
      base::WeakPtr<BlobStorageContext> context,
      BlobStatus status,
      std::vector<BlobMemoryController::FileCreationInfo> file_infos);

  void SendIPCRequests(TransportState* state, BlobStorageContext* context);
  void OnIPCResponses(const std::string& uuid,
                      TransportState* state,
                      const std::vector<BlobItemBytesResponse>& responses,
                      BlobStorageContext* context);

  // This is the 'main loop' of our memory requests to the renderer.
  void ContinueSharedMemoryRequests(const std::string& uuid,
                                    TransportState* state,
                                    BlobStorageContext* context);

  void OnSharedMemoryResponses(
      const std::string& uuid,
      TransportState* state,
      const std::vector<BlobItemBytesResponse>& responses,
      BlobStorageContext* context);

  void SendFileRequests(
      TransportState* state,
      BlobStorageContext* context,
      std::vector<BlobMemoryController::FileCreationInfo> files);

  void OnFileResponses(const std::string& uuid,
                       TransportState* state,
                       const std::vector<BlobItemBytesResponse>& responses,
                       BlobStorageContext* context);

  // This finishes creating the blob in the context, decrements blob references
  // that we were holding during construction, and erases our state.
  void CompleteTransport(TransportState* state, BlobStorageContext* context);

  AsyncBlobMap async_blob_map_;
  base::WeakPtrFactory<BlobTransportHost> ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlobTransportHost);
};

}  // namespace storage
#endif  // STORAGE_BROWSER_BLOB_BLOB_TRANSPORT_HOST_H_
