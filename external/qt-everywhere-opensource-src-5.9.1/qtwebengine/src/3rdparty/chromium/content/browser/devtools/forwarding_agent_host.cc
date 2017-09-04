// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/forwarding_agent_host.h"

#include "base/bind.h"
#include "content/browser/devtools/devtools_session.h"
#include "content/browser/devtools/protocol/inspector_handler.h"

namespace content {

ForwardingAgentHost::ForwardingAgentHost(
    const std::string& id,
    std::unique_ptr<DevToolsExternalAgentProxyDelegate> delegate)
      : DevToolsAgentHostImpl(id),
        delegate_(std::move(delegate)) {
  NotifyCreated();
}

ForwardingAgentHost::~ForwardingAgentHost() {
}

void ForwardingAgentHost::DispatchOnClientHost(const std::string& message) {
  SendMessageToClient(session() ? session()->session_id() : 0, message);
}

void ForwardingAgentHost::ConnectionClosed() {
  HostClosed();
}

void ForwardingAgentHost::Attach() {
  delegate_->Attach(this);
}

void ForwardingAgentHost::Detach() {
  delegate_->Detach();
}

bool ForwardingAgentHost::DispatchProtocolMessage(
    const std::string& message) {
  delegate_->SendMessageToBackend(message);
  return true;
}

std::string ForwardingAgentHost::GetType() {
  return delegate_->GetType();
}

std::string ForwardingAgentHost::GetTitle() {
  return delegate_->GetTitle();
}

GURL ForwardingAgentHost::GetURL() {
  return delegate_->GetURL();
}

GURL ForwardingAgentHost::GetFaviconURL() {
  return delegate_->GetFaviconURL();
}

std::string ForwardingAgentHost::GetFrontendURL() {
  return delegate_->GetFrontendURL();
}

bool ForwardingAgentHost::Activate() {
  return delegate_->Activate();
}

void ForwardingAgentHost::Reload() {
  delegate_->Reload();
}

bool ForwardingAgentHost::Close() {
  return delegate_->Close();
}

}  // content
