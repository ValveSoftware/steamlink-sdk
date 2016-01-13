// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_FORWARDING_AGENT_HOST_H
#define CONTENT_BROWSER_DEVTOOLS_FORWARDING_AGENT_HOST_H

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/public/browser/devtools_external_agent_proxy.h"
#include "content/public/browser/devtools_external_agent_proxy_delegate.h"

namespace content {

class ForwardingAgentHost
    : public DevToolsAgentHostImpl,
      public DevToolsExternalAgentProxy {
 public:
  ForwardingAgentHost(DevToolsExternalAgentProxyDelegate* delegate);

 private:
  virtual ~ForwardingAgentHost();

  // DevToolsExternalAgentProxy implementation.
  virtual void DispatchOnClientHost(const std::string& message) OVERRIDE;
  virtual void ConnectionClosed() OVERRIDE;

  // DevToolsAgentHostImpl implementation.
  virtual void Attach() OVERRIDE;
  virtual void Detach() OVERRIDE;
  virtual void DispatchOnInspectorBackend(const std::string& message) OVERRIDE;

  scoped_ptr<DevToolsExternalAgentProxyDelegate> delegate_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_FORWARDING_AGENT_HOST_H
