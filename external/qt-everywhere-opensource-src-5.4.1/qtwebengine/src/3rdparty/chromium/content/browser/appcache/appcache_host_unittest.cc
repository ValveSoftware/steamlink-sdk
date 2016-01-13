// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "content/browser/appcache/mock_appcache_policy.h"
#include "content/browser/appcache/mock_appcache_service.h"
#include "net/url_request/url_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/appcache/appcache.h"
#include "webkit/browser/appcache/appcache_backend_impl.h"
#include "webkit/browser/appcache/appcache_group.h"
#include "webkit/browser/appcache/appcache_host.h"
#include "webkit/browser/quota/quota_manager.h"

using appcache::AppCache;
using appcache::AppCacheBackendImpl;
using appcache::AppCacheEntry;
using appcache::AppCacheFrontend;
using appcache::AppCacheGroup;
using appcache::AppCacheHost;
using appcache::kAppCacheNoCacheId;
using appcache::APPCACHE_ERROR_EVENT;
using appcache::APPCACHE_STATUS_OBSOLETE;
using appcache::APPCACHE_OBSOLETE_EVENT;
using appcache::APPCACHE_PROGRESS_EVENT;
using appcache::AppCacheStatus;
using appcache::APPCACHE_STATUS_UNCACHED;

namespace content {

class AppCacheHostTest : public testing::Test {
 public:
  AppCacheHostTest() {
    get_status_callback_ =
        base::Bind(&AppCacheHostTest::GetStatusCallback,
                   base::Unretained(this));
    start_update_callback_ =
        base::Bind(&AppCacheHostTest::StartUpdateCallback,
                   base::Unretained(this));
    swap_cache_callback_ =
        base::Bind(&AppCacheHostTest::SwapCacheCallback,
                   base::Unretained(this));
  }

  class MockFrontend : public AppCacheFrontend {
   public:
    MockFrontend()
        : last_host_id_(-222), last_cache_id_(-222),
          last_status_(appcache::APPCACHE_STATUS_OBSOLETE),
          last_status_changed_(appcache::APPCACHE_STATUS_OBSOLETE),
          last_event_id_(appcache::APPCACHE_OBSOLETE_EVENT),
          content_blocked_(false) {
    }

    virtual void OnCacheSelected(
        int host_id, const appcache::AppCacheInfo& info) OVERRIDE {
      last_host_id_ = host_id;
      last_cache_id_ = info.cache_id;
      last_status_ = info.status;
    }

    virtual void OnStatusChanged(const std::vector<int>& host_ids,
                                 appcache::AppCacheStatus status) OVERRIDE {
      last_status_changed_ = status;
    }

    virtual void OnEventRaised(const std::vector<int>& host_ids,
                               appcache::AppCacheEventID event_id) OVERRIDE {
      last_event_id_ = event_id;
    }

    virtual void OnErrorEventRaised(
        const std::vector<int>& host_ids,
        const appcache::AppCacheErrorDetails& details) OVERRIDE {
      last_event_id_ = APPCACHE_ERROR_EVENT;
    }

    virtual void OnProgressEventRaised(const std::vector<int>& host_ids,
                                       const GURL& url,
                                       int num_total,
                                       int num_complete) OVERRIDE {
      last_event_id_ = APPCACHE_PROGRESS_EVENT;
    }

    virtual void OnLogMessage(int host_id,
                              appcache::AppCacheLogLevel log_level,
                              const std::string& message) OVERRIDE {
    }

    virtual void OnContentBlocked(int host_id,
                                  const GURL& manifest_url) OVERRIDE {
      content_blocked_ = true;
    }

    int last_host_id_;
    int64 last_cache_id_;
    appcache::AppCacheStatus last_status_;
    appcache::AppCacheStatus last_status_changed_;
    appcache::AppCacheEventID last_event_id_;
    bool content_blocked_;
  };

  class MockQuotaManagerProxy : public quota::QuotaManagerProxy {
   public:
    MockQuotaManagerProxy() : QuotaManagerProxy(NULL, NULL) {}

    // Not needed for our tests.
    virtual void RegisterClient(quota::QuotaClient* client) OVERRIDE {}
    virtual void NotifyStorageAccessed(quota::QuotaClient::ID client_id,
                                       const GURL& origin,
                                       quota::StorageType type) OVERRIDE {}
    virtual void NotifyStorageModified(quota::QuotaClient::ID client_id,
                                       const GURL& origin,
                                       quota::StorageType type,
                                       int64 delta) OVERRIDE {}
    virtual void SetUsageCacheEnabled(quota::QuotaClient::ID client_id,
                                      const GURL& origin,
                                      quota::StorageType type,
                                      bool enabled) OVERRIDE {}
    virtual void GetUsageAndQuota(
        base::SequencedTaskRunner* original_task_runner,
        const GURL& origin,
        quota::StorageType type,
        const GetUsageAndQuotaCallback& callback) OVERRIDE {}

    virtual void NotifyOriginInUse(const GURL& origin) OVERRIDE {
      inuse_[origin] += 1;
    }

    virtual void NotifyOriginNoLongerInUse(const GURL& origin) OVERRIDE {
      inuse_[origin] -= 1;
    }

    int GetInUseCount(const GURL& origin) {
      return inuse_[origin];
    }

    void reset() {
      inuse_.clear();
    }

    // Map from origin to count of inuse notifications.
    std::map<GURL, int> inuse_;

   protected:
    virtual ~MockQuotaManagerProxy() {}
  };

  void GetStatusCallback(AppCacheStatus status, void* param) {
    last_status_result_ = status;
    last_callback_param_ = param;
  }

  void StartUpdateCallback(bool result, void* param) {
    last_start_result_ = result;
    last_callback_param_ = param;
  }

  void SwapCacheCallback(bool result, void* param) {
    last_swap_result_ = result;
    last_callback_param_ = param;
  }

  base::MessageLoop message_loop_;

  // Mock classes for the 'host' to work with
  MockAppCacheService service_;
  MockFrontend mock_frontend_;

  // Mock callbacks we expect to receive from the 'host'
  appcache::GetStatusCallback get_status_callback_;
  appcache::StartUpdateCallback start_update_callback_;
  appcache::SwapCacheCallback swap_cache_callback_;

  AppCacheStatus last_status_result_;
  bool last_swap_result_;
  bool last_start_result_;
  void* last_callback_param_;
};

TEST_F(AppCacheHostTest, Basic) {
  // Construct a host and test what state it appears to be in.
  AppCacheHost host(1, &mock_frontend_, &service_);
  EXPECT_EQ(1, host.host_id());
  EXPECT_EQ(&service_, host.service());
  EXPECT_EQ(&mock_frontend_, host.frontend());
  EXPECT_EQ(NULL, host.associated_cache());
  EXPECT_FALSE(host.is_selection_pending());

  // See that the callbacks are delivered immediately
  // and respond as if there is no cache selected.
  last_status_result_ = APPCACHE_STATUS_OBSOLETE;
  host.GetStatusWithCallback(get_status_callback_, reinterpret_cast<void*>(1));
  EXPECT_EQ(APPCACHE_STATUS_UNCACHED, last_status_result_);
  EXPECT_EQ(reinterpret_cast<void*>(1), last_callback_param_);

  last_start_result_ = true;
  host.StartUpdateWithCallback(start_update_callback_,
                               reinterpret_cast<void*>(2));
  EXPECT_FALSE(last_start_result_);
  EXPECT_EQ(reinterpret_cast<void*>(2), last_callback_param_);

  last_swap_result_ = true;
  host.SwapCacheWithCallback(swap_cache_callback_, reinterpret_cast<void*>(3));
  EXPECT_FALSE(last_swap_result_);
  EXPECT_EQ(reinterpret_cast<void*>(3), last_callback_param_);
}

TEST_F(AppCacheHostTest, SelectNoCache) {
  scoped_refptr<MockQuotaManagerProxy> mock_quota_proxy(
      new MockQuotaManagerProxy);
  service_.set_quota_manager_proxy(mock_quota_proxy.get());

  // Reset our mock frontend
  mock_frontend_.last_cache_id_ = -333;
  mock_frontend_.last_host_id_ = -333;
  mock_frontend_.last_status_ = APPCACHE_STATUS_OBSOLETE;

  const GURL kDocAndOriginUrl(GURL("http://whatever/").GetOrigin());
  {
    AppCacheHost host(1, &mock_frontend_, &service_);
    host.SelectCache(kDocAndOriginUrl, kAppCacheNoCacheId, GURL());
    EXPECT_EQ(1, mock_quota_proxy->GetInUseCount(kDocAndOriginUrl));

    // We should have received an OnCacheSelected msg
    EXPECT_EQ(1, mock_frontend_.last_host_id_);
    EXPECT_EQ(kAppCacheNoCacheId, mock_frontend_.last_cache_id_);
    EXPECT_EQ(APPCACHE_STATUS_UNCACHED, mock_frontend_.last_status_);

    // Otherwise, see that it respond as if there is no cache selected.
    EXPECT_EQ(1, host.host_id());
    EXPECT_EQ(&service_, host.service());
    EXPECT_EQ(&mock_frontend_, host.frontend());
    EXPECT_EQ(NULL, host.associated_cache());
    EXPECT_FALSE(host.is_selection_pending());
    EXPECT_TRUE(host.preferred_manifest_url().is_empty());
  }
  EXPECT_EQ(0, mock_quota_proxy->GetInUseCount(kDocAndOriginUrl));
  service_.set_quota_manager_proxy(NULL);
}

TEST_F(AppCacheHostTest, ForeignEntry) {
  // Reset our mock frontend
  mock_frontend_.last_cache_id_ = -333;
  mock_frontend_.last_host_id_ = -333;
  mock_frontend_.last_status_ = APPCACHE_STATUS_OBSOLETE;

  // Precondition, a cache with an entry that is not marked as foreign.
  const int kCacheId = 22;
  const GURL kDocumentURL("http://origin/document");
  scoped_refptr<AppCache> cache = new AppCache(service_.storage(), kCacheId);
  cache->AddEntry(kDocumentURL, AppCacheEntry(AppCacheEntry::EXPLICIT));

  AppCacheHost host(1, &mock_frontend_, &service_);
  host.MarkAsForeignEntry(kDocumentURL, kCacheId);

  // We should have received an OnCacheSelected msg for kAppCacheNoCacheId.
  EXPECT_EQ(1, mock_frontend_.last_host_id_);
  EXPECT_EQ(kAppCacheNoCacheId, mock_frontend_.last_cache_id_);
  EXPECT_EQ(APPCACHE_STATUS_UNCACHED, mock_frontend_.last_status_);

  // See that it respond as if there is no cache selected.
  EXPECT_EQ(1, host.host_id());
  EXPECT_EQ(&service_, host.service());
  EXPECT_EQ(&mock_frontend_, host.frontend());
  EXPECT_EQ(NULL, host.associated_cache());
  EXPECT_FALSE(host.is_selection_pending());

  // See that the entry was marked as foreign.
  EXPECT_TRUE(cache->GetEntry(kDocumentURL)->IsForeign());
}

TEST_F(AppCacheHostTest, ForeignFallbackEntry) {
  // Reset our mock frontend
  mock_frontend_.last_cache_id_ = -333;
  mock_frontend_.last_host_id_ = -333;
  mock_frontend_.last_status_ = APPCACHE_STATUS_OBSOLETE;

  // Precondition, a cache with a fallback entry that is not marked as foreign.
  const int kCacheId = 22;
  const GURL kFallbackURL("http://origin/fallback_resource");
  scoped_refptr<AppCache> cache = new AppCache(service_.storage(), kCacheId);
  cache->AddEntry(kFallbackURL, AppCacheEntry(AppCacheEntry::FALLBACK));

  AppCacheHost host(1, &mock_frontend_, &service_);
  host.NotifyMainResourceIsNamespaceEntry(kFallbackURL);
  host.MarkAsForeignEntry(GURL("http://origin/missing_document"), kCacheId);

  // We should have received an OnCacheSelected msg for kAppCacheNoCacheId.
  EXPECT_EQ(1, mock_frontend_.last_host_id_);
  EXPECT_EQ(kAppCacheNoCacheId, mock_frontend_.last_cache_id_);
  EXPECT_EQ(APPCACHE_STATUS_UNCACHED, mock_frontend_.last_status_);

  // See that the fallback entry was marked as foreign.
  EXPECT_TRUE(cache->GetEntry(kFallbackURL)->IsForeign());
}

TEST_F(AppCacheHostTest, FailedCacheLoad) {
  // Reset our mock frontend
  mock_frontend_.last_cache_id_ = -333;
  mock_frontend_.last_host_id_ = -333;
  mock_frontend_.last_status_ = APPCACHE_STATUS_OBSOLETE;

  AppCacheHost host(1, &mock_frontend_, &service_);
  EXPECT_FALSE(host.is_selection_pending());

  const int kMockCacheId = 333;

  // Put it in a state where we're waiting on a cache
  // load prior to finishing cache selection.
  host.pending_selected_cache_id_ = kMockCacheId;
  EXPECT_TRUE(host.is_selection_pending());

  // The callback should not occur until we finish cache selection.
  last_status_result_ = APPCACHE_STATUS_OBSOLETE;
  last_callback_param_ = reinterpret_cast<void*>(-1);
  host.GetStatusWithCallback(get_status_callback_, reinterpret_cast<void*>(1));
  EXPECT_EQ(APPCACHE_STATUS_OBSOLETE, last_status_result_);
  EXPECT_EQ(reinterpret_cast<void*>(-1), last_callback_param_);

  // Satisfy the load with NULL, a failure.
  host.OnCacheLoaded(NULL, kMockCacheId);

  // Cache selection should have finished
  EXPECT_FALSE(host.is_selection_pending());
  EXPECT_EQ(1, mock_frontend_.last_host_id_);
  EXPECT_EQ(kAppCacheNoCacheId, mock_frontend_.last_cache_id_);
  EXPECT_EQ(APPCACHE_STATUS_UNCACHED, mock_frontend_.last_status_);

  // Callback should have fired upon completing the cache load too.
  EXPECT_EQ(APPCACHE_STATUS_UNCACHED, last_status_result_);
  EXPECT_EQ(reinterpret_cast<void*>(1), last_callback_param_);
}

TEST_F(AppCacheHostTest, FailedGroupLoad) {
  AppCacheHost host(1, &mock_frontend_, &service_);

  const GURL kMockManifestUrl("http://foo.bar/baz");

  // Put it in a state where we're waiting on a cache
  // load prior to finishing cache selection.
  host.pending_selected_manifest_url_ = kMockManifestUrl;
  EXPECT_TRUE(host.is_selection_pending());

  // The callback should not occur until we finish cache selection.
  last_status_result_ = APPCACHE_STATUS_OBSOLETE;
  last_callback_param_ = reinterpret_cast<void*>(-1);
  host.GetStatusWithCallback(get_status_callback_, reinterpret_cast<void*>(1));
  EXPECT_EQ(APPCACHE_STATUS_OBSOLETE, last_status_result_);
  EXPECT_EQ(reinterpret_cast<void*>(-1), last_callback_param_);

  // Satisfy the load will NULL, a failure.
  host.OnGroupLoaded(NULL, kMockManifestUrl);

  // Cache selection should have finished
  EXPECT_FALSE(host.is_selection_pending());
  EXPECT_EQ(1, mock_frontend_.last_host_id_);
  EXPECT_EQ(kAppCacheNoCacheId, mock_frontend_.last_cache_id_);
  EXPECT_EQ(APPCACHE_STATUS_UNCACHED, mock_frontend_.last_status_);

  // Callback should have fired upon completing the group load.
  EXPECT_EQ(APPCACHE_STATUS_UNCACHED, last_status_result_);
  EXPECT_EQ(reinterpret_cast<void*>(1), last_callback_param_);
}

TEST_F(AppCacheHostTest, SetSwappableCache) {
  AppCacheHost host(1, &mock_frontend_, &service_);
  host.SetSwappableCache(NULL);
  EXPECT_FALSE(host.swappable_cache_.get());

  scoped_refptr<AppCacheGroup> group1(new AppCacheGroup(
      service_.storage(), GURL(), service_.storage()->NewGroupId()));
  host.SetSwappableCache(group1.get());
  EXPECT_FALSE(host.swappable_cache_.get());

  AppCache* cache1 = new AppCache(service_.storage(), 111);
  cache1->set_complete(true);
  group1->AddCache(cache1);
  host.SetSwappableCache(group1.get());
  EXPECT_EQ(cache1, host.swappable_cache_.get());

  mock_frontend_.last_host_id_ = -222;  // to verify we received OnCacheSelected

  host.AssociateCompleteCache(cache1);
  EXPECT_FALSE(host.swappable_cache_.get());  // was same as associated cache
  EXPECT_EQ(appcache::APPCACHE_STATUS_IDLE, host.GetStatus());
  // verify OnCacheSelected was called
  EXPECT_EQ(host.host_id(), mock_frontend_.last_host_id_);
  EXPECT_EQ(cache1->cache_id(), mock_frontend_.last_cache_id_);
  EXPECT_EQ(appcache::APPCACHE_STATUS_IDLE, mock_frontend_.last_status_);

  AppCache* cache2 = new AppCache(service_.storage(), 222);
  cache2->set_complete(true);
  group1->AddCache(cache2);
  EXPECT_EQ(cache2, host.swappable_cache_.get());  // updated to newest

  scoped_refptr<AppCacheGroup> group2(
      new AppCacheGroup(service_.storage(), GURL("http://foo.com"),
                        service_.storage()->NewGroupId()));
  AppCache* cache3 = new AppCache(service_.storage(), 333);
  cache3->set_complete(true);
  group2->AddCache(cache3);

  AppCache* cache4 = new AppCache(service_.storage(), 444);
  cache4->set_complete(true);
  group2->AddCache(cache4);
  EXPECT_EQ(cache2, host.swappable_cache_.get());  // unchanged

  host.AssociateCompleteCache(cache3);
  EXPECT_EQ(cache4, host.swappable_cache_.get());  // newest cache in group2
  EXPECT_FALSE(group1->HasCache());  // both caches in group1 have refcount 0

  host.AssociateNoCache(GURL());
  EXPECT_FALSE(host.swappable_cache_.get());
  EXPECT_FALSE(group2->HasCache());  // both caches in group2 have refcount 0

  // Host adds reference to newest cache when an update is complete.
  AppCache* cache5 = new AppCache(service_.storage(), 555);
  cache5->set_complete(true);
  group2->AddCache(cache5);
  host.group_being_updated_ = group2;
  host.OnUpdateComplete(group2.get());
  EXPECT_FALSE(host.group_being_updated_.get());
  EXPECT_EQ(cache5, host.swappable_cache_.get());

  group2->RemoveCache(cache5);
  EXPECT_FALSE(group2->HasCache());
  host.group_being_updated_ = group2;
  host.OnUpdateComplete(group2.get());
  EXPECT_FALSE(host.group_being_updated_.get());
  EXPECT_FALSE(host.swappable_cache_.get());  // group2 had no newest cache
}

TEST_F(AppCacheHostTest, ForDedicatedWorker) {
  const int kMockProcessId = 1;
  const int kParentHostId = 1;
  const int kWorkerHostId = 2;

  AppCacheBackendImpl backend_impl;
  backend_impl.Initialize(&service_, &mock_frontend_, kMockProcessId);
  backend_impl.RegisterHost(kParentHostId);
  backend_impl.RegisterHost(kWorkerHostId);

  AppCacheHost* parent_host = backend_impl.GetHost(kParentHostId);
  EXPECT_FALSE(parent_host->is_for_dedicated_worker());

  AppCacheHost* worker_host = backend_impl.GetHost(kWorkerHostId);
  worker_host->SelectCacheForWorker(kParentHostId, kMockProcessId);
  EXPECT_TRUE(worker_host->is_for_dedicated_worker());
  EXPECT_EQ(parent_host, worker_host->GetParentAppCacheHost());

  // We should have received an OnCacheSelected msg for the worker_host.
  // The host for workers always indicates 'no cache selected' regardless
  // of its parent's state. This is OK because the worker cannot access
  // the scriptable interface, the only function available is resource
  // loading (see appcache_request_handler_unittests those tests).
  EXPECT_EQ(kWorkerHostId, mock_frontend_.last_host_id_);
  EXPECT_EQ(kAppCacheNoCacheId, mock_frontend_.last_cache_id_);
  EXPECT_EQ(APPCACHE_STATUS_UNCACHED, mock_frontend_.last_status_);

  // Simulate the parent being torn down.
  backend_impl.UnregisterHost(kParentHostId);
  parent_host = NULL;
  EXPECT_EQ(NULL, backend_impl.GetHost(kParentHostId));
  EXPECT_EQ(NULL, worker_host->GetParentAppCacheHost());
}

TEST_F(AppCacheHostTest, SelectCacheAllowed) {
  scoped_refptr<MockQuotaManagerProxy> mock_quota_proxy(
      new MockQuotaManagerProxy);
  MockAppCachePolicy mock_appcache_policy;
  mock_appcache_policy.can_create_return_value_ = true;
  service_.set_quota_manager_proxy(mock_quota_proxy.get());
  service_.set_appcache_policy(&mock_appcache_policy);

  // Reset our mock frontend
  mock_frontend_.last_cache_id_ = -333;
  mock_frontend_.last_host_id_ = -333;
  mock_frontend_.last_status_ = APPCACHE_STATUS_OBSOLETE;
  mock_frontend_.last_event_id_ = APPCACHE_OBSOLETE_EVENT;
  mock_frontend_.content_blocked_ = false;

  const GURL kDocAndOriginUrl(GURL("http://whatever/").GetOrigin());
  const GURL kManifestUrl(GURL("http://whatever/cache.manifest"));
  {
    AppCacheHost host(1, &mock_frontend_, &service_);
    host.first_party_url_ = kDocAndOriginUrl;
    host.SelectCache(kDocAndOriginUrl, kAppCacheNoCacheId, kManifestUrl);
    EXPECT_EQ(1, mock_quota_proxy->GetInUseCount(kDocAndOriginUrl));

    // MockAppCacheService::LoadOrCreateGroup is asynchronous, so we shouldn't
    // have received an OnCacheSelected msg yet.
    EXPECT_EQ(-333, mock_frontend_.last_host_id_);
    EXPECT_EQ(-333, mock_frontend_.last_cache_id_);
    EXPECT_EQ(APPCACHE_STATUS_OBSOLETE, mock_frontend_.last_status_);
    // No error events either
    EXPECT_EQ(APPCACHE_OBSOLETE_EVENT, mock_frontend_.last_event_id_);
    EXPECT_FALSE(mock_frontend_.content_blocked_);

    EXPECT_TRUE(host.is_selection_pending());
  }
  EXPECT_EQ(0, mock_quota_proxy->GetInUseCount(kDocAndOriginUrl));
  service_.set_quota_manager_proxy(NULL);
}

TEST_F(AppCacheHostTest, SelectCacheBlocked) {
  scoped_refptr<MockQuotaManagerProxy> mock_quota_proxy(
      new MockQuotaManagerProxy);
  MockAppCachePolicy mock_appcache_policy;
  mock_appcache_policy.can_create_return_value_ = false;
  service_.set_quota_manager_proxy(mock_quota_proxy.get());
  service_.set_appcache_policy(&mock_appcache_policy);

  // Reset our mock frontend
  mock_frontend_.last_cache_id_ = -333;
  mock_frontend_.last_host_id_ = -333;
  mock_frontend_.last_status_ = APPCACHE_STATUS_OBSOLETE;
  mock_frontend_.last_event_id_ = APPCACHE_OBSOLETE_EVENT;
  mock_frontend_.content_blocked_ = false;

  const GURL kDocAndOriginUrl(GURL("http://whatever/").GetOrigin());
  const GURL kManifestUrl(GURL("http://whatever/cache.manifest"));
  {
    AppCacheHost host(1, &mock_frontend_, &service_);
    host.first_party_url_ = kDocAndOriginUrl;
    host.SelectCache(kDocAndOriginUrl, kAppCacheNoCacheId, kManifestUrl);
    EXPECT_EQ(1, mock_quota_proxy->GetInUseCount(kDocAndOriginUrl));

    // We should have received an OnCacheSelected msg
    EXPECT_EQ(1, mock_frontend_.last_host_id_);
    EXPECT_EQ(kAppCacheNoCacheId, mock_frontend_.last_cache_id_);
    EXPECT_EQ(APPCACHE_STATUS_UNCACHED, mock_frontend_.last_status_);

    // Also, an error event was raised
    EXPECT_EQ(APPCACHE_ERROR_EVENT, mock_frontend_.last_event_id_);
    EXPECT_TRUE(mock_frontend_.content_blocked_);

    // Otherwise, see that it respond as if there is no cache selected.
    EXPECT_EQ(1, host.host_id());
    EXPECT_EQ(&service_, host.service());
    EXPECT_EQ(&mock_frontend_, host.frontend());
    EXPECT_EQ(NULL, host.associated_cache());
    EXPECT_FALSE(host.is_selection_pending());
    EXPECT_TRUE(host.preferred_manifest_url().is_empty());
  }
  EXPECT_EQ(0, mock_quota_proxy->GetInUseCount(kDocAndOriginUrl));
  service_.set_quota_manager_proxy(NULL);
}

}  // namespace content
