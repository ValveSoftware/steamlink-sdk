// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WORKER_WORKER_WEBAPPLICATIONCACHEHOST_IMPL_H_
#define CHROME_WORKER_WORKER_WEBAPPLICATIONCACHEHOST_IMPL_H_

#include "content/child/appcache/web_application_cache_host_impl.h"

namespace content {

class WorkerWebApplicationCacheHostImpl : public WebApplicationCacheHostImpl {
 public:
  WorkerWebApplicationCacheHostImpl(
      blink::WebApplicationCacheHostClient* client);

  // Main resource loading is different for workers. The main resource is
  // loaded by the worker using WorkerScriptLoader.
  // These overrides are stubbed out.
  virtual void willStartMainResourceRequest(
      blink::WebURLRequest&, const blink::WebApplicationCacheHost*);
  virtual void didReceiveResponseForMainResource(
      const blink::WebURLResponse&);
  virtual void didReceiveDataForMainResource(const char* data, int len);
  virtual void didFinishLoadingMainResource(bool success);

  // Cache selection is also different for workers. We know at construction
  // time what cache to select and do so then.
  // These overrides are stubbed out.
  virtual void selectCacheWithoutManifest();
  virtual bool selectCacheWithManifest(const blink::WebURL& manifestURL);
};

}  // namespace content

#endif  // CHROME_WORKER_WORKER_WEBAPPLICATIONCACHEHOST_IMPL_H_
