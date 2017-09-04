// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_QUOTA_CLIENT_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_QUOTA_CLIENT_H_

#include <set>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "storage/browser/quota/quota_client.h"
#include "storage/browser/quota/quota_task.h"
#include "storage/common/quota/quota_types.h"
#include "url/gurl.h"

namespace content {
class IndexedDBContextImpl;

// A QuotaClient implementation to integrate IndexedDB
// with the quota  management system. This interface is used
// on the IO thread by the quota manager.
class IndexedDBQuotaClient : public storage::QuotaClient {
 public:
  CONTENT_EXPORT explicit IndexedDBQuotaClient(
      IndexedDBContextImpl* indexed_db_context);
  CONTENT_EXPORT ~IndexedDBQuotaClient() override;

  // QuotaClient method overrides
  ID id() const override;
  void OnQuotaManagerDestroyed() override;
  CONTENT_EXPORT void GetOriginUsage(const GURL& origin_url,
                                     storage::StorageType type,
                                     const GetUsageCallback& callback) override;
  CONTENT_EXPORT void GetOriginsForType(
      storage::StorageType type,
      const GetOriginsCallback& callback) override;
  CONTENT_EXPORT void GetOriginsForHost(
      storage::StorageType type,
      const std::string& host,
      const GetOriginsCallback& callback) override;
  CONTENT_EXPORT void DeleteOriginData(
      const GURL& origin,
      storage::StorageType type,
      const DeletionCallback& callback) override;
  bool DoesSupport(storage::StorageType type) const override;

 private:
  scoped_refptr<IndexedDBContextImpl> indexed_db_context_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBQuotaClient);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_QUOTA_CLIENT_H_
