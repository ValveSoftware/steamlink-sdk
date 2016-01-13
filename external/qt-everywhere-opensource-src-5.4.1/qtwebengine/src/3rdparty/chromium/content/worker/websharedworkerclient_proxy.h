// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WORKER_WEBWORKERCLIENT_PROXY_H_
#define CONTENT_WORKER_WEBWORKERCLIENT_PROXY_H_

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "ipc/ipc_channel.h"
#include "third_party/WebKit/public/web/WebSharedWorkerClient.h"

namespace blink {
class WebApplicationCacheHost;
class WebApplicationCacheHostClient;
class WebFrame;
class WebSecurityOrigin;
}

namespace content {

class SharedWorkerDevToolsAgent;
class WebSharedWorkerStub;
class WorkerWebApplicationCacheHostImpl;

// This class receives IPCs from the renderer and calls the WebCore::Worker
// implementation (after the data types have been converted by glue code).  It
// is also called by the worker code and converts these function calls into
// IPCs that are sent to the renderer, where they're converted back to function
// calls by WebWorkerProxy.
class WebSharedWorkerClientProxy : public blink::WebSharedWorkerClient {
 public:
  WebSharedWorkerClientProxy(int route_id, WebSharedWorkerStub* stub);
  virtual ~WebSharedWorkerClientProxy();

  // WebSharedWorkerClient implementation.
  virtual void workerContextClosed();
  virtual void workerContextDestroyed();
  virtual void workerScriptLoaded();
  virtual void workerScriptLoadFailed();
  virtual void selectAppCacheID(long long app_cache_id);

  virtual blink::WebNotificationPresenter* notificationPresenter();

  virtual blink::WebApplicationCacheHost* createApplicationCacheHost(
      blink::WebApplicationCacheHostClient* client);
  virtual blink::WebWorkerPermissionClientProxy*
      createWorkerPermissionClientProxy(
          const blink::WebSecurityOrigin& origin);

  virtual void dispatchDevToolsMessage(const blink::WebString&);
  virtual void saveDevToolsAgentState(const blink::WebString&);

  void EnsureWorkerContextTerminates();

  void set_devtools_agent(SharedWorkerDevToolsAgent* devtools_agent) {
    devtools_agent_ = devtools_agent;
  }

 private:
  bool Send(IPC::Message* message);

  int route_id_;
  int appcache_host_id_;
  WebSharedWorkerStub* stub_;
  base::WeakPtrFactory<WebSharedWorkerClientProxy> weak_factory_;
  SharedWorkerDevToolsAgent* devtools_agent_;
  WorkerWebApplicationCacheHostImpl* app_cache_host_;

  DISALLOW_COPY_AND_ASSIGN(WebSharedWorkerClientProxy);
};

}  // namespace content

#endif  // CONTENT_WORKER_WEBWORKERCLIENT_PROXY_H_
