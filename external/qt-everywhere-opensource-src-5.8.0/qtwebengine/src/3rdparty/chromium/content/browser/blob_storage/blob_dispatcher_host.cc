// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/blob_storage/blob_dispatcher_host.h"

#include <algorithm>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "content/browser/bad_message.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/fileapi/browser_file_system_helper.h"
#include "content/common/fileapi/webblob_messages.h"
#include "ipc/ipc_platform_file.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/blob_transport_result.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/common/blob_storage/blob_item_bytes_request.h"
#include "storage/common/blob_storage/blob_item_bytes_response.h"
#include "storage/common/data_element.h"
#include "url/gurl.h"

using storage::BlobStorageContext;
using storage::BlobStorageRegistry;
using storage::BlobTransportResult;
using storage::DataElement;
using storage::IPCBlobCreationCancelCode;
using storage::FileSystemURL;

namespace content {
namespace {

// These are used for UMA stats, don't change.
enum RefcountOperation {
  BDH_DECREMENT = 0,
  BDH_INCREMENT,
  BDH_TRACING_ENUM_LAST
};

} // namespace

BlobDispatcherHost::BlobDispatcherHost(
    int process_id,
    scoped_refptr<ChromeBlobStorageContext> blob_storage_context,
    scoped_refptr<storage::FileSystemContext> file_system_context)
    : BrowserMessageFilter(BlobMsgStart),
      process_id_(process_id),
      file_system_context_(std::move(file_system_context)),
      blob_storage_context_(std::move(blob_storage_context)) {}

BlobDispatcherHost::~BlobDispatcherHost() {
  ClearHostFromBlobStorageContext();
}

void BlobDispatcherHost::OnChannelClosing() {
  ClearHostFromBlobStorageContext();
  public_blob_urls_.clear();
  blobs_inuse_map_.clear();
}

bool BlobDispatcherHost::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BlobDispatcherHost, message)
    IPC_MESSAGE_HANDLER(BlobStorageMsg_RegisterBlobUUID, OnRegisterBlobUUID)
    IPC_MESSAGE_HANDLER(BlobStorageMsg_StartBuildingBlob, OnStartBuildingBlob)
    IPC_MESSAGE_HANDLER(BlobStorageMsg_MemoryItemResponse, OnMemoryItemResponse)
    IPC_MESSAGE_HANDLER(BlobStorageMsg_CancelBuildingBlob, OnCancelBuildingBlob)
    IPC_MESSAGE_HANDLER(BlobHostMsg_IncrementRefCount, OnIncrementBlobRefCount)
    IPC_MESSAGE_HANDLER(BlobHostMsg_DecrementRefCount, OnDecrementBlobRefCount)
    IPC_MESSAGE_HANDLER(BlobHostMsg_RegisterPublicURL, OnRegisterPublicBlobURL)
    IPC_MESSAGE_HANDLER(BlobHostMsg_RevokePublicURL, OnRevokePublicBlobURL)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BlobDispatcherHost::OnRegisterBlobUUID(
    const std::string& uuid,
    const std::string& content_type,
    const std::string& content_disposition,
    const std::set<std::string>& referenced_blob_uuids) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BlobStorageContext* context = this->context();
  if (uuid.empty() || context->registry().HasEntry(uuid) ||
      async_builder_.IsBeingBuilt(uuid)) {
    bad_message::ReceivedBadMessage(this, bad_message::BDH_UUID_REGISTERED);
    return;
  }
  blobs_inuse_map_[uuid] = 1;
  BlobTransportResult result = async_builder_.RegisterBlobUUID(
      uuid, content_type, content_disposition, referenced_blob_uuids, context);
  switch (result) {
    case BlobTransportResult::BAD_IPC:
      blobs_inuse_map_.erase(uuid);
      bad_message::ReceivedBadMessage(this,
                                      bad_message::BDH_CONSTRUCTION_FAILED);
      break;
    case BlobTransportResult::CANCEL_REFERENCED_BLOB_BROKEN:
      // The async builder builds the blob as broken, and we just need to send
      // the cancel message back to the renderer.
      Send(new BlobStorageMsg_CancelBuildingBlob(
          uuid, IPCBlobCreationCancelCode::REFERENCED_BLOB_BROKEN));
      break;
    case BlobTransportResult::DONE:
      break;
    case BlobTransportResult::CANCEL_MEMORY_FULL:
    case BlobTransportResult::CANCEL_FILE_ERROR:
    case BlobTransportResult::CANCEL_UNKNOWN:
    case BlobTransportResult::PENDING_RESPONSES:
      NOTREACHED();
      break;
  }
}

void BlobDispatcherHost::OnStartBuildingBlob(
    const std::string& uuid,
    const std::vector<storage::DataElement>& descriptions) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (uuid.empty()) {
    SendIPCResponse(uuid, BlobTransportResult::BAD_IPC);
    return;
  }
  BlobStorageContext* context = this->context();
  const BlobStorageRegistry::Entry* entry = context->registry().GetEntry(uuid);
  if (!entry || entry->state == BlobStorageRegistry::BlobState::BROKEN) {
    // We ignore messages for blobs that don't exist to handle the case where
    // the renderer de-refs a blob that we're still constructing, and there are
    // no references to that blob. We ignore broken as well, in the case where
    // we decided to break a blob after RegisterBlobUUID is called.
    // Second, if the last dereference of the blob happened on a different host,
    // then we still haven't gotten rid of the 'building' state in the original
    // host. So we call cancel, and send the message just in case that happens.
    if (async_builder_.IsBeingBuilt(uuid)) {
      async_builder_.CancelBuildingBlob(
          uuid, IPCBlobCreationCancelCode::BLOB_DEREFERENCED_WHILE_BUILDING,
          context);
      Send(new BlobStorageMsg_CancelBuildingBlob(
          uuid, IPCBlobCreationCancelCode::BLOB_DEREFERENCED_WHILE_BUILDING));
    }
    return;
  }
  if (!async_builder_.IsBeingBuilt(uuid)) {
    SendIPCResponse(uuid, BlobTransportResult::BAD_IPC);
    return;
  }

  ChildProcessSecurityPolicyImpl* security_policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  for (const DataElement& item : descriptions) {
    if (item.type() == storage::DataElement::TYPE_FILE_FILESYSTEM) {
      FileSystemURL filesystem_url(
          file_system_context_->CrackURL(item.filesystem_url()));
      if (!FileSystemURLIsValid(file_system_context_.get(), filesystem_url) ||
          !security_policy->CanReadFileSystemFile(process_id_,
                                                  filesystem_url)) {
        async_builder_.CancelBuildingBlob(
            uuid, IPCBlobCreationCancelCode::FILE_WRITE_FAILED, context);
        Send(new BlobStorageMsg_CancelBuildingBlob(
            uuid, IPCBlobCreationCancelCode::FILE_WRITE_FAILED));
        return;
      }
    }
    if (item.type() == storage::DataElement::TYPE_FILE &&
        !security_policy->CanReadFile(process_id_, item.path())) {
      async_builder_.CancelBuildingBlob(
          uuid, IPCBlobCreationCancelCode::FILE_WRITE_FAILED, context);
      Send(new BlobStorageMsg_CancelBuildingBlob(
          uuid, IPCBlobCreationCancelCode::FILE_WRITE_FAILED));
      return;
    }
  }

  // |this| owns async_builder_ so using base::Unretained(this) is safe.
  BlobTransportResult result = async_builder_.StartBuildingBlob(
      uuid, descriptions, context->memory_available(), context,
      base::Bind(&BlobDispatcherHost::SendMemoryRequest, base::Unretained(this),
                 uuid));
  SendIPCResponse(uuid, result);
}

void BlobDispatcherHost::OnMemoryItemResponse(
    const std::string& uuid,
    const std::vector<storage::BlobItemBytesResponse>& responses) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (uuid.empty()) {
    SendIPCResponse(uuid, BlobTransportResult::BAD_IPC);
    return;
  }
  BlobStorageContext* context = this->context();
  const BlobStorageRegistry::Entry* entry = context->registry().GetEntry(uuid);
  if (!entry || entry->state == BlobStorageRegistry::BlobState::BROKEN) {
    // We ignore messages for blobs that don't exist to handle the case where
    // the renderer de-refs a blob that we're still constructing, and there are
    // no references to that blob. We ignore broken as well, in the case where
    // we decided to break a blob after sending the memory request.
    // Note: if a blob is broken, then it can't be in the async_builder.
    // Second, if the last dereference of the blob happened on a different host,
    // then we still haven't gotten rid of the 'building' state in the original
    // host. So we call cancel, and send the message just in case that happens.
    if (async_builder_.IsBeingBuilt(uuid)) {
      async_builder_.CancelBuildingBlob(
          uuid, IPCBlobCreationCancelCode::BLOB_DEREFERENCED_WHILE_BUILDING,
          context);
      Send(new BlobStorageMsg_CancelBuildingBlob(
          uuid, IPCBlobCreationCancelCode::BLOB_DEREFERENCED_WHILE_BUILDING));
    }
    return;
  }
  if (!async_builder_.IsBeingBuilt(uuid)) {
    SendIPCResponse(uuid, BlobTransportResult::BAD_IPC);
    return;
  }
  BlobTransportResult result =
      async_builder_.OnMemoryResponses(uuid, responses, context);
  SendIPCResponse(uuid, result);
}

void BlobDispatcherHost::OnCancelBuildingBlob(
    const std::string& uuid,
    const storage::IPCBlobCreationCancelCode code) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (uuid.empty()) {
    SendIPCResponse(uuid, BlobTransportResult::BAD_IPC);
    return;
  }
  BlobStorageContext* context = this->context();
  const BlobStorageRegistry::Entry* entry = context->registry().GetEntry(uuid);
  if (!entry || entry->state == BlobStorageRegistry::BlobState::BROKEN) {
    // We ignore messages for blobs that don't exist to handle the case where
    // the renderer de-refs a blob that we're still constructing, and there are
    // no references to that blob. We ignore broken as well, in the case where
    // we decided to break a blob and the renderer also decided to cancel.
    // Note: if a blob is broken, then it can't be in the async_builder.
    // Second, if the last dereference of the blob happened on a different host,
    // then we still haven't gotten rid of the 'building' state in the original
    // host. So we call cancel just in case this happens.
    if (async_builder_.IsBeingBuilt(uuid)) {
      async_builder_.CancelBuildingBlob(
          uuid, IPCBlobCreationCancelCode::BLOB_DEREFERENCED_WHILE_BUILDING,
          context);
    }
    return;
  }
  if (!async_builder_.IsBeingBuilt(uuid)) {
    SendIPCResponse(uuid, BlobTransportResult::BAD_IPC);
    return;
  }
  VLOG(1) << "Blob construction of " << uuid << " cancelled by renderer. "
          << " Reason: " << static_cast<int>(code) << ".";
  async_builder_.CancelBuildingBlob(uuid, code, context);
}

void BlobDispatcherHost::OnIncrementBlobRefCount(const std::string& uuid) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BlobStorageContext* context = this->context();
  if (uuid.empty()) {
    bad_message::ReceivedBadMessage(
        this, bad_message::BDH_INVALID_REFCOUNT_OPERATION);
    return;
  }
  if (!context->registry().HasEntry(uuid)) {
    UMA_HISTOGRAM_ENUMERATION("Storage.Blob.InvalidReference", BDH_INCREMENT,
                              BDH_TRACING_ENUM_LAST);
    return;
  }
  context->IncrementBlobRefCount(uuid);
  blobs_inuse_map_[uuid] += 1;
}

void BlobDispatcherHost::OnDecrementBlobRefCount(const std::string& uuid) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (uuid.empty()) {
    bad_message::ReceivedBadMessage(
        this, bad_message::BDH_INVALID_REFCOUNT_OPERATION);
    return;
  }
  if (!IsInUseInHost(uuid)) {
    UMA_HISTOGRAM_ENUMERATION("Storage.Blob.InvalidReference", BDH_DECREMENT,
                              BDH_TRACING_ENUM_LAST);
    return;
  }
  BlobStorageContext* context = this->context();
  context->DecrementBlobRefCount(uuid);
  blobs_inuse_map_[uuid] -= 1;
  if (blobs_inuse_map_[uuid] == 0) {
    blobs_inuse_map_.erase(uuid);
    // If the blob has been deleted in the context and we're still building it,
    // this means we have no references waiting to read it. Clear the building
    // state and send a cancel message to the renderer.
    if (async_builder_.IsBeingBuilt(uuid) &&
        !context->registry().HasEntry(uuid)) {
      async_builder_.CancelBuildingBlob(
          uuid, IPCBlobCreationCancelCode::BLOB_DEREFERENCED_WHILE_BUILDING,
          context);
      Send(new BlobStorageMsg_CancelBuildingBlob(
          uuid, IPCBlobCreationCancelCode::BLOB_DEREFERENCED_WHILE_BUILDING));
    }
  }
}

void BlobDispatcherHost::OnRegisterPublicBlobURL(const GURL& public_url,
                                                 const std::string& uuid) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ChildProcessSecurityPolicyImpl* security_policy =
      ChildProcessSecurityPolicyImpl::GetInstance();

  // Blob urls have embedded origins. A frame should only be creating blob URLs
  // in the origin of its current document. Make sure that the origin advertised
  // on the URL is allowed to be rendered in this process.
  if (!public_url.SchemeIsBlob() ||
      !security_policy->CanCommitURL(process_id_, public_url)) {
    bad_message::ReceivedBadMessage(this, bad_message::BDH_DISALLOWED_ORIGIN);
    return;
  }
  if (uuid.empty()) {
    bad_message::ReceivedBadMessage(this,
                                    bad_message::BDH_INVALID_URL_OPERATION);
    return;
  }
  BlobStorageContext* context = this->context();
  if (!IsInUseInHost(uuid) || context->registry().IsURLMapped(public_url)) {
    UMA_HISTOGRAM_ENUMERATION("Storage.Blob.InvalidURLRegister", BDH_INCREMENT,
                              BDH_TRACING_ENUM_LAST);
    return;
  }
  context->RegisterPublicBlobURL(public_url, uuid);
  public_blob_urls_.insert(public_url);
}

void BlobDispatcherHost::OnRevokePublicBlobURL(const GURL& public_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!public_url.is_valid()) {
    bad_message::ReceivedBadMessage(this,
                                    bad_message::BDH_INVALID_URL_OPERATION);
    return;
  }
  if (!IsUrlRegisteredInHost(public_url)) {
    UMA_HISTOGRAM_ENUMERATION("Storage.Blob.InvalidURLRegister", BDH_DECREMENT,
                              BDH_TRACING_ENUM_LAST);
    return;
  }
  context()->RevokePublicBlobURL(public_url);
  public_blob_urls_.erase(public_url);
}

storage::BlobStorageContext* BlobDispatcherHost::context() {
  return blob_storage_context_->context();
}

void BlobDispatcherHost::SendMemoryRequest(
    const std::string& uuid,
    std::unique_ptr<std::vector<storage::BlobItemBytesRequest>> requests,
    std::unique_ptr<std::vector<base::SharedMemoryHandle>> memory_handles,
    std::unique_ptr<std::vector<base::File>> files) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::vector<IPC::PlatformFileForTransit> file_handles;
  // TODO(dmurph): Support file-backed blob transportation.
  DCHECK(files->empty());
  Send(new BlobStorageMsg_RequestMemoryItem(uuid, *requests, *memory_handles,
                                            file_handles));
}

void BlobDispatcherHost::SendIPCResponse(const std::string& uuid,
                                         storage::BlobTransportResult result) {
  switch (result) {
    case BlobTransportResult::BAD_IPC:
      bad_message::ReceivedBadMessage(this,
                                      bad_message::BDH_CONSTRUCTION_FAILED);
      return;
    case BlobTransportResult::CANCEL_MEMORY_FULL:
      Send(new BlobStorageMsg_CancelBuildingBlob(
          uuid, IPCBlobCreationCancelCode::OUT_OF_MEMORY));
      return;
    case BlobTransportResult::CANCEL_FILE_ERROR:
      Send(new BlobStorageMsg_CancelBuildingBlob(
          uuid, IPCBlobCreationCancelCode::FILE_WRITE_FAILED));
      return;
    case BlobTransportResult::CANCEL_REFERENCED_BLOB_BROKEN:
      Send(new BlobStorageMsg_CancelBuildingBlob(
          uuid, IPCBlobCreationCancelCode::REFERENCED_BLOB_BROKEN));
      return;
    case BlobTransportResult::CANCEL_UNKNOWN:
      Send(new BlobStorageMsg_CancelBuildingBlob(
          uuid, IPCBlobCreationCancelCode::UNKNOWN));
      return;
    case BlobTransportResult::PENDING_RESPONSES:
      return;
    case BlobTransportResult::DONE:
      Send(new BlobStorageMsg_DoneBuildingBlob(uuid));
      return;
  }
  NOTREACHED();
}

bool BlobDispatcherHost::IsInUseInHost(const std::string& uuid) {
  return blobs_inuse_map_.find(uuid) != blobs_inuse_map_.end();
}

bool BlobDispatcherHost::IsUrlRegisteredInHost(const GURL& blob_url) {
  return public_blob_urls_.find(blob_url) != public_blob_urls_.end();
}

void BlobDispatcherHost::ClearHostFromBlobStorageContext() {
  BlobStorageContext* context = this->context();
  for (const auto& url : public_blob_urls_) {
    context->RevokePublicBlobURL(url);
  }
  for (const auto& uuid_refnum_pair : blobs_inuse_map_) {
    for (int i = 0; i < uuid_refnum_pair.second; ++i)
      context->DecrementBlobRefCount(uuid_refnum_pair.first);
  }
  async_builder_.CancelAll(context);
}

}  // namespace content
