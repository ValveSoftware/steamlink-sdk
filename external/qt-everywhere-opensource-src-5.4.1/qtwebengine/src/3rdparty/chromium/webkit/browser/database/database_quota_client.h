// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_DATABASE_DATABASE_QUOTA_CLIENT_H_
#define WEBKIT_BROWSER_DATABASE_DATABASE_QUOTA_CLIENT_H_

#include <set>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop_proxy.h"
#include "webkit/browser/quota/quota_client.h"
#include "webkit/browser/webkit_storage_browser_export.h"
#include "webkit/common/quota/quota_types.h"

namespace webkit_database {

class DatabaseTracker;

// A QuotaClient implementation to integrate WebSQLDatabases
// with the quota  management system. This interface is used
// on the IO thread by the quota manager.
class WEBKIT_STORAGE_BROWSER_EXPORT_PRIVATE DatabaseQuotaClient
    : public quota::QuotaClient {
 public:
  DatabaseQuotaClient(
      base::MessageLoopProxy* tracker_thread,
      DatabaseTracker* tracker);
  virtual ~DatabaseQuotaClient();

  // QuotaClient method overrides
  virtual ID id() const OVERRIDE;
  virtual void OnQuotaManagerDestroyed() OVERRIDE;
  virtual void GetOriginUsage(const GURL& origin_url,
                              quota::StorageType type,
                              const GetUsageCallback& callback) OVERRIDE;
  virtual void GetOriginsForType(quota::StorageType type,
                                 const GetOriginsCallback& callback) OVERRIDE;
  virtual void GetOriginsForHost(quota::StorageType type,
                                 const std::string& host,
                                 const GetOriginsCallback& callback) OVERRIDE;
  virtual void DeleteOriginData(const GURL& origin,
                                quota::StorageType type,
                                const DeletionCallback& callback) OVERRIDE;
  virtual bool DoesSupport(quota::StorageType type) const OVERRIDE;
 private:
  scoped_refptr<base::MessageLoopProxy> db_tracker_thread_;
  scoped_refptr<DatabaseTracker> db_tracker_;  // only used on its thread

  DISALLOW_COPY_AND_ASSIGN(DatabaseQuotaClient);
};

}  // namespace webkit_database

#endif  // WEBKIT_BROWSER_DATABASE_DATABASE_QUOTA_CLIENT_H_
