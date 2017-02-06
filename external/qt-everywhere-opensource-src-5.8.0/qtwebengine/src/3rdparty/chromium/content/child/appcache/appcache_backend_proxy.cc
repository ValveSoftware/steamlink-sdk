// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/appcache/appcache_backend_proxy.h"

#include "content/common/appcache_messages.h"

namespace content {

void AppCacheBackendProxy::RegisterHost(int host_id) {
  sender_->Send(new AppCacheHostMsg_RegisterHost(host_id));
}

void AppCacheBackendProxy::UnregisterHost(int host_id) {
  sender_->Send(new AppCacheHostMsg_UnregisterHost(host_id));
}

void AppCacheBackendProxy::SetSpawningHostId(int host_id,
                                             int spawning_host_id) {
  sender_->Send(new AppCacheHostMsg_SetSpawningHostId(
                                    host_id, spawning_host_id));
}

void AppCacheBackendProxy::SelectCache(
    int host_id,
    const GURL& document_url,
    const int64_t cache_document_was_loaded_from,
    const GURL& manifest_url) {
  sender_->Send(new AppCacheHostMsg_SelectCache(
                                    host_id, document_url,
                                    cache_document_was_loaded_from,
                                    manifest_url));
}

void AppCacheBackendProxy::SelectCacheForWorker(
    int host_id, int parent_process_id, int parent_host_id) {
  sender_->Send(new AppCacheHostMsg_SelectCacheForWorker(
                                    host_id, parent_process_id,
                                    parent_host_id));
}

void AppCacheBackendProxy::SelectCacheForSharedWorker(int host_id,
                                                      int64_t appcache_id) {
  sender_->Send(new AppCacheHostMsg_SelectCacheForSharedWorker(
                                    host_id, appcache_id));
}

void AppCacheBackendProxy::MarkAsForeignEntry(
    int host_id,
    const GURL& document_url,
    int64_t cache_document_was_loaded_from) {
  sender_->Send(new AppCacheHostMsg_MarkAsForeignEntry(
                                    host_id, document_url,
                                    cache_document_was_loaded_from));
}

AppCacheStatus AppCacheBackendProxy::GetStatus(int host_id) {
  AppCacheStatus status = APPCACHE_STATUS_UNCACHED;
  sender_->Send(new AppCacheHostMsg_GetStatus(host_id, &status));
  return status;
}

bool AppCacheBackendProxy::StartUpdate(int host_id) {
  bool result = false;
  sender_->Send(new AppCacheHostMsg_StartUpdate(host_id, &result));
  return result;
}

bool AppCacheBackendProxy::SwapCache(int host_id) {
  bool result = false;
  sender_->Send(new AppCacheHostMsg_SwapCache(host_id, &result));
  return result;
}

void AppCacheBackendProxy::GetResourceList(
    int host_id, std::vector<AppCacheResourceInfo>* resource_infos) {
  sender_->Send(new AppCacheHostMsg_GetResourceList(host_id, resource_infos));
}

}  // namespace content
