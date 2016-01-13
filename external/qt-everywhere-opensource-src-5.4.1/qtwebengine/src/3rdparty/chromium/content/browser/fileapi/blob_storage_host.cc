// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fileapi/blob_storage_host.h"

#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "url/gurl.h"
#include "webkit/browser/blob/blob_data_handle.h"
#include "webkit/browser/blob/blob_storage_context.h"

using webkit_blob::BlobStorageContext;
using webkit_blob::BlobData;

namespace content {

BlobStorageHost::BlobStorageHost(BlobStorageContext* context)
    : context_(context->AsWeakPtr()) {
}

BlobStorageHost::~BlobStorageHost() {
  if (!context_.get())
    return;
  for (std::set<GURL>::iterator iter = public_blob_urls_.begin();
       iter != public_blob_urls_.end(); ++iter) {
    context_->RevokePublicBlobURL(*iter);
  }
  for (BlobReferenceMap::iterator iter = blobs_inuse_map_.begin();
       iter != blobs_inuse_map_.end(); ++iter) {
    for (int i = 0; i < iter->second; ++i)
      context_->DecrementBlobRefCount(iter->first);
  }
}

bool BlobStorageHost::StartBuildingBlob(const std::string& uuid) {
  if (!context_.get() || uuid.empty() || context_->IsInUse(uuid))
    return false;
  context_->StartBuildingBlob(uuid);
  blobs_inuse_map_[uuid] = 1;
  return true;
}

bool BlobStorageHost::AppendBlobDataItem(
    const std::string& uuid, const BlobData::Item& data_item) {
  if (!context_.get() || !IsBeingBuiltInHost(uuid))
    return false;
  context_->AppendBlobDataItem(uuid, data_item);
  return true;
}

bool BlobStorageHost::CancelBuildingBlob(const std::string& uuid) {
  if (!context_.get() || !IsBeingBuiltInHost(uuid))
    return false;
  blobs_inuse_map_.erase(uuid);
  context_->CancelBuildingBlob(uuid);
  return true;
}

bool BlobStorageHost::FinishBuildingBlob(
    const std::string& uuid, const std::string& content_type) {
  if (!context_.get() || !IsBeingBuiltInHost(uuid))
    return false;
  context_->FinishBuildingBlob(uuid, content_type);
  return true;
}

bool BlobStorageHost::IncrementBlobRefCount(const std::string& uuid) {
  if (!context_.get() || !context_->IsInUse(uuid) ||
      context_->IsBeingBuilt(uuid))
    return false;
  context_->IncrementBlobRefCount(uuid);
  blobs_inuse_map_[uuid] += 1;
  return true;
}

bool BlobStorageHost::DecrementBlobRefCount(const std::string& uuid) {
  if (!context_.get() || !IsInUseInHost(uuid))
    return false;
  context_->DecrementBlobRefCount(uuid);
  blobs_inuse_map_[uuid] -= 1;
  if (blobs_inuse_map_[uuid] == 0)
    blobs_inuse_map_.erase(uuid);
  return true;
}

bool BlobStorageHost::RegisterPublicBlobURL(
    const GURL& blob_url, const std::string& uuid) {
  if (!context_.get() || !IsInUseInHost(uuid) ||
      context_->IsUrlRegistered(blob_url))
    return false;
  context_->RegisterPublicBlobURL(blob_url, uuid);
  public_blob_urls_.insert(blob_url);
  return true;
}

bool BlobStorageHost::RevokePublicBlobURL(const GURL& blob_url) {
  if (!context_.get() || !IsUrlRegisteredInHost(blob_url))
    return false;
  context_->RevokePublicBlobURL(blob_url);
  public_blob_urls_.erase(blob_url);
  return true;
}

bool BlobStorageHost::IsInUseInHost(const std::string& uuid) {
  return blobs_inuse_map_.find(uuid) != blobs_inuse_map_.end();
}

bool BlobStorageHost::IsBeingBuiltInHost(const std::string& uuid) {
  return IsInUseInHost(uuid) && context_->IsBeingBuilt(uuid);
}

bool BlobStorageHost::IsUrlRegisteredInHost(const GURL& blob_url) {
  return public_blob_urls_.find(blob_url) != public_blob_urls_.end();
}

}  // namespace content
