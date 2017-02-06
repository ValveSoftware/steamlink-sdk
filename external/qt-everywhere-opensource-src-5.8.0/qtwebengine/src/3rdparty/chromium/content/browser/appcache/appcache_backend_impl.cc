// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_backend_impl.h"

#include "base/stl_util.h"
#include "content/browser/appcache/appcache.h"
#include "content/browser/appcache/appcache_group.h"
#include "content/browser/appcache/appcache_service_impl.h"

namespace content {

AppCacheBackendImpl::AppCacheBackendImpl()
    : service_(NULL),
      frontend_(NULL),
      process_id_(0) {
}

AppCacheBackendImpl::~AppCacheBackendImpl() {
  STLDeleteValues(&hosts_);
  if (service_)
    service_->UnregisterBackend(this);
}

void AppCacheBackendImpl::Initialize(AppCacheServiceImpl* service,
                                     AppCacheFrontend* frontend,
                                     int process_id) {
  DCHECK(!service_ && !frontend_ && frontend && service);
  service_ = service;
  frontend_ = frontend;
  process_id_ = process_id;
  service_->RegisterBackend(this);
}

bool AppCacheBackendImpl::RegisterHost(int id) {
  if (GetHost(id))
    return false;

  hosts_.insert(
      HostMap::value_type(id, new AppCacheHost(id, frontend_, service_)));
  return true;
}

bool AppCacheBackendImpl::UnregisterHost(int id) {
  HostMap::iterator found = hosts_.find(id);
  if (found == hosts_.end())
    return false;

  delete found->second;
  hosts_.erase(found);
  return true;
}

bool AppCacheBackendImpl::SetSpawningHostId(
    int host_id,
    int spawning_host_id) {
  AppCacheHost* host = GetHost(host_id);
  if (!host)
    return false;
  host->SetSpawningHostId(process_id_, spawning_host_id);
  return true;
}

bool AppCacheBackendImpl::SelectCache(
    int host_id,
    const GURL& document_url,
    const int64_t cache_document_was_loaded_from,
    const GURL& manifest_url) {
  AppCacheHost* host = GetHost(host_id);
  if (!host)
    return false;

  return host->SelectCache(document_url, cache_document_was_loaded_from,
                    manifest_url);
}

bool AppCacheBackendImpl::SelectCacheForWorker(
    int host_id, int parent_process_id, int parent_host_id) {
  AppCacheHost* host = GetHost(host_id);
  if (!host)
    return false;

  return host->SelectCacheForWorker(parent_process_id, parent_host_id);
}

bool AppCacheBackendImpl::SelectCacheForSharedWorker(int host_id,
                                                     int64_t appcache_id) {
  AppCacheHost* host = GetHost(host_id);
  if (!host)
    return false;

  return host->SelectCacheForSharedWorker(appcache_id);
}

bool AppCacheBackendImpl::MarkAsForeignEntry(
    int host_id,
    const GURL& document_url,
    int64_t cache_document_was_loaded_from) {
  AppCacheHost* host = GetHost(host_id);
  if (!host)
    return false;

  return host->MarkAsForeignEntry(document_url, cache_document_was_loaded_from);
}

bool AppCacheBackendImpl::GetStatusWithCallback(
    int host_id, const GetStatusCallback& callback, void* callback_param) {
  AppCacheHost* host = GetHost(host_id);
  if (!host)
    return false;

  host->GetStatusWithCallback(callback, callback_param);
  return true;
}

bool AppCacheBackendImpl::StartUpdateWithCallback(
    int host_id, const StartUpdateCallback& callback, void* callback_param) {
  AppCacheHost* host = GetHost(host_id);
  if (!host)
    return false;

  host->StartUpdateWithCallback(callback, callback_param);
  return true;
}

bool AppCacheBackendImpl::SwapCacheWithCallback(
    int host_id, const SwapCacheCallback& callback, void* callback_param) {
  AppCacheHost* host = GetHost(host_id);
  if (!host)
    return false;

  host->SwapCacheWithCallback(callback, callback_param);
  return true;
}

void AppCacheBackendImpl::GetResourceList(
    int host_id, std::vector<AppCacheResourceInfo>* resource_infos) {
  AppCacheHost* host = GetHost(host_id);
  if (!host)
    return;

  host->GetResourceList(resource_infos);
}

std::unique_ptr<AppCacheHost> AppCacheBackendImpl::TransferHostOut(
    int host_id) {
  HostMap::iterator found = hosts_.find(host_id);
  if (found == hosts_.end()) {
    NOTREACHED();
    return std::unique_ptr<AppCacheHost>();
  }

  AppCacheHost* transferree = found->second;

  // Put a new empty host in its place.
  found->second = new AppCacheHost(host_id, frontend_, service_);

  // We give up ownership.
  transferree->PrepareForTransfer();
  return std::unique_ptr<AppCacheHost>(transferree);
}

void AppCacheBackendImpl::TransferHostIn(int new_host_id,
                                         std::unique_ptr<AppCacheHost> host) {
  HostMap::iterator found = hosts_.find(new_host_id);
  if (found == hosts_.end()) {
    NOTREACHED();
    return;
  }

  delete found->second;

  // We take onwership.
  host->CompleteTransfer(new_host_id, frontend_);
  found->second = host.release();
}

}  // namespace content
