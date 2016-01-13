// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_QUOTA_CLIENT_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_QUOTA_CLIENT_H_

#include <set>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "content/common/content_export.h"
#include "webkit/browser/quota/quota_client.h"
#include "webkit/browser/quota/quota_task.h"
#include "webkit/common/quota/quota_types.h"

namespace content {
class IndexedDBContextImpl;

// A QuotaClient implementation to integrate IndexedDB
// with the quota  management system. This interface is used
// on the IO thread by the quota manager.
class IndexedDBQuotaClient : public quota::QuotaClient,
                             public quota::QuotaTaskObserver {
 public:
  CONTENT_EXPORT explicit IndexedDBQuotaClient(
      IndexedDBContextImpl* indexed_db_context);
  CONTENT_EXPORT virtual ~IndexedDBQuotaClient();

  // QuotaClient method overrides
  virtual ID id() const OVERRIDE;
  virtual void OnQuotaManagerDestroyed() OVERRIDE;
  CONTENT_EXPORT virtual void GetOriginUsage(const GURL& origin_url,
                                             quota::StorageType type,
                                             const GetUsageCallback& callback)
      OVERRIDE;
  CONTENT_EXPORT virtual void GetOriginsForType(
      quota::StorageType type,
      const GetOriginsCallback& callback) OVERRIDE;
  CONTENT_EXPORT virtual void GetOriginsForHost(
      quota::StorageType type,
      const std::string& host,
      const GetOriginsCallback& callback) OVERRIDE;
  CONTENT_EXPORT virtual void DeleteOriginData(const GURL& origin,
                                               quota::StorageType type,
                                               const DeletionCallback& callback)
      OVERRIDE;
  virtual bool DoesSupport(quota::StorageType type) const OVERRIDE;

 private:
  scoped_refptr<IndexedDBContextImpl> indexed_db_context_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBQuotaClient);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_QUOTA_CLIENT_H_
