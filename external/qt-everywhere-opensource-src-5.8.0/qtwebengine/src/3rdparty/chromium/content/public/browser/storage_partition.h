// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_STORAGE_PARTITION_H_
#define CONTENT_PUBLIC_BROWSER_STORAGE_PARTITION_H_

#include <stdint.h>

#include <string>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "net/cookies/cookie_store.h"

class GURL;

namespace base {
class Time;
}

namespace storage {
class FileSystemContext;
}

namespace net {
class URLRequestContextGetter;
}

namespace storage {
class QuotaManager;
class SpecialStoragePolicy;
}

namespace storage {
class DatabaseTracker;
}

namespace content {

class AppCacheService;
class BrowserContext;
class CacheStorageContext;
class DOMStorageContext;
class HostZoomLevelContext;
class HostZoomMap;
class IndexedDBContext;
class PlatformNotificationContext;
class ServiceWorkerContext;
class ZoomLevelDelegate;

// Defines what persistent state a child process can access.
//
// The StoragePartition defines the view each child process has of the
// persistent state inside the BrowserContext. This is used to implement
// isolated storage where a renderer with isolated storage cannot see
// the cookies, localStorage, etc., that normal web renderers have access to.
class CONTENT_EXPORT StoragePartition {
 public:
  virtual base::FilePath GetPath() = 0;
  virtual net::URLRequestContextGetter* GetURLRequestContext() = 0;
  virtual net::URLRequestContextGetter* GetMediaURLRequestContext() = 0;
  virtual storage::QuotaManager* GetQuotaManager() = 0;
  virtual AppCacheService* GetAppCacheService() = 0;
  virtual storage::FileSystemContext* GetFileSystemContext() = 0;
  virtual storage::DatabaseTracker* GetDatabaseTracker() = 0;
  virtual DOMStorageContext* GetDOMStorageContext() = 0;
  virtual IndexedDBContext* GetIndexedDBContext() = 0;
  virtual ServiceWorkerContext* GetServiceWorkerContext() = 0;
  virtual CacheStorageContext* GetCacheStorageContext() = 0;
  virtual HostZoomMap* GetHostZoomMap() = 0;
  virtual HostZoomLevelContext* GetHostZoomLevelContext() = 0;
  virtual ZoomLevelDelegate* GetZoomLevelDelegate() = 0;
  virtual PlatformNotificationContext* GetPlatformNotificationContext() = 0;

  enum : uint32_t {
    REMOVE_DATA_MASK_APPCACHE = 1 << 0,
    REMOVE_DATA_MASK_COOKIES = 1 << 1,
    REMOVE_DATA_MASK_FILE_SYSTEMS = 1 << 2,
    REMOVE_DATA_MASK_INDEXEDDB = 1 << 3,
    REMOVE_DATA_MASK_LOCAL_STORAGE = 1 << 4,
    REMOVE_DATA_MASK_SHADER_CACHE = 1 << 5,
    REMOVE_DATA_MASK_WEBSQL = 1 << 6,
    REMOVE_DATA_MASK_WEBRTC_IDENTITY = 1 << 7,
    REMOVE_DATA_MASK_SERVICE_WORKERS = 1 << 8,
    REMOVE_DATA_MASK_CACHE_STORAGE = 1 << 9,
    REMOVE_DATA_MASK_PLUGIN_PRIVATE_DATA = 1 << 10,
    REMOVE_DATA_MASK_ALL = 0xFFFFFFFF,

    // Corresponds to storage::kStorageTypeTemporary.
    QUOTA_MANAGED_STORAGE_MASK_TEMPORARY = 1 << 0,
    // Corresponds to storage::kStorageTypePersistent.
    QUOTA_MANAGED_STORAGE_MASK_PERSISTENT = 1 << 1,
    // Corresponds to storage::kStorageTypeSyncable.
    QUOTA_MANAGED_STORAGE_MASK_SYNCABLE = 1 << 2,
    QUOTA_MANAGED_STORAGE_MASK_ALL = 0xFFFFFFFF,
  };

  // Starts an asynchronous task that does a best-effort clear the data
  // corresponding to the given |remove_mask| and |quota_storage_remove_mask|
  // inside this StoragePartition for the given |storage_origin|.
  // Note session dom storage is not cleared even if you specify
  // REMOVE_DATA_MASK_LOCAL_STORAGE.
  // |callback| is called when data deletion is done or at least the deletion is
  // scheduled.
  //
  // TODO(ajwong): Right now, the embedder may have some
  // URLRequestContextGetter objects that the StoragePartition does not know
  // about.  This will no longer be the case when we resolve
  // http://crbug.com/159193. Remove |request_context_getter| when that bug
  // is fixed.
  virtual void ClearDataForOrigin(uint32_t remove_mask,
                                  uint32_t quota_storage_remove_mask,
                                  const GURL& storage_origin,
                                  net::URLRequestContextGetter* rq_context,
                                  const base::Closure& callback) = 0;

  // A callback type to check if a given origin matches a storage policy.
  // Can be passed empty/null where used, which means the origin will always
  // match.
  typedef base::Callback<bool(const GURL&, storage::SpecialStoragePolicy*)>
      OriginMatcherFunction;

  // A callback type to check if a given cookie should be cleared.
  using CookieMatcherFunction = net::CookieStore::CookiePredicate;

  // Similar to ClearDataForOrigin().
  // Deletes all data out for the StoragePartition if |storage_origin| is empty.
  // |origin_matcher| is present if special storage policy is to be handled,
  // otherwise the callback can be null (base::Callback::is_null() == true).
  // |callback| is called when data deletion is done or at least the deletion is
  // scheduled.
  // storage_origin and origin_matcher_function are used for different
  // subsystems. One does not imply the other.
  virtual void ClearData(uint32_t remove_mask,
                         uint32_t quota_storage_remove_mask,
                         const GURL& storage_origin,
                         const OriginMatcherFunction& origin_matcher,
                         const base::Time begin,
                         const base::Time end,
                         const base::Closure& callback) = 0;

  // Similar to ClearData().
  // Deletes all data out for the StoragePartition.
  // * |origin_matcher| is present if special storage policy is to be handled,
  //   otherwise the callback should be null (base::Callback::is_null()==true).
  //   The origin matcher does not apply to cookies, instead use:
  // * |cookies_matcher| is present if special cookie clearing is to be handled.
  //   If the callback is null all cookies withing the time range will be
  //   cleared.
  // * |callback| is called when data deletion is done or at least the deletion
  //   is scheduled.
  // Note: Make sure you know what you are doing before clearing cookies
  // selectively. You don't want to break the web.
  virtual void ClearData(uint32_t remove_mask,
                         uint32_t quota_storage_remove_mask,
                         const OriginMatcherFunction& origin_matcher,
                         const CookieMatcherFunction& cookie_matcher,
                         const base::Time begin,
                         const base::Time end,
                         const base::Closure& callback) = 0;

  // Write any unwritten data to disk.
  // Note: this method does not sync the data - it only ensures that any
  // unwritten data has been written out to the filesystem.
  virtual void Flush() = 0;

 protected:
  virtual ~StoragePartition() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_STORAGE_PARTITION_H_
