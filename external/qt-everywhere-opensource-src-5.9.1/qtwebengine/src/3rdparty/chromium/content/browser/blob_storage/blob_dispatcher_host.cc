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
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_entry.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/common/blob_storage/blob_item_bytes_request.h"
#include "storage/common/blob_storage/blob_item_bytes_response.h"
#include "storage/common/data_element.h"
#include "url/gurl.h"

using storage::BlobStorageContext;
using storage::BlobStorageRegistry;
using storage::BlobStatus;
using storage::DataElement;
using storage::FileSystemURL;

namespace content {
namespace {
using storage::BlobStorageContext;
using storage::BlobStorageRegistry;
using storage::BlobStatus;
using storage::DataElement;
using storage::FileSystemURL;

// These are used for UMA stats, don't change.
enum RefcountOperation {
  BDH_DECREMENT = 0,
  BDH_INCREMENT,
  BDH_TRACING_ENUM_LAST
};

} // namespace

BlobDispatcherHost::HostedBlobState::HostedBlobState(
    std::unique_ptr<storage::BlobDataHandle> handle)
    : handle(std::move(handle)) {}
BlobDispatcherHost::HostedBlobState::~HostedBlobState() {}

BlobDispatcherHost::HostedBlobState::HostedBlobState(HostedBlobState&&) =
    default;
BlobDispatcherHost::HostedBlobState& BlobDispatcherHost::HostedBlobState::
operator=(BlobDispatcherHost::HostedBlobState&&) = default;

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
  // Note: The only time a renderer sends a blob status message is to cancel.
  IPC_BEGIN_MESSAGE_MAP(BlobDispatcherHost, message)
    IPC_MESSAGE_HANDLER(BlobStorageMsg_RegisterBlob, OnRegisterBlob)
    IPC_MESSAGE_HANDLER(BlobStorageMsg_MemoryItemResponse, OnMemoryItemResponse)
    IPC_MESSAGE_HANDLER(BlobStorageMsg_SendBlobStatus, OnCancelBuildingBlob)
    IPC_MESSAGE_HANDLER(BlobHostMsg_IncrementRefCount, OnIncrementBlobRefCount)
    IPC_MESSAGE_HANDLER(BlobHostMsg_DecrementRefCount, OnDecrementBlobRefCount)
    IPC_MESSAGE_HANDLER(BlobHostMsg_RegisterPublicURL, OnRegisterPublicBlobURL)
    IPC_MESSAGE_HANDLER(BlobHostMsg_RevokePublicURL, OnRevokePublicBlobURL)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BlobDispatcherHost::OnRegisterBlob(
    const std::string& uuid,
    const std::string& content_type,
    const std::string& content_disposition,
    const std::vector<storage::DataElement>& descriptions) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BlobStorageContext* context = this->context();
  if (uuid.empty() || context->registry().HasEntry(uuid) ||
      transport_host_.IsBeingBuilt(uuid)) {
    bad_message::ReceivedBadMessage(this, bad_message::BDH_UUID_REGISTERED);
    return;
  }

  DCHECK(!base::ContainsKey(blobs_inuse_map_, uuid));

  ChildProcessSecurityPolicyImpl* security_policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  for (const DataElement& item : descriptions) {
    // For each source object that provides the data for the blob, ensure that
    // this process has permission to read it.
    switch (item.type()) {
      case storage::DataElement::TYPE_FILE_FILESYSTEM: {
        FileSystemURL filesystem_url(
            file_system_context_->CrackURL(item.filesystem_url()));
        if (!FileSystemURLIsValid(file_system_context_.get(), filesystem_url) ||
            !security_policy->CanReadFileSystemFile(process_id_,
                                                    filesystem_url)) {
          HostedBlobState hosted_state(
              context->AddBrokenBlob(uuid, content_type, content_disposition,
                                     BlobStatus::ERR_FILE_WRITE_FAILED));
          blobs_inuse_map_.insert(
              std::make_pair(uuid, std::move(hosted_state)));
          SendFinalBlobStatus(uuid, BlobStatus::ERR_FILE_WRITE_FAILED);
          return;
        }
        break;
      }
      case storage::DataElement::TYPE_FILE: {
        if (!security_policy->CanReadFile(process_id_, item.path())) {
          HostedBlobState hosted_state(
              context->AddBrokenBlob(uuid, content_type, content_disposition,
                                     BlobStatus::ERR_FILE_WRITE_FAILED));
          blobs_inuse_map_.insert(
              std::make_pair(uuid, std::move(hosted_state)));
          SendFinalBlobStatus(uuid, BlobStatus::ERR_FILE_WRITE_FAILED);
          return;
        }
        break;
      }
      case storage::DataElement::TYPE_BLOB:
      case storage::DataElement::TYPE_BYTES_DESCRIPTION:
      case storage::DataElement::TYPE_BYTES: {
        // Bytes are already in hand; no need to check read permission.
        // TODO(nick): For TYPE_BLOB, can we actually get here for blobs
        // originally created by other processes? If so, is that cool?
        break;
      }
      case storage::DataElement::TYPE_UNKNOWN:
      case storage::DataElement::TYPE_DISK_CACHE_ENTRY: {
        NOTREACHED();  // Should have been caught by IPC deserialization.
        break;
      }
    }
  }

  HostedBlobState hosted_state(transport_host_.StartBuildingBlob(
      uuid, content_type, content_disposition, descriptions, context,
      base::Bind(&BlobDispatcherHost::SendMemoryRequest, base::Unretained(this),
                 uuid),
      base::Bind(&BlobDispatcherHost::SendFinalBlobStatus,
                 base::Unretained(this), uuid)));
  blobs_inuse_map_.insert(std::make_pair(uuid, std::move(hosted_state)));
}

void BlobDispatcherHost::OnMemoryItemResponse(
    const std::string& uuid,
    const std::vector<storage::BlobItemBytesResponse>& responses) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (uuid.empty()) {
    bad_message::ReceivedBadMessage(this, bad_message::BDH_CONSTRUCTION_FAILED);
    return;
  }
  BlobStorageContext* context = this->context();
  const storage::BlobEntry* entry = context->registry().GetEntry(uuid);
  if (!entry || BlobStatusIsError(entry->status())) {
    // We ignore messages for blobs that don't exist to handle the case where
    // the renderer de-refs a blob that we're still constructing, and there are
    // no references to that blob. We ignore broken as well, in the case where
    // we decided to break a blob after sending the memory request.
    // Note: if a blob is broken, then it can't be in the transport_host.
    // Second, if the last dereference of the blob happened on a different host,
    // then we still haven't gotten rid of the 'building' state in the original
    // host. So we call cancel, and send the message just in case that happens.
    if (transport_host_.IsBeingBuilt(uuid)) {
      transport_host_.CancelBuildingBlob(
          uuid, BlobStatus::ERR_BLOB_DEREFERENCED_WHILE_BUILDING, context);
    }
    return;
  }
  if (!transport_host_.IsBeingBuilt(uuid)) {
    bad_message::ReceivedBadMessage(this, bad_message::BDH_CONSTRUCTION_FAILED);
    return;
  }
  transport_host_.OnMemoryResponses(uuid, responses, context);
}

void BlobDispatcherHost::OnCancelBuildingBlob(const std::string& uuid,
                                              const storage::BlobStatus code) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (uuid.empty()) {
    bad_message::ReceivedBadMessage(this, bad_message::BDH_CONSTRUCTION_FAILED);
    return;
  }
  BlobStorageContext* context = this->context();
  const storage::BlobEntry* entry = context->registry().GetEntry(uuid);
  if (!entry || BlobStatusIsError(entry->status())) {
    // We ignore messages for blobs that don't exist to handle the case where
    // the renderer de-refs a blob that we're still constructing, and there are
    // no references to that blob. We ignore broken as well, in the case where
    // we decided to break a blob and the renderer also decided to cancel.
    // Note: if a blob is broken, then it can't be in the transport_host.
    // Second, if the last dereference of the blob happened on a different host,
    // then we still haven't gotten rid of the 'building' state in the original
    // host. So we call cancel just in case this happens.
    if (transport_host_.IsBeingBuilt(uuid)) {
      transport_host_.CancelBuildingBlob(
          uuid, BlobStatus::ERR_BLOB_DEREFERENCED_WHILE_BUILDING, context);
    }
    return;
  }
  if (!transport_host_.IsBeingBuilt(uuid) ||
      !storage::BlobStatusIsError(code)) {
    bad_message::ReceivedBadMessage(this, bad_message::BDH_CONSTRUCTION_FAILED);
    return;
  }
  VLOG(1) << "Blob construction of " << uuid << " cancelled by renderer. "
          << " Reason: " << static_cast<int>(code) << ".";
  transport_host_.CancelBuildingBlob(uuid, code, context);
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
  auto state_it = blobs_inuse_map_.find(uuid);
  if (state_it != blobs_inuse_map_.end()) {
    state_it->second.refcount += 1;
    return;
  }
  blobs_inuse_map_.insert(std::make_pair(
      uuid, HostedBlobState(context->GetBlobDataFromUUID(uuid))));
}

void BlobDispatcherHost::OnDecrementBlobRefCount(const std::string& uuid) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (uuid.empty()) {
    bad_message::ReceivedBadMessage(
        this, bad_message::BDH_INVALID_REFCOUNT_OPERATION);
    return;
  }
  auto state_it = blobs_inuse_map_.find(uuid);
  if (state_it == blobs_inuse_map_.end()) {
    UMA_HISTOGRAM_ENUMERATION("Storage.Blob.InvalidReference", BDH_DECREMENT,
                              BDH_TRACING_ENUM_LAST);
    return;
  }
  state_it->second.refcount -= 1;
  if (state_it->second.refcount == 0) {
    blobs_inuse_map_.erase(state_it);

    // If we're being built still and we don't have any other references, cancel
    // construction.
    BlobStorageContext* context = this->context();
    if (transport_host_.IsBeingBuilt(uuid) &&
        !context->registry().HasEntry(uuid)) {
      transport_host_.CancelBuildingBlob(
          uuid, BlobStatus::ERR_BLOB_DEREFERENCED_WHILE_BUILDING, context);
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
    std::vector<storage::BlobItemBytesRequest> requests,
    std::vector<base::SharedMemoryHandle> memory_handles,
    std::vector<base::File> files) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::vector<IPC::PlatformFileForTransit> file_handles;
  for (base::File& file : files) {
    file_handles.push_back(IPC::TakePlatformFileForTransit(std::move(file)));
  }
  Send(new BlobStorageMsg_RequestMemoryItem(uuid, requests, memory_handles,
                                            file_handles));
}

void BlobDispatcherHost::SendFinalBlobStatus(const std::string& uuid,
                                             BlobStatus status) {
  DCHECK(!BlobStatusIsPending(status));
  if (storage::BlobStatusIsBadIPC(status)) {
    bad_message::ReceivedBadMessage(this, bad_message::BDH_CONSTRUCTION_FAILED);
  }
  Send(new BlobStorageMsg_SendBlobStatus(uuid, status));
}

bool BlobDispatcherHost::IsInUseInHost(const std::string& uuid) {
  return base::ContainsKey(blobs_inuse_map_, uuid);
}

bool BlobDispatcherHost::IsUrlRegisteredInHost(const GURL& blob_url) {
  return base::ContainsKey(public_blob_urls_, blob_url);
}

void BlobDispatcherHost::ClearHostFromBlobStorageContext() {
  BlobStorageContext* context = this->context();
  for (const auto& url : public_blob_urls_) {
    context->RevokePublicBlobURL(url);
  }
  // Keep the blobs alive for the BlobTransportHost call.
  transport_host_.CancelAll(context);
  blobs_inuse_map_.clear();
}

}  // namespace content
