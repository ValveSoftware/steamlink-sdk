// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for HTML5 Blob and Stream.
// Multiply-included message file, hence no include guard.

#include <stddef.h>

#include <set>

#include "base/memory/shared_memory.h"
#include "content/common/content_export.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_param_traits.h"
#include "ipc/ipc_platform_file.h"
#include "storage/common/blob_storage/blob_item_bytes_request.h"
#include "storage/common/blob_storage/blob_item_bytes_response.h"
#include "storage/common/blob_storage/blob_storage_constants.h"
#include "storage/common/data_element.h"
#include "url/ipc/url_param_traits.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START BlobMsgStart

// Trait definitions for async blob transport messages.

IPC_ENUM_TRAITS_MAX_VALUE(storage::IPCBlobItemRequestStrategy,
                          storage::IPCBlobItemRequestStrategy::LAST)
IPC_ENUM_TRAITS_MAX_VALUE(storage::IPCBlobCreationCancelCode,
                          storage::IPCBlobCreationCancelCode::LAST)

IPC_STRUCT_TRAITS_BEGIN(storage::BlobItemBytesRequest)
  IPC_STRUCT_TRAITS_MEMBER(request_number)
  IPC_STRUCT_TRAITS_MEMBER(transport_strategy)
  IPC_STRUCT_TRAITS_MEMBER(renderer_item_index)
  IPC_STRUCT_TRAITS_MEMBER(renderer_item_offset)
  IPC_STRUCT_TRAITS_MEMBER(size)
  IPC_STRUCT_TRAITS_MEMBER(handle_index)
  IPC_STRUCT_TRAITS_MEMBER(handle_offset)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(storage::BlobItemBytesResponse)
  IPC_STRUCT_TRAITS_MEMBER(request_number)
  IPC_STRUCT_TRAITS_MEMBER(inline_data)
  IPC_STRUCT_TRAITS_MEMBER(time_file_modified)
IPC_STRUCT_TRAITS_END()

// This message is to tell the browser that we will be building a blob.
IPC_MESSAGE_CONTROL4(BlobStorageMsg_RegisterBlobUUID,
                     std::string /* uuid */,
                     std::string /* content_type */,
                     std::string /* content_disposition */,
                     std::set<std::string> /* referenced_blob_uuids */)

// The DataElements are used to:
// * describe & transport non-memory resources (blobs, files, etc)
// * describe the size of memory items
// * 'shortcut' transport the memory up to the IPC limit so the browser can use
//   it if it's not currently full.
// See https://bit.ly/BlobStorageRefactor
IPC_MESSAGE_CONTROL2(BlobStorageMsg_StartBuildingBlob,
                     std::string /* uuid */,
                     std::vector<storage::DataElement> /* item_descriptions */)

IPC_MESSAGE_CONTROL4(
    BlobStorageMsg_RequestMemoryItem,
    std::string /* uuid */,
    std::vector<storage::BlobItemBytesRequest> /* requests */,
    std::vector<base::SharedMemoryHandle> /* memory_handles */,
    std::vector<IPC::PlatformFileForTransit> /* file_handles */)

IPC_MESSAGE_CONTROL2(
    BlobStorageMsg_MemoryItemResponse,
    std::string /* uuid */,
    std::vector<storage::BlobItemBytesResponse> /* responses */)

IPC_MESSAGE_CONTROL2(BlobStorageMsg_CancelBuildingBlob,
                     std::string /* uuid */,
                     storage::IPCBlobCreationCancelCode /* code */)

IPC_MESSAGE_CONTROL1(BlobStorageMsg_DoneBuildingBlob, std::string /* uuid */)

IPC_MESSAGE_CONTROL1(BlobHostMsg_IncrementRefCount,
                     std::string /* uuid */)
IPC_MESSAGE_CONTROL1(BlobHostMsg_DecrementRefCount,
                     std::string /* uuid */)
IPC_MESSAGE_CONTROL2(BlobHostMsg_RegisterPublicURL,
                     GURL,
                     std::string /* uuid */)
IPC_MESSAGE_CONTROL1(BlobHostMsg_RevokePublicURL,
                     GURL)

// Stream messages sent from the renderer to the browser.

// Registers a stream as being built.
IPC_MESSAGE_CONTROL2(StreamHostMsg_StartBuilding,
                     GURL /* url */,
                     std::string /* content_type */)

// Appends data to a stream being built.
IPC_MESSAGE_CONTROL2(StreamHostMsg_AppendBlobDataItem,
                     GURL /* url */,
                     storage::DataElement)

// Appends data to a stream being built.
IPC_SYNC_MESSAGE_CONTROL3_0(StreamHostMsg_SyncAppendSharedMemory,
                            GURL /* url */,
                            base::SharedMemoryHandle,
                            uint32_t /* buffer size */)

// Flushes contents buffered in the stream.
IPC_MESSAGE_CONTROL1(StreamHostMsg_Flush,
                     GURL /* url */)

// Finishes building a stream.
IPC_MESSAGE_CONTROL1(StreamHostMsg_FinishBuilding,
                     GURL /* url */)

// Aborts building a stream.
IPC_MESSAGE_CONTROL1(StreamHostMsg_AbortBuilding,
                     GURL /* url */)

// Creates a new stream that's a clone of an existing src stream.
IPC_MESSAGE_CONTROL2(StreamHostMsg_Clone,
                     GURL /* url */,
                     GURL /* src_url */)

// Removes a stream.
IPC_MESSAGE_CONTROL1(StreamHostMsg_Remove,
                     GURL /* url */)
