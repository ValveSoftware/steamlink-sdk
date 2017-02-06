// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/worker_devtools_agent_host.h"

#include "content/browser/devtools/devtools_protocol_handler.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

namespace content {

BrowserContext* WorkerDevToolsAgentHost::GetBrowserContext() {
  RenderProcessHost* rph = RenderProcessHost::FromID(worker_id_.first);
  return rph ? rph->GetBrowserContext() : nullptr;
}

void WorkerDevToolsAgentHost::Attach() {
  if (state_ != WORKER_INSPECTED) {
    state_ = WORKER_INSPECTED;
    AttachToWorker();
  }
  if (RenderProcessHost* host = RenderProcessHost::FromID(worker_id_.first))
    host->Send(
        new DevToolsAgentMsg_Attach(worker_id_.second, GetId(), session_id()));
  OnAttachedStateChanged(true);
  DevToolsAgentHostImpl::NotifyCallbacks(this, true);
}

void WorkerDevToolsAgentHost::Detach() {
  if (RenderProcessHost* host = RenderProcessHost::FromID(worker_id_.first))
    host->Send(new DevToolsAgentMsg_Detach(worker_id_.second));
  OnAttachedStateChanged(false);
  if (state_ == WORKER_INSPECTED) {
    state_ = WORKER_UNINSPECTED;
    DetachFromWorker();
  } else if (state_ == WORKER_PAUSED_FOR_REATTACH) {
    state_ = WORKER_UNINSPECTED;
  }
  DevToolsAgentHostImpl::NotifyCallbacks(this, false);
}

bool WorkerDevToolsAgentHost::DispatchProtocolMessage(
    const std::string& message) {
  if (state_ != WORKER_INSPECTED)
    return true;

  int call_id;
  std::string method;
  if (protocol_handler_->HandleOptionalMessage(session_id(), message, &call_id,
                                               &method))
    return true;

  if (RenderProcessHost* host = RenderProcessHost::FromID(worker_id_.first)) {
    host->Send(new DevToolsAgentMsg_DispatchOnInspectorBackend(
        worker_id_.second, session_id(), call_id, method, message));
  }
  return true;
}

bool WorkerDevToolsAgentHost::OnMessageReceived(
    const IPC::Message& msg) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WorkerDevToolsAgentHost, msg)
  IPC_MESSAGE_HANDLER(DevToolsClientMsg_DispatchOnInspectorFrontend,
                      OnDispatchOnInspectorFrontend)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void WorkerDevToolsAgentHost::PauseForDebugOnStart() {
  DCHECK(state_ == WORKER_UNINSPECTED);
  state_ = WORKER_PAUSED_FOR_DEBUG_ON_START;
}

bool WorkerDevToolsAgentHost::IsPausedForDebugOnStart() {
  return state_ == WORKER_PAUSED_FOR_DEBUG_ON_START;
}

void WorkerDevToolsAgentHost::WorkerReadyForInspection() {
  if (state_ == WORKER_PAUSED_FOR_REATTACH) {
    DCHECK(IsAttached());
    state_ = WORKER_INSPECTED;
    AttachToWorker();
    if (RenderProcessHost* host = RenderProcessHost::FromID(worker_id_.first)) {
      host->Send(new DevToolsAgentMsg_Reattach(
          worker_id_.second, GetId(), session_id(),
          chunk_processor_.state_cookie()));
    }
    OnAttachedStateChanged(true);
  }
}

void WorkerDevToolsAgentHost::WorkerRestarted(WorkerId worker_id) {
  DCHECK_EQ(WORKER_TERMINATED, state_);
  state_ = IsAttached() ? WORKER_PAUSED_FOR_REATTACH : WORKER_UNINSPECTED;
  worker_id_ = worker_id;
  WorkerCreated();
}

void WorkerDevToolsAgentHost::WorkerDestroyed() {
  DCHECK_NE(WORKER_TERMINATED, state_);
  if (state_ == WORKER_INSPECTED) {
    DCHECK(IsAttached());
    // Client host is debugging this worker agent host.
    devtools::inspector::Client inspector(this);
    inspector.TargetCrashed(
        devtools::inspector::TargetCrashedParams::Create());
    DetachFromWorker();
  }
  state_ = WORKER_TERMINATED;
  Release();  // Balanced in WorkerCreated().
}

bool WorkerDevToolsAgentHost::IsTerminated() {
  return state_ == WORKER_TERMINATED;
}

WorkerDevToolsAgentHost::WorkerDevToolsAgentHost(WorkerId worker_id)
    : protocol_handler_(new DevToolsProtocolHandler(this)),
      chunk_processor_(base::Bind(&WorkerDevToolsAgentHost::SendMessageToClient,
                                  base::Unretained(this))),
      state_(WORKER_UNINSPECTED),
      worker_id_(worker_id) {
  WorkerCreated();
}

WorkerDevToolsAgentHost::~WorkerDevToolsAgentHost() {
  DCHECK_EQ(WORKER_TERMINATED, state_);
}

void WorkerDevToolsAgentHost::OnAttachedStateChanged(bool attached) {
}

void WorkerDevToolsAgentHost::AttachToWorker() {
  if (RenderProcessHost* host = RenderProcessHost::FromID(worker_id_.first))
    host->AddRoute(worker_id_.second, this);
}

void WorkerDevToolsAgentHost::DetachFromWorker() {
  if (RenderProcessHost* host = RenderProcessHost::FromID(worker_id_.first))
    host->RemoveRoute(worker_id_.second);
}

void WorkerDevToolsAgentHost::WorkerCreated() {
  AddRef();  // Balanced in WorkerDestroyed()
}

void WorkerDevToolsAgentHost::OnDispatchOnInspectorFrontend(
    const DevToolsMessageChunk& message) {
  if (!IsAttached())
    return;

  chunk_processor_.ProcessChunkedMessageFromAgent(message);
}

}  // namespace content
