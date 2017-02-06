// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_agent_host_impl.h"

#include <map>
#include <vector>

#include "base/guid.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "content/browser/devtools/devtools_manager.h"
#include "content/browser/devtools/forwarding_agent_host.h"
#include "content/browser/devtools/protocol/devtools_protocol_dispatcher.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/devtools/service_worker_devtools_agent_host.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/browser/devtools/shared_worker_devtools_agent_host.h"
#include "content/browser/devtools/shared_worker_devtools_manager.h"
#include "content/public/browser/browser_thread.h"

namespace content {

namespace {
typedef std::map<std::string, DevToolsAgentHostImpl*> Instances;
base::LazyInstance<Instances>::Leaky g_instances = LAZY_INSTANCE_INITIALIZER;

typedef std::vector<const DevToolsAgentHost::AgentStateCallback*>
    AgentStateCallbacks;
base::LazyInstance<AgentStateCallbacks>::Leaky g_callbacks =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

// static
std::string DevToolsAgentHost::GetProtocolVersion() {
  return std::string(devtools::kProtocolVersion);
}

// static
bool DevToolsAgentHost::IsSupportedProtocolVersion(const std::string& version) {
  return devtools::IsSupportedProtocolVersion(version);
}

// static
DevToolsAgentHost::List DevToolsAgentHost::GetOrCreateAll() {
  List result;
  SharedWorkerDevToolsAgentHost::List shared_list;
  SharedWorkerDevToolsManager::GetInstance()->AddAllAgentHosts(&shared_list);
  for (const auto& host : shared_list)
    result.push_back(host);

  ServiceWorkerDevToolsAgentHost::List service_list;
  ServiceWorkerDevToolsManager::GetInstance()->AddAllAgentHosts(&service_list);
  for (const auto& host : service_list)
    result.push_back(host);

  RenderFrameDevToolsAgentHost::AddAllAgentHosts(&result);
  return result;
}

// Called on the UI thread.
// static
scoped_refptr<DevToolsAgentHost> DevToolsAgentHost::GetForWorker(
    int worker_process_id,
    int worker_route_id) {
  if (scoped_refptr<DevToolsAgentHost> host =
      SharedWorkerDevToolsManager::GetInstance()
          ->GetDevToolsAgentHostForWorker(worker_process_id,
                                          worker_route_id)) {
    return host;
  }
  return ServiceWorkerDevToolsManager::GetInstance()
      ->GetDevToolsAgentHostForWorker(worker_process_id, worker_route_id);
}

DevToolsAgentHostImpl::DevToolsAgentHostImpl()
    : id_(base::GenerateGUID()), session_id_(0), client_(NULL) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  g_instances.Get()[id_] = this;
}

DevToolsAgentHostImpl::~DevToolsAgentHostImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  g_instances.Get().erase(g_instances.Get().find(id_));
}

// static
scoped_refptr<DevToolsAgentHost> DevToolsAgentHost::GetForId(
    const std::string& id) {
  if (g_instances == NULL)
    return NULL;
  Instances::iterator it = g_instances.Get().find(id);
  if (it == g_instances.Get().end())
    return NULL;
  return it->second;
}

// static
scoped_refptr<DevToolsAgentHost> DevToolsAgentHost::Create(
    DevToolsExternalAgentProxyDelegate* delegate) {
  return new ForwardingAgentHost(delegate);
}

bool DevToolsAgentHostImpl::InnerAttach(DevToolsAgentHostClient* client,
                                        bool force) {
  if (client_ && !force)
    return false;

  scoped_refptr<DevToolsAgentHostImpl> protect(this);
  ++session_id_;
  if (client_) {
    client_->AgentHostClosed(this, true);
    InnerDetach();
  }
  client_ = client;
  Attach();
  return true;
}

bool DevToolsAgentHostImpl::AttachClient(DevToolsAgentHostClient* client) {
  return InnerAttach(client, false);
}

void DevToolsAgentHostImpl::ForceAttachClient(DevToolsAgentHostClient* client) {
  InnerAttach(client, true);
}

bool DevToolsAgentHostImpl::DetachClient(DevToolsAgentHostClient* client) {
  if (!client_ || client_ != client)
    return false;

  scoped_refptr<DevToolsAgentHostImpl> protect(this);
  client_ = NULL;
  InnerDetach();
  return true;
}

bool DevToolsAgentHostImpl::DispatchProtocolMessage(
    DevToolsAgentHostClient* client,
    const std::string& message) {
  if (!client_ || client_ != client)
    return false;
  return DispatchProtocolMessage(message);
}

void DevToolsAgentHostImpl::InnerDetach() {
  Detach();
  io_context_.DiscardAllStreams();
}

bool DevToolsAgentHostImpl::IsAttached() {
  return !!client_;
}

void DevToolsAgentHostImpl::InspectElement(int x, int y) {
}

std::string DevToolsAgentHostImpl::GetId() {
  return id_;
}

BrowserContext* DevToolsAgentHostImpl::GetBrowserContext() {
  return nullptr;
}

WebContents* DevToolsAgentHostImpl::GetWebContents() {
  return NULL;
}

void DevToolsAgentHostImpl::DisconnectWebContents() {
}

void DevToolsAgentHostImpl::ConnectWebContents(WebContents* wc) {
}

void DevToolsAgentHostImpl::SendProtocolResponse(int session_id,
                                                 const std::string& message) {
  SendMessageToClient(session_id, message);
}

void DevToolsAgentHostImpl::SendProtocolNotification(
    const std::string& message) {
  SendMessageToClient(session_id_, message);
}

void DevToolsAgentHostImpl::HostClosed() {
  if (!client_)
    return;

  scoped_refptr<DevToolsAgentHostImpl> protect(this);
  // Clear |client_| before notifying it.
  DevToolsAgentHostClient* client = client_;
  client_ = NULL;
  client->AgentHostClosed(this, false);
}

void DevToolsAgentHostImpl::SendMessageToClient(int session_id,
                                                const std::string& message) {
  if (!client_)
    return;
  // Filter any messages from previous sessions.
  if (session_id != session_id_)
    return;
  client_->DispatchProtocolMessage(this, message);
}

// static
void DevToolsAgentHost::DetachAllClients() {
  if (g_instances == NULL)
    return;

  // Make a copy, since detaching may lead to agent destruction, which
  // removes it from the instances.
  Instances copy = g_instances.Get();
  for (Instances::iterator it(copy.begin()); it != copy.end(); ++it) {
    DevToolsAgentHostImpl* agent_host = it->second;
    if (agent_host->client_) {
      scoped_refptr<DevToolsAgentHostImpl> protect(agent_host);
      // Clear |client_| before notifying it.
      DevToolsAgentHostClient* client = agent_host->client_;
      agent_host->client_ = NULL;
      client->AgentHostClosed(agent_host, true);
      agent_host->InnerDetach();
    }
  }
}

// static
void DevToolsAgentHost::AddAgentStateCallback(
    const AgentStateCallback& callback) {
  g_callbacks.Get().push_back(&callback);
}

// static
void DevToolsAgentHost::RemoveAgentStateCallback(
    const AgentStateCallback& callback) {
  if (g_callbacks == NULL)
    return;

  AgentStateCallbacks* callbacks_ = g_callbacks.Pointer();
  AgentStateCallbacks::iterator it =
      std::find(callbacks_->begin(), callbacks_->end(), &callback);
  DCHECK(it != callbacks_->end());
  callbacks_->erase(it);
}

// static
void DevToolsAgentHostImpl::NotifyCallbacks(
    DevToolsAgentHostImpl* agent_host, bool attached) {
  AgentStateCallbacks copy(g_callbacks.Get());
  DevToolsManager* manager = DevToolsManager::GetInstance();
  manager->AgentHostStateChanged(agent_host, attached);
  if (manager->delegate())
    manager->delegate()->DevToolsAgentStateChanged(agent_host, attached);
  for (AgentStateCallbacks::iterator it = copy.begin(); it != copy.end(); ++it)
     (*it)->Run(agent_host, attached);
}

bool DevToolsAgentHostImpl::Inspect(BrowserContext* browser_context) {
  DevToolsManager* manager = DevToolsManager::GetInstance();
  if (manager->delegate()) {
    manager->delegate()->Inspect(browser_context, this);
    return true;
  }
  return false;
}

// DevToolsMessageChunkProcessor -----------------------------------------------

DevToolsMessageChunkProcessor::DevToolsMessageChunkProcessor(
    const SendMessageCallback& callback)
    : callback_(callback),
      message_buffer_size_(0),
      last_call_id_(0) {
}

DevToolsMessageChunkProcessor::~DevToolsMessageChunkProcessor() {
}

void DevToolsMessageChunkProcessor::ProcessChunkedMessageFromAgent(
    const DevToolsMessageChunk& chunk) {
  if (chunk.is_last && !chunk.post_state.empty())
    state_cookie_ = chunk.post_state;
  if (chunk.is_last)
    last_call_id_ = chunk.call_id;

  if (chunk.is_first && chunk.is_last) {
    CHECK(message_buffer_size_ == 0);
    callback_.Run(chunk.session_id, chunk.data);
    return;
  }

  if (chunk.is_first) {
    message_buffer_ = std::string();
    message_buffer_.reserve(chunk.message_size);
    message_buffer_size_ = chunk.message_size;
  }

  CHECK(message_buffer_.size() + chunk.data.size() <=
      message_buffer_size_);
  message_buffer_.append(chunk.data);

  if (chunk.is_last) {
    CHECK(message_buffer_.size() == message_buffer_size_);
    callback_.Run(chunk.session_id, message_buffer_);
    message_buffer_ = std::string();
    message_buffer_size_ = 0;
  }
}

}  // namespace content
