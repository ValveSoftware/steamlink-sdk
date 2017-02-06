// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_H_

#include <stdint.h>

#include <map>
#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/cache_storage/cache_storage_cache.h"

namespace base {
class SequencedTaskRunner;
}

namespace net {
class URLRequestContextGetter;
}

namespace storage {
class BlobStorageContext;
}

namespace content {
class CacheStorageScheduler;
class CacheStorageCacheHandle;

// TODO(jkarlin): Constrain the total bytes used per origin.

// CacheStorage holds the set of caches for a given origin. It is
// owned by the CacheStorageManager. This class expects to be run
// on the IO thread. The asynchronous methods are executed serially.
class CONTENT_EXPORT CacheStorage {
 public:
  typedef std::vector<std::string> StringVector;
  typedef base::Callback<void(bool, CacheStorageError)> BoolAndErrorCallback;
  typedef base::Callback<void(std::unique_ptr<CacheStorageCacheHandle>,
                              CacheStorageError)>
      CacheAndErrorCallback;
  typedef base::Callback<void(const StringVector&, CacheStorageError)>
      StringsAndErrorCallback;
  using SizeCallback = base::Callback<void(int64_t)>;

  static const char kIndexFileName[];

  CacheStorage(
      const base::FilePath& origin_path,
      bool memory_only,
      base::SequencedTaskRunner* cache_task_runner,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter,
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
      base::WeakPtr<storage::BlobStorageContext> blob_context,
      const GURL& origin);

  // Any unfinished asynchronous operations may not complete or call their
  // callbacks.
  virtual ~CacheStorage();

  // Get the cache for the given key. If the cache is not found it is
  // created. The CacheStorgeCacheHandle in the callback prolongs the lifetime
  // of the cache. Once all handles to a cache are deleted the cache is deleted.
  // The cache will also be deleted in the CacheStorage's destructor so be sure
  // to check the handle's value before using it.
  void OpenCache(const std::string& cache_name,
                 const CacheAndErrorCallback& callback);

  // Calls the callback with whether or not the cache exists.
  void HasCache(const std::string& cache_name,
                const BoolAndErrorCallback& callback);

  // Deletes the cache if it exists. If it doesn't exist,
  // CACHE_STORAGE_ERROR_NOT_FOUND is returned. Any existing
  // CacheStorageCacheHandle(s) to the cache will remain valid but future
  // CacheStorage operations won't be able to access the cache. The cache
  // isn't actually erased from disk until the last handle is dropped.
  // TODO(jkarlin): Rename to DoomCache.
  void DeleteCache(const std::string& cache_name,
                   const BoolAndErrorCallback& callback);

  // Calls the callback with a vector of cache names (keys) available.
  void EnumerateCaches(const StringsAndErrorCallback& callback);

  // Calls match on the cache with the given |cache_name|.
  void MatchCache(const std::string& cache_name,
                  std::unique_ptr<ServiceWorkerFetchRequest> request,
                  const CacheStorageCache::ResponseCallback& callback);

  // Calls match on all of the caches in parallel, calling |callback| with the
  // response from the first cache (in order of cache creation) to have the
  // entry. If no response is found then |callback| is called with
  // CACHE_STORAGE_ERROR_NOT_FOUND.
  void MatchAllCaches(std::unique_ptr<ServiceWorkerFetchRequest> request,
                      const CacheStorageCache::ResponseCallback& callback);

  // Sums the sizes of each cache and closes them. Runs |callback| with the
  // size.
  void GetSizeThenCloseAllCaches(const SizeCallback& callback);

  // The size of all of the origin's contents. This value should be used as an
  // estimate only since the cache may be modified at any time.
  void Size(const SizeCallback& callback);

  // The functions below are for tests to verify that the operations run
  // serially.
  void StartAsyncOperationForTesting();
  void CompleteAsyncOperationForTesting();

 private:
  friend class CacheStorageCacheHandle;
  friend class CacheStorageCache;
  class CacheLoader;
  class MemoryLoader;
  class SimpleCacheLoader;
  struct CacheMatchResponse;

  typedef std::map<std::string, std::unique_ptr<CacheStorageCache>> CacheMap;

  // Functions for exposing handles to CacheStorageCache to clients.
  std::unique_ptr<CacheStorageCacheHandle> CreateCacheHandle(
      CacheStorageCache* cache);
  void AddCacheHandleRef(CacheStorageCache* cache);
  void DropCacheHandleRef(CacheStorageCache* cache);

  // Returns a CacheStorageCacheHandle for the given name if the name is known.
  // If the CacheStorageCache has been deleted, creates a new one.
  std::unique_ptr<CacheStorageCacheHandle> GetLoadedCache(
      const std::string& cache_name);

  // Initializer and its callback are below.
  void LazyInit();
  void LazyInitImpl();
  void LazyInitDidLoadIndex(
      std::unique_ptr<std::vector<std::string>> indexed_cache_names);

  // The Open and CreateCache callbacks are below.
  void OpenCacheImpl(const std::string& cache_name,
                     const CacheAndErrorCallback& callback);
  void CreateCacheDidCreateCache(const std::string& cache_name,
                                 const CacheAndErrorCallback& callback,
                                 std::unique_ptr<CacheStorageCache> cache);
  void CreateCacheDidWriteIndex(
      const CacheAndErrorCallback& callback,
      std::unique_ptr<CacheStorageCacheHandle> cache_handle,
      bool success);

  // The HasCache callbacks are below.
  void HasCacheImpl(const std::string& cache_name,
                    const BoolAndErrorCallback& callback);

  // The DeleteCache callbacks are below.
  void DeleteCacheImpl(const std::string& cache_name,
                       const BoolAndErrorCallback& callback);
  void DeleteCacheDidWriteIndex(
      const std::string& cache_name,
      const StringVector& original_ordered_cache_names,
      const BoolAndErrorCallback& callback,
      bool success);
  void DeleteCacheFinalize(std::unique_ptr<CacheStorageCache> doomed_cache);
  void DeleteCacheDidGetSize(std::unique_ptr<CacheStorageCache> cache,
                             int64_t cache_size);
  void DeleteCacheDidCleanUp(bool success);

  // The EnumerateCache callbacks are below.
  void EnumerateCachesImpl(const StringsAndErrorCallback& callback);

  // The MatchCache callbacks are below.
  void MatchCacheImpl(const std::string& cache_name,
                      std::unique_ptr<ServiceWorkerFetchRequest> request,
                      const CacheStorageCache::ResponseCallback& callback);
  void MatchCacheDidMatch(std::unique_ptr<CacheStorageCacheHandle> cache_handle,
                          const CacheStorageCache::ResponseCallback& callback,
                          CacheStorageError error,
                          std::unique_ptr<ServiceWorkerResponse> response,
                          std::unique_ptr<storage::BlobDataHandle> handle);

  // The MatchAllCaches callbacks are below.
  void MatchAllCachesImpl(std::unique_ptr<ServiceWorkerFetchRequest> request,
                          const CacheStorageCache::ResponseCallback& callback);
  void MatchAllCachesDidMatch(
      std::unique_ptr<CacheStorageCacheHandle> cache_handle,
      CacheMatchResponse* out_match_response,
      const base::Closure& barrier_closure,
      CacheStorageError error,
      std::unique_ptr<ServiceWorkerResponse> service_worker_response,
      std::unique_ptr<storage::BlobDataHandle> handle);
  void MatchAllCachesDidMatchAll(
      std::unique_ptr<std::vector<CacheMatchResponse>> match_responses,
      const CacheStorageCache::ResponseCallback& callback);

  void GetSizeThenCloseAllCachesImpl(const SizeCallback& callback);

  void SizeImpl(const SizeCallback& callback);

  void PendingClosure(const base::Closure& callback);
  void PendingBoolAndErrorCallback(const BoolAndErrorCallback& callback,
                                   bool found,
                                   CacheStorageError error);
  void PendingCacheAndErrorCallback(
      const CacheAndErrorCallback& callback,
      std::unique_ptr<CacheStorageCacheHandle> cache_handle,
      CacheStorageError error);
  void PendingStringsAndErrorCallback(const StringsAndErrorCallback& callback,
                                      const StringVector& strings,
                                      CacheStorageError error);
  void PendingResponseCallback(
      const CacheStorageCache::ResponseCallback& callback,
      CacheStorageError error,
      std::unique_ptr<ServiceWorkerResponse> response,
      std::unique_ptr<storage::BlobDataHandle> blob_data_handle);

  void PendingSizeCallback(const SizeCallback& callback, int64_t size);

  // Whether or not we've loaded the list of cache names into memory.
  bool initialized_;
  bool initializing_;

  // True if the backend is supposed to reside in memory only.
  bool memory_only_;

  // The pending operation scheduler.
  std::unique_ptr<CacheStorageScheduler> scheduler_;

  // The map of cache names to CacheStorageCache objects.
  CacheMap cache_map_;

  // Caches that have been deleted but must still be held onto until all handles
  // have been released.
  std::map<CacheStorageCache*, std::unique_ptr<CacheStorageCache>>
      doomed_caches_;

  // CacheStorageCacheHandle reference counts
  std::map<CacheStorageCache*, size_t> cache_handle_counts_;

  // The names of caches in the order that they were created.
  StringVector ordered_cache_names_;

  // The file path for this CacheStorage.
  base::FilePath origin_path_;

  // The TaskRunner to run file IO on.
  scoped_refptr<base::SequencedTaskRunner> cache_task_runner_;

  // Performs backend specific operations (memory vs disk).
  std::unique_ptr<CacheLoader> cache_loader_;

  // The quota manager.
  scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;

  // The origin that this CacheStorage is associated with.
  GURL origin_;

  base::WeakPtrFactory<CacheStorage> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CacheStorage);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_H_
