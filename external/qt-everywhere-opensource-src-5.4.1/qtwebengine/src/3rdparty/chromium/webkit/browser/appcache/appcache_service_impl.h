// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_APPCACHE_APPCACHE_SERVICE_IMPL_H_
#define WEBKIT_BROWSER_APPCACHE_APPCACHE_SERVICE_IMPL_H_

#include <map>
#include <set>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"
#include "webkit/browser/appcache/appcache_service.h"
#include "webkit/browser/quota/quota_manager_proxy.h"
#include "webkit/browser/webkit_storage_browser_export.h"
#include "webkit/common/appcache/appcache_interfaces.h"

namespace net {
class URLRequestContext;
}  // namespace net

namespace base {
class FilePath;
class MessageLoopProxy;
}

namespace content {
FORWARD_DECLARE_TEST(AppCacheServiceImplTest, ScheduleReinitialize);
class AppCacheServiceImplTest;
class AppCacheStorageImplTest;
}

namespace quota {
class SpecialStoragePolicy;
}

namespace appcache {

class AppCacheBackendImpl;
class AppCacheExecutableHandlerFactory;
class AppCacheQuotaClient;
class AppCachePolicy;
class AppCacheStorage;

// Refcounted container to manage the lifetime of the old storage instance
// during Reinitialization.
class WEBKIT_STORAGE_BROWSER_EXPORT AppCacheStorageReference
    : public base::RefCounted<AppCacheStorageReference> {
public:
  AppCacheStorage* storage() const { return storage_.get(); }
private:
  friend class AppCacheServiceImpl;
  friend class base::RefCounted<AppCacheStorageReference>;
  AppCacheStorageReference(scoped_ptr<AppCacheStorage> storage);
  ~AppCacheStorageReference();

  scoped_ptr<AppCacheStorage> storage_;
};

// Class that manages the application cache service. Sends notifications
// to many frontends.  One instance per user-profile. Each instance has
// exclusive access to its cache_directory on disk.
class WEBKIT_STORAGE_BROWSER_EXPORT AppCacheServiceImpl
    : public AppCacheService {
 public:

  class WEBKIT_STORAGE_BROWSER_EXPORT Observer {
   public:
    // An observer method to inform consumers of reinitialzation. Managing
    // the lifetime of the old storage instance is a delicate process.
    // Consumers can keep the old disabled instance alive by hanging on to the
    // ref provided.
    virtual void OnServiceReinitialized(
        AppCacheStorageReference* old_storage_ref) = 0;
    virtual ~Observer() {}
  };

  // If not using quota management, the proxy may be NULL.
  explicit AppCacheServiceImpl(quota::QuotaManagerProxy* quota_manager_proxy);
  virtual ~AppCacheServiceImpl();

  void Initialize(const base::FilePath& cache_directory,
                  base::MessageLoopProxy* db_thread,
                  base::MessageLoopProxy* cache_thread);

  void AddObserver(Observer* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

  // For use in catastrophic failure modes to reboot the appcache system
  // without relaunching the browser.
  void ScheduleReinitialize();

  // AppCacheService implementation:
  virtual void CanHandleMainResourceOffline(
      const GURL& url,
      const GURL& first_party,
      const net::CompletionCallback& callback) OVERRIDE;
  virtual void GetAllAppCacheInfo(
      AppCacheInfoCollection* collection,
      const net::CompletionCallback& callback) OVERRIDE;
  virtual void DeleteAppCacheGroup(
      const GURL& manifest_url,
      const net::CompletionCallback& callback) OVERRIDE;

  // Deletes all appcaches for the origin, 'callback' is invoked upon
  // completion. This method always completes asynchronously.
  // (virtual for unit testing)
  virtual void DeleteAppCachesForOrigin(
      const GURL& origin, const net::CompletionCallback& callback);

  // Checks the integrity of 'response_id' by reading the headers and data.
  // If it cannot be read, the cache group for 'manifest_url' is deleted.
  void CheckAppCacheResponse(const GURL& manifest_url, int64 cache_id,
                             int64 response_id);

  // Context for use during cache updates, should only be accessed
  // on the IO thread. We do NOT add a reference to the request context,
  // it is the callers responsibility to ensure that the pointer
  // remains valid while set.
  net::URLRequestContext* request_context() const { return request_context_; }
  void set_request_context(net::URLRequestContext* context) {
    request_context_ = context;
  }

  // The appcache policy, may be null, in which case access is always allowed.
  // The service does NOT assume ownership of the policy, it is the callers
  // responsibility to ensure that the pointer remains valid while set.
  AppCachePolicy* appcache_policy() const { return appcache_policy_; }
  void set_appcache_policy(AppCachePolicy* policy) {
    appcache_policy_ = policy;
  }

  // The factory may be null, in which case invocations of exe handlers
  // will result in an error response.
  // The service does NOT assume ownership of the factory, it is the callers
  // responsibility to ensure that the pointer remains valid while set.
  AppCacheExecutableHandlerFactory* handler_factory() const {
    return handler_factory_;
  }
  void set_handler_factory(
      AppCacheExecutableHandlerFactory* factory) {
    handler_factory_ = factory;
  }

  quota::SpecialStoragePolicy* special_storage_policy() const {
    return special_storage_policy_.get();
  }
  void set_special_storage_policy(quota::SpecialStoragePolicy* policy);

  quota::QuotaManagerProxy* quota_manager_proxy() const {
    return quota_manager_proxy_.get();
  }

  AppCacheQuotaClient* quota_client() const {
    return quota_client_;
  }

  // Each child process in chrome uses a distinct backend instance.
  // See chrome/browser/AppCacheDispatcherHost.
  void RegisterBackend(AppCacheBackendImpl* backend_impl);
  void UnregisterBackend(AppCacheBackendImpl* backend_impl);
  AppCacheBackendImpl* GetBackend(int id) const {
    BackendMap::const_iterator it = backends_.find(id);
    return (it != backends_.end()) ? it->second : NULL;
  }

  AppCacheStorage* storage() const { return storage_.get(); }

  // Disables the exit-time deletion of session-only data.
  void set_force_keep_session_state() { force_keep_session_state_ = true; }
  bool force_keep_session_state() const { return force_keep_session_state_; }

 protected:
  friend class content::AppCacheServiceImplTest;
  friend class content::AppCacheStorageImplTest;
  FRIEND_TEST_ALL_PREFIXES(content::AppCacheServiceImplTest,
      ScheduleReinitialize);

  class AsyncHelper;
  class CanHandleOfflineHelper;
  class DeleteHelper;
  class DeleteOriginHelper;
  class GetInfoHelper;
  class CheckResponseHelper;

  typedef std::set<AsyncHelper*> PendingAsyncHelpers;
  typedef std::map<int, AppCacheBackendImpl*> BackendMap;

  void Reinitialize();

  base::FilePath cache_directory_;
  scoped_refptr<base::MessageLoopProxy> db_thread_;
  scoped_refptr<base::MessageLoopProxy> cache_thread_;
  AppCachePolicy* appcache_policy_;
  AppCacheQuotaClient* quota_client_;
  AppCacheExecutableHandlerFactory* handler_factory_;
  scoped_ptr<AppCacheStorage> storage_;
  scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy_;
  scoped_refptr<quota::QuotaManagerProxy> quota_manager_proxy_;
  PendingAsyncHelpers pending_helpers_;
  BackendMap backends_;  // One 'backend' per child process.
  // Context for use during cache updates.
  net::URLRequestContext* request_context_;
  // If true, nothing (not even session-only data) should be deleted on exit.
  bool force_keep_session_state_;
  base::Time last_reinit_time_;
  base::TimeDelta next_reinit_delay_;
  base::OneShotTimer<AppCacheServiceImpl> reinit_timer_;
  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(AppCacheServiceImpl);
};

}  // namespace appcache

#endif  // WEBKIT_BROWSER_APPCACHE_APPCACHE_SERVICE_IMPL_H_
