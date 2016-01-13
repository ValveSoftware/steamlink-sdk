// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_MANAGER_IMPL_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_MANAGER_IMPL_H_

#include <map>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/devtools_client_host.h"
#include "content/public/browser/devtools_manager.h"

class GURL;

namespace IPC {
class Message;
}

namespace content {

class BrowserContext;
class DevToolsManagerDelegate;
class RenderViewHost;

// This class is a singleton that manages DevToolsClientHost instances and
// routes messages between developer tools clients and agents.
//
// Methods below that accept inspected RenderViewHost as a parameter are
// just convenience methods that call corresponding methods accepting
// DevToolAgentHost.
class CONTENT_EXPORT DevToolsManagerImpl
    : public DevToolsAgentHostImpl::CloseListener,
      public DevToolsManager {
 public:
  // Returns single instance of this class. The instance is destroyed on the
  // browser main loop exit so this method MUST NOT be called after that point.
  static DevToolsManagerImpl* GetInstance();

  DevToolsManagerImpl();
  virtual ~DevToolsManagerImpl();

  // Opens the inspector for |agent_host|.
  void Inspect(BrowserContext* browser_context, DevToolsAgentHost* agent_host);

  void DispatchOnInspectorFrontend(DevToolsAgentHost* agent_host,
                                   const std::string& message);

  DevToolsManagerDelegate* delegate() const { return delegate_.get(); }

  // DevToolsManager implementation
  virtual bool DispatchOnInspectorBackend(DevToolsClientHost* from,
                                          const std::string& message) OVERRIDE;
  virtual void CloseAllClientHosts() OVERRIDE;
  virtual DevToolsAgentHost* GetDevToolsAgentHostFor(
      DevToolsClientHost* client_host) OVERRIDE;
  virtual void RegisterDevToolsClientHostFor(
      DevToolsAgentHost* agent_host,
      DevToolsClientHost* client_host) OVERRIDE;
  virtual void ClientHostClosing(DevToolsClientHost* host) OVERRIDE;
  virtual void AddAgentStateCallback(const Callback& callback) OVERRIDE;
  virtual void RemoveAgentStateCallback(const Callback& callback) OVERRIDE;

 private:
  friend class DevToolsAgentHostImpl;
  friend class RenderViewDevToolsAgentHost;
  friend struct DefaultSingletonTraits<DevToolsManagerImpl>;

  // DevToolsAgentHost::CloseListener implementation.
  virtual void AgentHostClosing(DevToolsAgentHostImpl* host) OVERRIDE;

  void BindClientHost(DevToolsAgentHostImpl* agent_host,
                      DevToolsClientHost* client_host);
  void UnbindClientHost(DevToolsAgentHostImpl* agent_host,
                        DevToolsClientHost* client_host);

  DevToolsClientHost* GetDevToolsClientHostFor(
      DevToolsAgentHostImpl* agent_host);

  void UnregisterDevToolsClientHostFor(DevToolsAgentHostImpl* agent_host);

  void NotifyObservers(DevToolsAgentHost* agent_host, bool attached);

  // These two maps are for tracking dependencies between inspected contents and
  // their DevToolsClientHosts. They are useful for routing devtools messages
  // and allow us to have at most one devtools client host per contents.
  //
  // DevToolsManagerImpl starts listening to DevToolsClientHosts when they are
  // put into these maps and removes them when they are closing.
  typedef std::map<DevToolsAgentHostImpl*, DevToolsClientHost*>
      AgentToClientHostMap;
  AgentToClientHostMap agent_to_client_host_;

  typedef std::map<DevToolsClientHost*, scoped_refptr<DevToolsAgentHostImpl> >
      ClientToAgentHostMap;
  ClientToAgentHostMap client_to_agent_host_;

  typedef std::vector<const Callback*> CallbackContainer;
  CallbackContainer callbacks_;

  scoped_ptr<DevToolsManagerDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsManagerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_MANAGER_IMPL_H_
