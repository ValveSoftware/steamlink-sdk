// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_PROTOCOL_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_PROTOCOL_HANDLER_H_

#include "content/browser/devtools/protocol/devtools_protocol_dispatcher.h"

namespace content {

class DevToolsAgentHost;
class DevToolsAgentHostImpl;
class DevToolsProtocolDelegate;

class DevToolsProtocolHandler {
 public:
  using Response = DevToolsProtocolClient::Response;

  explicit DevToolsProtocolHandler(DevToolsAgentHostImpl* agent_host);
  virtual ~DevToolsProtocolHandler();

  void HandleMessage(int session_id, const std::string& message);
  bool HandleOptionalMessage(int session_id,
                             const std::string& message,
                             int* call_id,
                             std::string* method);

  DevToolsProtocolDispatcher* dispatcher() { return &dispatcher_; }

 private:
  std::unique_ptr<base::DictionaryValue> ParseCommand(
      int session_id,
      const std::string& message);
  bool PassCommandToDelegate(int session_id, base::DictionaryValue* command);
  void HandleCommand(int session_id,
                     std::unique_ptr<base::DictionaryValue> command);
  bool HandleOptionalCommand(int session_id,
                             std::unique_ptr<base::DictionaryValue> command,
                             int* call_id,
                             std::string* method);

  DevToolsAgentHost* agent_host_;
  DevToolsProtocolClient client_;
  DevToolsProtocolDispatcher dispatcher_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_PROTOCOL_HANDLER_H_
