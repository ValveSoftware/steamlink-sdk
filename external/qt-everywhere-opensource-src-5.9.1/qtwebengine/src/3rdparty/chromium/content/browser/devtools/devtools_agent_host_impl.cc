// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_agent_host_impl.h"

#include <map>
#include <vector>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "base/observer_list.h"
#include "content/browser/devtools/devtools_manager.h"
#include "content/browser/devtools/devtools_session.h"
#include "content/browser/devtools/forwarding_agent_host.h"
#include "content/browser/devtools/protocol/devtools_protocol_dispatcher.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/devtools/service_worker_devtools_agent_host.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/browser/devtools/shared_worker_devtools_agent_host.h"
#include "content/browser/devtools/shared_worker_devtools_manager.h"
#include "content/browser/loader/netlog_observer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"

namespace content {

namespace {
typedef std::map<std::string, DevToolsAgentHostImpl*> Instances;
base::LazyInstance<Instances>::Leaky g_instances = LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<base::ObserverList<DevToolsAgentHostObserver>>::Leaky
    g_observers = LAZY_INSTANCE_INITIALIZER;
}  // namespace

char DevToolsAgentHost::kTypePage[] = "page";
char DevToolsAgentHost::kTypeFrame[] = "iframe";
char DevToolsAgentHost::kTypeSharedWorker[] = "shared_worker";
char DevToolsAgentHost::kTypeServiceWorker[] = "service_worker";
char DevToolsAgentHost::kTypeExternal[] = "external";
char DevToolsAgentHost::kTypeBrowser[] = "browser";
char DevToolsAgentHost::kTypeOther[] = "other";
int DevToolsAgentHostImpl::s_attached_count_ = 0;
int DevToolsAgentHostImpl::s_force_creation_count_ = 0;

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

#if DCHECK_IS_ON()
  for (auto it : result) {
    DevToolsAgentHostImpl* host = static_cast<DevToolsAgentHostImpl*>(it.get());
    DCHECK(g_instances.Get().find(host->id_) != g_instances.Get().end());
  }
#endif

  return result;
}

// static
void DevToolsAgentHost::DiscoverAllHosts(const DiscoveryCallback& callback) {
  DevToolsManager* manager = DevToolsManager::GetInstance();
  if (!manager->delegate() || !manager->delegate()->DiscoverTargets(callback))
    callback.Run(DevToolsAgentHost::GetOrCreateAll());
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

DevToolsAgentHostImpl::DevToolsAgentHostImpl(const std::string& id)
    : id_(id),
      last_session_id_(0),
      client_(NULL) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

DevToolsAgentHostImpl::~DevToolsAgentHostImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  NotifyDestroyed();
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
scoped_refptr<DevToolsAgentHost> DevToolsAgentHost::Forward(
    const std::string& id,
    std::unique_ptr<DevToolsExternalAgentProxyDelegate> delegate) {
  scoped_refptr<DevToolsAgentHost> result = DevToolsAgentHost::GetForId(id);
  if (result)
    return result;
  return new ForwardingAgentHost(id, std::move(delegate));
}

bool DevToolsAgentHostImpl::InnerAttach(DevToolsAgentHostClient* client,
                                        bool force) {
  if (client_ && !force)
    return false;

  scoped_refptr<DevToolsAgentHostImpl> protect(this);
  if (client_) {
    client_->AgentHostClosed(this, true);
    InnerDetach();
  }
  client_ = client;
  session_.reset(new DevToolsSession(this, ++last_session_id_));
  Attach();
  NotifyAttached();
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
  session_.reset();
  NotifyDetached();
}

bool DevToolsAgentHostImpl::IsAttached() {
  return !!client_;
}

void DevToolsAgentHostImpl::InspectElement(
    DevToolsAgentHostClient* client,
    int x,
    int y) {
 if (!client_ || (client && client_ != client))
   return;
 InspectElement(x, y);
}

std::string DevToolsAgentHostImpl::GetId() {
  return id_;
}

std::string DevToolsAgentHostImpl::GetParentId() {
  return "";
}

std::string DevToolsAgentHostImpl::GetDescription() {
  return "";
}

GURL DevToolsAgentHostImpl::GetFaviconURL() {
  return GURL();
}

std::string DevToolsAgentHostImpl::GetFrontendURL() {
  return std::string();
}

base::TimeTicks DevToolsAgentHostImpl::GetLastActivityTime() {
  return base::TimeTicks();
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

bool DevToolsAgentHostImpl::Inspect() {
  DevToolsManager* manager = DevToolsManager::GetInstance();
  if (manager->delegate()) {
    manager->delegate()->Inspect(this);
    return true;
  }
  return false;
}

void DevToolsAgentHostImpl::SendProtocolResponse(int session_id,
                                                 const std::string& message) {
  SendMessageToClient(session_id, message);
}

void DevToolsAgentHostImpl::SendProtocolNotification(
    const std::string& message) {
  SendMessageToClient(session_ ? session_->session_id() : 0, message);
}

void DevToolsAgentHostImpl::HostClosed() {
  if (!client_)
    return;

  scoped_refptr<DevToolsAgentHostImpl> protect(this);
  // Clear |client_| before notifying it.
  DevToolsAgentHostClient* client = client_;
  client_ = NULL;
  client->AgentHostClosed(this, false);
  NotifyDetached();
}

void DevToolsAgentHostImpl::InspectElement(int x, int y) {
}

void DevToolsAgentHostImpl::SendMessageToClient(int session_id,
                                                const std::string& message) {
  if (!client_)
    return;
  // Filter any messages from previous sessions.
  if (!session_ || session_id != session_->session_id())
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
void DevToolsAgentHost::AddObserver(DevToolsAgentHostObserver* observer) {
  if (observer->ShouldForceDevToolsAgentHostCreation()) {
    if (!DevToolsAgentHostImpl::s_force_creation_count_) {
      // Force all agent hosts when first observer is added.
      DevToolsAgentHost::GetOrCreateAll();
    }
    DevToolsAgentHostImpl::s_force_creation_count_++;
  }

  g_observers.Get().AddObserver(observer);
  for (const auto& id_host : g_instances.Get())
    observer->DevToolsAgentHostCreated(id_host.second);
}

// static
void DevToolsAgentHost::RemoveObserver(DevToolsAgentHostObserver* observer) {
  if (observer->ShouldForceDevToolsAgentHostCreation())
    DevToolsAgentHostImpl::s_force_creation_count_--;
  g_observers.Get().RemoveObserver(observer);
}

// static
bool DevToolsAgentHostImpl::ShouldForceCreation() {
  return !!s_force_creation_count_;
}

void DevToolsAgentHostImpl::NotifyCreated() {
  DCHECK(g_instances.Get().find(id_) == g_instances.Get().end());
  g_instances.Get()[id_] = this;
  for (auto& observer : g_observers.Get())
    observer.DevToolsAgentHostCreated(this);
}

void DevToolsAgentHostImpl::NotifyAttached() {
  if (!s_attached_count_) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&NetLogObserver::Attach,
                   GetContentClient()->browser()->GetNetLog()));
  }
  ++s_attached_count_;

  for (auto& observer : g_observers.Get())
    observer.DevToolsAgentHostAttached(this);
}

void DevToolsAgentHostImpl::NotifyDetached() {
  --s_attached_count_;
  if (!s_attached_count_) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&NetLogObserver::Detach));
  }

  for (auto& observer : g_observers.Get())
    observer.DevToolsAgentHostDetached(this);
}

void DevToolsAgentHostImpl::NotifyDestroyed() {
  DCHECK(g_instances.Get().find(id_) != g_instances.Get().end());
  for (auto& observer : g_observers.Get())
    observer.DevToolsAgentHostDestroyed(this);
  g_instances.Get().erase(id_);
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

bool DevToolsMessageChunkProcessor::ProcessChunkedMessageFromAgent(
    const DevToolsMessageChunk& chunk) {
  if (chunk.is_last && !chunk.post_state.empty())
    state_cookie_ = chunk.post_state;
  if (chunk.is_last)
    last_call_id_ = chunk.call_id;

  if (chunk.is_first && chunk.is_last) {
    if (message_buffer_size_ != 0)
      return false;
    callback_.Run(chunk.session_id, chunk.data);
    return true;
  }

  if (chunk.is_first) {
    message_buffer_ = std::string();
    message_buffer_.reserve(chunk.message_size);
    message_buffer_size_ = chunk.message_size;
  }

  if (message_buffer_.size() + chunk.data.size() > message_buffer_size_)
    return false;
  message_buffer_.append(chunk.data);

  if (chunk.is_last) {
    if (message_buffer_.size() != message_buffer_size_)
      return false;
    callback_.Run(chunk.session_id, message_buffer_);
    message_buffer_ = std::string();
    message_buffer_size_ = 0;
  }
  return true;
}

}  // namespace content
