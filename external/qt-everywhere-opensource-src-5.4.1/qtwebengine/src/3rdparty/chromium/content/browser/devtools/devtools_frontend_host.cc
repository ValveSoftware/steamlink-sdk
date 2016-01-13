// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_frontend_host.h"

#include "content/browser/devtools/devtools_manager_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/devtools_messages.h"
#include "content/public/browser/devtools_client_host.h"
#include "content/public/browser/devtools_frontend_host_delegate.h"

namespace content {

// static
DevToolsClientHost* DevToolsClientHost::CreateDevToolsFrontendHost(
    WebContents* client_web_contents,
    DevToolsFrontendHostDelegate* delegate) {
  return new DevToolsFrontendHost(
      static_cast<WebContentsImpl*>(client_web_contents), delegate);
}

// static
void DevToolsClientHost::SetupDevToolsFrontendClient(
    RenderViewHost* frontend_rvh) {
  frontend_rvh->Send(new DevToolsMsg_SetupDevToolsClient(
      frontend_rvh->GetRoutingID()));
}

DevToolsFrontendHost::DevToolsFrontendHost(
    WebContentsImpl* web_contents,
    DevToolsFrontendHostDelegate* delegate)
    : WebContentsObserver(web_contents),
      delegate_(delegate) {
}

DevToolsFrontendHost::~DevToolsFrontendHost() {
  DevToolsManager::GetInstance()->ClientHostClosing(this);
}

void DevToolsFrontendHost::DispatchOnInspectorFrontend(
    const std::string& message) {
  if (!web_contents())
    return;
  RenderViewHostImpl* target_host =
      static_cast<RenderViewHostImpl*>(web_contents()->GetRenderViewHost());
  target_host->Send(new DevToolsClientMsg_DispatchOnInspectorFrontend(
      target_host->GetRoutingID(),
      message));
}

void DevToolsFrontendHost::InspectedContentsClosing() {
  delegate_->InspectedContentsClosing();
}

void DevToolsFrontendHost::ReplacedWithAnotherClient() {
}

bool DevToolsFrontendHost::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DevToolsFrontendHost, message)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_DispatchOnInspectorBackend,
                        OnDispatchOnInspectorBackend)
    IPC_MESSAGE_HANDLER(DevToolsHostMsg_DispatchOnEmbedder,
                        OnDispatchOnEmbedder)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DevToolsFrontendHost::RenderProcessGone(
    base::TerminationStatus status) {
  switch(status) {
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
      DevToolsManager::GetInstance()->ClientHostClosing(this);
      break;
    default:
      break;
  }
}

void DevToolsFrontendHost::OnDispatchOnInspectorBackend(
    const std::string& message) {
  DevToolsManagerImpl::GetInstance()->DispatchOnInspectorBackend(this, message);
}

void DevToolsFrontendHost::OnDispatchOnEmbedder(
    const std::string& message) {
  delegate_->DispatchOnEmbedder(message);
}

}  // namespace content
