// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_manager.h"

#include <stdint.h>

#include <map>
#include <string>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/id_map.h"
#include "base/memory/ptr_util.h"
#include "base/sha1.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "content/browser/cache_storage/cache_storage.h"
#include "content/browser/cache_storage/cache_storage.pb.h"
#include "content/browser/cache_storage/cache_storage_quota_client.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/url_util.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/common/database/database_identifier.h"
#include "storage/common/quota/quota_status_code.h"
#include "url/gurl.h"

namespace content {

namespace {

bool DeleteDir(const base::FilePath& path) {
  return base::DeleteFile(path, true /* recursive */);
}

void DeleteOriginDidDeleteDir(
    const storage::QuotaClient::DeletionCallback& callback,
    bool rv) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, rv ? storage::kQuotaStatusOk
                                         : storage::kQuotaErrorAbort));
}

// Open the various cache directories' index files and extract their origins and
// last modified times.
void ListOriginsAndLastModifiedOnTaskRunner(
    std::vector<CacheStorageUsageInfo>* usages,
    base::FilePath root_path) {
  base::FileEnumerator file_enum(root_path, false /* recursive */,
                                 base::FileEnumerator::DIRECTORIES);

  base::FilePath path;
  while (!(path = file_enum.Next()).empty()) {
    std::string protobuf;
    base::ReadFileToString(path.AppendASCII(CacheStorage::kIndexFileName),
                           &protobuf);
    CacheStorageIndex index;
    if (index.ParseFromString(protobuf)) {
      if (index.has_origin()) {
        base::File::Info file_info;
        if (base::GetFileInfo(path, &file_info)) {
          usages->push_back(CacheStorageUsageInfo(
              GURL(index.origin()), 0 /* size */, file_info.last_modified));
        }
      }
    }
  }
}

std::set<GURL> ListOriginsOnTaskRunner(base::FilePath root_path) {
  std::vector<CacheStorageUsageInfo> usages;
  ListOriginsAndLastModifiedOnTaskRunner(&usages, root_path);

  std::set<GURL> out_origins;
  for (const CacheStorageUsageInfo& usage : usages)
    out_origins.insert(usage.origin);

  return out_origins;
}

void GetOriginsForHostDidListOrigins(
    const std::string& host,
    const storage::QuotaClient::GetOriginsCallback& callback,
    const std::set<GURL>& origins) {
  std::set<GURL> out_origins;
  for (const GURL& origin : origins) {
    if (host == net::GetHostOrSpecFromURL(origin))
      out_origins.insert(origin);
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, out_origins));
}

void EmptyQuotaStatusCallback(storage::QuotaStatusCode code) {}

void AllOriginSizesReported(
    std::unique_ptr<std::vector<CacheStorageUsageInfo>> usages,
    const CacheStorageContext::GetUsageInfoCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(callback, *usages));
}

void OneOriginSizeReported(const base::Closure& callback,
                           CacheStorageUsageInfo* usage,
                           int64_t size) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  usage->total_size_bytes = size;
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
}

}  // namespace

// static
std::unique_ptr<CacheStorageManager> CacheStorageManager::Create(
    const base::FilePath& path,
    scoped_refptr<base::SequencedTaskRunner> cache_task_runner,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy) {
  base::FilePath root_path = path;
  if (!path.empty()) {
    root_path = path.Append(ServiceWorkerContextCore::kServiceWorkerDirectory)
                    .AppendASCII("CacheStorage");
  }

  return base::WrapUnique(new CacheStorageManager(
      root_path, std::move(cache_task_runner), std::move(quota_manager_proxy)));
}

// static
std::unique_ptr<CacheStorageManager> CacheStorageManager::Create(
    CacheStorageManager* old_manager) {
  std::unique_ptr<CacheStorageManager> manager(new CacheStorageManager(
      old_manager->root_path(), old_manager->cache_task_runner(),
      old_manager->quota_manager_proxy_.get()));
  // These values may be NULL, in which case this will be called again later by
  // the dispatcher host per usual.
  manager->SetBlobParametersForCache(old_manager->url_request_context_getter(),
                                     old_manager->blob_storage_context());
  return manager;
}

CacheStorageManager::~CacheStorageManager() = default;

void CacheStorageManager::OpenCache(
    const GURL& origin,
    const std::string& cache_name,
    const CacheStorage::CacheAndErrorCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  CacheStorage* cache_storage = FindOrCreateCacheStorage(origin);

  cache_storage->OpenCache(cache_name, callback);
}

void CacheStorageManager::HasCache(
    const GURL& origin,
    const std::string& cache_name,
    const CacheStorage::BoolAndErrorCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  CacheStorage* cache_storage = FindOrCreateCacheStorage(origin);
  cache_storage->HasCache(cache_name, callback);
}

void CacheStorageManager::DeleteCache(
    const GURL& origin,
    const std::string& cache_name,
    const CacheStorage::BoolAndErrorCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  CacheStorage* cache_storage = FindOrCreateCacheStorage(origin);
  cache_storage->DeleteCache(cache_name, callback);
}

void CacheStorageManager::EnumerateCaches(
    const GURL& origin,
    const CacheStorage::StringsAndErrorCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  CacheStorage* cache_storage = FindOrCreateCacheStorage(origin);

  cache_storage->EnumerateCaches(callback);
}

void CacheStorageManager::MatchCache(
    const GURL& origin,
    const std::string& cache_name,
    std::unique_ptr<ServiceWorkerFetchRequest> request,
    const CacheStorageCache::ResponseCallback& callback) {
  CacheStorage* cache_storage = FindOrCreateCacheStorage(origin);

  cache_storage->MatchCache(cache_name, std::move(request), callback);
}

void CacheStorageManager::MatchAllCaches(
    const GURL& origin,
    std::unique_ptr<ServiceWorkerFetchRequest> request,
    const CacheStorageCache::ResponseCallback& callback) {
  CacheStorage* cache_storage = FindOrCreateCacheStorage(origin);

  cache_storage->MatchAllCaches(std::move(request), callback);
}

void CacheStorageManager::SetBlobParametersForCache(
    scoped_refptr<net::URLRequestContextGetter> request_context_getter,
    base::WeakPtr<storage::BlobStorageContext> blob_storage_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(cache_storage_map_.empty());
  DCHECK(!request_context_getter_ ||
         request_context_getter_.get() == request_context_getter.get());
  DCHECK(!blob_context_ || blob_context_.get() == blob_storage_context.get());
  request_context_getter_ = std::move(request_context_getter);
  blob_context_ = blob_storage_context;
}

void CacheStorageManager::GetAllOriginsUsage(
    const CacheStorageContext::GetUsageInfoCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::unique_ptr<std::vector<CacheStorageUsageInfo>> usages(
      new std::vector<CacheStorageUsageInfo>());

  if (IsMemoryBacked()) {
    for (const auto& origin_details : cache_storage_map_) {
      usages->push_back(
          CacheStorageUsageInfo(origin_details.first, 0 /* size */,
                                base::Time() /* last modified */));
    }
    GetAllOriginsUsageGetSizes(std::move(usages), callback);
    return;
  }

  std::vector<CacheStorageUsageInfo>* usages_ptr = usages.get();
  cache_task_runner_->PostTaskAndReply(
      FROM_HERE, base::Bind(&ListOriginsAndLastModifiedOnTaskRunner, usages_ptr,
                            root_path_),
      base::Bind(&CacheStorageManager::GetAllOriginsUsageGetSizes,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(std::move(usages)), callback));
}

void CacheStorageManager::GetAllOriginsUsageGetSizes(
    std::unique_ptr<std::vector<CacheStorageUsageInfo>> usages,
    const CacheStorageContext::GetUsageInfoCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(usages);

  // The origin GURL and last modified times are set in |usages| but not the
  // size in bytes. Call each CacheStorage's Size() function to fill that out.
  std::vector<CacheStorageUsageInfo>* usages_ptr = usages.get();

  if (usages->empty()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, *usages));
    return;
  }

  base::Closure barrier_closure = base::BarrierClosure(
      usages_ptr->size(),
      base::Bind(&AllOriginSizesReported, base::Passed(std::move(usages)),
                 callback));

  for (CacheStorageUsageInfo& usage : *usages_ptr) {
    CacheStorage* cache_storage = FindOrCreateCacheStorage(usage.origin);
    cache_storage->Size(
        base::Bind(&OneOriginSizeReported, barrier_closure, &usage));
  }
}

void CacheStorageManager::GetOriginUsage(
    const GURL& origin_url,
    const storage::QuotaClient::GetUsageCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  CacheStorage* cache_storage = FindOrCreateCacheStorage(origin_url);

  cache_storage->Size(callback);
}

void CacheStorageManager::GetOrigins(
    const storage::QuotaClient::GetOriginsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (IsMemoryBacked()) {
    std::set<GURL> origins;
    for (const auto& key_value : cache_storage_map_)
      origins.insert(key_value.first);

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, origins));
    return;
  }

  PostTaskAndReplyWithResult(cache_task_runner_.get(), FROM_HERE,
                             base::Bind(&ListOriginsOnTaskRunner, root_path_),
                             base::Bind(callback));
}

void CacheStorageManager::GetOriginsForHost(
    const std::string& host,
    const storage::QuotaClient::GetOriginsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (IsMemoryBacked()) {
    std::set<GURL> origins;
    for (const auto& key_value : cache_storage_map_) {
      if (host == net::GetHostOrSpecFromURL(key_value.first))
        origins.insert(key_value.first);
    }
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, origins));
    return;
  }

  PostTaskAndReplyWithResult(
      cache_task_runner_.get(), FROM_HERE,
      base::Bind(&ListOriginsOnTaskRunner, root_path_),
      base::Bind(&GetOriginsForHostDidListOrigins, host, callback));
}

void CacheStorageManager::DeleteOriginData(
    const GURL& origin,
    const storage::QuotaClient::DeletionCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Create the CacheStorage for the origin if it hasn't been loaded yet.
  FindOrCreateCacheStorage(origin);

  CacheStorageMap::iterator it = cache_storage_map_.find(origin);
  DCHECK(it != cache_storage_map_.end());

  CacheStorage* cache_storage = it->second.release();
  cache_storage_map_.erase(origin);
  cache_storage->GetSizeThenCloseAllCaches(
      base::Bind(&CacheStorageManager::DeleteOriginDidClose,
                 weak_ptr_factory_.GetWeakPtr(), origin, callback,
                 base::Passed(base::WrapUnique(cache_storage))));
}

void CacheStorageManager::DeleteOriginData(const GURL& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DeleteOriginData(origin, base::Bind(&EmptyQuotaStatusCallback));
}

void CacheStorageManager::DeleteOriginDidClose(
    const GURL& origin,
    const storage::QuotaClient::DeletionCallback& callback,
    std::unique_ptr<CacheStorage> cache_storage,
    int64_t origin_size) {
  // TODO(jkarlin): Deleting the storage leaves any unfinished operations
  // hanging, resulting in unresolved promises. Fix this by returning early from
  // CacheStorage operations posted after GetSizeThenCloseAllCaches is called.
  cache_storage.reset();

  quota_manager_proxy_->NotifyStorageModified(
      storage::QuotaClient::kServiceWorkerCache, origin,
      storage::kStorageTypeTemporary, -1 * origin_size);

  if (IsMemoryBacked()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, storage::kQuotaStatusOk));
    return;
  }

  MigrateOrigin(origin);
  PostTaskAndReplyWithResult(
      cache_task_runner_.get(), FROM_HERE,
      base::Bind(&DeleteDir, ConstructOriginPath(root_path_, origin)),
      base::Bind(&DeleteOriginDidDeleteDir, callback));
}

CacheStorageManager::CacheStorageManager(
    const base::FilePath& path,
    scoped_refptr<base::SequencedTaskRunner> cache_task_runner,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy)
    : root_path_(path),
      cache_task_runner_(std::move(cache_task_runner)),
      quota_manager_proxy_(std::move(quota_manager_proxy)),
      weak_ptr_factory_(this) {
  if (quota_manager_proxy_.get()) {
    quota_manager_proxy_->RegisterClient(
        new CacheStorageQuotaClient(weak_ptr_factory_.GetWeakPtr()));
  }
}

CacheStorage* CacheStorageManager::FindOrCreateCacheStorage(
    const GURL& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(request_context_getter_);
  CacheStorageMap::const_iterator it = cache_storage_map_.find(origin);
  if (it == cache_storage_map_.end()) {
    MigrateOrigin(origin);
    CacheStorage* cache_storage = new CacheStorage(
        ConstructOriginPath(root_path_, origin), IsMemoryBacked(),
        cache_task_runner_.get(), request_context_getter_, quota_manager_proxy_,
        blob_context_, origin);
    cache_storage_map_.insert(
        std::make_pair(origin, base::WrapUnique(cache_storage)));
    return cache_storage;
  }
  return it->second.get();
}

// static
base::FilePath CacheStorageManager::ConstructLegacyOriginPath(
    const base::FilePath& root_path,
    const GURL& origin) {
  const std::string origin_hash = base::SHA1HashString(origin.spec());
  const std::string origin_hash_hex = base::ToLowerASCII(
      base::HexEncode(origin_hash.c_str(), origin_hash.length()));
  return root_path.AppendASCII(origin_hash_hex);
}

// static
base::FilePath CacheStorageManager::ConstructOriginPath(
    const base::FilePath& root_path,
    const GURL& origin) {
  const std::string identifier = storage::GetIdentifierFromOrigin(origin);
  const std::string origin_hash = base::SHA1HashString(identifier);
  const std::string origin_hash_hex = base::ToLowerASCII(
      base::HexEncode(origin_hash.c_str(), origin_hash.length()));
  return root_path.AppendASCII(origin_hash_hex);
}

// Migrate from old origin-based path to storage identifier-based path.
// TODO(jsbell); Remove after a few releases.
void CacheStorageManager::MigrateOrigin(const GURL& origin) {
  if (IsMemoryBacked())
    return;
  base::FilePath old_path = ConstructLegacyOriginPath(root_path_, origin);
  base::FilePath new_path = ConstructOriginPath(root_path_, origin);
  cache_task_runner_->PostTask(
      FROM_HERE, base::Bind(&MigrateOriginOnTaskRunner, old_path, new_path));
}

// static
void CacheStorageManager::MigrateOriginOnTaskRunner(
    const base::FilePath& old_path,
    const base::FilePath& new_path) {
  if (base::PathExists(old_path)) {
    if (!base::PathExists(new_path))
      base::Move(old_path, new_path);
    base::DeleteFile(old_path, /*recursive*/ true);
  }
}

}  // namespace content
