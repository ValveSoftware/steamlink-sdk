// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_AGENT_HOST_IMPL_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_AGENT_HOST_IMPL_H_

#include <string>

#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "content/public/browser/devtools_agent_host.h"

namespace IPC {
class Message;
}

namespace content {

// Describes interface for managing devtools agents from the browser process.
class CONTENT_EXPORT DevToolsAgentHostImpl : public DevToolsAgentHost {
 public:
  class CONTENT_EXPORT CloseListener {
   public:
    virtual void AgentHostClosing(DevToolsAgentHostImpl*) = 0;
   protected:
    virtual ~CloseListener() {}
  };

  // Informs the hosted agent that a client host has attached.
  virtual void Attach() = 0;

  // Informs the hosted agent that a client host has detached.
  virtual void Detach() = 0;

  // Sends a message to the agent hosted by this object.
  virtual void DispatchOnInspectorBackend(const std::string& message) = 0;

  void set_close_listener(CloseListener* listener) {
    close_listener_ = listener;
  }

  // DevToolsAgentHost implementation.
  virtual bool IsAttached() OVERRIDE;

  virtual void InspectElement(int x, int y) OVERRIDE;

  virtual std::string GetId() OVERRIDE;

  virtual RenderViewHost* GetRenderViewHost() OVERRIDE;

  virtual void DisconnectRenderViewHost() OVERRIDE;

  virtual void ConnectRenderViewHost(RenderViewHost* rvh) OVERRIDE;

  virtual bool IsWorker() const OVERRIDE;

 protected:
  DevToolsAgentHostImpl();
  virtual ~DevToolsAgentHostImpl();

  void NotifyCloseListener();

 private:
  CloseListener* close_listener_;
  const std::string id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_AGENT_HOST_IMPL_H_
