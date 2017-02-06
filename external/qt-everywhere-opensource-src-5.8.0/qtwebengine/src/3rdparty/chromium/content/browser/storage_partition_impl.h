// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_STORAGE_PARTITION_IMPL_H_
#define CONTENT_BROWSER_STORAGE_PARTITION_IMPL_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/webmessaging/broadcast_channel_provider.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/background_sync/background_sync_context.h"
#include "content/browser/cache_storage/cache_storage_context_impl.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/host_zoom_level_context.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/media/webrtc/webrtc_identity_store.h"
#include "content/browser/notifications/platform_notification_context_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/content_export.h"
#include "content/common/storage_partition_service.mojom.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "net/cookies/cookie_store.h"
#include "storage/browser/quota/special_storage_policy.h"

namespace content {

class CONTENT_EXPORT  StoragePartitionImpl
    : public StoragePartition,
      public NON_EXPORTED_BASE(mojom::StoragePartitionService) {
 public:
  ~StoragePartitionImpl() override;

  // Quota managed data uses a different bitmask for types than
  // StoragePartition uses. This method generates that mask.
  static int GenerateQuotaClientMask(uint32_t remove_mask);

  // This creates a CookiePredicate that matches all host (NOT domain) cookies
  // that match the host of |url|. This is intended to be used with
  // DeleteAllCreatedBetweenWithPredicateAsync.
  static net::CookieStore::CookiePredicate
  CreatePredicateForHostCookies(const GURL& url);

  void OverrideQuotaManagerForTesting(
      storage::QuotaManager* quota_manager);
  void OverrideSpecialStoragePolicyForTesting(
      storage::SpecialStoragePolicy* special_storage_policy);

  // StoragePartition interface.
  base::FilePath GetPath() override;
  net::URLRequestContextGetter* GetURLRequestContext() override;
  net::URLRequestContextGetter* GetMediaURLRequestContext() override;
  storage::QuotaManager* GetQuotaManager() override;
  ChromeAppCacheService* GetAppCacheService() override;
  storage::FileSystemContext* GetFileSystemContext() override;
  storage::DatabaseTracker* GetDatabaseTracker() override;
  DOMStorageContextWrapper* GetDOMStorageContext() override;
  IndexedDBContextImpl* GetIndexedDBContext() override;
  CacheStorageContextImpl* GetCacheStorageContext() override;
  ServiceWorkerContextWrapper* GetServiceWorkerContext() override;
  HostZoomMap* GetHostZoomMap() override;
  HostZoomLevelContext* GetHostZoomLevelContext() override;
  ZoomLevelDelegate* GetZoomLevelDelegate() override;
  PlatformNotificationContextImpl* GetPlatformNotificationContext() override;

  BackgroundSyncContext* GetBackgroundSyncContext();
  webmessaging::BroadcastChannelProvider* GetBroadcastChannelProvider();

  // mojom::StoragePartitionService interface.
  void OpenLocalStorage(
      const url::Origin& origin,
      mojom::LevelDBObserverPtr observer,
      mojo::InterfaceRequest<mojom::LevelDBWrapper> request) override;

  void ClearDataForOrigin(uint32_t remove_mask,
                          uint32_t quota_storage_remove_mask,
                          const GURL& storage_origin,
                          net::URLRequestContextGetter* request_context_getter,
                          const base::Closure& callback) override;
  void ClearData(uint32_t remove_mask,
                 uint32_t quota_storage_remove_mask,
                 const GURL& storage_origin,
                 const OriginMatcherFunction& origin_matcher,
                 const base::Time begin,
                 const base::Time end,
                 const base::Closure& callback) override;

  void ClearData(uint32_t remove_mask,
                 uint32_t quota_storage_remove_mask,
                 const OriginMatcherFunction& origin_matcher,
                 const CookieMatcherFunction& cookie_matcher,
                 const base::Time begin,
                 const base::Time end,
                 const base::Closure& callback) override;

  void Flush() override;

  WebRTCIdentityStore* GetWebRTCIdentityStore();

  // Can return nullptr while |this| is being destroyed.
  BrowserContext* browser_context() const;

  // Called by each renderer process once.
  void Bind(mojo::InterfaceRequest<mojom::StoragePartitionService> request);

  struct DataDeletionHelper;
  struct QuotaManagedDataDeletionHelper;

 private:
  friend class BackgroundSyncManagerTest;
  friend class BackgroundSyncServiceImplTest;
  friend class StoragePartitionImplMap;
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionShaderClearTest, ClearShaderCache);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedDataForeverBoth);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedDataForeverOnlyTemporary);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedDataForeverOnlyPersistent);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedDataForeverNeither);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedDataForeverSpecificOrigin);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedDataForLastHour);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedDataForLastWeek);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedUnprotectedOrigins);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedProtectedSpecificOrigin);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedProtectedOrigins);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedIgnoreDevTools);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest, RemoveCookieForever);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest, RemoveCookieLastHour);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest, RemoveCookieWithMatcher);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveUnprotectedLocalStorageForever);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveProtectedLocalStorageForever);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveLocalStorageForLastWeek);

  // |relative_partition_path| is the relative path under |profile_path| to the
  // StoragePartition's on-disk-storage.
  //
  // If |in_memory| is true, the |relative_partition_path| is (ab)used as a way
  // of distinguishing different in-memory partitions, but nothing is persisted
  // on to disk.
  static StoragePartitionImpl* Create(
      BrowserContext* context,
      bool in_memory,
      const base::FilePath& relative_partition_path);

  StoragePartitionImpl(
      BrowserContext* browser_context,
      const base::FilePath& partition_path,
      storage::QuotaManager* quota_manager,
      ChromeAppCacheService* appcache_service,
      storage::FileSystemContext* filesystem_context,
      storage::DatabaseTracker* database_tracker,
      DOMStorageContextWrapper* dom_storage_context,
      IndexedDBContextImpl* indexed_db_context,
      CacheStorageContextImpl* cache_storage_context,
      ServiceWorkerContextWrapper* service_worker_context,
      WebRTCIdentityStore* webrtc_identity_store,
      storage::SpecialStoragePolicy* special_storage_policy,
      HostZoomLevelContext* host_zoom_level_context,
      PlatformNotificationContextImpl* platform_notification_context,
      BackgroundSyncContext* background_sync_context,
      scoped_refptr<webmessaging::BroadcastChannelProvider>
          broadcast_channel_provider);

  // We will never have both remove_origin be populated and a cookie_matcher.
  void ClearDataImpl(uint32_t remove_mask,
                     uint32_t quota_storage_remove_mask,
                     const GURL& remove_origin,
                     const OriginMatcherFunction& origin_matcher,
                     const CookieMatcherFunction& cookie_matcher,
                     net::URLRequestContextGetter* rq_context,
                     const base::Time begin,
                     const base::Time end,
                     const base::Closure& callback);

  // Used by StoragePartitionImplMap.
  //
  // TODO(ajwong): These should be taken in the constructor and in Create() but
  // because the URLRequestContextGetter still lives in Profile with a tangled
  // initialization, if we try to retrieve the URLRequestContextGetter()
  // before the default StoragePartition is created, we end up reentering the
  // construction and double-initializing.  For now, we retain the legacy
  // behavior while allowing StoragePartitionImpl to expose these accessors by
  // letting StoragePartitionImplMap call these two private settings at the
  // appropriate time.  These should move back into the constructor once
  // URLRequestContextGetter's lifetime is sorted out. We should also move the
  // PostCreateInitialization() out of StoragePartitionImplMap.
  void SetURLRequestContext(
      net::URLRequestContextGetter* url_request_context);
  void SetMediaURLRequestContext(
      net::URLRequestContextGetter* media_url_request_context);

  base::FilePath partition_path_;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_;
  scoped_refptr<net::URLRequestContextGetter> media_url_request_context_;
  scoped_refptr<storage::QuotaManager> quota_manager_;
  scoped_refptr<ChromeAppCacheService> appcache_service_;
  scoped_refptr<storage::FileSystemContext> filesystem_context_;
  scoped_refptr<storage::DatabaseTracker> database_tracker_;
  scoped_refptr<DOMStorageContextWrapper> dom_storage_context_;
  scoped_refptr<IndexedDBContextImpl> indexed_db_context_;
  scoped_refptr<CacheStorageContextImpl> cache_storage_context_;
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;
  scoped_refptr<WebRTCIdentityStore> webrtc_identity_store_;
  scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy_;
  scoped_refptr<HostZoomLevelContext> host_zoom_level_context_;
  scoped_refptr<PlatformNotificationContextImpl> platform_notification_context_;
  scoped_refptr<BackgroundSyncContext> background_sync_context_;
  scoped_refptr<webmessaging::BroadcastChannelProvider>
      broadcast_channel_provider_;

  mojo::BindingSet<mojom::StoragePartitionService> bindings_;

  // Raw pointer that should always be valid. The BrowserContext owns the
  // StoragePartitionImplMap which then owns StoragePartitionImpl. When the
  // BrowserContext is destroyed, |this| will be destroyed too.
  BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(StoragePartitionImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_STORAGE_PARTITION_IMPL_H_
