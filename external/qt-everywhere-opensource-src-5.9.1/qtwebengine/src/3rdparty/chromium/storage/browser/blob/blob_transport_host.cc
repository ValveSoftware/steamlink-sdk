// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "base/stl_util.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_memory_controller.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/blob_transport_host.h"

namespace storage {
namespace {
using MemoryStrategy = BlobMemoryController::Strategy;
using MemoryItemRequest =
    BlobTransportRequestBuilder::RendererMemoryItemRequest;

bool CalculateBlobMemorySize(const std::vector<DataElement>& elements,
                             size_t* shortcut_bytes,
                             uint64_t* total_bytes) {
  DCHECK(shortcut_bytes);
  DCHECK(total_bytes);

  base::CheckedNumeric<uint64_t> total_size_checked = 0;
  base::CheckedNumeric<size_t> shortcut_size_checked = 0;
  for (const auto& e : elements) {
    if (e.type() == DataElement::TYPE_BYTES) {
      total_size_checked += e.length();
      shortcut_size_checked += e.length();
    } else if (e.type() == DataElement::TYPE_BYTES_DESCRIPTION) {
      total_size_checked += e.length();
    } else {
      continue;
    }
    if (!total_size_checked.IsValid() || !shortcut_size_checked.IsValid())
      return false;
  }
  *shortcut_bytes = shortcut_size_checked.ValueOrDie();
  *total_bytes = total_size_checked.ValueOrDie();
  return true;
}
}  // namespace

BlobTransportHost::TransportState::TransportState(
    const std::string& uuid,
    const std::string& content_type,
    const std::string& content_disposition,
    RequestMemoryCallback request_memory_callback,
    BlobStatusCallback completion_callback)
    : data_builder(uuid),
      request_memory_callback(std::move(request_memory_callback)),
      completion_callback(std::move(completion_callback)) {
  data_builder.set_content_type(content_type);
  data_builder.set_content_disposition(content_disposition);
}

BlobTransportHost::TransportState::TransportState(
    BlobTransportHost::TransportState&&) = default;
BlobTransportHost::TransportState& BlobTransportHost::TransportState::operator=(
    BlobTransportHost::TransportState&&) = default;

BlobTransportHost::TransportState::~TransportState() {}

BlobTransportHost::BlobTransportHost() : ptr_factory_(this) {}

BlobTransportHost::~BlobTransportHost() {}

std::unique_ptr<BlobDataHandle> BlobTransportHost::StartBuildingBlob(
    const std::string& uuid,
    const std::string& content_type,
    const std::string& content_disposition,
    const std::vector<DataElement>& elements,
    BlobStorageContext* context,
    const RequestMemoryCallback& request_memory,
    const BlobStatusCallback& completion_callback) {
  DCHECK(context);
  DCHECK(async_blob_map_.find(uuid) == async_blob_map_.end());
  std::unique_ptr<BlobDataHandle> handle;
  // Validate that our referenced blobs aren't us.
  for (const DataElement& e : elements) {
    if (e.type() == DataElement::TYPE_BLOB && e.blob_uuid() == uuid) {
      handle = context->AddBrokenBlob(
          uuid, content_type, content_disposition,
          BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS);
      completion_callback.Run(BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS);
      return handle;
    }
  }
  uint64_t transport_memory_size = 0;
  size_t shortcut_size = 0;
  if (!CalculateBlobMemorySize(elements, &shortcut_size,
                               &transport_memory_size)) {
    handle =
        context->AddBrokenBlob(uuid, content_type, content_disposition,
                               BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS);
    completion_callback.Run(BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS);
    return handle;
  }

  const BlobMemoryController& memory_controller = context->memory_controller();
  MemoryStrategy memory_strategy =
      memory_controller.DetermineStrategy(shortcut_size, transport_memory_size);
  TransportState state(uuid, content_type, content_disposition, request_memory,
                       completion_callback);
  switch (memory_strategy) {
    case MemoryStrategy::TOO_LARGE: {
      handle = context->AddBrokenBlob(uuid, content_type, content_disposition,
                                      BlobStatus::ERR_OUT_OF_MEMORY);
      completion_callback.Run(BlobStatus::ERR_OUT_OF_MEMORY);
      return handle;
    }
    case MemoryStrategy::NONE_NEEDED: {
      // This means we don't need to transport anything, so just send the
      // elements along and tell our callback we're done transporting.
      for (const DataElement& e : elements) {
        DCHECK_NE(e.type(), DataElement::TYPE_BYTES_DESCRIPTION);
        state.data_builder.AppendIPCDataElement(e);
      }
      handle = context->BuildBlob(
          state.data_builder, BlobStorageContext::TransportAllowedCallback());
      completion_callback.Run(BlobStatus::DONE);
      return handle;
    }
    case MemoryStrategy::IPC:
      state.strategy = IPCBlobItemRequestStrategy::IPC;
      state.request_builder.InitializeForIPCRequests(
          memory_controller.limits().max_ipc_memory_size, transport_memory_size,
          elements, &(state.data_builder));
      break;
    case MemoryStrategy::SHARED_MEMORY:
      state.strategy = IPCBlobItemRequestStrategy::SHARED_MEMORY;
      state.request_builder.InitializeForSharedMemoryRequests(
          memory_controller.limits().max_shared_memory_size,
          transport_memory_size, elements, &(state.data_builder));
      break;
    case MemoryStrategy::FILE:
      state.strategy = IPCBlobItemRequestStrategy::FILE;
      state.request_builder.InitializeForFileRequests(
          memory_controller.limits().max_file_size, transport_memory_size,
          elements, &(state.data_builder));
      break;
  }
  // We initialize our requests received state now that they are populated.
  state.request_received.resize(state.request_builder.requests().size(), false);
  auto it_pair = async_blob_map_.insert(std::make_pair(uuid, std::move(state)));

  // OnReadyForTransport can be called synchronously.
  handle = context->BuildBlob(
      it_pair.first->second.data_builder,
      base::Bind(&BlobTransportHost::OnReadyForTransport,
                 ptr_factory_.GetWeakPtr(), uuid, context->AsWeakPtr()));
  return handle;
}

void BlobTransportHost::OnMemoryResponses(
    const std::string& uuid,
    const std::vector<BlobItemBytesResponse>& responses,
    BlobStorageContext* context) {
  AsyncBlobMap::iterator state_it = async_blob_map_.find(uuid);
  DCHECK(state_it != async_blob_map_.end()) << "Could not find blob " << uuid;
  if (responses.empty()) {
    CancelBuildingBlob(uuid, BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS,
                       context);
    return;
  }

  // Validate response sanity: it should refer to a legal request number, and
  // we shouldn't have received an answer for that request yet.
  TransportState& state = state_it->second;
  const auto& requests = state.request_builder.requests();
  for (const BlobItemBytesResponse& response : responses) {
    if (response.request_number >= requests.size()) {
      // Bad IPC, so we delete our record and ignore.
      DVLOG(1) << "Invalid request number " << response.request_number;
      CancelBuildingBlob(uuid, BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS,
                         context);
      return;
    }
    DCHECK_LT(response.request_number, state.request_received.size());
    if (state.request_received[response.request_number]) {
      // Bad IPC, so we delete our record.
      DVLOG(1) << "Already received response for that request.";
      CancelBuildingBlob(uuid, BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS,
                         context);
      return;
    }
    state.request_received[response.request_number] = true;
  }
  switch (state.strategy) {
    case IPCBlobItemRequestStrategy::IPC:
      OnIPCResponses(uuid, &state, responses, context);
      break;
    case IPCBlobItemRequestStrategy::SHARED_MEMORY:
      OnSharedMemoryResponses(uuid, &state, responses, context);
      break;
    case IPCBlobItemRequestStrategy::FILE:
      OnFileResponses(uuid, &state, responses, context);
      break;
    case IPCBlobItemRequestStrategy::UNKNOWN:
      NOTREACHED();
      break;
  }
}

void BlobTransportHost::CancelBuildingBlob(const std::string& uuid,
                                           BlobStatus code,
                                           BlobStorageContext* context) {
  DCHECK(context);
  DCHECK(BlobStatusIsError(code));
  AsyncBlobMap::iterator state_it = async_blob_map_.find(uuid);
  if (state_it == async_blob_map_.end())
    return;
  // We can have the blob dereferenced by the renderer, but have it still being
  // 'built'. In this case, it's destructed in the context, but we still have
  // it in our map. Hence we make sure the context has the entry before
  // calling cancel.
  BlobStatusCallback completion_callback = state_it->second.completion_callback;
  async_blob_map_.erase(state_it);
  if (context->registry().HasEntry(uuid))
    context->CancelBuildingBlob(uuid, code);

  completion_callback.Run(code);
}

void BlobTransportHost::CancelAll(BlobStorageContext* context) {
  DCHECK(context);
  // We cancel all of our blobs just in case another renderer referenced them.
  std::vector<std::string> pending_blobs;
  for (const auto& uuid_state_pair : async_blob_map_) {
    pending_blobs.push_back(uuid_state_pair.second.data_builder.uuid());
  }
  // We clear the map before canceling them to prevent any strange reentry into
  // our class (see OnReadyForTransport) if any blobs were waiting for others
  // to construct.
  async_blob_map_.clear();
  for (auto& uuid : pending_blobs) {
    if (context->registry().HasEntry(uuid))
      context->CancelBuildingBlob(uuid, BlobStatus::ERR_SOURCE_DIED_IN_TRANSIT);
  }
}

void BlobTransportHost::StartRequests(
    const std::string& uuid,
    TransportState* state,
    BlobStorageContext* context,
    std::vector<BlobMemoryController::FileCreationInfo> file_infos) {
  switch (state->strategy) {
    case IPCBlobItemRequestStrategy::IPC:
      DCHECK(file_infos.empty());
      SendIPCRequests(state, context);
      break;
    case IPCBlobItemRequestStrategy::SHARED_MEMORY:
      DCHECK(file_infos.empty());
      ContinueSharedMemoryRequests(uuid, state, context);
      break;
    case IPCBlobItemRequestStrategy::FILE:
      DCHECK(!file_infos.empty());
      SendFileRequests(state, context, std::move(file_infos));
      break;
    case IPCBlobItemRequestStrategy::UNKNOWN:
      NOTREACHED();
      break;
  }
}

// Note: This can be called when we cancel a blob in the context.
void BlobTransportHost::OnReadyForTransport(
    const std::string& uuid,
    base::WeakPtr<BlobStorageContext> context,
    BlobStatus status,
    std::vector<BlobMemoryController::FileCreationInfo> file_infos) {
  if (!context) {
    async_blob_map_.erase(uuid);
    return;
  }
  AsyncBlobMap::iterator state_it = async_blob_map_.find(uuid);
  if (state_it == async_blob_map_.end())
    return;

  TransportState& state = state_it->second;
  if (BlobStatusIsPending(status)) {
    DCHECK(status == BlobStatus::PENDING_TRANSPORT);
    StartRequests(uuid, &state, context.get(), std::move(file_infos));
    return;
  }
  // Done or error.
  BlobStatusCallback completion_callback = state.completion_callback;
  async_blob_map_.erase(state_it);
  completion_callback.Run(status);
}

void BlobTransportHost::SendIPCRequests(TransportState* state,
                                        BlobStorageContext* context) {
  const std::vector<MemoryItemRequest>& requests =
      state->request_builder.requests();
  std::vector<BlobItemBytesRequest> byte_requests;

  DCHECK(!requests.empty());
  for (const MemoryItemRequest& request : requests) {
    byte_requests.push_back(request.message);
  }

  state->request_memory_callback.Run(std::move(byte_requests),
                                     std::vector<base::SharedMemoryHandle>(),
                                     std::vector<base::File>());
}

void BlobTransportHost::OnIPCResponses(
    const std::string& uuid,
    TransportState* state,
    const std::vector<BlobItemBytesResponse>& responses,
    BlobStorageContext* context) {
  const auto& requests = state->request_builder.requests();
  size_t num_requests = requests.size();
  for (const BlobItemBytesResponse& response : responses) {
    const MemoryItemRequest& request = requests[response.request_number];
    if (response.inline_data.size() < request.message.size) {
      DVLOG(1) << "Invalid data size " << response.inline_data.size()
               << " vs requested size of " << request.message.size;
      CancelBuildingBlob(uuid, BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS,
                         context);
      return;
    }
    bool success = state->data_builder.PopulateFutureData(
        request.browser_item_index, response.inline_data.data(),
        request.browser_item_offset, request.message.size);
    if (!success) {
      CancelBuildingBlob(uuid, BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS,
                         context);
      return;
    }
    state->num_fulfilled_requests++;
  }
  if (state->num_fulfilled_requests == num_requests)
    CompleteTransport(state, context);
}

void BlobTransportHost::ContinueSharedMemoryRequests(
    const std::string& uuid,
    TransportState* state,
    BlobStorageContext* context) {
  BlobTransportRequestBuilder& request_builder = state->request_builder;
  const std::vector<MemoryItemRequest>& requests = request_builder.requests();
  size_t num_requests = requests.size();
  DCHECK_LT(state->num_fulfilled_requests, num_requests);
  if (state->next_request == num_requests) {
    // We are still waiting on other requests to come back.
    return;
  }

  std::vector<BlobItemBytesRequest> byte_requests;
  std::vector<base::SharedMemoryHandle> shared_memory;

  for (; state->next_request < num_requests; ++state->next_request) {
    const MemoryItemRequest& request = requests[state->next_request];
    bool using_shared_memory_handle = state->num_shared_memory_requests > 0;
    if (using_shared_memory_handle &&
        state->current_shared_memory_handle_index !=
            request.message.handle_index) {
      // We only want one shared memory per requesting blob.
      break;
    }
    state->current_shared_memory_handle_index = request.message.handle_index;
    state->num_shared_memory_requests++;

    if (!state->shared_memory_block) {
      state->shared_memory_block.reset(new base::SharedMemory());
      size_t size =
          request_builder.shared_memory_sizes()[request.message.handle_index];
      if (!state->shared_memory_block->CreateAnonymous(size)) {
        DVLOG(1) << "Unable to allocate shared memory for blob transfer.";
        CancelBuildingBlob(uuid, BlobStatus::ERR_OUT_OF_MEMORY, context);
        return;
      }
    }
    shared_memory.push_back(state->shared_memory_block->handle());
    byte_requests.push_back(request.message);
    // Since we are only using one handle at a time, transform our handle
    // index correctly back to 0.
    byte_requests.back().handle_index = 0;
  }
  DCHECK(!requests.empty());

  state->request_memory_callback.Run(std::move(byte_requests),
                                     std::move(shared_memory),
                                     std::vector<base::File>());
  return;
}

void BlobTransportHost::OnSharedMemoryResponses(
    const std::string& uuid,
    TransportState* state,
    const std::vector<BlobItemBytesResponse>& responses,
    BlobStorageContext* context) {
  BlobTransportRequestBuilder& request_builder = state->request_builder;
  const auto& requests = request_builder.requests();
  for (const BlobItemBytesResponse& response : responses) {
    const MemoryItemRequest& request = requests[response.request_number];
    if (state->num_shared_memory_requests == 0) {
      DVLOG(1) << "Received too many responses for shared memory.";
      CancelBuildingBlob(uuid, BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS,
                         context);
      return;
    }
    state->num_shared_memory_requests--;
    if (!state->shared_memory_block->memory()) {
      // We just map the whole block, as we'll probably be accessing the
      // whole thing in this group of responses.
      size_t handle_size =
          request_builder
              .shared_memory_sizes()[state->current_shared_memory_handle_index];
      if (!state->shared_memory_block->Map(handle_size)) {
        DVLOG(1) << "Unable to map memory to size " << handle_size;
        CancelBuildingBlob(uuid, BlobStatus::ERR_OUT_OF_MEMORY, context);
        return;
      }
    }

    bool success = state->data_builder.PopulateFutureData(
        request.browser_item_index,
        static_cast<const char*>(state->shared_memory_block->memory()) +
            request.message.handle_offset,
        request.browser_item_offset, request.message.size);

    if (!success) {
      CancelBuildingBlob(uuid, BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS,
                         context);
      return;
    }
    state->num_fulfilled_requests++;
  }
  if (state->num_fulfilled_requests == requests.size()) {
    CompleteTransport(state, context);
    return;
  }
  ContinueSharedMemoryRequests(uuid, state, context);
}

void BlobTransportHost::SendFileRequests(
    TransportState* state,
    BlobStorageContext* context,
    std::vector<BlobMemoryController::FileCreationInfo> file_infos) {
  std::vector<base::File> files;

  for (BlobMemoryController::FileCreationInfo& file_info : file_infos) {
    state->files.push_back(std::move(file_info.file_reference));
    files.push_back(std::move(file_info.file));
  }

  const std::vector<MemoryItemRequest>& requests =
      state->request_builder.requests();
  std::vector<BlobItemBytesRequest> byte_requests;

  DCHECK(!requests.empty());
  for (const MemoryItemRequest& request : requests) {
    byte_requests.push_back(request.message);
  }

  state->request_memory_callback.Run(std::move(byte_requests),
                                     std::vector<base::SharedMemoryHandle>(),
                                     std::move(files));
}

void BlobTransportHost::OnFileResponses(
    const std::string& uuid,
    TransportState* state,
    const std::vector<BlobItemBytesResponse>& responses,
    BlobStorageContext* context) {
  BlobTransportRequestBuilder& request_builder = state->request_builder;
  const auto& requests = request_builder.requests();
  for (const BlobItemBytesResponse& response : responses) {
    const MemoryItemRequest& request = requests[response.request_number];
    const scoped_refptr<ShareableFileReference>& file_ref =
        state->files[request.message.handle_index];
    bool success = state->data_builder.PopulateFutureFile(
        request.browser_item_index, file_ref, response.time_file_modified);
    if (!success) {
      CancelBuildingBlob(uuid, BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS,
                         context);
      return;
    }
    state->num_fulfilled_requests++;
  }
  if (state->num_fulfilled_requests == requests.size())
    CompleteTransport(state, context);
}

void BlobTransportHost::CompleteTransport(TransportState* state,
                                          BlobStorageContext* context) {
  std::string uuid = state->data_builder.uuid();
  BlobStatusCallback complete_callback = state->completion_callback;
  async_blob_map_.erase(uuid);
  context->NotifyTransportComplete(uuid);
  complete_callback.Run(BlobStatus::DONE);
}

}  // namespace storage
