// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_manager_impl.h"

#include <vector>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "content/browser/devtools/devtools_netlog_observer.h"
#include "content/browser/devtools/render_view_devtools_agent_host.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/devtools_client_host.h"
#include "content/public/browser/devtools_manager_delegate.h"

namespace content {

// static
DevToolsManager* DevToolsManager::GetInstance() {
  return DevToolsManagerImpl::GetInstance();
}

// static
DevToolsManagerImpl* DevToolsManagerImpl::GetInstance() {
  return Singleton<DevToolsManagerImpl>::get();
}

DevToolsManagerImpl::DevToolsManagerImpl()
    : delegate_(GetContentClient()->browser()->GetDevToolsManagerDelegate()) {
}

DevToolsManagerImpl::~DevToolsManagerImpl() {
  DCHECK(agent_to_client_host_.empty());
  DCHECK(client_to_agent_host_.empty());
}

void DevToolsManagerImpl::Inspect(BrowserContext* browser_context,
                                  DevToolsAgentHost* agent_host) {
  if (delegate_)
    delegate_->Inspect(browser_context, agent_host);
}

DevToolsClientHost* DevToolsManagerImpl::GetDevToolsClientHostFor(
    DevToolsAgentHostImpl* agent_host_impl) {
  AgentToClientHostMap::iterator it =
      agent_to_client_host_.find(agent_host_impl);
  if (it != agent_to_client_host_.end())
    return it->second;
  return NULL;
}

DevToolsAgentHost* DevToolsManagerImpl::GetDevToolsAgentHostFor(
    DevToolsClientHost* client_host) {
  ClientToAgentHostMap::iterator it = client_to_agent_host_.find(client_host);
  if (it != client_to_agent_host_.end())
    return it->second.get();
  return NULL;
}

void DevToolsManagerImpl::RegisterDevToolsClientHostFor(
    DevToolsAgentHost* agent_host,
    DevToolsClientHost* client_host) {
  DevToolsAgentHostImpl* agent_host_impl =
      static_cast<DevToolsAgentHostImpl*>(agent_host);
  DevToolsClientHost* old_client_host =
      GetDevToolsClientHostFor(agent_host_impl);
  if (old_client_host) {
    old_client_host->ReplacedWithAnotherClient();
    UnregisterDevToolsClientHostFor(agent_host_impl);
  }
  BindClientHost(agent_host_impl, client_host);
  agent_host_impl->Attach();
}

bool DevToolsManagerImpl::DispatchOnInspectorBackend(
    DevToolsClientHost* from,
    const std::string& message) {
  DevToolsAgentHost* agent_host = GetDevToolsAgentHostFor(from);
  if (!agent_host)
    return false;
  DevToolsAgentHostImpl* agent_host_impl =
      static_cast<DevToolsAgentHostImpl*>(agent_host);
  agent_host_impl->DispatchOnInspectorBackend(message);
  return true;
}

void DevToolsManagerImpl::DispatchOnInspectorFrontend(
    DevToolsAgentHost* agent_host,
    const std::string& message) {
  DevToolsAgentHostImpl* agent_host_impl =
      static_cast<DevToolsAgentHostImpl*>(agent_host);
  DevToolsClientHost* client_host = GetDevToolsClientHostFor(agent_host_impl);
  if (!client_host) {
    // Client window was closed while there were messages
    // being sent to it.
    return;
  }
  client_host->DispatchOnInspectorFrontend(message);
}

void DevToolsManagerImpl::ClientHostClosing(DevToolsClientHost* client_host) {
  DevToolsAgentHost* agent_host = GetDevToolsAgentHostFor(client_host);
  if (!agent_host)
    return;
  DevToolsAgentHostImpl* agent_host_impl =
      static_cast<DevToolsAgentHostImpl*>(agent_host);
  UnbindClientHost(agent_host_impl, client_host);
}

void DevToolsManagerImpl::AgentHostClosing(DevToolsAgentHostImpl* agent_host) {
  UnregisterDevToolsClientHostFor(agent_host);
}

void DevToolsManagerImpl::UnregisterDevToolsClientHostFor(
    DevToolsAgentHostImpl* agent_host_impl) {
  DevToolsClientHost* client_host = GetDevToolsClientHostFor(agent_host_impl);
  if (!client_host)
    return;
  UnbindClientHost(agent_host_impl, client_host);
  client_host->InspectedContentsClosing();
}

void DevToolsManagerImpl::BindClientHost(
    DevToolsAgentHostImpl* agent_host,
    DevToolsClientHost* client_host) {
  DCHECK(agent_to_client_host_.find(agent_host) ==
      agent_to_client_host_.end());
  DCHECK(client_to_agent_host_.find(client_host) ==
      client_to_agent_host_.end());

  if (client_to_agent_host_.empty()) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&DevToolsNetLogObserver::Attach));
  }
  agent_to_client_host_[agent_host] = client_host;
  client_to_agent_host_[client_host] = agent_host;
  agent_host->set_close_listener(this);
}

void DevToolsManagerImpl::UnbindClientHost(DevToolsAgentHostImpl* agent_host,
                                           DevToolsClientHost* client_host) {
  DCHECK(agent_host);
  scoped_refptr<DevToolsAgentHostImpl> protect(agent_host);
  DCHECK(agent_to_client_host_.find(agent_host)->second ==
      client_host);
  DCHECK(client_to_agent_host_.find(client_host)->second.get() == agent_host);
  agent_host->set_close_listener(NULL);

  agent_to_client_host_.erase(agent_host);
  client_to_agent_host_.erase(client_host);

  if (client_to_agent_host_.empty()) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&DevToolsNetLogObserver::Detach));
  }
  // Lazy agent hosts can be deleted from within detach.
  // Do not access agent_host below this line.
  agent_host->Detach();
}

void DevToolsManagerImpl::CloseAllClientHosts() {
  std::vector<DevToolsAgentHostImpl*> agents;
  for (AgentToClientHostMap::iterator it =
           agent_to_client_host_.begin();
       it != agent_to_client_host_.end(); ++it) {
    agents.push_back(it->first);
  }
  for (std::vector<DevToolsAgentHostImpl*>::iterator it = agents.begin();
       it != agents.end(); ++it) {
    UnregisterDevToolsClientHostFor(*it);
  }
}

void DevToolsManagerImpl::AddAgentStateCallback(const Callback& callback) {
  callbacks_.push_back(&callback);
}

void DevToolsManagerImpl::RemoveAgentStateCallback(const Callback& callback) {
  CallbackContainer::iterator it =
      std::find(callbacks_.begin(), callbacks_.end(), &callback);
  DCHECK(it != callbacks_.end());
  callbacks_.erase(it);
}

void DevToolsManagerImpl::NotifyObservers(DevToolsAgentHost* agent_host,
                                          bool attached) {
  CallbackContainer copy(callbacks_);
  if (delegate_)
    delegate_->DevToolsAgentStateChanged(agent_host, attached);
  for (CallbackContainer::iterator it = copy.begin(); it != copy.end(); ++it)
     (*it)->Run(agent_host, attached);
}

}  // namespace content
