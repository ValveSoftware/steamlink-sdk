// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/forward.h"

namespace content {

class DevToolsAgentHostImpl;

class DevToolsSession : public protocol::FrontendChannel {
 public:
  DevToolsSession(DevToolsAgentHostImpl* agent_host, int session_id);
  ~DevToolsSession() override;

  int session_id() { return session_id_; }
  protocol::UberDispatcher* dispatcher() { return dispatcher_.get(); }

 private:
  // protocol::FrontendChannel implementation.
  void sendProtocolResponse(int call_id, const std::string& message) override;
  void sendProtocolNotification(const std::string& message) override;
  void flushProtocolNotifications() override;

  DevToolsAgentHostImpl* agent_host_;
  int session_id_;
  std::unique_ptr<protocol::UberDispatcher> dispatcher_;
};

}  // namespace content
