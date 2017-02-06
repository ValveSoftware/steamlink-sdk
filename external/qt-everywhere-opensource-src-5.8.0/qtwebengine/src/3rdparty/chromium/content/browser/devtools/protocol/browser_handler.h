// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_BROWSER_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_BROWSER_HANDLER_H_

#include <stdint.h>

#include "content/browser/devtools/protocol/devtools_protocol_dispatcher.h"
#include "content/public/browser/devtools_agent_host.h"

namespace net {
class ServerSocket;
}

// Windows headers will redefine SendMessage.
#ifdef SendMessage
#undef SendMessage
#endif

namespace content {
namespace devtools {
namespace browser {

class BrowserHandler : public DevToolsAgentHostClient {
 public:
  using Response = DevToolsProtocolClient::Response;

  BrowserHandler();
  ~BrowserHandler() override;

  void SetClient(std::unique_ptr<Client> client);

  using TargetInfos = std::vector<scoped_refptr<devtools::browser::TargetInfo>>;
  Response GetTargets(TargetInfos* infos);
  Response Attach(const std::string& targetId);
  Response Detach(const std::string& targetId);
  Response SendMessage(const std::string& targetId, const std::string& message);

 private:
  void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                               const std::string& message) override;

  void AgentHostClosed(DevToolsAgentHost* agent_host,
                       bool replaced_with_another_client) override;

  std::unique_ptr<Client> client_;
  DISALLOW_COPY_AND_ASSIGN(BrowserHandler);
};

}  // namespace browser
}  // namespace devtools
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_BROWSER_HANDLER_H_
