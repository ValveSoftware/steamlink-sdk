// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "storage/common/blob_storage/blob_item_bytes_request.h"

namespace storage {

BlobItemBytesRequest BlobItemBytesRequest::CreateIPCRequest(
    uint32_t request_number,
    uint32_t renderer_item_index,
    uint64_t renderer_item_offset,
    uint64_t size) {
  return BlobItemBytesRequest(request_number, IPCBlobItemRequestStrategy::IPC,
                              renderer_item_index, renderer_item_offset, size,
                              kInvalidIndex, kInvalidSize);
}
BlobItemBytesRequest BlobItemBytesRequest::CreateSharedMemoryRequest(
    uint32_t request_number,
    uint32_t renderer_item_index,
    uint64_t renderer_item_offset,
    uint64_t size,
    uint32_t handle_index,
    uint64_t handle_offset) {
  return BlobItemBytesRequest(request_number,
                              IPCBlobItemRequestStrategy::SHARED_MEMORY,
                              renderer_item_index, renderer_item_offset, size,
                              handle_index, handle_offset);
}

BlobItemBytesRequest BlobItemBytesRequest::CreateFileRequest(
    uint32_t request_number,
    uint32_t renderer_item_index,
    uint64_t renderer_item_offset,
    uint64_t size,
    uint32_t handle_index,
    uint64_t handle_offset) {
  return BlobItemBytesRequest(request_number, IPCBlobItemRequestStrategy::FILE,
                              renderer_item_index, renderer_item_offset, size,
                              handle_index, handle_offset);
}

BlobItemBytesRequest::BlobItemBytesRequest()
    : request_number(kInvalidIndex),
      transport_strategy(IPCBlobItemRequestStrategy::UNKNOWN),
      renderer_item_index(kInvalidIndex),
      renderer_item_offset(kInvalidSize),
      size(kInvalidSize),
      handle_index(kInvalidIndex),
      handle_offset(kInvalidSize) {}

BlobItemBytesRequest::BlobItemBytesRequest(
    uint32_t request_number,
    IPCBlobItemRequestStrategy transport_strategy,
    uint32_t renderer_item_index,
    uint64_t renderer_item_offset,
    uint64_t size,
    uint32_t handle_index,
    uint64_t handle_offset)
    : request_number(request_number),
      transport_strategy(transport_strategy),
      renderer_item_index(renderer_item_index),
      renderer_item_offset(renderer_item_offset),
      size(size),
      handle_index(handle_index),
      handle_offset(handle_offset) {}

BlobItemBytesRequest::~BlobItemBytesRequest() {}

void PrintTo(const BlobItemBytesRequest& request, std::ostream* os) {
  *os << "{request_number: " << request.request_number
      << ", transport_strategy: "
      << static_cast<int>(request.transport_strategy)
      << ", renderer_item_index: " << request.renderer_item_index
      << ", renderer_item_offset: " << request.renderer_item_offset
      << ", size: " << request.size
      << ", handle_index: " << request.handle_index
      << ", handle_offset: " << request.handle_offset << "}";
}

bool operator==(const BlobItemBytesRequest& a, const BlobItemBytesRequest& b) {
  return a.request_number == b.request_number &&
         a.transport_strategy == b.transport_strategy &&
         a.renderer_item_index == b.renderer_item_index &&
         a.renderer_item_offset == b.renderer_item_offset && a.size == b.size &&
         a.handle_index == b.handle_index && a.handle_offset == b.handle_offset;
}

bool operator!=(const BlobItemBytesRequest& a, const BlobItemBytesRequest& b) {
  return !(a == b);
}

}  // namespace storage
