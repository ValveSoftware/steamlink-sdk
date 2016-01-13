// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/forwarding_agent_host.h"

#include "content/browser/devtools/devtools_manager_impl.h"

namespace content {

ForwardingAgentHost::ForwardingAgentHost(
    DevToolsExternalAgentProxyDelegate* delegate)
      : delegate_(delegate) {
}

ForwardingAgentHost::~ForwardingAgentHost() {
}

void ForwardingAgentHost::DispatchOnClientHost(const std::string& message) {
  DevToolsManagerImpl::GetInstance()->DispatchOnInspectorFrontend(
      this, message);
}

void ForwardingAgentHost::ConnectionClosed() {
  NotifyCloseListener();
}

void ForwardingAgentHost::Attach() {
  delegate_->Attach(this);
}

void ForwardingAgentHost::Detach() {
  delegate_->Detach();
}

void ForwardingAgentHost::DispatchOnInspectorBackend(
    const std::string& message) {
  delegate_->SendMessageToBackend(message);
}

}  // content
