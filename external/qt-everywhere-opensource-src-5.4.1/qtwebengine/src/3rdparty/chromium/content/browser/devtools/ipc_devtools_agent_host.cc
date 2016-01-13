// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/ipc_devtools_agent_host.h"

#include "content/common/devtools_messages.h"

namespace content {

void IPCDevToolsAgentHost::Attach() {
  SendMessageToAgent(new DevToolsAgentMsg_Attach(MSG_ROUTING_NONE, GetId()));
  OnClientAttached();
}

void IPCDevToolsAgentHost::Detach() {
  SendMessageToAgent(new DevToolsAgentMsg_Detach(MSG_ROUTING_NONE));
  OnClientDetached();
}

void IPCDevToolsAgentHost::DispatchOnInspectorBackend(
    const std::string& message) {
  SendMessageToAgent(new DevToolsAgentMsg_DispatchOnInspectorBackend(
      MSG_ROUTING_NONE, message));
}

void IPCDevToolsAgentHost::InspectElement(int x, int y) {
  SendMessageToAgent(new DevToolsAgentMsg_InspectElement(MSG_ROUTING_NONE,
                                                         GetId(), x, y));
}

IPCDevToolsAgentHost::~IPCDevToolsAgentHost() {
}

void IPCDevToolsAgentHost::Reattach(const std::string& saved_agent_state) {
  SendMessageToAgent(new DevToolsAgentMsg_Reattach(
      MSG_ROUTING_NONE, GetId(), saved_agent_state));
  OnClientAttached();
}

}  // namespace content
