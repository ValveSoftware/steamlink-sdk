// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/devtools/devtools_agent_filter.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "content/child/child_process.h"
#include "content/common/devtools_messages.h"
#include "content/renderer/devtools/devtools_agent.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebDevToolsAgent.h"

using blink::WebDevToolsAgent;
using blink::WebString;

namespace content {

namespace {

class MessageImpl : public WebDevToolsAgent::MessageDescriptor {
 public:
  MessageImpl(
      const std::string& method,
      const std::string& message,
      int routing_id)
      : method_(method),
        msg_(message),
        routing_id_(routing_id) {
  }
  ~MessageImpl() override {}
  WebDevToolsAgent* agent() override {
    DevToolsAgent* agent = DevToolsAgent::FromRoutingId(routing_id_);
    if (!agent)
      return 0;
    return agent->GetWebAgent();
  }
  WebString message() override { return WebString::fromUTF8(msg_); }
  WebString method() override { return WebString::fromUTF8(method_); }

 private:
  std::string method_;
  std::string msg_;
  int routing_id_;
};

}  // namespace

DevToolsAgentFilter::DevToolsAgentFilter()
    : render_thread_loop_(base::MessageLoop::current()),
      io_task_runner_(ChildProcess::current()->io_task_runner()),
      current_routing_id_(0) {
}

bool DevToolsAgentFilter::OnMessageReceived(const IPC::Message& message) {
  // Dispatch debugger commands directly from IO.
  current_routing_id_ = message.routing_id();
  IPC_BEGIN_MESSAGE_MAP(DevToolsAgentFilter, message)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_DispatchOnInspectorBackend,
                        OnDispatchOnInspectorBackend)
  IPC_END_MESSAGE_MAP()
  return false;
}

DevToolsAgentFilter::~DevToolsAgentFilter() {}

void DevToolsAgentFilter::OnDispatchOnInspectorBackend(
    int session_id,
    int call_id,
    const std::string& method,
    const std::string& message) {
  if (embedded_worker_routes_.find(current_routing_id_) !=
      embedded_worker_routes_.end()) {
    return;
  }

  if (WebDevToolsAgent::shouldInterruptForMethod(
          WebString::fromUTF8(method))) {
    WebDevToolsAgent::interruptAndDispatch(
        session_id, new MessageImpl(method, message, current_routing_id_));
  }
}

void DevToolsAgentFilter::AddEmbeddedWorkerRouteOnMainThread(
    int32_t routing_id) {
  io_task_runner_->PostTask(
      FROM_HERE, base::Bind(&DevToolsAgentFilter::AddEmbeddedWorkerRoute, this,
                            routing_id));
}

void DevToolsAgentFilter::RemoveEmbeddedWorkerRouteOnMainThread(
    int32_t routing_id) {
  io_task_runner_->PostTask(
      FROM_HERE, base::Bind(&DevToolsAgentFilter::RemoveEmbeddedWorkerRoute,
                            this, routing_id));
}

void DevToolsAgentFilter::AddEmbeddedWorkerRoute(int32_t routing_id) {
  embedded_worker_routes_.insert(routing_id);
}

void DevToolsAgentFilter::RemoveEmbeddedWorkerRoute(int32_t routing_id) {
  embedded_worker_routes_.erase(routing_id);
}

}  // namespace content
