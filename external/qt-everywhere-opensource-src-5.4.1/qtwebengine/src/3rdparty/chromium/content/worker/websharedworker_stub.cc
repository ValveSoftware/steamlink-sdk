// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/worker/websharedworker_stub.h"

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "content/child/child_process.h"
#include "content/child/child_thread.h"
#include "content/child/fileapi/file_system_dispatcher.h"
#include "content/child/shared_worker_devtools_agent.h"
#include "content/child/webmessageportchannel_impl.h"
#include "content/common/worker_messages.h"
#include "content/public/common/content_switches.h"
#include "content/worker/worker_thread.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebSharedWorker.h"

namespace content {

WebSharedWorkerStub::WebSharedWorkerStub(
    const GURL& url,
    const base::string16& name,
    const base::string16& content_security_policy,
    blink::WebContentSecurityPolicyType security_policy_type,
    bool pause_on_start,
    int route_id)
    : route_id_(route_id),
      client_(route_id, this),
      running_(false),
      url_(url) {

  WorkerThread* worker_thread = WorkerThread::current();
  DCHECK(worker_thread);
  worker_thread->AddWorkerStub(this);
  // Start processing incoming IPCs for this worker.
  worker_thread->GetRouter()->AddRoute(route_id_, this);

  // TODO(atwilson): Add support for NaCl when they support MessagePorts.
  impl_ = blink::WebSharedWorker::create(client());
  if (pause_on_start) {
    // Pause worker context when it starts and wait until either DevTools client
    // is attached or explicit resume notification is received.
    impl_->pauseWorkerContextOnStart();
  }

  worker_devtools_agent_.reset(new SharedWorkerDevToolsAgent(route_id, impl_));
  client()->set_devtools_agent(worker_devtools_agent_.get());
  impl_->startWorkerContext(url_, name,
                            content_security_policy, security_policy_type);
}

WebSharedWorkerStub::~WebSharedWorkerStub() {
  impl_->clientDestroyed();
  WorkerThread* worker_thread = WorkerThread::current();
  DCHECK(worker_thread);
  worker_thread->RemoveWorkerStub(this);
  worker_thread->GetRouter()->RemoveRoute(route_id_);
}

void WebSharedWorkerStub::Shutdown() {
  // The worker has exited - free ourselves and the client.
  delete this;
}

void WebSharedWorkerStub::EnsureWorkerContextTerminates() {
  client_.EnsureWorkerContextTerminates();
}

bool WebSharedWorkerStub::OnMessageReceived(const IPC::Message& message) {
  if (worker_devtools_agent_->OnMessageReceived(message))
    return true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebSharedWorkerStub, message)
    IPC_MESSAGE_HANDLER(WorkerMsg_TerminateWorkerContext,
                        OnTerminateWorkerContext)
    IPC_MESSAGE_HANDLER(WorkerMsg_Connect, OnConnect)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void WebSharedWorkerStub::OnChannelError() {
    OnTerminateWorkerContext();
}

const GURL& WebSharedWorkerStub::url() {
  return url_;
}

void WebSharedWorkerStub::OnConnect(int sent_message_port_id, int routing_id) {
  WebMessagePortChannelImpl* channel =
      new WebMessagePortChannelImpl(routing_id,
                                    sent_message_port_id,
                                    base::MessageLoopProxy::current().get());
  if (running_) {
    impl_->connect(channel);
    WorkerThread::current()->Send(
        new WorkerHostMsg_WorkerConnected(channel->message_port_id(),
                                          route_id_));
  } else {
    // If two documents try to load a SharedWorker at the same time, the
    // WorkerMsg_Connect for one of the documents can come in before the
    // worker is started. Just queue up the connect and deliver it once the
    // worker starts.
    pending_channels_.push_back(channel);
  }
}

void WebSharedWorkerStub::OnTerminateWorkerContext() {
  running_ = false;
  // Call the client to make sure context exits.
  EnsureWorkerContextTerminates();
  // This may call "delete this" via WorkerScriptLoadFailed and Shutdown.
  impl_->terminateWorkerContext();
}

void WebSharedWorkerStub::WorkerScriptLoaded() {
  running_ = true;
  // Process any pending connections.
  for (PendingChannelList::const_iterator iter = pending_channels_.begin();
       iter != pending_channels_.end();
       ++iter) {
    impl_->connect(*iter);
    WorkerThread::current()->Send(
        new WorkerHostMsg_WorkerConnected((*iter)->message_port_id(),
                                          route_id_));
  }
  pending_channels_.clear();
}

void WebSharedWorkerStub::WorkerScriptLoadFailed() {
  for (PendingChannelList::const_iterator iter = pending_channels_.begin();
       iter != pending_channels_.end();
       ++iter) {
    blink::WebMessagePortChannel* channel = *iter;
    channel->destroy();
  }
  pending_channels_.clear();
  Shutdown();
}

}  // namespace content
