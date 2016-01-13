// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/appcache/appcache_storage.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/stl_util.h"
#include "webkit/browser/appcache/appcache_response.h"
#include "webkit/browser/appcache/appcache_service_impl.h"
#include "webkit/browser/quota/quota_client.h"
#include "webkit/browser/quota/quota_manager_proxy.h"

namespace appcache {

// static
const int64 AppCacheStorage::kUnitializedId = -1;

AppCacheStorage::AppCacheStorage(AppCacheServiceImpl* service)
    : last_cache_id_(kUnitializedId), last_group_id_(kUnitializedId),
      last_response_id_(kUnitializedId), service_(service)  {
}

AppCacheStorage::~AppCacheStorage() {
  STLDeleteValues(&pending_info_loads_);
  DCHECK(delegate_references_.empty());
}

AppCacheStorage::DelegateReference::DelegateReference(
    Delegate* delegate, AppCacheStorage* storage)
    : delegate(delegate), storage(storage) {
  storage->delegate_references_.insert(
      DelegateReferenceMap::value_type(delegate, this));
}

AppCacheStorage::DelegateReference::~DelegateReference() {
  if (delegate)
    storage->delegate_references_.erase(delegate);
}

AppCacheStorage::ResponseInfoLoadTask::ResponseInfoLoadTask(
    const GURL& manifest_url,
    int64 group_id,
    int64 response_id,
    AppCacheStorage* storage)
    : storage_(storage),
      manifest_url_(manifest_url),
      group_id_(group_id),
      response_id_(response_id),
      info_buffer_(new HttpResponseInfoIOBuffer) {
  storage_->pending_info_loads_.insert(
      PendingResponseInfoLoads::value_type(response_id, this));
}

AppCacheStorage::ResponseInfoLoadTask::~ResponseInfoLoadTask() {
}

void AppCacheStorage::ResponseInfoLoadTask::StartIfNeeded() {
  if (reader_)
    return;
  reader_.reset(
      storage_->CreateResponseReader(manifest_url_, group_id_, response_id_));
  reader_->ReadInfo(info_buffer_.get(),
                    base::Bind(&ResponseInfoLoadTask::OnReadComplete,
                               base::Unretained(this)));
}

void AppCacheStorage::ResponseInfoLoadTask::OnReadComplete(int result) {
  storage_->pending_info_loads_.erase(response_id_);
  scoped_refptr<AppCacheResponseInfo> info;
  if (result >= 0) {
    info = new AppCacheResponseInfo(storage_, manifest_url_,
                                    response_id_,
                                    info_buffer_->http_info.release(),
                                    info_buffer_->response_data_size);
  }
  FOR_EACH_DELEGATE(delegates_, OnResponseInfoLoaded(info.get(), response_id_));
  delete this;
}

void AppCacheStorage::LoadResponseInfo(
    const GURL& manifest_url, int64 group_id, int64 id, Delegate* delegate) {
  AppCacheResponseInfo* info = working_set_.GetResponseInfo(id);
  if (info) {
    delegate->OnResponseInfoLoaded(info, id);
    return;
  }
  ResponseInfoLoadTask* info_load =
      GetOrCreateResponseInfoLoadTask(manifest_url, group_id, id);
  DCHECK(manifest_url == info_load->manifest_url());
  DCHECK(group_id == info_load->group_id());
  DCHECK(id == info_load->response_id());
  info_load->AddDelegate(GetOrCreateDelegateReference(delegate));
  info_load->StartIfNeeded();
}

void AppCacheStorage::UpdateUsageMapAndNotify(
    const GURL& origin, int64 new_usage) {
  DCHECK_GE(new_usage, 0);
  int64 old_usage = usage_map_[origin];
  if (new_usage > 0)
    usage_map_[origin] = new_usage;
  else
    usage_map_.erase(origin);
  if (new_usage != old_usage && service()->quota_manager_proxy()) {
    service()->quota_manager_proxy()->NotifyStorageModified(
        quota::QuotaClient::kAppcache,
        origin, quota::kStorageTypeTemporary,
        new_usage - old_usage);
  }
}

void AppCacheStorage::ClearUsageMapAndNotify() {
  if (service()->quota_manager_proxy()) {
    for (UsageMap::const_iterator iter = usage_map_.begin();
         iter != usage_map_.end(); ++iter) {
      service()->quota_manager_proxy()->NotifyStorageModified(
          quota::QuotaClient::kAppcache,
          iter->first, quota::kStorageTypeTemporary,
          -(iter->second));
    }
  }
  usage_map_.clear();
}

void AppCacheStorage::NotifyStorageAccessed(const GURL& origin) {
  if (service()->quota_manager_proxy() &&
      usage_map_.find(origin) != usage_map_.end())
    service()->quota_manager_proxy()->NotifyStorageAccessed(
        quota::QuotaClient::kAppcache,
        origin, quota::kStorageTypeTemporary);
}

}  // namespace appcache

