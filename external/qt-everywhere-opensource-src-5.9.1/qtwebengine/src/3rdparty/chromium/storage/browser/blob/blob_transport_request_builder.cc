// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "base/numerics/safe_math.h"
#include "storage/browser/blob/blob_transport_request_builder.h"
#include "storage/common/blob_storage/blob_storage_constants.h"

namespace storage {
namespace {
bool IsBytes(DataElement::Type type) {
  return type == DataElement::TYPE_BYTES ||
         type == DataElement::TYPE_BYTES_DESCRIPTION;
}

// This is the general template that each strategy below implements. See the
// ForEachWithSegment method for a description of how these are called.
// class BlobSegmentVisitor {
//  public:
//   typedef ___ SizeType;
//   void VisitBytesSegment(size_t element_index, uint64_t element_offset,
//                          size_t segment_index, uint64_t segment_offset,
//                          uint64_t size);
//   void VisitNonBytesSegment(const DataElement& element, size_t element_idx);
//   void Done();
// };

// This class handles the logic of how transported memory is going to be
// represented as storage in the browser.  The main idea is that all the memory
// is now packed into file chunks, and the browser items will just reference
// the file with offsets and sizes.
class FileStorageStrategy {
 public:
  FileStorageStrategy(
      std::vector<BlobTransportRequestBuilder::RendererMemoryItemRequest>*
          requests,
      BlobDataBuilder* builder)
      : requests(requests), builder(builder), current_item_index(0) {}

  ~FileStorageStrategy() {}

  void VisitBytesSegment(size_t element_index,
                         uint64_t element_offset,
                         size_t segment_index,
                         uint64_t segment_offset,
                         uint64_t size) {
    BlobTransportRequestBuilder::RendererMemoryItemRequest request;
    request.browser_item_index = current_item_index;
    request.browser_item_offset = 0;
    request.message.request_number = requests->size();
    request.message.transport_strategy = IPCBlobItemRequestStrategy::FILE;
    request.message.renderer_item_index = element_index;
    request.message.renderer_item_offset = element_offset;
    request.message.size = size;
    request.message.handle_index = segment_index;
    request.message.handle_offset = segment_offset;

    requests->push_back(request);
    builder->AppendFutureFile(segment_offset, size, segment_index);
    current_item_index++;
  }

  void VisitNonBytesSegment(const DataElement& element, size_t element_index) {
    builder->AppendIPCDataElement(element);
    current_item_index++;
  }

  void Done() {}

  std::vector<BlobTransportRequestBuilder::RendererMemoryItemRequest>* requests;
  BlobDataBuilder* builder;

  size_t current_item_index;
};

// This class handles the logic of storing memory that is transported as
// consolidated shared memory.
class SharedMemoryStorageStrategy {
 public:
  SharedMemoryStorageStrategy(
      size_t max_segment_size,
      std::vector<BlobTransportRequestBuilder::RendererMemoryItemRequest>*
          requests,
      BlobDataBuilder* builder)
      : requests(requests),
        max_segment_size(max_segment_size),
        builder(builder),
        current_item_size(0),
        current_item_index(0) {}
  ~SharedMemoryStorageStrategy() {}

  void VisitBytesSegment(size_t element_index,
                         uint64_t element_offset,
                         size_t segment_index,
                         uint64_t segment_offset,
                         uint64_t size) {
    if (current_item_size + size > max_segment_size) {
      builder->AppendFutureData(current_item_size);
      current_item_index++;
      current_item_size = 0;
    }
    BlobTransportRequestBuilder::RendererMemoryItemRequest request;
    request.browser_item_index = current_item_index;
    request.browser_item_offset = current_item_size;
    request.message.request_number = requests->size();
    request.message.transport_strategy =
        IPCBlobItemRequestStrategy::SHARED_MEMORY;
    request.message.renderer_item_index = element_index;
    request.message.renderer_item_offset = element_offset;
    request.message.size = size;
    request.message.handle_index = segment_index;
    request.message.handle_offset = segment_offset;

    requests->push_back(request);
    current_item_size += size;
  }

  void VisitNonBytesSegment(const DataElement& element, size_t element_index) {
    if (current_item_size != 0) {
      builder->AppendFutureData(current_item_size);
      current_item_index++;
    }
    builder->AppendIPCDataElement(element);
    current_item_index++;
    current_item_size = 0;
  }

  void Done() {
    if (current_item_size != 0) {
      builder->AppendFutureData(current_item_size);
    }
  }

  std::vector<BlobTransportRequestBuilder::RendererMemoryItemRequest>* requests;

  size_t max_segment_size;
  BlobDataBuilder* builder;
  size_t current_item_size;
  uint64_t current_item_index;
};

// This iterates of the data elements and segments the 'bytes' data into
// the smallest number of segments given the max_segment_size.
// The callback describes either:
// * A non-memory item
// * A partition of a bytes element which will be populated into a given
//   segment and segment offset.
// More specifically, we split each |element| into one or more |segments| of a
// max_size, invokes the strategy to determine the request to make for each
// |segment| produced. A |segment| can also span multiple |elements|.
// Assumptions: All memory items are consolidated.  As in, there are no two
//              'bytes' items next to eachother.
template <typename Visitor>
void ForEachWithSegment(const std::vector<DataElement>& elements,
                        uint64_t max_segment_size,
                        Visitor* visitor) {
  DCHECK_GT(max_segment_size, 0ull);
  size_t segment_index = 0;
  uint64_t segment_offset = 0;
  size_t elements_length = elements.size();
  for (size_t element_index = 0; element_index < elements_length;
       ++element_index) {
    const auto& element = elements.at(element_index);
    DataElement::Type type = element.type();
    if (!IsBytes(type)) {
      visitor->VisitNonBytesSegment(element, element_index);
      continue;
    }
    uint64_t element_memory_left = element.length();
    uint64_t element_offset = 0;
    while (element_memory_left > 0) {
      if (segment_offset == max_segment_size) {
        ++segment_index;
        segment_offset = 0;
      }
      uint64_t memory_writing =
          std::min(max_segment_size - segment_offset, element_memory_left);
      visitor->VisitBytesSegment(element_index, element_offset, segment_index,
                                 segment_offset, memory_writing);
      element_memory_left -= memory_writing;
      segment_offset += memory_writing;
      element_offset += memory_writing;
    }
  }
  visitor->Done();
}
}  // namespace

BlobTransportRequestBuilder::RendererMemoryItemRequest::
    RendererMemoryItemRequest()
    : browser_item_index(0), browser_item_offset(0) {}

BlobTransportRequestBuilder::BlobTransportRequestBuilder()
    : total_bytes_size_(0) {}

BlobTransportRequestBuilder::BlobTransportRequestBuilder(
    BlobTransportRequestBuilder&&) = default;
BlobTransportRequestBuilder& BlobTransportRequestBuilder::operator=(
    BlobTransportRequestBuilder&&) = default;
BlobTransportRequestBuilder::~BlobTransportRequestBuilder() {}

// Initializes the transport strategy for file requests.
void BlobTransportRequestBuilder::InitializeForFileRequests(
    size_t max_file_size,
    uint64_t blob_total_size,
    const std::vector<DataElement>& elements,
    BlobDataBuilder* builder) {
  DCHECK(requests_.empty());
  total_bytes_size_ = blob_total_size;
  ComputeHandleSizes(total_bytes_size_, max_file_size, &file_sizes_);
  FileStorageStrategy strategy(&requests_, builder);
  ForEachWithSegment(elements, static_cast<uint64_t>(max_file_size), &strategy);
}

void BlobTransportRequestBuilder::InitializeForSharedMemoryRequests(
    size_t max_shared_memory_size,
    uint64_t blob_total_size,
    const std::vector<DataElement>& elements,
    BlobDataBuilder* builder) {
  DCHECK(requests_.empty());
  DCHECK(blob_total_size <= std::numeric_limits<size_t>::max());
  total_bytes_size_ = blob_total_size;
  ComputeHandleSizes(total_bytes_size_, max_shared_memory_size,
                     &shared_memory_sizes_);
  SharedMemoryStorageStrategy strategy(max_shared_memory_size, &requests_,
                                       builder);
  ForEachWithSegment(elements, static_cast<uint64_t>(max_shared_memory_size),
                     &strategy);
}

void BlobTransportRequestBuilder::InitializeForIPCRequests(
    size_t max_ipc_memory_size,
    uint64_t blob_total_size,
    const std::vector<DataElement>& elements,
    BlobDataBuilder* builder) {
  DCHECK(requests_.empty());
  // We don't segment anything, and just request the memory items directly
  // in IPC.
  size_t items_length = elements.size();
  total_bytes_size_ = blob_total_size;
  for (size_t i = 0; i < items_length; i++) {
    const auto& info = elements.at(i);
    if (!IsBytes(info.type())) {
      builder->AppendIPCDataElement(info);
      continue;
    }
    BlobTransportRequestBuilder::RendererMemoryItemRequest request;
    request.browser_item_index = i;
    request.browser_item_offset = 0;
    request.message.request_number = requests_.size();
    request.message.transport_strategy = IPCBlobItemRequestStrategy::IPC;
    request.message.renderer_item_index = i;
    request.message.renderer_item_offset = 0;
    request.message.size = info.length();
    requests_.push_back(request);
    builder->AppendFutureData(info.length());
  }
}

/* static */
void BlobTransportRequestBuilder::ComputeHandleSizes(
    uint64_t total_memory_size,
    size_t max_segment_size,
    std::vector<size_t>* segment_sizes) {
  size_t total_max_segments =
      static_cast<size_t>(total_memory_size / max_segment_size);
  bool has_extra_segment = (total_memory_size % max_segment_size) > 0;
  segment_sizes->reserve(total_max_segments + (has_extra_segment ? 1 : 0));
  segment_sizes->insert(segment_sizes->begin(), total_max_segments,
                        max_segment_size);
  if (has_extra_segment) {
    segment_sizes->push_back(total_memory_size % max_segment_size);
  }
}

}  // namespace storage
