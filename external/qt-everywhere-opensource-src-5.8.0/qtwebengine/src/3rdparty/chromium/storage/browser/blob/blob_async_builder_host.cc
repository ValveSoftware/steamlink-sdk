// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_async_builder_host.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_storage_context.h"

namespace storage {
namespace {

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
    if (!total_size_checked.IsValid() || !shortcut_size_checked.IsValid()) {
      return false;
    }
  }
  *shortcut_bytes = shortcut_size_checked.ValueOrDie();
  *total_bytes = total_size_checked.ValueOrDie();
  return true;
}

IPCBlobCreationCancelCode ConvertReferencedBlobErrorToConstructingError(
    IPCBlobCreationCancelCode referenced_blob_error) {
  switch (referenced_blob_error) {
    // For most cases we propagate the error.
    case IPCBlobCreationCancelCode::FILE_WRITE_FAILED:
    case IPCBlobCreationCancelCode::SOURCE_DIED_IN_TRANSIT:
    case IPCBlobCreationCancelCode::REFERENCED_BLOB_BROKEN:
    case IPCBlobCreationCancelCode::OUT_OF_MEMORY:
      return referenced_blob_error;
    // Others we report that the referenced blob is broken, as we don't know
    // why (the BLOB_DEREFERENCED_WHILE_BUILDING should never happen, as we hold
    // onto the reference of the blobs we're using).
    case IPCBlobCreationCancelCode::BLOB_DEREFERENCED_WHILE_BUILDING:
      DCHECK(false) << "Referenced blob should never be dereferenced while we "
                    << "are depending on it, as our system holds a handle.";
    case IPCBlobCreationCancelCode::UNKNOWN:
      return IPCBlobCreationCancelCode::REFERENCED_BLOB_BROKEN;
  }
  NOTREACHED();
  return IPCBlobCreationCancelCode::REFERENCED_BLOB_BROKEN;
}

}  // namespace

using MemoryItemRequest =
    BlobAsyncTransportRequestBuilder::RendererMemoryItemRequest;

BlobAsyncBuilderHost::BlobBuildingState::BlobBuildingState(
    const std::string& uuid,
    std::set<std::string> referenced_blob_uuids,
    std::vector<std::unique_ptr<BlobDataHandle>>* referenced_blob_handles)
    : data_builder(uuid),
      referenced_blob_uuids(referenced_blob_uuids),
      referenced_blob_handles(std::move(*referenced_blob_handles)) {}

BlobAsyncBuilderHost::BlobBuildingState::~BlobBuildingState() {}

BlobAsyncBuilderHost::BlobAsyncBuilderHost() : ptr_factory_(this) {}

BlobAsyncBuilderHost::~BlobAsyncBuilderHost() {}

BlobTransportResult BlobAsyncBuilderHost::RegisterBlobUUID(
    const std::string& uuid,
    const std::string& content_type,
    const std::string& content_disposition,
    const std::set<std::string>& referenced_blob_uuids,
    BlobStorageContext* context) {
  if (async_blob_map_.find(uuid) != async_blob_map_.end())
    return BlobTransportResult::BAD_IPC;
  if (referenced_blob_uuids.find(uuid) != referenced_blob_uuids.end())
    return BlobTransportResult::BAD_IPC;
  context->CreatePendingBlob(uuid, content_type, content_disposition);
  std::vector<std::unique_ptr<BlobDataHandle>> handles;
  for (const std::string& referenced_uuid : referenced_blob_uuids) {
    std::unique_ptr<BlobDataHandle> handle =
        context->GetBlobDataFromUUID(referenced_uuid);
    if (!handle || handle->IsBroken()) {
      // We cancel the blob right away, and don't bother storing our state.
      context->CancelPendingBlob(
          uuid, IPCBlobCreationCancelCode::REFERENCED_BLOB_BROKEN);
      return BlobTransportResult::CANCEL_REFERENCED_BLOB_BROKEN;
    }
    handles.emplace_back(std::move(handle));
  }
  async_blob_map_[uuid] = base::WrapUnique(
      new BlobBuildingState(uuid, referenced_blob_uuids, &handles));
  return BlobTransportResult::DONE;
}

BlobTransportResult BlobAsyncBuilderHost::StartBuildingBlob(
    const std::string& uuid,
    const std::vector<DataElement>& elements,
    size_t memory_available,
    BlobStorageContext* context,
    const RequestMemoryCallback& request_memory) {
  DCHECK(context);
  DCHECK(async_blob_map_.find(uuid) != async_blob_map_.end());

  // Step 1: Get the sizes.
  size_t shortcut_memory_size_bytes = 0;
  uint64_t total_memory_size_bytes = 0;
  if (!CalculateBlobMemorySize(elements, &shortcut_memory_size_bytes,
                               &total_memory_size_bytes)) {
    CancelBuildingBlob(uuid, IPCBlobCreationCancelCode::UNKNOWN, context);
    return BlobTransportResult::BAD_IPC;
  }

  // Step 2: Check if we have enough memory to store the blob.
  if (total_memory_size_bytes > memory_available) {
    CancelBuildingBlob(uuid, IPCBlobCreationCancelCode::OUT_OF_MEMORY, context);
    return BlobTransportResult::CANCEL_MEMORY_FULL;
  }

  // From here on, we know we can fit the blob in memory.
  BlobBuildingState* state_ptr = async_blob_map_[uuid].get();
  if (!state_ptr->request_builder.requests().empty()) {
    // Check that we're not a duplicate call.
    return BlobTransportResult::BAD_IPC;
  }
  state_ptr->request_memory_callback = request_memory;

  // Step 3: Check to make sure the referenced blob information we received
  //         earlier is correct:
  std::set<std::string> extracted_blob_uuids;
  for (const DataElement& e : elements) {
    if (e.type() == DataElement::TYPE_BLOB) {
      extracted_blob_uuids.insert(e.blob_uuid());
      // We can't depend on ourselves.
      if (e.blob_uuid() == uuid) {
        CancelBuildingBlob(uuid, IPCBlobCreationCancelCode::UNKNOWN, context);
        return BlobTransportResult::BAD_IPC;
      }
    }
  }
  if (extracted_blob_uuids != state_ptr->referenced_blob_uuids) {
    CancelBuildingBlob(uuid, IPCBlobCreationCancelCode::UNKNOWN, context);
    return BlobTransportResult::BAD_IPC;
  }

  // Step 4: Decide if we're using the shortcut method. This will also catch
  //         the case where we don't have any memory items.
  if (shortcut_memory_size_bytes == total_memory_size_bytes &&
      shortcut_memory_size_bytes <= memory_available) {
    for (const DataElement& e : elements) {
      state_ptr->data_builder.AppendIPCDataElement(e);
    }
    FinishBuildingBlob(state_ptr, context);
    return BlobTransportResult::DONE;
  }

  // From here on, we know the blob's size is less than |memory_available|,
  // so we know we're < max(size_t).
  // Step 5: Decide if we're using shared memory.
  if (total_memory_size_bytes > max_ipc_memory_size_) {
    state_ptr->request_builder.InitializeForSharedMemoryRequests(
        max_shared_memory_size_, total_memory_size_bytes, elements,
        &(state_ptr->data_builder));
  } else {
    // Step 6: We can fit in IPC.
    state_ptr->request_builder.InitializeForIPCRequests(
        max_ipc_memory_size_, total_memory_size_bytes, elements,
        &(state_ptr->data_builder));
  }
  // We initialize our requests received state now that they are populated.
  state_ptr->request_received.resize(
      state_ptr->request_builder.requests().size(), false);
  return ContinueBlobMemoryRequests(uuid, context);
}

BlobTransportResult BlobAsyncBuilderHost::OnMemoryResponses(
    const std::string& uuid,
    const std::vector<BlobItemBytesResponse>& responses,
    BlobStorageContext* context) {
  AsyncBlobMap::const_iterator state_it = async_blob_map_.find(uuid);
  if (state_it == async_blob_map_.end()) {
    DVLOG(1) << "Could not find blob " << uuid;
    return BlobTransportResult::BAD_IPC;
  }
  if (responses.empty()) {
    CancelBuildingBlob(uuid, IPCBlobCreationCancelCode::UNKNOWN, context);
    return BlobTransportResult::BAD_IPC;
  }
  BlobAsyncBuilderHost::BlobBuildingState* state = state_it->second.get();
  BlobAsyncTransportRequestBuilder& request_builder = state->request_builder;
  const auto& requests = request_builder.requests();
  for (const BlobItemBytesResponse& response : responses) {
    if (response.request_number >= requests.size()) {
      // Bad IPC, so we delete our record and ignore.
      DVLOG(1) << "Invalid request number " << response.request_number;
      CancelBuildingBlob(uuid, IPCBlobCreationCancelCode::UNKNOWN, context);
      return BlobTransportResult::BAD_IPC;
    }
    DCHECK_LT(response.request_number, state->request_received.size());
    const MemoryItemRequest& request = requests[response.request_number];
    if (state->request_received[response.request_number]) {
      // Bad IPC, so we delete our record.
      DVLOG(1) << "Already received response for that request.";
      CancelBuildingBlob(uuid, IPCBlobCreationCancelCode::UNKNOWN, context);
      return BlobTransportResult::BAD_IPC;
    }
    state->request_received[response.request_number] = true;
    bool invalid_ipc = false;
    bool memory_error = false;
    switch (request.message.transport_strategy) {
      case IPCBlobItemRequestStrategy::IPC:
        if (response.inline_data.size() < request.message.size) {
          DVLOG(1) << "Invalid data size " << response.inline_data.size()
                   << " vs requested size of " << request.message.size;
          invalid_ipc = true;
          break;
        }
        invalid_ipc = !state->data_builder.PopulateFutureData(
            request.browser_item_index, &response.inline_data[0],
            request.browser_item_offset, request.message.size);
        break;
      case IPCBlobItemRequestStrategy::SHARED_MEMORY:
        if (state->num_shared_memory_requests == 0) {
          DVLOG(1) << "Received too many responses for shared memory.";
          invalid_ipc = true;
          break;
        }
        state->num_shared_memory_requests--;
        if (!state->shared_memory_block->memory()) {
          // We just map the whole block, as we'll probably be accessing the
          // whole thing in this group of responses. Another option is to use
          // MapAt, remove the mapped boolean, and then exclude the
          // handle_offset below.
          size_t handle_size = request_builder.shared_memory_sizes()
                                   [state->current_shared_memory_handle_index];
          if (!state->shared_memory_block->Map(handle_size)) {
            DVLOG(1) << "Unable to map memory to size " << handle_size;
            memory_error = true;
            break;
          }
        }

        invalid_ipc = !state->data_builder.PopulateFutureData(
            request.browser_item_index,
            static_cast<const char*>(state->shared_memory_block->memory()) +
                request.message.handle_offset,
            request.browser_item_offset, request.message.size);
        break;
      case IPCBlobItemRequestStrategy::FILE:
      case IPCBlobItemRequestStrategy::UNKNOWN:
        DVLOG(1) << "Not implemented.";
        invalid_ipc = true;
        break;
    }
    if (invalid_ipc) {
      // Bad IPC, so we delete our record and return false.
      CancelBuildingBlob(uuid, IPCBlobCreationCancelCode::UNKNOWN, context);
      return BlobTransportResult::BAD_IPC;
    }
    if (memory_error) {
      DVLOG(1) << "Shared memory error.";
      CancelBuildingBlob(uuid, IPCBlobCreationCancelCode::OUT_OF_MEMORY,
                         context);
      return BlobTransportResult::CANCEL_MEMORY_FULL;
    }
    state->num_fulfilled_requests++;
  }
  return ContinueBlobMemoryRequests(uuid, context);
}

void BlobAsyncBuilderHost::CancelBuildingBlob(const std::string& uuid,
                                              IPCBlobCreationCancelCode code,
                                              BlobStorageContext* context) {
  DCHECK(context);
  auto state_it = async_blob_map_.find(uuid);
  if (state_it == async_blob_map_.end()) {
    return;
  }
  // We can have the blob dereferenced by the renderer, but have it still being
  // 'built'. In this case, it's destructed in the context, but we still have
  // it in our map. Hence we make sure the context has the entry before
  // calling cancel.
  if (context->registry().HasEntry(uuid))
    context->CancelPendingBlob(uuid, code);
  async_blob_map_.erase(state_it);
}

void BlobAsyncBuilderHost::CancelAll(BlobStorageContext* context) {
  DCHECK(context);
  // If the blob still exists in the context (and is being built), then we know
  // that someone else is expecting our blob, and we need to cancel it to let
  // the dependency know it's gone.
  std::vector<std::unique_ptr<BlobDataHandle>> referenced_pending_blobs;
  for (const auto& uuid_state_pair : async_blob_map_) {
    if (context->IsBeingBuilt(uuid_state_pair.first)) {
      referenced_pending_blobs.emplace_back(
          context->GetBlobDataFromUUID(uuid_state_pair.first));
    }
  }
  // We clear the map before canceling them to prevent any strange reentry into
  // our class (see ReferencedBlobFinished) if any blobs were waiting for others
  // to construct.
  async_blob_map_.clear();
  for (const std::unique_ptr<BlobDataHandle>& handle :
       referenced_pending_blobs) {
    context->CancelPendingBlob(
        handle->uuid(), IPCBlobCreationCancelCode::SOURCE_DIED_IN_TRANSIT);
  }
}

BlobTransportResult BlobAsyncBuilderHost::ContinueBlobMemoryRequests(
    const std::string& uuid,
    BlobStorageContext* context) {
  AsyncBlobMap::const_iterator state_it = async_blob_map_.find(uuid);
  DCHECK(state_it != async_blob_map_.end());
  BlobAsyncBuilderHost::BlobBuildingState* state = state_it->second.get();

  BlobAsyncTransportRequestBuilder& request_builder = state->request_builder;
  const std::vector<MemoryItemRequest>& requests = request_builder.requests();
  size_t num_requests = requests.size();
  if (state->num_fulfilled_requests == num_requests) {
    FinishBuildingBlob(state, context);
    return BlobTransportResult::DONE;
  }
  DCHECK_LT(state->num_fulfilled_requests, num_requests);
  if (state->next_request == num_requests) {
    // We are still waiting on other requests to come back.
    return BlobTransportResult::PENDING_RESPONSES;
  }

  std::unique_ptr<std::vector<BlobItemBytesRequest>> byte_requests(
      new std::vector<BlobItemBytesRequest>());
  std::unique_ptr<std::vector<base::SharedMemoryHandle>> shared_memory(
      new std::vector<base::SharedMemoryHandle>());

  for (; state->next_request < num_requests; ++state->next_request) {
    const MemoryItemRequest& request = requests[state->next_request];

    bool stop_accumulating = false;
    bool using_shared_memory_handle = state->num_shared_memory_requests > 0;
    switch (request.message.transport_strategy) {
      case IPCBlobItemRequestStrategy::IPC:
        byte_requests->push_back(request.message);
        break;
      case IPCBlobItemRequestStrategy::SHARED_MEMORY:
        if (using_shared_memory_handle &&
            state->current_shared_memory_handle_index !=
                request.message.handle_index) {
          // We only want one shared memory per requesting blob.
          stop_accumulating = true;
          break;
        }
        using_shared_memory_handle = true;
        state->current_shared_memory_handle_index =
            request.message.handle_index;
        state->num_shared_memory_requests++;

        if (!state->shared_memory_block) {
          state->shared_memory_block.reset(new base::SharedMemory());
          size_t size =
              request_builder
                  .shared_memory_sizes()[request.message.handle_index];
          if (!state->shared_memory_block->CreateAnonymous(size)) {
            DVLOG(1) << "Unable to allocate shared memory for blob transfer.";
            return BlobTransportResult::CANCEL_MEMORY_FULL;
          }
        }
        shared_memory->push_back(state->shared_memory_block->handle());
        byte_requests->push_back(request.message);
        // Since we are only using one handle at a time, transform our handle
        // index correctly back to 0.
        byte_requests->back().handle_index = 0;
        break;
      case IPCBlobItemRequestStrategy::FILE:
      case IPCBlobItemRequestStrategy::UNKNOWN:
        NOTREACHED() << "Not implemented yet.";
        break;
    }
    if (stop_accumulating) {
      break;
    }
  }
  DCHECK(!requests.empty());

  state->request_memory_callback.Run(
      std::move(byte_requests), std::move(shared_memory),
      base::WrapUnique(new std::vector<base::File>()));
  return BlobTransportResult::PENDING_RESPONSES;
}

void BlobAsyncBuilderHost::ReferencedBlobFinished(
    const std::string& owning_blob_uuid,
    base::WeakPtr<BlobStorageContext> context,
    bool construction_success,
    IPCBlobCreationCancelCode reason) {
  if (!context) {
    return;
  }
  auto state_it = async_blob_map_.find(owning_blob_uuid);
  if (state_it == async_blob_map_.end()) {
    return;
  }
  if (!construction_success) {
    CancelBuildingBlob(owning_blob_uuid,
                       ConvertReferencedBlobErrorToConstructingError(reason),
                       context.get());
    return;
  }
  BlobBuildingState* state = state_it->second.get();
  DCHECK_GT(state->num_referenced_blobs_building, 0u);
  if (--state->num_referenced_blobs_building == 0) {
    context->CompletePendingBlob(state->data_builder);
    async_blob_map_.erase(state->data_builder.uuid());
  }
}

void BlobAsyncBuilderHost::FinishBuildingBlob(BlobBuildingState* state,
                                              BlobStorageContext* context) {
  if (!state->referenced_blob_uuids.empty()) {
    DCHECK_EQ(0u, state->num_referenced_blobs_building);
    state->num_referenced_blobs_building = 0;
    // We assume re-entry is not possible, as RunOnConstructionComplete
    // will schedule a task when the blob is being built. Thus we can't have the
    // case where |num_referenced_blobs_building| reaches 0 in the
    // ReferencedBlobFinished method before we're finished looping.
    for (const std::string& referenced_uuid : state->referenced_blob_uuids) {
      if (context->IsBeingBuilt(referenced_uuid)) {
        state->num_referenced_blobs_building++;
        context->RunOnConstructionComplete(
            referenced_uuid,
            base::Bind(&BlobAsyncBuilderHost::ReferencedBlobFinished,
                       ptr_factory_.GetWeakPtr(), state->data_builder.uuid(),
                       context->AsWeakPtr()));
      }
    }
    if (state->num_referenced_blobs_building > 0) {
      // We wait until referenced blobs are done.
      return;
    }
  }
  context->CompletePendingBlob(state->data_builder);
  async_blob_map_.erase(state->data_builder.uuid());
}

}  // namespace storage
