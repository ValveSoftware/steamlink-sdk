// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/shared_worker_devtools_agent.h"

#include <stddef.h>

#include "content/child/child_thread_impl.h"
#include "content/common/devtools_messages.h"
#include "ipc/ipc_channel.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebSharedWorker.h"

using blink::WebSharedWorker;
using blink::WebString;

namespace content {

static const size_t kMaxMessageChunkSize =
    IPC::Channel::kMaximumMessageSize / 4;

SharedWorkerDevToolsAgent::SharedWorkerDevToolsAgent(
    int route_id,
    WebSharedWorker* webworker)
    : route_id_(route_id),
      webworker_(webworker) {
}

SharedWorkerDevToolsAgent::~SharedWorkerDevToolsAgent() {
}

// Called on the Worker thread.
bool SharedWorkerDevToolsAgent::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SharedWorkerDevToolsAgent, message)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_Attach, OnAttach)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_Reattach, OnReattach)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_Detach, OnDetach)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_DispatchOnInspectorBackend,
                        OnDispatchOnInspectorBackend)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void SharedWorkerDevToolsAgent::SendDevToolsMessage(
    int session_id,
    int call_id,
    const blink::WebString& msg,
    const blink::WebString& state) {
  std::string message = msg.utf8();
  std::string post_state = state.utf8();
  DevToolsMessageChunk chunk;
  chunk.message_size = message.size();
  chunk.is_first = true;

  if (message.length() < kMaxMessageChunkSize) {
    chunk.data.swap(message);
    chunk.session_id = session_id;
    chunk.call_id = call_id;
    chunk.post_state = post_state;
    chunk.is_last = true;
    Send(new DevToolsClientMsg_DispatchOnInspectorFrontend(
        route_id_, chunk));
    return;
  }

  for (size_t pos = 0; pos < message.length(); pos += kMaxMessageChunkSize) {
    chunk.is_last = pos + kMaxMessageChunkSize >= message.length();
    chunk.session_id = chunk.is_last ? session_id : 0;
    chunk.call_id = chunk.is_last ? call_id : 0;
    chunk.post_state = chunk.is_last ? post_state : std::string();
    chunk.data = message.substr(pos, kMaxMessageChunkSize);
    Send(new DevToolsClientMsg_DispatchOnInspectorFrontend(
        route_id_, chunk));
    chunk.is_first = false;
    chunk.message_size = 0;
  }
}

void SharedWorkerDevToolsAgent::OnAttach(const std::string& host_id,
                                         int session_id) {
  webworker_->attachDevTools(WebString::fromUTF8(host_id), session_id);
}

void SharedWorkerDevToolsAgent::OnReattach(const std::string& host_id,
                                           int session_id,
                                           const std::string& state) {
  webworker_->reattachDevTools(WebString::fromUTF8(host_id), session_id,
                               WebString::fromUTF8(state));
}

void SharedWorkerDevToolsAgent::OnDetach() {
  webworker_->detachDevTools();
}

void SharedWorkerDevToolsAgent::OnDispatchOnInspectorBackend(
    int session_id,
    int call_id,
    const std::string& method,
    const std::string& message) {
  webworker_->dispatchDevToolsMessage(session_id, call_id,
                                      WebString::fromUTF8(method),
                                      WebString::fromUTF8(message));
}

bool SharedWorkerDevToolsAgent::Send(IPC::Message* message) {
  return ChildThreadImpl::current()->Send(message);
}

}  // namespace content
