// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_AGENT_HOST_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

namespace content {

class DevToolsExternalAgentProxyDelegate;
class RenderViewHost;
class WebContents;

// Describes interface for managing devtools agents from browser process.
class CONTENT_EXPORT DevToolsAgentHost
    : public base::RefCounted<DevToolsAgentHost> {
 public:
  // Returns DevToolsAgentHost with a given |id| or NULL of it does not exist.
  static scoped_refptr<DevToolsAgentHost> GetForId(const std::string& id);

  // Returns DevToolsAgentHost that can be used for inspecting |web_contents|.
  // New DevToolsAgentHost will be created if it does not exist.
  static scoped_refptr<DevToolsAgentHost> GetOrCreateFor(
      WebContents* web_contents);

  // Returns DevToolsAgentHost that can be used for inspecting |rvh|.
  // New DevToolsAgentHost will be created if it does not exist.
  static scoped_refptr<DevToolsAgentHost> GetOrCreateFor(RenderViewHost* rvh);

  // Returns true iff an instance of DevToolsAgentHost for the |rvh|
  // does exist.
  static bool HasFor(RenderViewHost* rvh);

  // Returns DevToolsAgentHost that can be used for inspecting shared worker
  // with given worker process host id and routing id.
  static scoped_refptr<DevToolsAgentHost> GetForWorker(int worker_process_id,
                                                       int worker_route_id);

  // Creates DevToolsAgentHost that communicates to the target by means of
  // provided |delegate|. |delegate| ownership is passed to the created agent
  // host.
  static scoped_refptr<DevToolsAgentHost> Create(
      DevToolsExternalAgentProxyDelegate* delegate);

  static bool IsDebuggerAttached(WebContents* web_contents);

  // Returns a list of all existing RenderViewHost's that can be debugged.
  static std::vector<RenderViewHost*> GetValidRenderViewHosts();

  // Returns true if there is a client attached.
  virtual bool IsAttached() = 0;

  // Starts inspecting element at position (|x|, |y|) in the specified page.
  virtual void InspectElement(int x, int y) = 0;

  // Returns the unique id of the agent.
  virtual std::string GetId() = 0;

  // Returns render view host instance for this host if any.
  virtual RenderViewHost* GetRenderViewHost() = 0;

  // Temporarily detaches render view host from this host. Must be followed by
  // a call to ConnectRenderViewHost (may leak the host instance otherwise).
  virtual void DisconnectRenderViewHost() = 0;

  // Attaches render view host to this host.
  virtual void ConnectRenderViewHost(RenderViewHost* rvh) = 0;

  // Returns true if DevToolsAgentHost is for worker.
  virtual bool IsWorker() const = 0;

 protected:
  friend class base::RefCounted<DevToolsAgentHost>;
  virtual ~DevToolsAgentHost() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_AGENT_HOST_H_
