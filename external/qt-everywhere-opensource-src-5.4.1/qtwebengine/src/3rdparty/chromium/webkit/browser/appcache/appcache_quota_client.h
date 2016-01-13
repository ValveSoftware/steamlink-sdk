// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_APPCACHE_APPCACHE_QUOTA_CLIENT_H_
#define WEBKIT_BROWSER_APPCACHE_APPCACHE_QUOTA_CLIENT_H_

#include <deque>
#include <map>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/completion_callback.h"
#include "webkit/browser/appcache/appcache_storage.h"
#include "webkit/browser/quota/quota_client.h"
#include "webkit/browser/quota/quota_task.h"
#include "webkit/browser/webkit_storage_browser_export.h"
#include "webkit/common/quota/quota_types.h"

namespace content {
class AppCacheQuotaClientTest;
}

namespace appcache {

class AppCacheServiceImpl;
class AppCacheStorageImpl;
class AppCacheQuotaClientTest;

// A QuotaClient implementation to integrate the appcache service
// with the quota management system. The QuotaClient interface is
// used on the IO thread by the quota manager. This class deletes
// itself when both the quota manager and the appcache service have
// been destroyed.
class AppCacheQuotaClient : public quota::QuotaClient {
 public:
  typedef std::deque<base::Closure> RequestQueue;

  virtual ~AppCacheQuotaClient();

  // QuotaClient method overrides
  virtual ID id() const OVERRIDE;
  virtual void OnQuotaManagerDestroyed() OVERRIDE;
  virtual void GetOriginUsage(const GURL& origin,
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
  friend class content::AppCacheQuotaClientTest;
  friend class AppCacheServiceImpl;  // for NotifyAppCacheIsDestroyed
  friend class AppCacheStorageImpl;  // for NotifyAppCacheIsReady

  WEBKIT_STORAGE_BROWSER_EXPORT
      explicit AppCacheQuotaClient(AppCacheServiceImpl* service);

  void DidDeleteAppCachesForOrigin(int rv);
  void GetOriginsHelper(quota::StorageType type,
                        const std::string& opt_host,
                        const GetOriginsCallback& callback);
  void ProcessPendingRequests();
  void DeletePendingRequests();
  const AppCacheStorage::UsageMap* GetUsageMap();
  net::CancelableCompletionCallback* GetServiceDeleteCallback();

  // For use by appcache internals during initialization and shutdown.
  WEBKIT_STORAGE_BROWSER_EXPORT void NotifyAppCacheReady();
  WEBKIT_STORAGE_BROWSER_EXPORT void NotifyAppCacheDestroyed();

  // Prior to appcache service being ready, we have to queue
  // up reqeusts and defer acting on them until we're ready.
  RequestQueue pending_batch_requests_;
  RequestQueue pending_serial_requests_;

  // And once it's ready, we can only handle one delete request at a time,
  // so we queue up additional requests while one is in already in progress.
  DeletionCallback current_delete_request_callback_;
  scoped_ptr<net::CancelableCompletionCallback> service_delete_callback_;

  AppCacheServiceImpl* service_;
  bool appcache_is_ready_;
  bool quota_manager_is_destroyed_;

  DISALLOW_COPY_AND_ASSIGN(AppCacheQuotaClient);
};

}  // namespace appcache

#endif  // WEBKIT_BROWSER_APPCACHE_APPCACHE_QUOTA_CLIENT_H_
