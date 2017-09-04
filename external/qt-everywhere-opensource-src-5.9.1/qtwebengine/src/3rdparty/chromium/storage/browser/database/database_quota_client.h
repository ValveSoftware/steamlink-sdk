// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_DATABASE_DATABASE_QUOTA_CLIENT_H_
#define STORAGE_BROWSER_DATABASE_DATABASE_QUOTA_CLIENT_H_

#include <set>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "storage/browser/quota/quota_client.h"
#include "storage/browser/storage_browser_export.h"
#include "storage/common/quota/quota_types.h"

namespace storage {

class DatabaseTracker;

// A QuotaClient implementation to integrate WebSQLDatabases
// with the quota  management system. This interface is used
// on the IO thread by the quota manager.
class STORAGE_EXPORT DatabaseQuotaClient : public storage::QuotaClient {
 public:
  DatabaseQuotaClient(
      base::SingleThreadTaskRunner* tracker_thread,
      DatabaseTracker* tracker);
  ~DatabaseQuotaClient() override;

  // QuotaClient method overrides
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
  scoped_refptr<base::SingleThreadTaskRunner> db_tracker_thread_;
  scoped_refptr<DatabaseTracker> db_tracker_;  // only used on its thread

  DISALLOW_COPY_AND_ASSIGN(DatabaseQuotaClient);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_DATABASE_DATABASE_QUOTA_CLIENT_H_
