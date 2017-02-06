// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_QUOTA_CLIENT_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_QUOTA_CLIENT_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "storage/browser/quota/quota_client.h"
#include "storage/common/quota/quota_types.h"

namespace storage {
class QuotaManagerProxy;
}

namespace content {
class CacheStorageManager;

// CacheStorageQuotaClient is owned by the QuotaManager. There is one per
// CacheStorageManager, and therefore one per
// ServiceWorkerContextCore.
class CONTENT_EXPORT CacheStorageQuotaClient : public storage::QuotaClient {
 public:
  explicit CacheStorageQuotaClient(
      base::WeakPtr<CacheStorageManager> cache_manager);
  ~CacheStorageQuotaClient() override;

  // QuotaClient overrides
  ID id() const override;
  void OnQuotaManagerDestroyed() override;
  void GetOriginUsage(const GURL& origin_url,
                      storage::StorageType type,
                      const GetUsageCallback& callback) override;
  void GetOriginsForType(storage::StorageType type,
                         const GetOriginsCallback& callback) override;
  void GetOriginsForHost(storage::StorageType type,
                         const std::string& host,
                         const GetOriginsCallback& callback) override;
  void DeleteOriginData(const GURL& origin,
                        storage::StorageType type,
                        const DeletionCallback& callback) override;
  bool DoesSupport(storage::StorageType type) const override;

 private:
  base::WeakPtr<CacheStorageManager> cache_manager_;

  DISALLOW_COPY_AND_ASSIGN(CacheStorageQuotaClient);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_QUOTA_CLIENT_H_
