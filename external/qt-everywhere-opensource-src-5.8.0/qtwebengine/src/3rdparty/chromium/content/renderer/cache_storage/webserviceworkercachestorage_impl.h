// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_WEB_SERVICE_WORKER_CACHE_STORAGE_IMPL_H_
#define CONTENT_RENDERER_SERVICE_WORKER_WEB_SERVICE_WORKER_CACHE_STORAGE_IMPL_H_

#include "base/macros.h"
#include "content/child/thread_safe_sender.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerCache.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerCacheError.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerCacheStorage.h"
#include "url/origin.h"

namespace content {

class CacheStorageDispatcher;
class ThreadSafeSender;

// This corresponds to an instance of the script-facing CacheStorage object.
// One exists per script execution context (window, worker) and has an origin
// which provides a namespace for the caches. Script calls to the CacheStorage
// object are routed through here to the (per-thread) dispatcher then on to
// the browser, with the origin added to the call parameters. Cache instances
// returned by open() calls are minted by and implemented within the code of
// the CacheStorageDispatcher, and include routing information.
class WebServiceWorkerCacheStorageImpl
    : public blink::WebServiceWorkerCacheStorage {
 public:
  WebServiceWorkerCacheStorageImpl(ThreadSafeSender* thread_safe_sender,
                                   const url::Origin& origin);
  ~WebServiceWorkerCacheStorageImpl() override;

  // From WebServiceWorkerCacheStorage:
  void dispatchHas(CacheStorageCallbacks* callbacks,
                   const blink::WebString& cacheName) override;
  void dispatchOpen(CacheStorageWithCacheCallbacks* callbacks,
                    const blink::WebString& cacheName) override;
  void dispatchDelete(CacheStorageCallbacks* callbacks,
                      const blink::WebString& cacheName) override;
  void dispatchKeys(CacheStorageKeysCallbacks* callbacks) override;
  void dispatchMatch(
      CacheStorageMatchCallbacks* callbacks,
      const blink::WebServiceWorkerRequest& request,
      const blink::WebServiceWorkerCache::QueryParams& query_params) override;

 private:
  // Helper to return the thread-specific dispatcher.
  CacheStorageDispatcher* GetDispatcher() const;

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  const url::Origin origin_;

  DISALLOW_COPY_AND_ASSIGN(WebServiceWorkerCacheStorageImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_WEB_SERVICE_WORKER_CACHE_STORAGE_IMPL_H_
