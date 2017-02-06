// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_dispatcher_host.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/bad_message.h"
#include "content/browser/cache_storage/cache_storage_cache.h"
#include "content/browser/cache_storage/cache_storage_cache_handle.h"
#include "content/browser/cache_storage/cache_storage_context_impl.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/common/cache_storage/cache_storage_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/origin_util.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerCacheError.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

const uint32_t kFilteredMessageClasses[] = {CacheStorageMsgStart};
const int32_t kCachePreservationSeconds = 5;

blink::WebServiceWorkerCacheError ToWebServiceWorkerCacheError(
    CacheStorageError err) {
  switch (err) {
    case CACHE_STORAGE_OK:
      NOTREACHED();
      return blink::WebServiceWorkerCacheErrorNotImplemented;
    case CACHE_STORAGE_ERROR_EXISTS:
      return blink::WebServiceWorkerCacheErrorExists;
    case CACHE_STORAGE_ERROR_STORAGE:
      // TODO(nhiroki): Add WebServiceWorkerCacheError equivalent to
      // CACHE_STORAGE_ERROR_STORAGE.
      return blink::WebServiceWorkerCacheErrorNotFound;
    case CACHE_STORAGE_ERROR_NOT_FOUND:
      return blink::WebServiceWorkerCacheErrorNotFound;
    case CACHE_STORAGE_ERROR_QUOTA_EXCEEDED:
      return blink::WebServiceWorkerCacheErrorQuotaExceeded;
    case CACHE_STORAGE_ERROR_CACHE_NAME_NOT_FOUND:
      return blink::WebServiceWorkerCacheErrorCacheNameNotFound;
  }
  NOTREACHED();
  return blink::WebServiceWorkerCacheErrorNotImplemented;
}

bool OriginCanAccessCacheStorage(const url::Origin& origin) {
  return !origin.unique() && IsOriginSecure(GURL(origin.Serialize()));
}

void StopPreservingCache(
    std::unique_ptr<CacheStorageCacheHandle> cache_handle) {}

}  // namespace

CacheStorageDispatcherHost::CacheStorageDispatcherHost()
    : BrowserMessageFilter(kFilteredMessageClasses,
                           arraysize(kFilteredMessageClasses)) {}

CacheStorageDispatcherHost::~CacheStorageDispatcherHost() {
}

void CacheStorageDispatcherHost::Init(CacheStorageContextImpl* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&CacheStorageDispatcherHost::CreateCacheListener, this,
                 base::RetainedRef(context)));
}

void CacheStorageDispatcherHost::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

bool CacheStorageDispatcherHost::OnMessageReceived(
    const IPC::Message& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CacheStorageDispatcherHost, message)
  IPC_MESSAGE_HANDLER(CacheStorageHostMsg_CacheStorageHas, OnCacheStorageHas)
  IPC_MESSAGE_HANDLER(CacheStorageHostMsg_CacheStorageOpen, OnCacheStorageOpen)
  IPC_MESSAGE_HANDLER(CacheStorageHostMsg_CacheStorageDelete,
                      OnCacheStorageDelete)
  IPC_MESSAGE_HANDLER(CacheStorageHostMsg_CacheStorageKeys, OnCacheStorageKeys)
  IPC_MESSAGE_HANDLER(CacheStorageHostMsg_CacheStorageMatch,
                      OnCacheStorageMatch)
  IPC_MESSAGE_HANDLER(CacheStorageHostMsg_CacheMatch, OnCacheMatch)
  IPC_MESSAGE_HANDLER(CacheStorageHostMsg_CacheMatchAll, OnCacheMatchAll)
  IPC_MESSAGE_HANDLER(CacheStorageHostMsg_CacheKeys, OnCacheKeys)
  IPC_MESSAGE_HANDLER(CacheStorageHostMsg_CacheBatch, OnCacheBatch)
  IPC_MESSAGE_HANDLER(CacheStorageHostMsg_CacheClosed, OnCacheClosed)
  IPC_MESSAGE_HANDLER(CacheStorageHostMsg_BlobDataHandled, OnBlobDataHandled)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  if (!handled)
    bad_message::ReceivedBadMessage(this, bad_message::CSDH_NOT_RECOGNIZED);
  return handled;
}

void CacheStorageDispatcherHost::CreateCacheListener(
    CacheStorageContextImpl* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  context_ = context;
}

void CacheStorageDispatcherHost::OnCacheStorageHas(
    int thread_id,
    int request_id,
    const url::Origin& origin,
    const base::string16& cache_name) {
  TRACE_EVENT0("CacheStorage", "CacheStorageDispatcherHost::OnCacheStorageHas");
  if (!OriginCanAccessCacheStorage(origin)) {
    bad_message::ReceivedBadMessage(this, bad_message::CSDH_INVALID_ORIGIN);
    return;
  }
  context_->cache_manager()->HasCache(
      GURL(origin.Serialize()), base::UTF16ToUTF8(cache_name),
      base::Bind(&CacheStorageDispatcherHost::OnCacheStorageHasCallback, this,
                 thread_id, request_id));
}

void CacheStorageDispatcherHost::OnCacheStorageOpen(
    int thread_id,
    int request_id,
    const url::Origin& origin,
    const base::string16& cache_name) {
  TRACE_EVENT0("CacheStorage",
               "CacheStorageDispatcherHost::OnCacheStorageOpen");
  if (!OriginCanAccessCacheStorage(origin)) {
    bad_message::ReceivedBadMessage(this, bad_message::CSDH_INVALID_ORIGIN);
    return;
  }
  context_->cache_manager()->OpenCache(
      GURL(origin.Serialize()), base::UTF16ToUTF8(cache_name),
      base::Bind(&CacheStorageDispatcherHost::OnCacheStorageOpenCallback, this,
                 thread_id, request_id));
}

void CacheStorageDispatcherHost::OnCacheStorageDelete(
    int thread_id,
    int request_id,
    const url::Origin& origin,
    const base::string16& cache_name) {
  TRACE_EVENT0("CacheStorage",
               "CacheStorageDispatcherHost::OnCacheStorageDelete");
  if (!OriginCanAccessCacheStorage(origin)) {
    bad_message::ReceivedBadMessage(this, bad_message::CSDH_INVALID_ORIGIN);
    return;
  }
  context_->cache_manager()->DeleteCache(
      GURL(origin.Serialize()), base::UTF16ToUTF8(cache_name),
      base::Bind(&CacheStorageDispatcherHost::OnCacheStorageDeleteCallback,
                 this, thread_id, request_id));
}

void CacheStorageDispatcherHost::OnCacheStorageKeys(int thread_id,
                                                    int request_id,
                                                    const url::Origin& origin) {
  TRACE_EVENT0("CacheStorage",
               "CacheStorageDispatcherHost::OnCacheStorageKeys");
  if (!OriginCanAccessCacheStorage(origin)) {
    bad_message::ReceivedBadMessage(this, bad_message::CSDH_INVALID_ORIGIN);
    return;
  }
  context_->cache_manager()->EnumerateCaches(
      GURL(origin.Serialize()),
      base::Bind(&CacheStorageDispatcherHost::OnCacheStorageKeysCallback, this,
                 thread_id, request_id));
}

void CacheStorageDispatcherHost::OnCacheStorageMatch(
    int thread_id,
    int request_id,
    const url::Origin& origin,
    const ServiceWorkerFetchRequest& request,
    const CacheStorageCacheQueryParams& match_params) {
  TRACE_EVENT0("CacheStorage",
               "CacheStorageDispatcherHost::OnCacheStorageMatch");
  if (!OriginCanAccessCacheStorage(origin)) {
    bad_message::ReceivedBadMessage(this, bad_message::CSDH_INVALID_ORIGIN);
    return;
  }
  std::unique_ptr<ServiceWorkerFetchRequest> scoped_request(
      new ServiceWorkerFetchRequest(request.url, request.method,
                                    request.headers, request.referrer,
                                    request.is_reload));

  if (match_params.cache_name.empty()) {
    context_->cache_manager()->MatchAllCaches(
        GURL(origin.Serialize()), std::move(scoped_request),
        base::Bind(&CacheStorageDispatcherHost::OnCacheStorageMatchCallback,
                   this, thread_id, request_id));
    return;
  }
  context_->cache_manager()->MatchCache(
      GURL(origin.Serialize()), base::UTF16ToUTF8(match_params.cache_name),
      std::move(scoped_request),
      base::Bind(&CacheStorageDispatcherHost::OnCacheStorageMatchCallback, this,
                 thread_id, request_id));
}

void CacheStorageDispatcherHost::OnCacheMatch(
    int thread_id,
    int request_id,
    int cache_id,
    const ServiceWorkerFetchRequest& request,
    const CacheStorageCacheQueryParams& match_params) {
  IDToCacheMap::iterator it = id_to_cache_map_.find(cache_id);
  if (it == id_to_cache_map_.end() || !it->second->value()) {
    Send(new CacheStorageMsg_CacheMatchError(
        thread_id, request_id, blink::WebServiceWorkerCacheErrorNotFound));
    return;
  }

  CacheStorageCache* cache = it->second->value();
  std::unique_ptr<ServiceWorkerFetchRequest> scoped_request(
      new ServiceWorkerFetchRequest(request.url, request.method,
                                    request.headers, request.referrer,
                                    request.is_reload));
  cache->Match(
      std::move(scoped_request),
      base::Bind(&CacheStorageDispatcherHost::OnCacheMatchCallback, this,
                 thread_id, request_id, base::Passed(it->second->Clone())));
}

void CacheStorageDispatcherHost::OnCacheMatchAll(
    int thread_id,
    int request_id,
    int cache_id,
    const ServiceWorkerFetchRequest& request,
    const CacheStorageCacheQueryParams& match_params) {
  IDToCacheMap::iterator it = id_to_cache_map_.find(cache_id);
  if (it == id_to_cache_map_.end() || !it->second->value()) {
    Send(new CacheStorageMsg_CacheMatchError(
        thread_id, request_id, blink::WebServiceWorkerCacheErrorNotFound));
    return;
  }

  CacheStorageCache* cache = it->second->value();
  if (request.url.is_empty()) {
    cache->MatchAll(
        std::unique_ptr<ServiceWorkerFetchRequest>(), match_params,
        base::Bind(&CacheStorageDispatcherHost::OnCacheMatchAllCallback, this,
                   thread_id, request_id, base::Passed(it->second->Clone())));
    return;
  }

  std::unique_ptr<ServiceWorkerFetchRequest> scoped_request(
      new ServiceWorkerFetchRequest(request.url, request.method,
                                    request.headers, request.referrer,
                                    request.is_reload));
  if (match_params.ignore_search) {
    cache->MatchAll(
        std::move(scoped_request), match_params,
        base::Bind(&CacheStorageDispatcherHost::OnCacheMatchAllCallback, this,
                   thread_id, request_id, base::Passed(it->second->Clone())));
    return;
  }
  cache->Match(
      std::move(scoped_request),
      base::Bind(&CacheStorageDispatcherHost::OnCacheMatchAllCallbackAdapter,
                 this, thread_id, request_id,
                 base::Passed(it->second->Clone())));
}

void CacheStorageDispatcherHost::OnCacheKeys(
    int thread_id,
    int request_id,
    int cache_id,
    const ServiceWorkerFetchRequest& request,
    const CacheStorageCacheQueryParams& match_params) {
  IDToCacheMap::iterator it = id_to_cache_map_.find(cache_id);
  if (it == id_to_cache_map_.end() || !it->second->value()) {
    Send(new CacheStorageMsg_CacheKeysError(
        thread_id, request_id, blink::WebServiceWorkerCacheErrorNotFound));
    return;
  }

  CacheStorageCache* cache = it->second->value();
  cache->Keys(base::Bind(&CacheStorageDispatcherHost::OnCacheKeysCallback, this,
                         thread_id, request_id,
                         base::Passed(it->second->Clone())));
}

void CacheStorageDispatcherHost::OnCacheBatch(
    int thread_id,
    int request_id,
    int cache_id,
    const std::vector<CacheStorageBatchOperation>& operations) {
  IDToCacheMap::iterator it = id_to_cache_map_.find(cache_id);
  if (it == id_to_cache_map_.end() || !it->second->value()) {
    Send(new CacheStorageMsg_CacheBatchError(
        thread_id, request_id, blink::WebServiceWorkerCacheErrorNotFound));
    return;
  }

  CacheStorageCache* cache = it->second->value();
  cache->BatchOperation(
      operations,
      base::Bind(&CacheStorageDispatcherHost::OnCacheBatchCallback, this,
                 thread_id, request_id, base::Passed(it->second->Clone())));
}

void CacheStorageDispatcherHost::OnCacheClosed(int cache_id) {
  DropCacheReference(cache_id);
}

void CacheStorageDispatcherHost::OnBlobDataHandled(const std::string& uuid) {
  DropBlobDataHandle(uuid);
}

void CacheStorageDispatcherHost::OnCacheStorageHasCallback(
    int thread_id,
    int request_id,
    bool has_cache,
    CacheStorageError error) {
  if (error != CACHE_STORAGE_OK) {
    Send(new CacheStorageMsg_CacheStorageHasError(
        thread_id, request_id, ToWebServiceWorkerCacheError(error)));
    return;
  }
  if (!has_cache) {
    Send(new CacheStorageMsg_CacheStorageHasError(
        thread_id, request_id, blink::WebServiceWorkerCacheErrorNotFound));
    return;
  }
  Send(new CacheStorageMsg_CacheStorageHasSuccess(thread_id, request_id));
}

void CacheStorageDispatcherHost::OnCacheStorageOpenCallback(
    int thread_id,
    int request_id,
    std::unique_ptr<CacheStorageCacheHandle> cache_handle,
    CacheStorageError error) {
  if (error != CACHE_STORAGE_OK) {
    Send(new CacheStorageMsg_CacheStorageOpenError(
        thread_id, request_id, ToWebServiceWorkerCacheError(error)));
    return;
  }

  // Hang on to the cache for a few seconds. This way if the user quickly closes
  // and reopens it the cache backend won't have to be reinitialized.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&StopPreservingCache, base::Passed(cache_handle->Clone())),
      base::TimeDelta::FromSeconds(kCachePreservationSeconds));

  CacheID cache_id = StoreCacheReference(std::move(cache_handle));
  Send(new CacheStorageMsg_CacheStorageOpenSuccess(thread_id, request_id,
                                                   cache_id));
}

void CacheStorageDispatcherHost::OnCacheStorageDeleteCallback(
    int thread_id,
    int request_id,
    bool deleted,
    CacheStorageError error) {
  if (!deleted || error != CACHE_STORAGE_OK) {
    Send(new CacheStorageMsg_CacheStorageDeleteError(
        thread_id, request_id, ToWebServiceWorkerCacheError(error)));
    return;
  }
  Send(new CacheStorageMsg_CacheStorageDeleteSuccess(thread_id, request_id));
}

void CacheStorageDispatcherHost::OnCacheStorageKeysCallback(
    int thread_id,
    int request_id,
    const std::vector<std::string>& strings,
    CacheStorageError error) {
  if (error != CACHE_STORAGE_OK) {
    Send(new CacheStorageMsg_CacheStorageKeysError(
        thread_id, request_id, ToWebServiceWorkerCacheError(error)));
    return;
  }

  std::vector<base::string16> string16s;
  for (size_t i = 0, max = strings.size(); i < max; ++i) {
    string16s.push_back(base::UTF8ToUTF16(strings[i]));
  }
  Send(new CacheStorageMsg_CacheStorageKeysSuccess(thread_id, request_id,
                                                   string16s));
}

void CacheStorageDispatcherHost::OnCacheStorageMatchCallback(
    int thread_id,
    int request_id,
    CacheStorageError error,
    std::unique_ptr<ServiceWorkerResponse> response,
    std::unique_ptr<storage::BlobDataHandle> blob_data_handle) {
  if (error != CACHE_STORAGE_OK) {
    Send(new CacheStorageMsg_CacheStorageMatchError(
        thread_id, request_id, ToWebServiceWorkerCacheError(error)));
    return;
  }

  if (blob_data_handle)
    StoreBlobDataHandle(*blob_data_handle);

  Send(new CacheStorageMsg_CacheStorageMatchSuccess(thread_id, request_id,
                                                    *response));
}

void CacheStorageDispatcherHost::OnCacheMatchCallback(
    int thread_id,
    int request_id,
    std::unique_ptr<CacheStorageCacheHandle> cache_handle,
    CacheStorageError error,
    std::unique_ptr<ServiceWorkerResponse> response,
    std::unique_ptr<storage::BlobDataHandle> blob_data_handle) {
  if (error != CACHE_STORAGE_OK) {
    Send(new CacheStorageMsg_CacheMatchError(
        thread_id, request_id, ToWebServiceWorkerCacheError(error)));
    return;
  }

  if (blob_data_handle)
    StoreBlobDataHandle(*blob_data_handle);

  Send(new CacheStorageMsg_CacheMatchSuccess(thread_id, request_id, *response));
}

void CacheStorageDispatcherHost::OnCacheMatchAllCallbackAdapter(
    int thread_id,
    int request_id,
    std::unique_ptr<CacheStorageCacheHandle> cache_handle,
    CacheStorageError error,
    std::unique_ptr<ServiceWorkerResponse> response,
    std::unique_ptr<storage::BlobDataHandle> blob_data_handle) {
  std::unique_ptr<CacheStorageCache::Responses> responses(
      new CacheStorageCache::Responses);
  std::unique_ptr<CacheStorageCache::BlobDataHandles> blob_data_handles(
      new CacheStorageCache::BlobDataHandles);
  if (error == CACHE_STORAGE_OK) {
    DCHECK(response);
    responses->push_back(*response);
    if (blob_data_handle)
      blob_data_handles->push_back(*blob_data_handle);
  }
  OnCacheMatchAllCallback(thread_id, request_id, std::move(cache_handle), error,
                          std::move(responses), std::move(blob_data_handles));
}

void CacheStorageDispatcherHost::OnCacheMatchAllCallback(
    int thread_id,
    int request_id,
    std::unique_ptr<CacheStorageCacheHandle> cache_handle,
    CacheStorageError error,
    std::unique_ptr<CacheStorageCache::Responses> responses,
    std::unique_ptr<CacheStorageCache::BlobDataHandles> blob_data_handles) {
  if (error != CACHE_STORAGE_OK && error != CACHE_STORAGE_ERROR_NOT_FOUND) {
    Send(new CacheStorageMsg_CacheMatchAllError(
        thread_id, request_id, ToWebServiceWorkerCacheError(error)));
    return;
  }

  for (const storage::BlobDataHandle& handle : *blob_data_handles)
    StoreBlobDataHandle(handle);

  Send(new CacheStorageMsg_CacheMatchAllSuccess(thread_id, request_id,
                                                *responses));
}

void CacheStorageDispatcherHost::OnCacheKeysCallback(
    int thread_id,
    int request_id,
    std::unique_ptr<CacheStorageCacheHandle> cache_handle,
    CacheStorageError error,
    std::unique_ptr<CacheStorageCache::Requests> requests) {
  if (error != CACHE_STORAGE_OK) {
    Send(new CacheStorageMsg_CacheKeysError(
        thread_id, request_id, ToWebServiceWorkerCacheError(error)));
    return;
  }

  CacheStorageCache::Requests out;

  for (CacheStorageCache::Requests::const_iterator it = requests->begin();
       it != requests->end(); ++it) {
    ServiceWorkerFetchRequest request(it->url, it->method, it->headers,
                                      it->referrer, it->is_reload);
    out.push_back(request);
  }

  Send(new CacheStorageMsg_CacheKeysSuccess(thread_id, request_id, out));
}

void CacheStorageDispatcherHost::OnCacheBatchCallback(
    int thread_id,
    int request_id,
    std::unique_ptr<CacheStorageCacheHandle> cache_handle,
    CacheStorageError error) {
  if (error != CACHE_STORAGE_OK) {
    Send(new CacheStorageMsg_CacheBatchError(
        thread_id, request_id, ToWebServiceWorkerCacheError(error)));
    return;
  }

  Send(new CacheStorageMsg_CacheBatchSuccess(thread_id, request_id));
}

CacheStorageDispatcherHost::CacheID
CacheStorageDispatcherHost::StoreCacheReference(
    std::unique_ptr<CacheStorageCacheHandle> cache_handle) {
  int cache_id = next_cache_id_++;
  id_to_cache_map_[cache_id] = std::move(cache_handle);
  return cache_id;
}

void CacheStorageDispatcherHost::DropCacheReference(CacheID cache_id) {
  id_to_cache_map_.erase(cache_id);
}

void CacheStorageDispatcherHost::StoreBlobDataHandle(
    const storage::BlobDataHandle& blob_data_handle) {
  std::pair<UUIDToBlobDataHandleList::iterator, bool> rv =
      blob_handle_store_.insert(std::make_pair(
          blob_data_handle.uuid(), std::list<storage::BlobDataHandle>()));
  rv.first->second.push_front(storage::BlobDataHandle(blob_data_handle));
}

void CacheStorageDispatcherHost::DropBlobDataHandle(const std::string& uuid) {
  UUIDToBlobDataHandleList::iterator it = blob_handle_store_.find(uuid);
  if (it == blob_handle_store_.end())
    return;
  DCHECK(!it->second.empty());
  it->second.pop_front();
  if (it->second.empty())
    blob_handle_store_.erase(it);
}

}  // namespace content
