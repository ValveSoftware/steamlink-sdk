// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_session.h"

#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/protocol/protocol.h"

namespace content {

DevToolsSession::DevToolsSession(
    DevToolsAgentHostImpl* agent_host,
    int session_id)
    : agent_host_(agent_host),
      session_id_(session_id),
      dispatcher_(new protocol::UberDispatcher(this)) {
}

DevToolsSession::~DevToolsSession() {}

void DevToolsSession::sendProtocolResponse(
    int call_id,
    const std::string& message) {
  agent_host_->SendMessageToClient(session_id_, message);
}

void DevToolsSession::sendProtocolNotification(const std::string& message) {
  agent_host_->SendMessageToClient(session_id_, message);
}

void DevToolsSession::flushProtocolNotifications() {
}

}  // namespace content
