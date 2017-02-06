// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_CACHE_STORAGE_CACHE_STORAGE_DISPATCHER_H_
#define CONTENT_RENDERER_CACHE_STORAGE_CACHE_STORAGE_DISPATCHER_H_

#include <stdint.h>

#include <vector>

#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "content/public/child/worker_thread.h"
#include "ipc/ipc_message.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerCache.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerCacheError.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerCacheStorage.h"

namespace url {
class Origin;
}

namespace content {

class ThreadSafeSender;
struct ServiceWorkerFetchRequest;
struct ServiceWorkerResponse;

// Handle the Cache Storage messaging for this context thread. The
// main thread and each worker thread have their own instances.
class CacheStorageDispatcher : public WorkerThread::Observer {
 public:
  explicit CacheStorageDispatcher(ThreadSafeSender* thread_safe_sender);
  ~CacheStorageDispatcher() override;

  // |thread_safe_sender| needs to be passed in because if the call leads to
  // construction it will be needed.
  static CacheStorageDispatcher* ThreadSpecificInstance(
      ThreadSafeSender* thread_safe_sender);

  // WorkerThread::Observer implementation.
  void WillStopCurrentWorkerThread() override;

  bool Send(IPC::Message* msg);

  // ServiceWorkerScriptContext calls our OnMessageReceived directly.
  bool OnMessageReceived(const IPC::Message& message);

  // Message handlers for CacheStorage messages from the browser process.
  void OnCacheStorageHasSuccess(int thread_id, int request_id);
  void OnCacheStorageOpenSuccess(int thread_id, int request_id, int cache_id);
  void OnCacheStorageDeleteSuccess(int thread_id, int request_id);
  void OnCacheStorageKeysSuccess(int thread_id,
                                 int request_id,
                                 const std::vector<base::string16>& keys);
  void OnCacheStorageMatchSuccess(int thread_id,
                                  int request_id,
                                  const ServiceWorkerResponse& response);

  void OnCacheStorageHasError(int thread_id,
                              int request_id,
                              blink::WebServiceWorkerCacheError reason);
  void OnCacheStorageOpenError(int thread_id,
                               int request_id,
                               blink::WebServiceWorkerCacheError reason);
  void OnCacheStorageDeleteError(int thread_id,
                                 int request_id,
                                 blink::WebServiceWorkerCacheError reason);
  void OnCacheStorageKeysError(int thread_id,
                               int request_id,
                               blink::WebServiceWorkerCacheError reason);
  void OnCacheStorageMatchError(int thread_id,
                                int request_id,
                                blink::WebServiceWorkerCacheError reason);

  // Message handlers for Cache messages from the browser process.
  void OnCacheMatchSuccess(int thread_id,
                           int request_id,
                           const ServiceWorkerResponse& response);
  void OnCacheMatchAllSuccess(
      int thread_id,
      int request_id,
      const std::vector<ServiceWorkerResponse>& response);
  void OnCacheKeysSuccess(
      int thread_id,
      int request_id,
      const std::vector<ServiceWorkerFetchRequest>& response);
  void OnCacheBatchSuccess(int thread_id,
                           int request_id);

  void OnCacheMatchError(int thread_id,
                         int request_id,
                         blink::WebServiceWorkerCacheError reason);
  void OnCacheMatchAllError(int thread_id,
                            int request_id,
                            blink::WebServiceWorkerCacheError reason);
  void OnCacheKeysError(int thread_id,
                        int request_id,
                        blink::WebServiceWorkerCacheError reason);
  void OnCacheBatchError(int thread_id,
                         int request_id,
                         blink::WebServiceWorkerCacheError reason);

  // TODO(jsbell): These are only called by WebServiceWorkerCacheStorageImpl
  // and should be renamed to match Chromium conventions. crbug.com/439389
  void dispatchHas(
      blink::WebServiceWorkerCacheStorage::CacheStorageCallbacks* callbacks,
      const url::Origin& origin,
      const blink::WebString& cacheName);
  void dispatchOpen(
      blink::WebServiceWorkerCacheStorage::CacheStorageWithCacheCallbacks*
          callbacks,
      const url::Origin& origin,
      const blink::WebString& cacheName);
  void dispatchDelete(
      blink::WebServiceWorkerCacheStorage::CacheStorageCallbacks* callbacks,
      const url::Origin& origin,
      const blink::WebString& cacheName);
  void dispatchKeys(
      blink::WebServiceWorkerCacheStorage::CacheStorageKeysCallbacks* callbacks,
      const url::Origin& origin);
  void dispatchMatch(
      blink::WebServiceWorkerCacheStorage::CacheStorageMatchCallbacks*
          callbacks,
      const url::Origin& origin,
      const blink::WebServiceWorkerRequest& request,
      const blink::WebServiceWorkerCache::QueryParams& query_params);

  // These methods are used by WebCache to forward events to the browser
  // process.
  void dispatchMatchForCache(
      int cache_id,
      blink::WebServiceWorkerCache::CacheMatchCallbacks* callbacks,
      const blink::WebServiceWorkerRequest& request,
      const blink::WebServiceWorkerCache::QueryParams& query_params);
  void dispatchMatchAllForCache(
      int cache_id,
      blink::WebServiceWorkerCache::CacheWithResponsesCallbacks* callbacks,
      const blink::WebServiceWorkerRequest& request,
      const blink::WebServiceWorkerCache::QueryParams& query_params);
  void dispatchKeysForCache(
      int cache_id,
      blink::WebServiceWorkerCache::CacheWithRequestsCallbacks* callbacks,
      const blink::WebServiceWorkerRequest* request,
      const blink::WebServiceWorkerCache::QueryParams& query_params);
  void dispatchBatchForCache(
      int cache_id,
      blink::WebServiceWorkerCache::CacheBatchCallbacks* callbacks,
      const blink::WebVector<blink::WebServiceWorkerCache::BatchOperation>&
          batch_operations);

  void OnWebCacheDestruction(int cache_id);

 private:
  class WebCache;

  typedef IDMap<blink::WebServiceWorkerCacheStorage::CacheStorageCallbacks,
                IDMapOwnPointer> CallbacksMap;
  typedef IDMap<
      blink::WebServiceWorkerCacheStorage::CacheStorageWithCacheCallbacks,
      IDMapOwnPointer> WithCacheCallbacksMap;
  typedef IDMap<blink::WebServiceWorkerCacheStorage::CacheStorageKeysCallbacks,
                IDMapOwnPointer> KeysCallbacksMap;
  typedef IDMap<blink::WebServiceWorkerCacheStorage::CacheStorageMatchCallbacks,
                IDMapOwnPointer> StorageMatchCallbacksMap;

  typedef base::hash_map<int32_t, base::TimeTicks> TimeMap;

  typedef IDMap<blink::WebServiceWorkerCache::CacheMatchCallbacks,
                IDMapOwnPointer> MatchCallbacksMap;
  typedef IDMap<blink::WebServiceWorkerCache::CacheWithResponsesCallbacks,
                IDMapOwnPointer> WithResponsesCallbacksMap;
  typedef IDMap<blink::WebServiceWorkerCache::CacheWithRequestsCallbacks,
                IDMapOwnPointer> WithRequestsCallbacksMap;
  using BatchCallbacksMap =
      IDMap<blink::WebServiceWorkerCache::CacheBatchCallbacks, IDMapOwnPointer>;

  static int32_t CurrentWorkerId() { return WorkerThread::GetCurrentId(); }

  void PopulateWebResponseFromResponse(
      const ServiceWorkerResponse& response,
      blink::WebServiceWorkerResponse* web_response);

  blink::WebVector<blink::WebServiceWorkerResponse> WebResponsesFromResponses(
      const std::vector<ServiceWorkerResponse>& responses);

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;

  CallbacksMap has_callbacks_;
  WithCacheCallbacksMap open_callbacks_;
  CallbacksMap delete_callbacks_;
  KeysCallbacksMap keys_callbacks_;
  StorageMatchCallbacksMap match_callbacks_;

  TimeMap has_times_;
  TimeMap open_times_;
  TimeMap delete_times_;
  TimeMap keys_times_;
  TimeMap match_times_;

  // The individual caches created under this CacheStorage object.
  IDMap<WebCache, IDMapExternalPointer> web_caches_;

  // These ID maps are held in the CacheStorage object rather than the Cache
  // object to ensure that the IDs are unique.
  MatchCallbacksMap cache_match_callbacks_;
  WithResponsesCallbacksMap cache_match_all_callbacks_;
  WithRequestsCallbacksMap cache_keys_callbacks_;
  BatchCallbacksMap cache_batch_callbacks_;

  TimeMap cache_match_times_;
  TimeMap cache_match_all_times_;
  TimeMap cache_keys_times_;
  TimeMap cache_batch_times_;

  base::WeakPtrFactory<CacheStorageDispatcher> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CacheStorageDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_CACHE_STORAGE_CACHE_STORAGE_DISPATCHER_H_
