// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/shared_worker/embedded_shared_worker_stub.h"

#include "base/message_loop/message_loop_proxy.h"
#include "content/child/appcache/appcache_dispatcher.h"
#include "content/child/appcache/web_application_cache_host_impl.h"
#include "content/child/scoped_child_process_reference.h"
#include "content/child/shared_worker_devtools_agent.h"
#include "content/child/webmessageportchannel_impl.h"
#include "content/common/worker_messages.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/shared_worker/embedded_shared_worker_permission_client_proxy.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"
#include "third_party/WebKit/public/web/WebSharedWorker.h"
#include "third_party/WebKit/public/web/WebSharedWorkerClient.h"

namespace content {

namespace {

class SharedWorkerWebApplicationCacheHostImpl
    : public WebApplicationCacheHostImpl {
 public:
  SharedWorkerWebApplicationCacheHostImpl(
      blink::WebApplicationCacheHostClient* client)
      : WebApplicationCacheHostImpl(client,
                                    RenderThreadImpl::current()
                                        ->appcache_dispatcher()
                                        ->backend_proxy()) {}

  // Main resource loading is different for workers. The main resource is
  // loaded by the worker using WorkerScriptLoader.
  // These overrides are stubbed out.
  virtual void willStartMainResourceRequest(
      blink::WebURLRequest&,
      const blink::WebApplicationCacheHost*) {}
  virtual void didReceiveResponseForMainResource(const blink::WebURLResponse&) {
  }
  virtual void didReceiveDataForMainResource(const char* data, int len) {}
  virtual void didFinishLoadingMainResource(bool success) {}

  // Cache selection is also different for workers. We know at construction
  // time what cache to select and do so then.
  // These overrides are stubbed out.
  virtual void selectCacheWithoutManifest() {}
  virtual bool selectCacheWithManifest(const blink::WebURL& manifestURL) {
    return true;
  }
};
}

EmbeddedSharedWorkerStub::EmbeddedSharedWorkerStub(
    const GURL& url,
    const base::string16& name,
    const base::string16& content_security_policy,
    blink::WebContentSecurityPolicyType security_policy_type,
    bool pause_on_start,
    int route_id)
    : route_id_(route_id), name_(name), runing_(false), url_(url) {
  RenderThreadImpl::current()->AddEmbeddedWorkerRoute(route_id_, this);
  impl_ = blink::WebSharedWorker::create(this);
  if (pause_on_start) {
    // Pause worker context when it starts and wait until either DevTools client
    // is attached or explicit resume notification is received.
    impl_->pauseWorkerContextOnStart();
  }
  worker_devtools_agent_.reset(
      new SharedWorkerDevToolsAgent(route_id, impl_));
  impl_->startWorkerContext(url, name_,
                            content_security_policy, security_policy_type);
}

EmbeddedSharedWorkerStub::~EmbeddedSharedWorkerStub() {
  RenderThreadImpl::current()->RemoveEmbeddedWorkerRoute(route_id_);
}

bool EmbeddedSharedWorkerStub::OnMessageReceived(
    const IPC::Message& message) {
  if (worker_devtools_agent_->OnMessageReceived(message))
    return true;
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(EmbeddedSharedWorkerStub, message)
    IPC_MESSAGE_HANDLER(WorkerMsg_TerminateWorkerContext,
                        OnTerminateWorkerContext)
    IPC_MESSAGE_HANDLER(WorkerMsg_Connect, OnConnect)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void EmbeddedSharedWorkerStub::OnChannelError() {
  OnTerminateWorkerContext();
}

void EmbeddedSharedWorkerStub::workerScriptLoaded() {
  Send(new WorkerHostMsg_WorkerScriptLoaded(route_id_));
  runing_ = true;
  // Process any pending connections.
  for (PendingChannelList::const_iterator iter = pending_channels_.begin();
       iter != pending_channels_.end();
       ++iter) {
    ConnectToChannel(*iter);
  }
  pending_channels_.clear();
}

void EmbeddedSharedWorkerStub::workerScriptLoadFailed() {
  Send(new WorkerHostMsg_WorkerScriptLoadFailed(route_id_));
  for (PendingChannelList::const_iterator iter = pending_channels_.begin();
       iter != pending_channels_.end();
       ++iter) {
    blink::WebMessagePortChannel* channel = *iter;
    channel->destroy();
  }
  pending_channels_.clear();
  Shutdown();
}

void EmbeddedSharedWorkerStub::workerContextClosed() {
  Send(new WorkerHostMsg_WorkerContextClosed(route_id_));
}

void EmbeddedSharedWorkerStub::workerContextDestroyed() {
  Send(new WorkerHostMsg_WorkerContextDestroyed(route_id_));
  Shutdown();
}

void EmbeddedSharedWorkerStub::selectAppCacheID(long long app_cache_id) {
  if (app_cache_host_) {
    // app_cache_host_ could become stale as it's owned by blink's
    // DocumentLoader. This method is assumed to be called while it's valid.
    app_cache_host_->backend()->SelectCacheForSharedWorker(
        app_cache_host_->host_id(), app_cache_id);
  }
}

blink::WebNotificationPresenter*
EmbeddedSharedWorkerStub::notificationPresenter() {
  // TODO(horo): delete this method if we have no plan to implement this.
  NOTREACHED();
  return NULL;
}

blink::WebApplicationCacheHost*
EmbeddedSharedWorkerStub::createApplicationCacheHost(
    blink::WebApplicationCacheHostClient* client) {
  app_cache_host_ = new SharedWorkerWebApplicationCacheHostImpl(client);
  return app_cache_host_;
}

blink::WebWorkerPermissionClientProxy*
    EmbeddedSharedWorkerStub::createWorkerPermissionClientProxy(
    const blink::WebSecurityOrigin& origin) {
  return new EmbeddedSharedWorkerPermissionClientProxy(
      GURL(origin.toString()),
      origin.isUnique(),
      route_id_,
      ChildThread::current()->thread_safe_sender());
}

void EmbeddedSharedWorkerStub::dispatchDevToolsMessage(
      const blink::WebString& message) {
  worker_devtools_agent_->SendDevToolsMessage(message);
}

void EmbeddedSharedWorkerStub::saveDevToolsAgentState(
      const blink::WebString& state) {
  worker_devtools_agent_->SaveDevToolsAgentState(state);
}

void EmbeddedSharedWorkerStub::Shutdown() {
  delete this;
}

bool EmbeddedSharedWorkerStub::Send(IPC::Message* message) {
  return RenderThreadImpl::current()->Send(message);
}

void EmbeddedSharedWorkerStub::ConnectToChannel(
    WebMessagePortChannelImpl* channel) {
  impl_->connect(channel);
  Send(
      new WorkerHostMsg_WorkerConnected(channel->message_port_id(), route_id_));
}

void EmbeddedSharedWorkerStub::OnConnect(int sent_message_port_id,
                                         int routing_id) {
  WebMessagePortChannelImpl* channel =
      new WebMessagePortChannelImpl(routing_id,
                                    sent_message_port_id,
                                    base::MessageLoopProxy::current().get());
  if (runing_) {
    ConnectToChannel(channel);
  } else {
    // If two documents try to load a SharedWorker at the same time, the
    // WorkerMsg_Connect for one of the documents can come in before the
    // worker is started. Just queue up the connect and deliver it once the
    // worker starts.
    pending_channels_.push_back(channel);
  }
}

void EmbeddedSharedWorkerStub::OnTerminateWorkerContext() {
  runing_ = false;
  impl_->terminateWorkerContext();
}

}  // namespace content
