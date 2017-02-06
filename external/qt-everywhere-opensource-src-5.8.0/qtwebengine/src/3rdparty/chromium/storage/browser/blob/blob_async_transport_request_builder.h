// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_ASYNC_TRANSPORT_REQUEST_BUILDER_H_
#define STORAGE_BROWSER_BLOB_BLOB_ASYNC_TRANSPORT_REQUEST_BUILDER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/storage_browser_export.h"
#include "storage/common/blob_storage/blob_item_bytes_request.h"
#include "storage/common/data_element.h"

namespace storage {

// This class generates the requests needed to asynchronously transport the
// given blob items from the renderer to the browser. The main job of this class
// is to segment the memory being transfered to efficiently use shared memory,
// file, and IPC max sizes.
// Note: This class does not compute requests by using the 'shortcut' method,
//       where the data is already present in the blob description, and will
//       always give the caller requests for requesting all data from the
//       renderer.
class STORAGE_EXPORT BlobAsyncTransportRequestBuilder {
 public:
  struct RendererMemoryItemRequest {
    RendererMemoryItemRequest();
    // This is the index of the item in the builder on the browser side.
    size_t browser_item_index;
    // Note: For files this offset should always be 0, as the file offset in
    //       segmentation is handled by the handle_offset in the message.  This
    //       offset is used for populating a chunk when the data comes back to
    //       the browser.
    size_t browser_item_offset;
    BlobItemBytesRequest message;
  };

  BlobAsyncTransportRequestBuilder();
  virtual ~BlobAsyncTransportRequestBuilder();

  // Initializes the request builder for file requests. One or more files are
  // created to hold the given data. Each file can hold data from multiple
  // items, and the data from each item can be in multiple files.
  // See file_handle_sizes() for the generated file sizes.
  // max_file_size: This is the maximum size for a file to back a blob.
  // blob_total_size: This is the total in-memory size of the blob.
  // elements: These are the descriptions of the blob items being sent from the
  //           renderer.
  // builder: This is the builder that is populated with the 'future' versions
  //          of the data elements. In this case, we call 'AppendFutureData' in
  //          the items that we expect to be backed by files writen by the
  //          renderer.
  void InitializeForFileRequests(size_t max_file_size,
                                 uint64_t blob_total_size,
                                 const std::vector<DataElement>& elements,
                                 BlobDataBuilder* builder);

  // Initializes the request builder for shared memory requests. We try to
  // consolidate as much memory as possible in each shared memory segment we
  // use.
  // See shared_memory_handle_sizes() for the shared memory sizes.
  // max_shared_memory_size: This is the maximum size for a shared memory
  //                         segment used to transport the data between renderer
  //                         and browser.
  // blob_total_size: This is the total in-memory size of the blob.
  // elements: These are the descriptions of the blob items being sent from the
  //           renderer.
  // builder: This is the builder that is populated with the 'future' versions
  //          of the data elements. In this case, we call 'AppendFutureData' for
  //          the items we expect to be populated later.
  void InitializeForSharedMemoryRequests(
      size_t max_shared_memory_size,
      uint64_t blob_total_size,
      const std::vector<DataElement>& elements,
      BlobDataBuilder* builder);

  // Initializes the request builder for IPC requests. We put as much memory
  // in a single IPC request as possible.
  // max_ipc_memory_size: This is the maximum size for an IPC message which will
  //                      be used to transport memory from the renderer to the
  //                      browser.
  // blob_total_size: This is the total in-memory size of the blob.
  // elements: These are the descriptions of the blob items being sent from the
  //           renderer.
  // builder: This is the builder that is populated with the 'future' versions
  //          of the data elements. In this case, we call 'AppendFutureData' for
  //          the items we expect to be populated later.
  void InitializeForIPCRequests(size_t max_ipc_memory_size,
                                uint64_t blob_total_size,
                                const std::vector<DataElement>& elements,
                                BlobDataBuilder* builder);

  // The sizes of the shared memory handles being used (by handle index).
  const std::vector<size_t>& shared_memory_sizes() const {
    return shared_memory_sizes_;
  }

  // The sizes of the file handles being used (by handle index).
  const std::vector<size_t>& file_sizes() const { return file_sizes_; }

  // The requests for memory, segmented as described above, along with their
  // destination browser indexes and offsets.
  const std::vector<RendererMemoryItemRequest>& requests() const {
    return requests_;
  }

  // The total bytes size of memory items in the blob.
  uint64_t total_bytes_size() const { return total_bytes_size_; }

  static bool ShouldBeShortcut(const std::vector<DataElement>& items,
                               size_t memory_available);

 private:
  static void ComputeHandleSizes(uint64_t total_memory_size,
                                 size_t max_segment_size,
                                 std::vector<size_t>* segment_sizes);

  std::vector<size_t> shared_memory_sizes_;
  // The size of the files is capped by the |max_file_size| argument in
  // InitializeForFileRequests, so we can just use size_t.
  std::vector<size_t> file_sizes_;

  uint64_t total_bytes_size_;
  std::vector<RendererMemoryItemRequest> requests_;

  DISALLOW_COPY_AND_ASSIGN(BlobAsyncTransportRequestBuilder);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_BLOB_BLOB_ASYNC_TRANSPORT_REQUEST_BUILDER_H_
