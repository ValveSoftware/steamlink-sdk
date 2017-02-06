// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/embedded_worker_devtools_agent.h"

#include "content/common/devtools_messages.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebEmbeddedWorker.h"

using blink::WebEmbeddedWorker;
using blink::WebString;

namespace content {

EmbeddedWorkerDevToolsAgent::EmbeddedWorkerDevToolsAgent(
    blink::WebEmbeddedWorker* webworker,
    int route_id)
    : webworker_(webworker), route_id_(route_id) {
  RenderThreadImpl::current()->AddEmbeddedWorkerRoute(route_id_, this);
}

EmbeddedWorkerDevToolsAgent::~EmbeddedWorkerDevToolsAgent() {
  RenderThreadImpl::current()->RemoveEmbeddedWorkerRoute(route_id_);
}

bool EmbeddedWorkerDevToolsAgent::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(EmbeddedWorkerDevToolsAgent, message)
  IPC_MESSAGE_HANDLER(DevToolsAgentMsg_Attach, OnAttach)
  IPC_MESSAGE_HANDLER(DevToolsAgentMsg_Reattach, OnReattach)
  IPC_MESSAGE_HANDLER(DevToolsAgentMsg_Detach, OnDetach)
  IPC_MESSAGE_HANDLER(DevToolsAgentMsg_DispatchOnInspectorBackend,
                      OnDispatchOnInspectorBackend)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void EmbeddedWorkerDevToolsAgent::OnAttach(const std::string& host_id,
                                           int session_id) {
  webworker_->attachDevTools(WebString::fromUTF8(host_id), session_id);
}

void EmbeddedWorkerDevToolsAgent::OnReattach(const std::string& host_id,
                                             int session_id,
                                             const std::string& state) {
  webworker_->reattachDevTools(WebString::fromUTF8(host_id), session_id,
                               WebString::fromUTF8(state));
}

void EmbeddedWorkerDevToolsAgent::OnDetach() {
  webworker_->detachDevTools();
}

void EmbeddedWorkerDevToolsAgent::OnDispatchOnInspectorBackend(
    int session_id,
    int call_id,
    const std::string& method,
    const std::string& message) {
  webworker_->dispatchDevToolsMessage(session_id, call_id,
                                      WebString::fromUTF8(method),
                                      WebString::fromUTF8(message));
}

}  // namespace content
