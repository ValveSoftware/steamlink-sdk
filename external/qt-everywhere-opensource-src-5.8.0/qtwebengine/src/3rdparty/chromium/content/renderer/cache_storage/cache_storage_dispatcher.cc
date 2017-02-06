// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/cache_storage/cache_storage_dispatcher.h"

#include <stddef.h>

#include <map>
#include <string>
#include <utility>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_local.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/cache_storage/cache_storage_messages.h"
#include "content/public/common/referrer.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/service_worker/service_worker_type_util.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerCache.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerRequest.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerResponse.h"
#include "url/origin.h"

using base::TimeTicks;

namespace content {

using blink::WebServiceWorkerCacheStorage;
using blink::WebServiceWorkerCacheError;
using blink::WebServiceWorkerRequest;

static base::LazyInstance<base::ThreadLocalPointer<CacheStorageDispatcher>>::
    Leaky g_cache_storage_dispatcher_tls = LAZY_INSTANCE_INITIALIZER;

namespace {

CacheStorageDispatcher* const kHasBeenDeleted =
    reinterpret_cast<CacheStorageDispatcher*>(0x1);

ServiceWorkerFetchRequest FetchRequestFromWebRequest(
    const blink::WebServiceWorkerRequest& web_request) {
  ServiceWorkerHeaderMap headers;
  GetServiceWorkerHeaderMapFromWebRequest(web_request, &headers);

  return ServiceWorkerFetchRequest(
      web_request.url(),
      base::UTF16ToASCII(base::StringPiece16(web_request.method())), headers,
      Referrer(web_request.referrerUrl(), web_request.referrerPolicy()),
      web_request.isReload());
}

void PopulateWebRequestFromFetchRequest(
    const ServiceWorkerFetchRequest& request,
    blink::WebServiceWorkerRequest* web_request) {
  web_request->setURL(request.url);
  web_request->setMethod(base::ASCIIToUTF16(request.method));
  for (ServiceWorkerHeaderMap::const_iterator i = request.headers.begin(),
                                              end = request.headers.end();
       i != end; ++i) {
    web_request->setHeader(base::ASCIIToUTF16(i->first),
                           base::ASCIIToUTF16(i->second));
  }
  web_request->setReferrer(base::ASCIIToUTF16(request.referrer.url.spec()),
                           request.referrer.policy);
  web_request->setIsReload(request.is_reload);
}

blink::WebVector<blink::WebServiceWorkerRequest> WebRequestsFromRequests(
    const std::vector<ServiceWorkerFetchRequest>& requests) {
  blink::WebVector<blink::WebServiceWorkerRequest> web_requests(
      requests.size());
  for (size_t i = 0; i < requests.size(); ++i)
    PopulateWebRequestFromFetchRequest(requests[i], &(web_requests[i]));
  return web_requests;
}

ServiceWorkerResponse ResponseFromWebResponse(
    const blink::WebServiceWorkerResponse& web_response) {
  ServiceWorkerHeaderMap headers;
  GetServiceWorkerHeaderMapFromWebResponse(web_response, &headers);
  ServiceWorkerHeaderList cors_exposed_header_names;
  GetCorsExposedHeaderNamesFromWebResponse(web_response,
                                           &cors_exposed_header_names);
  // We don't support streaming for cache.
  DCHECK(web_response.streamURL().isEmpty());
  return ServiceWorkerResponse(
      web_response.url(), web_response.status(),
      base::UTF16ToASCII(base::StringPiece16(web_response.statusText())),
      web_response.responseType(), headers,
      base::UTF16ToASCII(base::StringPiece16(web_response.blobUUID())),
      web_response.blobSize(), web_response.streamURL(),
      blink::WebServiceWorkerResponseErrorUnknown,
      base::Time::FromInternalValue(web_response.responseTime()),
      !web_response.cacheStorageCacheName().isNull(),
      web_response.cacheStorageCacheName().utf8(), cors_exposed_header_names);
}

CacheStorageCacheQueryParams QueryParamsFromWebQueryParams(
    const blink::WebServiceWorkerCache::QueryParams& web_query_params) {
  CacheStorageCacheQueryParams query_params;
  query_params.ignore_search = web_query_params.ignoreSearch;
  query_params.ignore_method = web_query_params.ignoreMethod;
  query_params.ignore_vary = web_query_params.ignoreVary;
  query_params.cache_name = web_query_params.cacheName;
  return query_params;
}

CacheStorageCacheOperationType CacheOperationTypeFromWebCacheOperationType(
    blink::WebServiceWorkerCache::OperationType operation_type) {
  switch (operation_type) {
    case blink::WebServiceWorkerCache::OperationTypePut:
      return CACHE_STORAGE_CACHE_OPERATION_TYPE_PUT;
    case blink::WebServiceWorkerCache::OperationTypeDelete:
      return CACHE_STORAGE_CACHE_OPERATION_TYPE_DELETE;
    default:
      return CACHE_STORAGE_CACHE_OPERATION_TYPE_UNDEFINED;
  }
}

CacheStorageBatchOperation BatchOperationFromWebBatchOperation(
    const blink::WebServiceWorkerCache::BatchOperation& web_operation) {
  CacheStorageBatchOperation operation;
  operation.operation_type =
      CacheOperationTypeFromWebCacheOperationType(web_operation.operationType);
  operation.request = FetchRequestFromWebRequest(web_operation.request);
  operation.response = ResponseFromWebResponse(web_operation.response);
  operation.match_params =
      QueryParamsFromWebQueryParams(web_operation.matchParams);
  return operation;
}

template <typename T>
void ClearCallbacksMapWithErrors(T* callbacks_map) {
  typename T::iterator iter(callbacks_map);
  while (!iter.IsAtEnd()) {
    iter.GetCurrentValue()->onError(blink::WebServiceWorkerCacheErrorNotFound);
    callbacks_map->Remove(iter.GetCurrentKey());
    iter.Advance();
  }
}

}  // namespace

// The WebCache object is the Chromium side implementation of the Blink
// WebServiceWorkerCache API. Most of its methods delegate directly to the
// ServiceWorkerStorage object, which is able to assign unique IDs as well
// as have a lifetime longer than the requests.
class CacheStorageDispatcher::WebCache : public blink::WebServiceWorkerCache {
 public:
  WebCache(base::WeakPtr<CacheStorageDispatcher> dispatcher, int cache_id)
      : dispatcher_(dispatcher), cache_id_(cache_id) {}

  ~WebCache() override {
    if (dispatcher_)
      dispatcher_->OnWebCacheDestruction(cache_id_);
  }

  // From blink::WebServiceWorkerCache:
  void dispatchMatch(CacheMatchCallbacks* callbacks,
                     const blink::WebServiceWorkerRequest& request,
                     const QueryParams& query_params) override {
    if (!dispatcher_)
      return;
    dispatcher_->dispatchMatchForCache(cache_id_, callbacks, request,
                                       query_params);
  }
  void dispatchMatchAll(CacheWithResponsesCallbacks* callbacks,
                        const blink::WebServiceWorkerRequest& request,
                        const QueryParams& query_params) override {
    if (!dispatcher_)
      return;
    dispatcher_->dispatchMatchAllForCache(cache_id_, callbacks, request,
                                          query_params);
  }
  void dispatchKeys(CacheWithRequestsCallbacks* callbacks,
                    const blink::WebServiceWorkerRequest* request,
                    const QueryParams& query_params) override {
    if (!dispatcher_)
      return;
    dispatcher_->dispatchKeysForCache(cache_id_, callbacks, request,
                                      query_params);
  }
  void dispatchBatch(
      CacheBatchCallbacks* callbacks,
      const blink::WebVector<BatchOperation>& batch_operations) override {
    if (!dispatcher_)
      return;
    dispatcher_->dispatchBatchForCache(cache_id_, callbacks, batch_operations);
  }

 private:
  const base::WeakPtr<CacheStorageDispatcher> dispatcher_;
  const int cache_id_;
};

CacheStorageDispatcher::CacheStorageDispatcher(
    ThreadSafeSender* thread_safe_sender)
    : thread_safe_sender_(thread_safe_sender), weak_factory_(this) {
  g_cache_storage_dispatcher_tls.Pointer()->Set(this);
}

CacheStorageDispatcher::~CacheStorageDispatcher() {
  ClearCallbacksMapWithErrors(&has_callbacks_);
  ClearCallbacksMapWithErrors(&open_callbacks_);
  ClearCallbacksMapWithErrors(&delete_callbacks_);
  ClearCallbacksMapWithErrors(&keys_callbacks_);
  ClearCallbacksMapWithErrors(&match_callbacks_);

  ClearCallbacksMapWithErrors(&cache_match_callbacks_);
  ClearCallbacksMapWithErrors(&cache_match_all_callbacks_);
  ClearCallbacksMapWithErrors(&cache_keys_callbacks_);
  ClearCallbacksMapWithErrors(&cache_batch_callbacks_);

  g_cache_storage_dispatcher_tls.Pointer()->Set(kHasBeenDeleted);
}

CacheStorageDispatcher* CacheStorageDispatcher::ThreadSpecificInstance(
    ThreadSafeSender* thread_safe_sender) {
  if (g_cache_storage_dispatcher_tls.Pointer()->Get() == kHasBeenDeleted) {
    NOTREACHED() << "Re-instantiating TLS CacheStorageDispatcher.";
    g_cache_storage_dispatcher_tls.Pointer()->Set(NULL);
  }
  if (g_cache_storage_dispatcher_tls.Pointer()->Get())
    return g_cache_storage_dispatcher_tls.Pointer()->Get();

  CacheStorageDispatcher* dispatcher =
      new CacheStorageDispatcher(thread_safe_sender);
  if (WorkerThread::GetCurrentId())
    WorkerThread::AddObserver(dispatcher);
  return dispatcher;
}

void CacheStorageDispatcher::WillStopCurrentWorkerThread() {
  delete this;
}

bool CacheStorageDispatcher::Send(IPC::Message* msg) {
  return thread_safe_sender_->Send(msg);
}

bool CacheStorageDispatcher::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CacheStorageDispatcher, message)
      IPC_MESSAGE_HANDLER(CacheStorageMsg_CacheStorageHasSuccess,
                          OnCacheStorageHasSuccess)
      IPC_MESSAGE_HANDLER(CacheStorageMsg_CacheStorageOpenSuccess,
                          OnCacheStorageOpenSuccess)
      IPC_MESSAGE_HANDLER(CacheStorageMsg_CacheStorageDeleteSuccess,
                          OnCacheStorageDeleteSuccess)
      IPC_MESSAGE_HANDLER(CacheStorageMsg_CacheStorageKeysSuccess,
                          OnCacheStorageKeysSuccess)
      IPC_MESSAGE_HANDLER(CacheStorageMsg_CacheStorageMatchSuccess,
                          OnCacheStorageMatchSuccess)
      IPC_MESSAGE_HANDLER(CacheStorageMsg_CacheStorageHasError,
                          OnCacheStorageHasError)
      IPC_MESSAGE_HANDLER(CacheStorageMsg_CacheStorageOpenError,
                          OnCacheStorageOpenError)
      IPC_MESSAGE_HANDLER(CacheStorageMsg_CacheStorageDeleteError,
                          OnCacheStorageDeleteError)
      IPC_MESSAGE_HANDLER(CacheStorageMsg_CacheStorageKeysError,
                          OnCacheStorageKeysError)
      IPC_MESSAGE_HANDLER(CacheStorageMsg_CacheStorageMatchError,
                          OnCacheStorageMatchError)
      IPC_MESSAGE_HANDLER(CacheStorageMsg_CacheMatchSuccess,
                          OnCacheMatchSuccess)
      IPC_MESSAGE_HANDLER(CacheStorageMsg_CacheMatchAllSuccess,
                          OnCacheMatchAllSuccess)
      IPC_MESSAGE_HANDLER(CacheStorageMsg_CacheKeysSuccess, OnCacheKeysSuccess)
      IPC_MESSAGE_HANDLER(CacheStorageMsg_CacheBatchSuccess,
                          OnCacheBatchSuccess)
      IPC_MESSAGE_HANDLER(CacheStorageMsg_CacheMatchError, OnCacheMatchError)
      IPC_MESSAGE_HANDLER(CacheStorageMsg_CacheMatchAllError,
                          OnCacheMatchAllError)
      IPC_MESSAGE_HANDLER(CacheStorageMsg_CacheKeysError, OnCacheKeysError)
      IPC_MESSAGE_HANDLER(CacheStorageMsg_CacheBatchError, OnCacheBatchError)
      IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void CacheStorageDispatcher::OnCacheStorageHasSuccess(int thread_id,
                                                      int request_id) {
  DCHECK_EQ(thread_id, CurrentWorkerId());
  UMA_HISTOGRAM_TIMES("ServiceWorkerCache.CacheStorage.Has",
                      TimeTicks::Now() - has_times_[request_id]);
  WebServiceWorkerCacheStorage::CacheStorageCallbacks* callbacks =
      has_callbacks_.Lookup(request_id);
  callbacks->onSuccess();
  has_callbacks_.Remove(request_id);
  has_times_.erase(request_id);
}

void CacheStorageDispatcher::OnCacheStorageOpenSuccess(int thread_id,
                                                       int request_id,
                                                       int cache_id) {
  DCHECK_EQ(thread_id, CurrentWorkerId());
  std::unique_ptr<WebCache> web_cache(
      new WebCache(weak_factory_.GetWeakPtr(), cache_id));
  web_caches_.AddWithID(web_cache.get(), cache_id);
  UMA_HISTOGRAM_TIMES("ServiceWorkerCache.CacheStorage.Open",
                      TimeTicks::Now() - open_times_[request_id]);
  WebServiceWorkerCacheStorage::CacheStorageWithCacheCallbacks* callbacks =
      open_callbacks_.Lookup(request_id);
  callbacks->onSuccess(std::move(web_cache));
  open_callbacks_.Remove(request_id);
  open_times_.erase(request_id);
}

void CacheStorageDispatcher::OnCacheStorageDeleteSuccess(int thread_id,
                                                         int request_id) {
  DCHECK_EQ(thread_id, CurrentWorkerId());
  UMA_HISTOGRAM_TIMES("ServiceWorkerCache.CacheStorage.Delete",
                      TimeTicks::Now() - delete_times_[request_id]);
  WebServiceWorkerCacheStorage::CacheStorageCallbacks* callbacks =
      delete_callbacks_.Lookup(request_id);
  callbacks->onSuccess();
  delete_callbacks_.Remove(request_id);
  delete_times_.erase(request_id);
}

void CacheStorageDispatcher::OnCacheStorageKeysSuccess(
    int thread_id,
    int request_id,
    const std::vector<base::string16>& keys) {
  DCHECK_EQ(thread_id, CurrentWorkerId());
  blink::WebVector<blink::WebString> webKeys(keys.size());
  for (size_t i = 0; i < keys.size(); ++i)
    webKeys[i] = keys[i];

  UMA_HISTOGRAM_TIMES("ServiceWorkerCache.CacheStorage.Keys",
                      TimeTicks::Now() - keys_times_[request_id]);
  WebServiceWorkerCacheStorage::CacheStorageKeysCallbacks* callbacks =
      keys_callbacks_.Lookup(request_id);
  callbacks->onSuccess(webKeys);
  keys_callbacks_.Remove(request_id);
  keys_times_.erase(request_id);
}

void CacheStorageDispatcher::OnCacheStorageMatchSuccess(
    int thread_id,
    int request_id,
    const ServiceWorkerResponse& response) {
  DCHECK_EQ(thread_id, CurrentWorkerId());
  blink::WebServiceWorkerResponse web_response;
  PopulateWebResponseFromResponse(response, &web_response);

  UMA_HISTOGRAM_TIMES("ServiceWorkerCache.CacheStorage.Match",
                      TimeTicks::Now() - match_times_[request_id]);
  WebServiceWorkerCacheStorage::CacheStorageMatchCallbacks* callbacks =
      match_callbacks_.Lookup(request_id);
  callbacks->onSuccess(web_response);
  match_callbacks_.Remove(request_id);
  match_times_.erase(request_id);
}

void CacheStorageDispatcher::OnCacheStorageHasError(
    int thread_id,
    int request_id,
    blink::WebServiceWorkerCacheError reason) {
  DCHECK_EQ(thread_id, CurrentWorkerId());
  WebServiceWorkerCacheStorage::CacheStorageCallbacks* callbacks =
      has_callbacks_.Lookup(request_id);
  callbacks->onError(reason);
  has_callbacks_.Remove(request_id);
  has_times_.erase(request_id);
}

void CacheStorageDispatcher::OnCacheStorageOpenError(
    int thread_id,
    int request_id,
    blink::WebServiceWorkerCacheError reason) {
  DCHECK_EQ(thread_id, CurrentWorkerId());
  WebServiceWorkerCacheStorage::CacheStorageWithCacheCallbacks* callbacks =
      open_callbacks_.Lookup(request_id);
  callbacks->onError(reason);
  open_callbacks_.Remove(request_id);
  open_times_.erase(request_id);
}

void CacheStorageDispatcher::OnCacheStorageDeleteError(
    int thread_id,
    int request_id,
    blink::WebServiceWorkerCacheError reason) {
  DCHECK_EQ(thread_id, CurrentWorkerId());
  WebServiceWorkerCacheStorage::CacheStorageCallbacks* callbacks =
      delete_callbacks_.Lookup(request_id);
  callbacks->onError(reason);
  delete_callbacks_.Remove(request_id);
  delete_times_.erase(request_id);
}

void CacheStorageDispatcher::OnCacheStorageKeysError(
    int thread_id,
    int request_id,
    blink::WebServiceWorkerCacheError reason) {
  DCHECK_EQ(thread_id, CurrentWorkerId());
  WebServiceWorkerCacheStorage::CacheStorageKeysCallbacks* callbacks =
      keys_callbacks_.Lookup(request_id);
  callbacks->onError(reason);
  keys_callbacks_.Remove(request_id);
  keys_times_.erase(request_id);
}

void CacheStorageDispatcher::OnCacheStorageMatchError(
    int thread_id,
    int request_id,
    blink::WebServiceWorkerCacheError reason) {
  DCHECK_EQ(thread_id, CurrentWorkerId());
  WebServiceWorkerCacheStorage::CacheStorageMatchCallbacks* callbacks =
      match_callbacks_.Lookup(request_id);
  callbacks->onError(reason);
  match_callbacks_.Remove(request_id);
  match_times_.erase(request_id);
}

void CacheStorageDispatcher::OnCacheMatchSuccess(
    int thread_id,
    int request_id,
    const ServiceWorkerResponse& response) {
  DCHECK_EQ(thread_id, CurrentWorkerId());
  blink::WebServiceWorkerResponse web_response;
  PopulateWebResponseFromResponse(response, &web_response);

  UMA_HISTOGRAM_TIMES("ServiceWorkerCache.Cache.Match",
                      TimeTicks::Now() - cache_match_times_[request_id]);
  blink::WebServiceWorkerCache::CacheMatchCallbacks* callbacks =
      cache_match_callbacks_.Lookup(request_id);
  callbacks->onSuccess(web_response);
  cache_match_callbacks_.Remove(request_id);
  cache_match_times_.erase(request_id);
}

void CacheStorageDispatcher::OnCacheMatchAllSuccess(
    int thread_id,
    int request_id,
    const std::vector<ServiceWorkerResponse>& responses) {
  DCHECK_EQ(thread_id, CurrentWorkerId());

  UMA_HISTOGRAM_TIMES("ServiceWorkerCache.Cache.MatchAll",
                      TimeTicks::Now() - cache_match_all_times_[request_id]);
  blink::WebServiceWorkerCache::CacheWithResponsesCallbacks* callbacks =
      cache_match_all_callbacks_.Lookup(request_id);
  callbacks->onSuccess(WebResponsesFromResponses(responses));
  cache_match_all_callbacks_.Remove(request_id);
  cache_match_all_times_.erase(request_id);
}

void CacheStorageDispatcher::OnCacheKeysSuccess(
    int thread_id,
    int request_id,
    const std::vector<ServiceWorkerFetchRequest>& requests) {
  DCHECK_EQ(thread_id, CurrentWorkerId());

  UMA_HISTOGRAM_TIMES("ServiceWorkerCache.Cache.Keys",
                      TimeTicks::Now() - cache_keys_times_[request_id]);
  blink::WebServiceWorkerCache::CacheWithRequestsCallbacks* callbacks =
      cache_keys_callbacks_.Lookup(request_id);
  callbacks->onSuccess(WebRequestsFromRequests(requests));
  cache_keys_callbacks_.Remove(request_id);
  cache_keys_times_.erase(request_id);
}

void CacheStorageDispatcher::OnCacheBatchSuccess(
    int thread_id,
    int request_id) {
  DCHECK_EQ(thread_id, CurrentWorkerId());

  UMA_HISTOGRAM_TIMES("ServiceWorkerCache.Cache.Batch",
                      TimeTicks::Now() - cache_batch_times_[request_id]);
  blink::WebServiceWorkerCache::CacheBatchCallbacks* callbacks =
      cache_batch_callbacks_.Lookup(request_id);
  callbacks->onSuccess();
  cache_batch_callbacks_.Remove(request_id);
  cache_batch_times_.erase(request_id);
}

void CacheStorageDispatcher::OnCacheMatchError(
    int thread_id,
    int request_id,
    blink::WebServiceWorkerCacheError reason) {
  DCHECK_EQ(thread_id, CurrentWorkerId());
  blink::WebServiceWorkerCache::CacheMatchCallbacks* callbacks =
      cache_match_callbacks_.Lookup(request_id);
  callbacks->onError(reason);
  cache_match_callbacks_.Remove(request_id);
  cache_match_times_.erase(request_id);
}

void CacheStorageDispatcher::OnCacheMatchAllError(
    int thread_id,
    int request_id,
    blink::WebServiceWorkerCacheError reason) {
  DCHECK_EQ(thread_id, CurrentWorkerId());
  blink::WebServiceWorkerCache::CacheWithResponsesCallbacks* callbacks =
      cache_match_all_callbacks_.Lookup(request_id);
  callbacks->onError(reason);
  cache_match_all_callbacks_.Remove(request_id);
  cache_match_all_times_.erase(request_id);
}

void CacheStorageDispatcher::OnCacheKeysError(
    int thread_id,
    int request_id,
    blink::WebServiceWorkerCacheError reason) {
  DCHECK_EQ(thread_id, CurrentWorkerId());
  blink::WebServiceWorkerCache::CacheWithRequestsCallbacks* callbacks =
      cache_keys_callbacks_.Lookup(request_id);
  callbacks->onError(reason);
  cache_keys_callbacks_.Remove(request_id);
  cache_keys_times_.erase(request_id);
}

void CacheStorageDispatcher::OnCacheBatchError(
    int thread_id,
    int request_id,
    blink::WebServiceWorkerCacheError reason) {
  DCHECK_EQ(thread_id, CurrentWorkerId());
  blink::WebServiceWorkerCache::CacheBatchCallbacks* callbacks =
      cache_batch_callbacks_.Lookup(request_id);
  callbacks->onError(blink::WebServiceWorkerCacheError(reason));
  cache_batch_callbacks_.Remove(request_id);
  cache_batch_times_.erase(request_id);
}

void CacheStorageDispatcher::dispatchHas(
    WebServiceWorkerCacheStorage::CacheStorageCallbacks* callbacks,
    const url::Origin& origin,
    const blink::WebString& cacheName) {
  int request_id = has_callbacks_.Add(callbacks);
  has_times_[request_id] = base::TimeTicks::Now();
  Send(new CacheStorageHostMsg_CacheStorageHas(CurrentWorkerId(), request_id,
                                               origin, cacheName));
}

void CacheStorageDispatcher::dispatchOpen(
    WebServiceWorkerCacheStorage::CacheStorageWithCacheCallbacks* callbacks,
    const url::Origin& origin,
    const blink::WebString& cacheName) {
  int request_id = open_callbacks_.Add(callbacks);
  open_times_[request_id] = base::TimeTicks::Now();
  Send(new CacheStorageHostMsg_CacheStorageOpen(CurrentWorkerId(), request_id,
                                                origin, cacheName));
}

void CacheStorageDispatcher::dispatchDelete(
    WebServiceWorkerCacheStorage::CacheStorageCallbacks* callbacks,
    const url::Origin& origin,
    const blink::WebString& cacheName) {
  int request_id = delete_callbacks_.Add(callbacks);
  delete_times_[request_id] = base::TimeTicks::Now();
  Send(new CacheStorageHostMsg_CacheStorageDelete(CurrentWorkerId(), request_id,
                                                  origin, cacheName));
}

void CacheStorageDispatcher::dispatchKeys(
    WebServiceWorkerCacheStorage::CacheStorageKeysCallbacks* callbacks,
    const url::Origin& origin) {
  int request_id = keys_callbacks_.Add(callbacks);
  keys_times_[request_id] = base::TimeTicks::Now();
  Send(new CacheStorageHostMsg_CacheStorageKeys(CurrentWorkerId(), request_id,
                                                origin));
}

void CacheStorageDispatcher::dispatchMatch(
    WebServiceWorkerCacheStorage::CacheStorageMatchCallbacks* callbacks,
    const url::Origin& origin,
    const blink::WebServiceWorkerRequest& request,
    const blink::WebServiceWorkerCache::QueryParams& query_params) {
  int request_id = match_callbacks_.Add(callbacks);
  match_times_[request_id] = base::TimeTicks::Now();
  Send(new CacheStorageHostMsg_CacheStorageMatch(
      CurrentWorkerId(), request_id, origin,
      FetchRequestFromWebRequest(request),
      QueryParamsFromWebQueryParams(query_params)));
}

void CacheStorageDispatcher::dispatchMatchForCache(
    int cache_id,
    blink::WebServiceWorkerCache::CacheMatchCallbacks* callbacks,
    const blink::WebServiceWorkerRequest& request,
    const blink::WebServiceWorkerCache::QueryParams& query_params) {
  int request_id = cache_match_callbacks_.Add(callbacks);
  cache_match_times_[request_id] = base::TimeTicks::Now();

  Send(new CacheStorageHostMsg_CacheMatch(
      CurrentWorkerId(), request_id, cache_id,
      FetchRequestFromWebRequest(request),
      QueryParamsFromWebQueryParams(query_params)));
}

void CacheStorageDispatcher::dispatchMatchAllForCache(
    int cache_id,
    blink::WebServiceWorkerCache::CacheWithResponsesCallbacks* callbacks,
    const blink::WebServiceWorkerRequest& request,
    const blink::WebServiceWorkerCache::QueryParams& query_params) {
  int request_id = cache_match_all_callbacks_.Add(callbacks);
  cache_match_all_times_[request_id] = base::TimeTicks::Now();

  Send(new CacheStorageHostMsg_CacheMatchAll(
      CurrentWorkerId(), request_id, cache_id,
      FetchRequestFromWebRequest(request),
      QueryParamsFromWebQueryParams(query_params)));
}

void CacheStorageDispatcher::dispatchKeysForCache(
    int cache_id,
    blink::WebServiceWorkerCache::CacheWithRequestsCallbacks* callbacks,
    const blink::WebServiceWorkerRequest* request,
    const blink::WebServiceWorkerCache::QueryParams& query_params) {
  int request_id = cache_keys_callbacks_.Add(callbacks);
  cache_keys_times_[request_id] = base::TimeTicks::Now();

  Send(new CacheStorageHostMsg_CacheKeys(
      CurrentWorkerId(), request_id, cache_id,
      request ? FetchRequestFromWebRequest(*request)
              : ServiceWorkerFetchRequest(),
      QueryParamsFromWebQueryParams(query_params)));
}

void CacheStorageDispatcher::dispatchBatchForCache(
    int cache_id,
    blink::WebServiceWorkerCache::CacheBatchCallbacks* callbacks,
    const blink::WebVector<blink::WebServiceWorkerCache::BatchOperation>&
        web_operations) {
  int request_id = cache_batch_callbacks_.Add(callbacks);
  cache_batch_times_[request_id] = base::TimeTicks::Now();

  std::vector<CacheStorageBatchOperation> operations;
  operations.reserve(web_operations.size());
  for (size_t i = 0; i < web_operations.size(); ++i) {
    operations.push_back(
        BatchOperationFromWebBatchOperation(web_operations[i]));
  }

  Send(new CacheStorageHostMsg_CacheBatch(CurrentWorkerId(), request_id,
                                          cache_id, operations));
}

void CacheStorageDispatcher::OnWebCacheDestruction(int cache_id) {
  web_caches_.Remove(cache_id);
  Send(new CacheStorageHostMsg_CacheClosed(cache_id));
}

void CacheStorageDispatcher::PopulateWebResponseFromResponse(
    const ServiceWorkerResponse& response,
    blink::WebServiceWorkerResponse* web_response) {
  web_response->setURL(response.url);
  web_response->setStatus(response.status_code);
  web_response->setStatusText(base::ASCIIToUTF16(response.status_text));
  web_response->setResponseType(response.response_type);
  web_response->setResponseTime(response.response_time.ToInternalValue());
  web_response->setCacheStorageCacheName(
      response.is_in_cache_storage
          ? blink::WebString::fromUTF8(response.cache_storage_cache_name)
          : blink::WebString());
  blink::WebVector<blink::WebString> headers(
      response.cors_exposed_header_names.size());
  std::transform(
      response.cors_exposed_header_names.begin(),
      response.cors_exposed_header_names.end(), headers.begin(),
      [](const std::string& h) { return blink::WebString::fromLatin1(h); });
  web_response->setCorsExposedHeaderNames(headers);

  for (const auto& i : response.headers) {
    web_response->setHeader(base::ASCIIToUTF16(i.first),
                            base::ASCIIToUTF16(i.second));
  }

  if (!response.blob_uuid.empty()) {
    web_response->setBlob(blink::WebString::fromUTF8(response.blob_uuid),
                          response.blob_size);
    // Let the host know that it can release its reference to the blob.
    Send(new CacheStorageHostMsg_BlobDataHandled(response.blob_uuid));
  }
}

blink::WebVector<blink::WebServiceWorkerResponse>
CacheStorageDispatcher::WebResponsesFromResponses(
    const std::vector<ServiceWorkerResponse>& responses) {
  blink::WebVector<blink::WebServiceWorkerResponse> web_responses(
      responses.size());
  for (size_t i = 0; i < responses.size(); ++i)
    PopulateWebResponseFromResponse(responses[i], &(web_responses[i]));
  return web_responses;
}

}  // namespace content
