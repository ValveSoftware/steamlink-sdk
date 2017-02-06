// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage.h"

#include <stddef.h>

#include <set>
#include <string>
#include <utility>

#include "base/barrier_closure.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/sha1.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/cache_storage/cache_storage.pb.h"
#include "content/browser/cache_storage/cache_storage_cache.h"
#include "content/browser/cache_storage/cache_storage_cache_handle.h"
#include "content/browser/cache_storage/cache_storage_scheduler.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/directory_lister.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_context_getter.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/quota/quota_manager_proxy.h"

namespace content {

namespace {

std::string HexedHash(const std::string& value) {
  std::string value_hash = base::SHA1HashString(value);
  std::string valued_hexed_hash = base::ToLowerASCII(
      base::HexEncode(value_hash.c_str(), value_hash.length()));
  return valued_hexed_hash;
}

void SizeRetrievedFromCache(
    std::unique_ptr<CacheStorageCacheHandle> cache_handle,
    const base::Closure& closure,
    int64_t* accumulator,
    int64_t size) {
  *accumulator += size;
  closure.Run();
}

void SizeRetrievedFromAllCaches(std::unique_ptr<int64_t> accumulator,
                                const CacheStorage::SizeCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, *accumulator));
}

}  // namespace

const char CacheStorage::kIndexFileName[] = "index.txt";

struct CacheStorage::CacheMatchResponse {
  CacheMatchResponse() = default;
  ~CacheMatchResponse() = default;

  CacheStorageError error;
  std::unique_ptr<ServiceWorkerResponse> service_worker_response;
  std::unique_ptr<storage::BlobDataHandle> blob_data_handle;
};

// Handles the loading and clean up of CacheStorageCache objects.
class CacheStorage::CacheLoader {
 public:
  typedef base::Callback<void(std::unique_ptr<CacheStorageCache>)>
      CacheCallback;
  typedef base::Callback<void(bool)> BoolCallback;
  typedef base::Callback<void(std::unique_ptr<std::vector<std::string>>)>
      StringVectorCallback;

  CacheLoader(
      base::SequencedTaskRunner* cache_task_runner,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter,
      storage::QuotaManagerProxy* quota_manager_proxy,
      base::WeakPtr<storage::BlobStorageContext> blob_context,
      CacheStorage* cache_storage,
      const GURL& origin)
      : cache_task_runner_(cache_task_runner),
        request_context_getter_(request_context_getter),
        quota_manager_proxy_(quota_manager_proxy),
        blob_context_(blob_context),
        cache_storage_(cache_storage),
        origin_(origin) {
    DCHECK(!origin_.is_empty());
  }

  virtual ~CacheLoader() {}

  // Creates a CacheStorageCache with the given name. It does not attempt to
  // load the backend, that happens lazily when the cache is used.
  virtual std::unique_ptr<CacheStorageCache> CreateCache(
      const std::string& cache_name) = 0;

  // Deletes any pre-existing cache of the same name and then loads it.
  virtual void PrepareNewCacheDestination(const std::string& cache_name,
                                          const CacheCallback& callback) = 0;

  // After the backend has been deleted, do any extra house keeping such as
  // removing the cache's directory.
  virtual void CleanUpDeletedCache(const std::string& key) = 0;

  // Writes the cache names (and sizes) to disk if applicable.
  virtual void WriteIndex(const StringVector& cache_names,
                          const BoolCallback& callback) = 0;

  // Loads the cache names from disk if applicable.
  virtual void LoadIndex(std::unique_ptr<std::vector<std::string>> cache_names,
                         const StringVectorCallback& callback) = 0;

  // Called when CacheStorage has created a cache. Used to hold onto a handle to
  // the cache if necessary.
  virtual void NotifyCacheCreated(
      const std::string& cache_name,
      std::unique_ptr<CacheStorageCacheHandle> cache_handle){};

  // Notification that a cache has been doomed and will be deleted once the last
  // cache handle has been dropped. If the loader is holding a handle to the
  // cache, it should drop it now.
  virtual void NotifyCacheDoomed(const std::string& cache_name){};

 protected:
  scoped_refptr<base::SequencedTaskRunner> cache_task_runner_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

  // Owned by CacheStorage which owns this.
  storage::QuotaManagerProxy* quota_manager_proxy_;

  base::WeakPtr<storage::BlobStorageContext> blob_context_;

  // Raw pointer is safe because this object is owned by cache_storage_.
  CacheStorage* cache_storage_;

  GURL origin_;
};

// Creates memory-only ServiceWorkerCaches. Because these caches have no
// persistent storage it is not safe to free them from memory if they might be
// used again. Therefore this class holds a reference to each cache until the
// cache is doomed.
class CacheStorage::MemoryLoader : public CacheStorage::CacheLoader {
 public:
  MemoryLoader(base::SequencedTaskRunner* cache_task_runner,
               scoped_refptr<net::URLRequestContextGetter> request_context,
               storage::QuotaManagerProxy* quota_manager_proxy,
               base::WeakPtr<storage::BlobStorageContext> blob_context,
               CacheStorage* cache_storage,
               const GURL& origin)
      : CacheLoader(cache_task_runner,
                    request_context,
                    quota_manager_proxy,
                    blob_context,
                    cache_storage,
                    origin) {}

  std::unique_ptr<CacheStorageCache> CreateCache(
      const std::string& cache_name) override {
    return CacheStorageCache::CreateMemoryCache(
        origin_, cache_name, cache_storage_, request_context_getter_,
        quota_manager_proxy_, blob_context_);
  }

  void PrepareNewCacheDestination(const std::string& cache_name,
                                  const CacheCallback& callback) override {
    std::unique_ptr<CacheStorageCache> cache = CreateCache(cache_name);
    callback.Run(std::move(cache));
  }

  void CleanUpDeletedCache(const std::string& cache_name) override {}

  void WriteIndex(const StringVector& cache_names,
                  const BoolCallback& callback) override {
    callback.Run(true);
  }

  void LoadIndex(std::unique_ptr<std::vector<std::string>> cache_names,
                 const StringVectorCallback& callback) override {
    callback.Run(std::move(cache_names));
  }

  void NotifyCacheCreated(
      const std::string& cache_name,
      std::unique_ptr<CacheStorageCacheHandle> cache_handle) override {
    DCHECK(!ContainsKey(cache_handles_, cache_name));
    cache_handles_.insert(std::make_pair(cache_name, std::move(cache_handle)));
  };

  void NotifyCacheDoomed(const std::string& cache_name) override {
    DCHECK(ContainsKey(cache_handles_, cache_name));
    cache_handles_.erase(cache_name);
  };

 private:
  typedef std::map<std::string, std::unique_ptr<CacheStorageCacheHandle>>
      CacheHandles;
  ~MemoryLoader() override {}

  // Keep a reference to each cache to ensure that it's not freed before the
  // client calls CacheStorage::Delete or the CacheStorage is
  // freed.
  CacheHandles cache_handles_;
};

class CacheStorage::SimpleCacheLoader : public CacheStorage::CacheLoader {
 public:
  SimpleCacheLoader(const base::FilePath& origin_path,
                    base::SequencedTaskRunner* cache_task_runner,
                    scoped_refptr<net::URLRequestContextGetter> request_context,
                    storage::QuotaManagerProxy* quota_manager_proxy,
                    base::WeakPtr<storage::BlobStorageContext> blob_context,
                    CacheStorage* cache_storage,
                    const GURL& origin)
      : CacheLoader(cache_task_runner,
                    request_context,
                    quota_manager_proxy,
                    blob_context,
                    cache_storage,
                    origin),
        origin_path_(origin_path),
        weak_ptr_factory_(this) {}

  std::unique_ptr<CacheStorageCache> CreateCache(
      const std::string& cache_name) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(ContainsKey(cache_name_to_cache_dir_, cache_name));

    std::string cache_dir = cache_name_to_cache_dir_[cache_name];
    base::FilePath cache_path = origin_path_.AppendASCII(cache_dir);
    return CacheStorageCache::CreatePersistentCache(
        origin_, cache_name, cache_storage_, cache_path,
        request_context_getter_, quota_manager_proxy_, blob_context_);
  }

  void PrepareNewCacheDestination(const std::string& cache_name,
                                  const CacheCallback& callback) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    PostTaskAndReplyWithResult(
        cache_task_runner_.get(), FROM_HERE,
        base::Bind(&SimpleCacheLoader::PrepareNewCacheDirectoryInPool,
                   origin_path_),
        base::Bind(&SimpleCacheLoader::PrepareNewCacheCreateCache,
                   weak_ptr_factory_.GetWeakPtr(), cache_name, callback));
  }

  // Runs on the cache_task_runner_.
  static std::string PrepareNewCacheDirectoryInPool(
      const base::FilePath& origin_path) {
    std::string cache_dir;
    base::FilePath cache_path;
    do {
      cache_dir = base::GenerateGUID();
      cache_path = origin_path.AppendASCII(cache_dir);
    } while (base::PathExists(cache_path));

    return base::CreateDirectory(cache_path) ? cache_dir : "";
  }

  void PrepareNewCacheCreateCache(const std::string& cache_name,
                                  const CacheCallback& callback,
                                  const std::string& cache_dir) {
    if (cache_dir.empty()) {
      callback.Run(std::unique_ptr<CacheStorageCache>());
      return;
    }

    cache_name_to_cache_dir_[cache_name] = cache_dir;
    callback.Run(CreateCache(cache_name));
  }

  void CleanUpDeletedCache(const std::string& cache_name) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(ContainsKey(cache_name_to_cache_dir_, cache_name));

    base::FilePath cache_path =
        origin_path_.AppendASCII(cache_name_to_cache_dir_[cache_name]);
    cache_name_to_cache_dir_.erase(cache_name);

    cache_task_runner_->PostTask(
        FROM_HERE, base::Bind(&SimpleCacheLoader::CleanUpDeleteCacheDirInPool,
                              cache_path));
  }

  static void CleanUpDeleteCacheDirInPool(const base::FilePath& cache_path) {
    base::DeleteFile(cache_path, true /* recursive */);
  }

  void WriteIndex(const StringVector& cache_names,
                  const BoolCallback& callback) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    // 1. Create the index file as a string. (WriteIndex)
    // 2. Write the file to disk. (WriteIndexWriteToFileInPool)

    CacheStorageIndex index;
    index.set_origin(origin_.spec());

    for (size_t i = 0u, max = cache_names.size(); i < max; ++i) {
      DCHECK(ContainsKey(cache_name_to_cache_dir_, cache_names[i]));

      CacheStorageIndex::Cache* index_cache = index.add_cache();
      index_cache->set_name(cache_names[i]);
      index_cache->set_cache_dir(cache_name_to_cache_dir_[cache_names[i]]);
    }

    std::string serialized;
    bool success = index.SerializeToString(&serialized);
    DCHECK(success);

    base::FilePath tmp_path = origin_path_.AppendASCII("index.txt.tmp");
    base::FilePath index_path =
        origin_path_.AppendASCII(CacheStorage::kIndexFileName);

    PostTaskAndReplyWithResult(
        cache_task_runner_.get(), FROM_HERE,
        base::Bind(&SimpleCacheLoader::WriteIndexWriteToFileInPool, tmp_path,
                   index_path, serialized),
        callback);
  }

  static bool WriteIndexWriteToFileInPool(const base::FilePath& tmp_path,
                                          const base::FilePath& index_path,
                                          const std::string& data) {
    int bytes_written = base::WriteFile(tmp_path, data.c_str(), data.size());
    if (bytes_written != base::checked_cast<int>(data.size())) {
      base::DeleteFile(tmp_path, /* recursive */ false);
      return false;
    }

    // Atomically rename the temporary index file to become the real one.
    return base::ReplaceFile(tmp_path, index_path, NULL);
  }

  void LoadIndex(std::unique_ptr<std::vector<std::string>> names,
                 const StringVectorCallback& callback) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    // 1. Read the file from disk. (LoadIndexReadFileInPool)
    // 2. Parse file and return the names of the caches (LoadIndexDidReadFile)

    base::FilePath index_path =
        origin_path_.AppendASCII(CacheStorage::kIndexFileName);

    PostTaskAndReplyWithResult(
        cache_task_runner_.get(), FROM_HERE,
        base::Bind(&SimpleCacheLoader::ReadAndMigrateIndexInPool, index_path),
        base::Bind(&SimpleCacheLoader::LoadIndexDidReadFile,
                   weak_ptr_factory_.GetWeakPtr(), base::Passed(&names),
                   callback));
  }

  void LoadIndexDidReadFile(std::unique_ptr<std::vector<std::string>> names,
                            const StringVectorCallback& callback,
                            const std::string& serialized) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    std::unique_ptr<std::set<std::string>> cache_dirs(
        new std::set<std::string>);

    CacheStorageIndex index;
    if (index.ParseFromString(serialized)) {
      for (int i = 0, max = index.cache_size(); i < max; ++i) {
        const CacheStorageIndex::Cache& cache = index.cache(i);
        DCHECK(cache.has_cache_dir());
        names->push_back(cache.name());
        cache_name_to_cache_dir_[cache.name()] = cache.cache_dir();
        cache_dirs->insert(cache.cache_dir());
      }
    }

    cache_task_runner_->PostTask(
        FROM_HERE, base::Bind(&DeleteUnreferencedCachesInPool, origin_path_,
                              base::Passed(&cache_dirs)));
    callback.Run(std::move(names));
  }

 private:
  friend class MigratedLegacyCacheDirectoryNameTest;
  ~SimpleCacheLoader() override {}

  // Iterates over the caches and deletes any directory not found in
  // |cache_dirs|. Runs on cache_task_runner_
  static void DeleteUnreferencedCachesInPool(
      const base::FilePath& cache_base_dir,
      std::unique_ptr<std::set<std::string>> cache_dirs) {
    base::FileEnumerator file_enum(cache_base_dir, false /* recursive */,
                                   base::FileEnumerator::DIRECTORIES);
    std::vector<base::FilePath> dirs_to_delete;
    base::FilePath cache_path;
    while (!(cache_path = file_enum.Next()).empty()) {
      if (!ContainsKey(*cache_dirs, cache_path.BaseName().AsUTF8Unsafe()))
        dirs_to_delete.push_back(cache_path);
    }

    for (const base::FilePath& cache_path : dirs_to_delete)
      base::DeleteFile(cache_path, true /* recursive */);
  }

  // Runs on cache_task_runner_
  static std::string MigrateCachesIfNecessaryInPool(
      const std::string& body,
      const base::FilePath& index_path) {
    CacheStorageIndex index;
    if (!index.ParseFromString(body))
      return body;

    base::FilePath origin_path = index_path.DirName();
    bool index_is_dirty = false;
    const std::string kBadIndexState("");

    // Look for caches that have no cache_dir. Give any such caches a directory
    // with a random name and move them there. Then, rewrite the index file.
    for (int i = 0, max = index.cache_size(); i < max; ++i) {
      const CacheStorageIndex::Cache& cache = index.cache(i);
      if (!cache.has_cache_dir()) {
        // Find a new home for the cache.
        base::FilePath legacy_cache_path =
            origin_path.AppendASCII(HexedHash(cache.name()));
        std::string cache_dir;
        base::FilePath cache_path;
        do {
          cache_dir = base::GenerateGUID();
          cache_path = origin_path.AppendASCII(cache_dir);
        } while (base::PathExists(cache_path));

        if (!base::Move(legacy_cache_path, cache_path)) {
          // If the move fails then the cache is in a bad state. Return an empty
          // index so that the CacheStorage can start fresh. The unreferenced
          // caches will be discarded later in initialization.
          return kBadIndexState;
        }

        index.mutable_cache(i)->set_cache_dir(cache_dir);
        index_is_dirty = true;
      }
    }

    if (index_is_dirty) {
      std::string new_body;
      if (!index.SerializeToString(&new_body))
        return kBadIndexState;
      if (base::WriteFile(index_path, new_body.c_str(), new_body.size()) !=
          base::checked_cast<int>(new_body.size()))
        return kBadIndexState;
      return new_body;
    }

    return body;
  }

  // Runs on cache_task_runner_
  static std::string ReadAndMigrateIndexInPool(
      const base::FilePath& index_path) {
    std::string body;
    base::ReadFileToString(index_path, &body);

    return MigrateCachesIfNecessaryInPool(body, index_path);
  }

  const base::FilePath origin_path_;
  std::map<std::string, std::string> cache_name_to_cache_dir_;

  base::WeakPtrFactory<SimpleCacheLoader> weak_ptr_factory_;
};

CacheStorage::CacheStorage(
    const base::FilePath& path,
    bool memory_only,
    base::SequencedTaskRunner* cache_task_runner,
    scoped_refptr<net::URLRequestContextGetter> request_context,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
    base::WeakPtr<storage::BlobStorageContext> blob_context,
    const GURL& origin)
    : initialized_(false),
      initializing_(false),
      memory_only_(memory_only),
      scheduler_(new CacheStorageScheduler()),
      origin_path_(path),
      cache_task_runner_(cache_task_runner),
      quota_manager_proxy_(quota_manager_proxy),
      origin_(origin),
      weak_factory_(this) {
  if (memory_only)
    cache_loader_.reset(new MemoryLoader(
        cache_task_runner_.get(), std::move(request_context),
        quota_manager_proxy.get(), blob_context, this, origin));
  else
    cache_loader_.reset(new SimpleCacheLoader(
        origin_path_, cache_task_runner_.get(), std::move(request_context),
        quota_manager_proxy.get(), blob_context, this, origin));
}

CacheStorage::~CacheStorage() {
}

void CacheStorage::OpenCache(const std::string& cache_name,
                             const CacheAndErrorCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!initialized_)
    LazyInit();

  quota_manager_proxy_->NotifyStorageAccessed(
      storage::QuotaClient::kServiceWorkerCache, origin_,
      storage::kStorageTypeTemporary);

  CacheAndErrorCallback pending_callback =
      base::Bind(&CacheStorage::PendingCacheAndErrorCallback,
                 weak_factory_.GetWeakPtr(), callback);
  scheduler_->ScheduleOperation(base::Bind(&CacheStorage::OpenCacheImpl,
                                           weak_factory_.GetWeakPtr(),
                                           cache_name, pending_callback));
}

void CacheStorage::HasCache(const std::string& cache_name,
                            const BoolAndErrorCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!initialized_)
    LazyInit();

  quota_manager_proxy_->NotifyStorageAccessed(
      storage::QuotaClient::kServiceWorkerCache, origin_,
      storage::kStorageTypeTemporary);

  BoolAndErrorCallback pending_callback =
      base::Bind(&CacheStorage::PendingBoolAndErrorCallback,
                 weak_factory_.GetWeakPtr(), callback);
  scheduler_->ScheduleOperation(base::Bind(&CacheStorage::HasCacheImpl,
                                           weak_factory_.GetWeakPtr(),
                                           cache_name, pending_callback));
}

void CacheStorage::DeleteCache(const std::string& cache_name,
                               const BoolAndErrorCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!initialized_)
    LazyInit();

  quota_manager_proxy_->NotifyStorageAccessed(
      storage::QuotaClient::kServiceWorkerCache, origin_,
      storage::kStorageTypeTemporary);

  BoolAndErrorCallback pending_callback =
      base::Bind(&CacheStorage::PendingBoolAndErrorCallback,
                 weak_factory_.GetWeakPtr(), callback);
  scheduler_->ScheduleOperation(base::Bind(&CacheStorage::DeleteCacheImpl,
                                           weak_factory_.GetWeakPtr(),
                                           cache_name, pending_callback));
}

void CacheStorage::EnumerateCaches(const StringsAndErrorCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!initialized_)
    LazyInit();

  quota_manager_proxy_->NotifyStorageAccessed(
      storage::QuotaClient::kServiceWorkerCache, origin_,
      storage::kStorageTypeTemporary);

  StringsAndErrorCallback pending_callback =
      base::Bind(&CacheStorage::PendingStringsAndErrorCallback,
                 weak_factory_.GetWeakPtr(), callback);
  scheduler_->ScheduleOperation(base::Bind(&CacheStorage::EnumerateCachesImpl,
                                           weak_factory_.GetWeakPtr(),
                                           pending_callback));
}

void CacheStorage::MatchCache(
    const std::string& cache_name,
    std::unique_ptr<ServiceWorkerFetchRequest> request,
    const CacheStorageCache::ResponseCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!initialized_)
    LazyInit();

  quota_manager_proxy_->NotifyStorageAccessed(
      storage::QuotaClient::kServiceWorkerCache, origin_,
      storage::kStorageTypeTemporary);

  CacheStorageCache::ResponseCallback pending_callback =
      base::Bind(&CacheStorage::PendingResponseCallback,
                 weak_factory_.GetWeakPtr(), callback);
  scheduler_->ScheduleOperation(base::Bind(
      &CacheStorage::MatchCacheImpl, weak_factory_.GetWeakPtr(), cache_name,
      base::Passed(std::move(request)), pending_callback));
}

void CacheStorage::MatchAllCaches(
    std::unique_ptr<ServiceWorkerFetchRequest> request,
    const CacheStorageCache::ResponseCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!initialized_)
    LazyInit();

  quota_manager_proxy_->NotifyStorageAccessed(
      storage::QuotaClient::kServiceWorkerCache, origin_,
      storage::kStorageTypeTemporary);

  CacheStorageCache::ResponseCallback pending_callback =
      base::Bind(&CacheStorage::PendingResponseCallback,
                 weak_factory_.GetWeakPtr(), callback);
  scheduler_->ScheduleOperation(
      base::Bind(&CacheStorage::MatchAllCachesImpl, weak_factory_.GetWeakPtr(),
                 base::Passed(std::move(request)), pending_callback));
}

void CacheStorage::GetSizeThenCloseAllCaches(const SizeCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!initialized_)
    LazyInit();

  CacheStorageCache::SizeCallback pending_callback = base::Bind(
      &CacheStorage::PendingSizeCallback, weak_factory_.GetWeakPtr(), callback);

  scheduler_->ScheduleOperation(
      base::Bind(&CacheStorage::GetSizeThenCloseAllCachesImpl,
                 weak_factory_.GetWeakPtr(), pending_callback));
}

void CacheStorage::Size(const CacheStorage::SizeCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!initialized_)
    LazyInit();

  CacheStorageCache::SizeCallback pending_callback = base::Bind(
      &CacheStorage::PendingSizeCallback, weak_factory_.GetWeakPtr(), callback);

  scheduler_->ScheduleOperation(base::Bind(
      &CacheStorage::SizeImpl, weak_factory_.GetWeakPtr(), pending_callback));
}

void CacheStorage::StartAsyncOperationForTesting() {
  scheduler_->ScheduleOperation(base::Bind(&base::DoNothing));
}

void CacheStorage::CompleteAsyncOperationForTesting() {
  scheduler_->CompleteOperationAndRunNext();
}

// Init is run lazily so that it is called on the proper MessageLoop.
void CacheStorage::LazyInit() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!initialized_);

  if (initializing_)
    return;

  DCHECK(!scheduler_->ScheduledOperations());

  initializing_ = true;
  scheduler_->ScheduleOperation(
      base::Bind(&CacheStorage::LazyInitImpl, weak_factory_.GetWeakPtr()));
}

void CacheStorage::LazyInitImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!initialized_);
  DCHECK(initializing_);

  // 1. Get the list of cache names (async call)
  // 2. For each cache name, load the cache (async call)
  // 3. Once each load is complete, update the map variables.
  // 4. Call the list of waiting callbacks.

  std::unique_ptr<std::vector<std::string>> indexed_cache_names(
      new std::vector<std::string>());

  cache_loader_->LoadIndex(std::move(indexed_cache_names),
                           base::Bind(&CacheStorage::LazyInitDidLoadIndex,
                                      weak_factory_.GetWeakPtr()));
}

void CacheStorage::LazyInitDidLoadIndex(
    std::unique_ptr<std::vector<std::string>> indexed_cache_names) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (size_t i = 0u, max = indexed_cache_names->size(); i < max; ++i) {
    cache_map_.insert(std::make_pair(indexed_cache_names->at(i),
                                     std::unique_ptr<CacheStorageCache>()));
    ordered_cache_names_.push_back(indexed_cache_names->at(i));
  }

  initializing_ = false;
  initialized_ = true;

  scheduler_->CompleteOperationAndRunNext();
}

void CacheStorage::OpenCacheImpl(const std::string& cache_name,
                                 const CacheAndErrorCallback& callback) {
  std::unique_ptr<CacheStorageCacheHandle> cache_handle =
      GetLoadedCache(cache_name);
  if (cache_handle) {
    callback.Run(std::move(cache_handle), CACHE_STORAGE_OK);
    return;
  }

  cache_loader_->PrepareNewCacheDestination(
      cache_name, base::Bind(&CacheStorage::CreateCacheDidCreateCache,
                             weak_factory_.GetWeakPtr(), cache_name, callback));
}

void CacheStorage::CreateCacheDidCreateCache(
    const std::string& cache_name,
    const CacheAndErrorCallback& callback,
    std::unique_ptr<CacheStorageCache> cache) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  UMA_HISTOGRAM_BOOLEAN("ServiceWorkerCache.CreateCacheStorageResult",
                        static_cast<bool>(cache));

  if (!cache) {
    callback.Run(std::unique_ptr<CacheStorageCacheHandle>(),
                 CACHE_STORAGE_ERROR_STORAGE);
    return;
  }

  CacheStorageCache* cache_ptr = cache.get();

  cache_map_.insert(std::make_pair(cache_name, std::move(cache)));
  ordered_cache_names_.push_back(cache_name);

  cache_loader_->WriteIndex(
      ordered_cache_names_,
      base::Bind(&CacheStorage::CreateCacheDidWriteIndex,
                 weak_factory_.GetWeakPtr(), callback,
                 base::Passed(CreateCacheHandle(cache_ptr))));

  cache_loader_->NotifyCacheCreated(cache_name, CreateCacheHandle(cache_ptr));
}

void CacheStorage::CreateCacheDidWriteIndex(
    const CacheAndErrorCallback& callback,
    std::unique_ptr<CacheStorageCacheHandle> cache_handle,
    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(cache_handle);

  // TODO(jkarlin): Handle !success.

  callback.Run(std::move(cache_handle), CACHE_STORAGE_OK);
}

void CacheStorage::HasCacheImpl(const std::string& cache_name,
                                const BoolAndErrorCallback& callback) {
  bool has_cache = ContainsKey(cache_map_, cache_name);
  callback.Run(has_cache, CACHE_STORAGE_OK);
}

void CacheStorage::DeleteCacheImpl(const std::string& cache_name,
                                   const BoolAndErrorCallback& callback) {
  if (!GetLoadedCache(cache_name)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, false, CACHE_STORAGE_ERROR_NOT_FOUND));
    return;
  }

  // Delete the name from ordered_cache_names_.
  StringVector original_ordered_cache_names = ordered_cache_names_;
  StringVector::iterator iter = std::find(
      ordered_cache_names_.begin(), ordered_cache_names_.end(), cache_name);
  DCHECK(iter != ordered_cache_names_.end());
  ordered_cache_names_.erase(iter);

  cache_loader_->WriteIndex(ordered_cache_names_,
                            base::Bind(&CacheStorage::DeleteCacheDidWriteIndex,
                                       weak_factory_.GetWeakPtr(), cache_name,
                                       original_ordered_cache_names, callback));
}

void CacheStorage::DeleteCacheDidWriteIndex(
    const std::string& cache_name,
    const StringVector& original_ordered_cache_names,
    const BoolAndErrorCallback& callback,
    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!success) {
    // Undo any changes if the change couldn't be written to disk.
    ordered_cache_names_ = original_ordered_cache_names;
    callback.Run(false, CACHE_STORAGE_ERROR_STORAGE);
    return;
  }

  CacheMap::iterator map_iter = cache_map_.find(cache_name);
  doomed_caches_.insert(
      std::make_pair(map_iter->second.get(), std::move(map_iter->second)));
  cache_map_.erase(map_iter);

  cache_loader_->NotifyCacheDoomed(cache_name);

  callback.Run(true, CACHE_STORAGE_OK);
}

// Call this once the last handle to a doomed cache is gone. It's okay if this
// doesn't get to complete before shutdown, the cache will be removed from disk
// on next startup in that case.
void CacheStorage::DeleteCacheFinalize(
    std::unique_ptr<CacheStorageCache> doomed_cache) {
  CacheStorageCache* cache = doomed_cache.get();
  cache->Size(base::Bind(&CacheStorage::DeleteCacheDidGetSize,
                         weak_factory_.GetWeakPtr(),
                         base::Passed(std::move(doomed_cache))));
}

void CacheStorage::DeleteCacheDidGetSize(
    std::unique_ptr<CacheStorageCache> cache,
    int64_t cache_size) {
  quota_manager_proxy_->NotifyStorageModified(
      storage::QuotaClient::kServiceWorkerCache, origin_,
      storage::kStorageTypeTemporary, -1 * cache_size);

  cache_loader_->CleanUpDeletedCache(cache->cache_name());
}

void CacheStorage::EnumerateCachesImpl(
    const StringsAndErrorCallback& callback) {
  callback.Run(ordered_cache_names_, CACHE_STORAGE_OK);
}

void CacheStorage::MatchCacheImpl(
    const std::string& cache_name,
    std::unique_ptr<ServiceWorkerFetchRequest> request,
    const CacheStorageCache::ResponseCallback& callback) {
  std::unique_ptr<CacheStorageCacheHandle> cache_handle =
      GetLoadedCache(cache_name);

  if (!cache_handle) {
    callback.Run(CACHE_STORAGE_ERROR_CACHE_NAME_NOT_FOUND,
                 std::unique_ptr<ServiceWorkerResponse>(),
                 std::unique_ptr<storage::BlobDataHandle>());
    return;
  }

  // Pass the cache handle along to the callback to keep the cache open until
  // match is done.
  CacheStorageCache* cache_ptr = cache_handle->value();
  cache_ptr->Match(
      std::move(request),
      base::Bind(&CacheStorage::MatchCacheDidMatch, weak_factory_.GetWeakPtr(),
                 base::Passed(std::move(cache_handle)), callback));
}

void CacheStorage::MatchCacheDidMatch(
    std::unique_ptr<CacheStorageCacheHandle> cache_handle,
    const CacheStorageCache::ResponseCallback& callback,
    CacheStorageError error,
    std::unique_ptr<ServiceWorkerResponse> response,
    std::unique_ptr<storage::BlobDataHandle> handle) {
  callback.Run(error, std::move(response), std::move(handle));
}

void CacheStorage::MatchAllCachesImpl(
    std::unique_ptr<ServiceWorkerFetchRequest> request,
    const CacheStorageCache::ResponseCallback& callback) {
  std::vector<CacheMatchResponse>* match_responses =
      new std::vector<CacheMatchResponse>(ordered_cache_names_.size());

  base::Closure barrier_closure = base::BarrierClosure(
      ordered_cache_names_.size(),
      base::Bind(&CacheStorage::MatchAllCachesDidMatchAll,
                 weak_factory_.GetWeakPtr(),
                 base::Passed(base::WrapUnique(match_responses)), callback));

  for (size_t i = 0, max = ordered_cache_names_.size(); i < max; ++i) {
    std::unique_ptr<CacheStorageCacheHandle> cache_handle =
        GetLoadedCache(ordered_cache_names_[i]);
    DCHECK(cache_handle);

    CacheStorageCache* cache_ptr = cache_handle->value();
    cache_ptr->Match(base::WrapUnique(new ServiceWorkerFetchRequest(*request)),
                     base::Bind(&CacheStorage::MatchAllCachesDidMatch,
                                weak_factory_.GetWeakPtr(),
                                base::Passed(std::move(cache_handle)),
                                &match_responses->at(i), barrier_closure));
  }
}

void CacheStorage::MatchAllCachesDidMatch(
    std::unique_ptr<CacheStorageCacheHandle> cache_handle,
    CacheMatchResponse* out_match_response,
    const base::Closure& barrier_closure,
    CacheStorageError error,
    std::unique_ptr<ServiceWorkerResponse> service_worker_response,
    std::unique_ptr<storage::BlobDataHandle> handle) {
  out_match_response->error = error;
  out_match_response->service_worker_response =
      std::move(service_worker_response);
  out_match_response->blob_data_handle = std::move(handle);
  barrier_closure.Run();
}

void CacheStorage::MatchAllCachesDidMatchAll(
    std::unique_ptr<std::vector<CacheMatchResponse>> match_responses,
    const CacheStorageCache::ResponseCallback& callback) {
  for (CacheMatchResponse& match_response : *match_responses) {
    if (match_response.error == CACHE_STORAGE_ERROR_NOT_FOUND)
      continue;
    callback.Run(match_response.error,
                 std::move(match_response.service_worker_response),
                 std::move(match_response.blob_data_handle));
    return;
  }
  callback.Run(CACHE_STORAGE_ERROR_NOT_FOUND,
               std::unique_ptr<ServiceWorkerResponse>(),
               std::unique_ptr<storage::BlobDataHandle>());
}

void CacheStorage::AddCacheHandleRef(CacheStorageCache* cache) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  auto iter = cache_handle_counts_.find(cache);
  if (iter == cache_handle_counts_.end()) {
    cache_handle_counts_[cache] = 1;
    return;
  }

  iter->second += 1;
}

void CacheStorage::DropCacheHandleRef(CacheStorageCache* cache) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  auto iter = cache_handle_counts_.find(cache);
  DCHECK(iter != cache_handle_counts_.end());
  DCHECK(iter->second >= 1);

  iter->second -= 1;
  if (iter->second == 0) {
    auto doomed_caches_iter = doomed_caches_.find(cache);
    if (doomed_caches_iter != doomed_caches_.end()) {
      // The last reference to a doomed cache is gone, perform clean up.
      DeleteCacheFinalize(std::move(doomed_caches_iter->second));
      doomed_caches_.erase(doomed_caches_iter);
      return;
    }

    auto cache_map_iter = cache_map_.find(cache->cache_name());
    DCHECK(cache_map_iter != cache_map_.end());

    cache_map_iter->second.reset();
    cache_handle_counts_.erase(iter);
  }
}

std::unique_ptr<CacheStorageCacheHandle> CacheStorage::CreateCacheHandle(
    CacheStorageCache* cache) {
  DCHECK(cache);
  return std::unique_ptr<CacheStorageCacheHandle>(new CacheStorageCacheHandle(
      cache->AsWeakPtr(), weak_factory_.GetWeakPtr()));
}

std::unique_ptr<CacheStorageCacheHandle> CacheStorage::GetLoadedCache(
    const std::string& cache_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(initialized_);

  CacheMap::iterator map_iter = cache_map_.find(cache_name);
  if (map_iter == cache_map_.end())
    return std::unique_ptr<CacheStorageCacheHandle>();

  CacheStorageCache* cache = map_iter->second.get();

  if (!cache) {
    std::unique_ptr<CacheStorageCache> new_cache =
        cache_loader_->CreateCache(cache_name);
    CacheStorageCache* cache_ptr = new_cache.get();
    map_iter->second = std::move(new_cache);

    return CreateCacheHandle(cache_ptr);
  }

  return CreateCacheHandle(cache);
}

void CacheStorage::GetSizeThenCloseAllCachesImpl(const SizeCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(initialized_);

  std::unique_ptr<int64_t> accumulator(new int64_t(0));
  int64_t* accumulator_ptr = accumulator.get();

  base::Closure barrier_closure = base::BarrierClosure(
      ordered_cache_names_.size(),
      base::Bind(&SizeRetrievedFromAllCaches,
                 base::Passed(std::move(accumulator)), callback));

  for (const std::string& cache_name : ordered_cache_names_) {
    std::unique_ptr<CacheStorageCacheHandle> cache_handle =
        GetLoadedCache(cache_name);
    CacheStorageCache* cache = cache_handle->value();
    cache->GetSizeThenClose(base::Bind(&SizeRetrievedFromCache,
                                       base::Passed(std::move(cache_handle)),
                                       barrier_closure, accumulator_ptr));
  }
}

void CacheStorage::SizeImpl(const SizeCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(initialized_);

  std::unique_ptr<int64_t> accumulator(new int64_t(0));
  int64_t* accumulator_ptr = accumulator.get();

  base::Closure barrier_closure = base::BarrierClosure(
      ordered_cache_names_.size(),
      base::Bind(&SizeRetrievedFromAllCaches,
                 base::Passed(std::move(accumulator)), callback));

  for (const std::string& cache_name : ordered_cache_names_) {
    std::unique_ptr<CacheStorageCacheHandle> cache_handle =
        GetLoadedCache(cache_name);
    CacheStorageCache* cache = cache_handle->value();
    cache->Size(base::Bind(&SizeRetrievedFromCache,
                           base::Passed(std::move(cache_handle)),
                           barrier_closure, accumulator_ptr));
  }
}

void CacheStorage::PendingClosure(const base::Closure& callback) {
  base::WeakPtr<CacheStorage> cache_storage = weak_factory_.GetWeakPtr();

  callback.Run();
  if (cache_storage)
    scheduler_->CompleteOperationAndRunNext();
}

void CacheStorage::PendingBoolAndErrorCallback(
    const BoolAndErrorCallback& callback,
    bool found,
    CacheStorageError error) {
  base::WeakPtr<CacheStorage> cache_storage = weak_factory_.GetWeakPtr();

  callback.Run(found, error);
  if (cache_storage)
    scheduler_->CompleteOperationAndRunNext();
}

void CacheStorage::PendingCacheAndErrorCallback(
    const CacheAndErrorCallback& callback,
    std::unique_ptr<CacheStorageCacheHandle> cache_handle,
    CacheStorageError error) {
  base::WeakPtr<CacheStorage> cache_storage = weak_factory_.GetWeakPtr();

  callback.Run(std::move(cache_handle), error);
  if (cache_storage)
    scheduler_->CompleteOperationAndRunNext();
}

void CacheStorage::PendingStringsAndErrorCallback(
    const StringsAndErrorCallback& callback,
    const StringVector& strings,
    CacheStorageError error) {
  base::WeakPtr<CacheStorage> cache_storage = weak_factory_.GetWeakPtr();

  callback.Run(strings, error);
  if (cache_storage)
    scheduler_->CompleteOperationAndRunNext();
}

void CacheStorage::PendingResponseCallback(
    const CacheStorageCache::ResponseCallback& callback,
    CacheStorageError error,
    std::unique_ptr<ServiceWorkerResponse> response,
    std::unique_ptr<storage::BlobDataHandle> blob_data_handle) {
  base::WeakPtr<CacheStorage> cache_storage = weak_factory_.GetWeakPtr();

  callback.Run(error, std::move(response), std::move(blob_data_handle));
  if (cache_storage)
    scheduler_->CompleteOperationAndRunNext();
}

void CacheStorage::PendingSizeCallback(const SizeCallback& callback,
                                       int64_t size) {
  base::WeakPtr<CacheStorage> cache_storage = weak_factory_.GetWeakPtr();

  callback.Run(size);
  if (cache_storage)
    scheduler_->CompleteOperationAndRunNext();
}

}  // namespace content
