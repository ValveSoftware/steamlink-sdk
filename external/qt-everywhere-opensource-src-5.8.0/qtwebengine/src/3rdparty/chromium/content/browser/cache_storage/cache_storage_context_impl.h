// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CONTEXT_IMPL_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CONTEXT_IMPL_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/browser/cache_storage_context.h"
#include "content/public/browser/cache_storage_usage_info.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}

namespace net {
class URLRequestContextGetter;
}

namespace storage {
class QuotaManagerProxy;
class SpecialStoragePolicy;
}

namespace content {

class BrowserContext;
class ChromeBlobStorageContext;
class CacheStorageManager;

// One instance of this exists per StoragePartition, and services multiple
// child processes/origins. Most logic is delegated to the owned
// CacheStorageManager instance, which is only accessed on the IO
// thread.
class CONTENT_EXPORT CacheStorageContextImpl
    : NON_EXPORTED_BASE(public CacheStorageContext) {
 public:
  explicit CacheStorageContextImpl(BrowserContext* browser_context);

  // Init and Shutdown are for use on the UI thread when the profile,
  // storagepartition is being setup and torn down.
  void Init(const base::FilePath& user_data_directory,
            scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy);
  void Shutdown();

  // Only callable on the IO thread.
  CacheStorageManager* cache_manager() const;

  bool is_incognito() const { return is_incognito_; }

  // The URLRequestContext doesn't exist until after the StoragePartition is
  // made (which is after this object is made). This function must be called
  // after this object is created but before any CacheStorageCache operations.
  // It must be called on the IO thread. If either parameter is NULL the
  // function immediately returns without forwarding to the
  // CacheStorageManager.
  void SetBlobParametersForCache(
      net::URLRequestContextGetter* request_context_getter,
      ChromeBlobStorageContext* blob_storage_context);

  // CacheStorageContext
  void GetAllOriginsInfo(const GetUsageInfoCallback& callback) override;
  void DeleteForOrigin(const GURL& origin) override;

 protected:
  ~CacheStorageContextImpl() override;

 private:
  void CreateCacheStorageManager(
      const base::FilePath& user_data_directory,
      scoped_refptr<base::SequencedTaskRunner> cache_task_runner,
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy);

  void ShutdownOnIO();

  // Initialized in Init(); true if the user data directory is empty.
  bool is_incognito_ = false;

  // Only accessed on the IO thread.
  std::unique_ptr<CacheStorageManager> cache_manager_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CONTEXT_IMPL_H_
