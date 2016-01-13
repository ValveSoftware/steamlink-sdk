// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_agent_host_impl.h"

#include <map>

#include "base/basictypes.h"
#include "base/guid.h"
#include "base/lazy_instance.h"
#include "content/browser/devtools/devtools_manager_impl.h"
#include "content/browser/devtools/forwarding_agent_host.h"
#include "content/public/browser/browser_thread.h"

namespace content {

namespace {
typedef std::map<std::string, DevToolsAgentHostImpl*> Instances;
base::LazyInstance<Instances>::Leaky g_instances = LAZY_INSTANCE_INITIALIZER;
}  // namespace

DevToolsAgentHostImpl::DevToolsAgentHostImpl()
    : close_listener_(NULL),
      id_(base::GenerateGUID()) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  g_instances.Get()[id_] = this;
}

DevToolsAgentHostImpl::~DevToolsAgentHostImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  g_instances.Get().erase(g_instances.Get().find(id_));
}

//static
scoped_refptr<DevToolsAgentHost> DevToolsAgentHost::GetForId(
    const std::string& id) {
  if (g_instances == NULL)
    return NULL;
  Instances::iterator it = g_instances.Get().find(id);
  if (it == g_instances.Get().end())
    return NULL;
  return it->second;
}

//static
scoped_refptr<DevToolsAgentHost> DevToolsAgentHost::Create(
    DevToolsExternalAgentProxyDelegate* delegate) {
  return new ForwardingAgentHost(delegate);
}

bool DevToolsAgentHostImpl::IsAttached() {
  return !!DevToolsManagerImpl::GetInstance()->GetDevToolsClientHostFor(this);
}

void DevToolsAgentHostImpl::InspectElement(int x, int y) {
}

std::string DevToolsAgentHostImpl::GetId() {
  return id_;
}

RenderViewHost* DevToolsAgentHostImpl::GetRenderViewHost() {
  return NULL;
}

void DevToolsAgentHostImpl::DisconnectRenderViewHost() {}

void DevToolsAgentHostImpl::ConnectRenderViewHost(RenderViewHost* rvh) {}

bool DevToolsAgentHostImpl::IsWorker() const {
  return false;
}

void DevToolsAgentHostImpl::NotifyCloseListener() {
  if (close_listener_) {
    scoped_refptr<DevToolsAgentHostImpl> protect(this);
    close_listener_->AgentHostClosing(this);
    close_listener_ = NULL;
  }
}

}  // namespace content
