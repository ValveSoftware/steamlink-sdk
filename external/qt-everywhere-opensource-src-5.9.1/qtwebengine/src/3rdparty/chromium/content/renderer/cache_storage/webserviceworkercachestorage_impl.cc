// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/cache_storage/webserviceworkercachestorage_impl.h"

#include "content/child/thread_safe_sender.h"
#include "content/renderer/cache_storage/cache_storage_dispatcher.h"
#include "third_party/WebKit/public/platform/WebHTTPHeaderVisitor.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerCache.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerRequest.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerResponse.h"

using base::TimeTicks;

namespace content {

WebServiceWorkerCacheStorageImpl::WebServiceWorkerCacheStorageImpl(
    ThreadSafeSender* thread_safe_sender,
    const url::Origin& origin)
    : thread_safe_sender_(thread_safe_sender), origin_(origin) {}

WebServiceWorkerCacheStorageImpl::~WebServiceWorkerCacheStorageImpl() {
}

void WebServiceWorkerCacheStorageImpl::dispatchHas(
    CacheStorageCallbacks* callbacks,
    const blink::WebString& cacheName) {
  GetDispatcher()->dispatchHas(callbacks, origin_, cacheName);
}

void WebServiceWorkerCacheStorageImpl::dispatchOpen(
    CacheStorageWithCacheCallbacks* callbacks,
    const blink::WebString& cacheName) {
  GetDispatcher()->dispatchOpen(callbacks, origin_, cacheName);
}

void WebServiceWorkerCacheStorageImpl::dispatchDelete(
    CacheStorageCallbacks* callbacks,
    const blink::WebString& cacheName) {
  GetDispatcher()->dispatchDelete(callbacks, origin_, cacheName);
}

void WebServiceWorkerCacheStorageImpl::dispatchKeys(
    CacheStorageKeysCallbacks* callbacks) {
  GetDispatcher()->dispatchKeys(callbacks, origin_);
}

void WebServiceWorkerCacheStorageImpl::dispatchMatch(
    CacheStorageMatchCallbacks* callbacks,
    const blink::WebServiceWorkerRequest& request,
    const blink::WebServiceWorkerCache::QueryParams& query_params) {
  GetDispatcher()->dispatchMatch(callbacks, origin_, request, query_params);
}

CacheStorageDispatcher* WebServiceWorkerCacheStorageImpl::GetDispatcher()
    const {
  return CacheStorageDispatcher::ThreadSpecificInstance(
      thread_safe_sender_.get());
}

}  // namespace content
