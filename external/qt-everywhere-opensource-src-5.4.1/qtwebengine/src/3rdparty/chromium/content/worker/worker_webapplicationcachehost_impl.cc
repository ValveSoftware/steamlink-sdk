// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/worker/worker_webapplicationcachehost_impl.h"

#include "content/child/appcache/appcache_dispatcher.h"
#include "content/worker/worker_thread.h"

namespace content {

WorkerWebApplicationCacheHostImpl::WorkerWebApplicationCacheHostImpl(
    blink::WebApplicationCacheHostClient* client)
    : WebApplicationCacheHostImpl(client,
          WorkerThread::current()->appcache_dispatcher()->backend_proxy()) {
}

void WorkerWebApplicationCacheHostImpl::willStartMainResourceRequest(
    blink::WebURLRequest&, const blink::WebApplicationCacheHost*) {
}

void WorkerWebApplicationCacheHostImpl::didReceiveResponseForMainResource(
    const blink::WebURLResponse&) {
}

void WorkerWebApplicationCacheHostImpl::didReceiveDataForMainResource(
    const char*, int) {
}

void WorkerWebApplicationCacheHostImpl::didFinishLoadingMainResource(
    bool) {
}

void WorkerWebApplicationCacheHostImpl::selectCacheWithoutManifest() {
}

bool WorkerWebApplicationCacheHostImpl::selectCacheWithManifest(
    const blink::WebURL&) {
  return true;
}

}  // namespace content
