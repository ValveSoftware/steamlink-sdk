// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/forwarding_agent_host.h"

#include "base/bind.h"
#include "content/browser/devtools/protocol/inspector_handler.h"

namespace content {

ForwardingAgentHost::ForwardingAgentHost(
    DevToolsExternalAgentProxyDelegate* delegate)
      : delegate_(delegate) {
}

ForwardingAgentHost::~ForwardingAgentHost() {
}

void ForwardingAgentHost::DispatchOnClientHost(const std::string& message) {
  SendMessageToClient(session_id(), message);
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

DevToolsAgentHost::Type ForwardingAgentHost::GetType() {
  return TYPE_EXTERNAL;
}

std::string ForwardingAgentHost::GetTitle() {
  return "";
}

GURL ForwardingAgentHost::GetURL() {
  return GURL();
}

bool ForwardingAgentHost::Activate() {
  return false;
}

bool ForwardingAgentHost::Close() {
  return false;
}

}  // content
