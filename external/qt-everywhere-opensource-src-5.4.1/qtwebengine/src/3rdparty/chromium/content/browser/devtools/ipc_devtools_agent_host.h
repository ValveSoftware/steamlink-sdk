// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_IPC_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_IPC_DEVTOOLS_AGENT_HOST_H_

#include "content/browser/devtools/devtools_agent_host_impl.h"

namespace IPC {
class Message;
}

namespace content {

class CONTENT_EXPORT IPCDevToolsAgentHost : public DevToolsAgentHostImpl {
 public:
  // DevToolsAgentHostImpl implementation.
  virtual void Attach() OVERRIDE;
  virtual void Detach() OVERRIDE;
  virtual void DispatchOnInspectorBackend(const std::string& message) OVERRIDE;
  virtual void InspectElement(int x, int y) OVERRIDE;

 protected:
  virtual ~IPCDevToolsAgentHost();

  void Reattach(const std::string& saved_agent_state);

  virtual void SendMessageToAgent(IPC::Message* msg) = 0;
  virtual void OnClientAttached() = 0;
  virtual void OnClientDetached() = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_IPC_DEVTOOLS_AGENT_HOST_H_
