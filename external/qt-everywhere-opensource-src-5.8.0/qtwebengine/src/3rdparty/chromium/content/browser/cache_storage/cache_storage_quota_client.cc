// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_quota_client.h"

#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/public/browser/browser_thread.h"

namespace content {

CacheStorageQuotaClient::CacheStorageQuotaClient(
    base::WeakPtr<CacheStorageManager> cache_manager)
    : cache_manager_(cache_manager) {
}

CacheStorageQuotaClient::~CacheStorageQuotaClient() {
}

storage::QuotaClient::ID CacheStorageQuotaClient::id() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return kServiceWorkerCache;
}

void CacheStorageQuotaClient::OnQuotaManagerDestroyed() {
  delete this;
}

void CacheStorageQuotaClient::GetOriginUsage(const GURL& origin_url,
                                             storage::StorageType type,
                                             const GetUsageCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!cache_manager_ || !DoesSupport(type)) {
    callback.Run(0);
    return;
  }

  cache_manager_->GetOriginUsage(origin_url, callback);
}

void CacheStorageQuotaClient::GetOriginsForType(
    storage::StorageType type,
    const GetOriginsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!cache_manager_ || !DoesSupport(type)) {
    callback.Run(std::set<GURL>());
    return;
  }

  cache_manager_->GetOrigins(callback);
}

void CacheStorageQuotaClient::GetOriginsForHost(
    storage::StorageType type,
    const std::string& host,
    const GetOriginsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!cache_manager_ || !DoesSupport(type)) {
    callback.Run(std::set<GURL>());
    return;
  }

  cache_manager_->GetOriginsForHost(host, callback);
}

void CacheStorageQuotaClient::DeleteOriginData(
    const GURL& origin,
    storage::StorageType type,
    const DeletionCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!cache_manager_) {
    callback.Run(storage::kQuotaErrorAbort);
    return;
  }

  if (!DoesSupport(type)) {
    callback.Run(storage::kQuotaStatusOk);
    return;
  }

  cache_manager_->DeleteOriginData(origin, callback);
}

bool CacheStorageQuotaClient::DoesSupport(storage::StorageType type) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  return type == storage::kStorageTypeTemporary;
}

}  // namespace content
