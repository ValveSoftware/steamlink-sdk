// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/embedded_worker_devtools_manager.h"

#include "content/browser/devtools/devtools_manager_impl.h"
#include "content/browser/devtools/devtools_protocol.h"
#include "content/browser/devtools/devtools_protocol_constants.h"
#include "content/browser/devtools/ipc_devtools_agent_host.h"
#include "content/browser/shared_worker/shared_worker_instance.h"
#include "content/common/devtools_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/worker_service.h"
#include "ipc/ipc_listener.h"

namespace content {

namespace {

bool SendMessageToWorker(
    const EmbeddedWorkerDevToolsManager::WorkerId& worker_id,
    IPC::Message* message) {
  RenderProcessHost* host = RenderProcessHost::FromID(worker_id.first);
  if (!host) {
    delete message;
    return false;
  }
  message->set_routing_id(worker_id.second);
  host->Send(message);
  return true;
}

}  // namespace

EmbeddedWorkerDevToolsManager::ServiceWorkerIdentifier::ServiceWorkerIdentifier(
    const ServiceWorkerContextCore* const service_worker_context,
    int64 service_worker_version_id)
    : service_worker_context_(service_worker_context),
      service_worker_version_id_(service_worker_version_id) {
}

EmbeddedWorkerDevToolsManager::ServiceWorkerIdentifier::ServiceWorkerIdentifier(
    const ServiceWorkerIdentifier& other)
    : service_worker_context_(other.service_worker_context_),
      service_worker_version_id_(other.service_worker_version_id_) {
}

bool EmbeddedWorkerDevToolsManager::ServiceWorkerIdentifier::Matches(
    const ServiceWorkerIdentifier& other) const {
  return service_worker_context_ == other.service_worker_context_ &&
         service_worker_version_id_ == other.service_worker_version_id_;
}

EmbeddedWorkerDevToolsManager::WorkerInfo::WorkerInfo(
    const SharedWorkerInstance& instance)
    : shared_worker_instance_(new SharedWorkerInstance(instance)),
      state_(WORKER_UNINSPECTED),
      agent_host_(NULL) {
}

EmbeddedWorkerDevToolsManager::WorkerInfo::WorkerInfo(
    const ServiceWorkerIdentifier& service_worker_id)
    : service_worker_id_(new ServiceWorkerIdentifier(service_worker_id)),
      state_(WORKER_UNINSPECTED),
      agent_host_(NULL) {
}

bool EmbeddedWorkerDevToolsManager::WorkerInfo::Matches(
    const SharedWorkerInstance& other) {
  if (!shared_worker_instance_)
    return false;
  return shared_worker_instance_->Matches(other);
}

bool EmbeddedWorkerDevToolsManager::WorkerInfo::Matches(
    const ServiceWorkerIdentifier& other) {
  if (!service_worker_id_)
    return false;
  return service_worker_id_->Matches(other);
}

EmbeddedWorkerDevToolsManager::WorkerInfo::~WorkerInfo() {
}

class EmbeddedWorkerDevToolsManager::EmbeddedWorkerDevToolsAgentHost
    : public IPCDevToolsAgentHost,
      public IPC::Listener {
 public:
  explicit EmbeddedWorkerDevToolsAgentHost(WorkerId worker_id)
      : worker_id_(worker_id), worker_attached_(false) {
    AttachToWorker();
  }

  // DevToolsAgentHost override.
  virtual bool IsWorker() const OVERRIDE { return true; }

  // IPCDevToolsAgentHost implementation.
  virtual void SendMessageToAgent(IPC::Message* message) OVERRIDE {
    if (worker_attached_)
      SendMessageToWorker(worker_id_, message);
    else
      delete message;
  }
  virtual void Attach() OVERRIDE {
    AttachToWorker();
    IPCDevToolsAgentHost::Attach();
  }
  virtual void OnClientAttached() OVERRIDE {}
  virtual void OnClientDetached() OVERRIDE { DetachFromWorker(); }

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(EmbeddedWorkerDevToolsAgentHost, msg)
    IPC_MESSAGE_HANDLER(DevToolsClientMsg_DispatchOnInspectorFrontend,
                        OnDispatchOnInspectorFrontend)
    IPC_MESSAGE_HANDLER(DevToolsHostMsg_SaveAgentRuntimeState,
                        OnSaveAgentRuntimeState)
    IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

  void ReattachToWorker(WorkerId worker_id) {
    CHECK(!worker_attached_);
    worker_id_ = worker_id;
    if (!IsAttached())
      return;
    AttachToWorker();
    Reattach(state_);
  }

  void DetachFromWorker() {
    if (!worker_attached_)
      return;
    worker_attached_ = false;
    if (RenderProcessHost* host = RenderProcessHost::FromID(worker_id_.first))
      host->RemoveRoute(worker_id_.second);
    Release();
  }

  WorkerId worker_id() const { return worker_id_; }

 private:
  virtual ~EmbeddedWorkerDevToolsAgentHost() {
    CHECK(!worker_attached_);
    EmbeddedWorkerDevToolsManager::GetInstance()->RemoveInspectedWorkerData(
        this);
  }

  void OnDispatchOnInspectorFrontend(const std::string& message) {
    DevToolsManagerImpl::GetInstance()->DispatchOnInspectorFrontend(this,
                                                                    message);
  }

  void OnSaveAgentRuntimeState(const std::string& state) { state_ = state; }

  void AttachToWorker() {
    if (worker_attached_)
      return;
    worker_attached_ = true;
    AddRef();
    if (RenderProcessHost* host = RenderProcessHost::FromID(worker_id_.first))
      host->AddRoute(worker_id_.second, this);
  }

  WorkerId worker_id_;
  bool worker_attached_;
  std::string state_;
  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerDevToolsAgentHost);
};

// static
EmbeddedWorkerDevToolsManager* EmbeddedWorkerDevToolsManager::GetInstance() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return Singleton<EmbeddedWorkerDevToolsManager>::get();
}

DevToolsAgentHost* EmbeddedWorkerDevToolsManager::GetDevToolsAgentHostForWorker(
    int worker_process_id,
    int worker_route_id) {
  WorkerId id(worker_process_id, worker_route_id);

  WorkerInfoMap::iterator it = workers_.find(id);
  if (it == workers_.end())
    return NULL;

  WorkerInfo* info = it->second;
  if (info->state() != WORKER_UNINSPECTED &&
      info->state() != WORKER_PAUSED_FOR_DEBUG_ON_START) {
    return info->agent_host();
  }

  EmbeddedWorkerDevToolsAgentHost* agent_host =
      new EmbeddedWorkerDevToolsAgentHost(id);
  info->set_agent_host(agent_host);
  info->set_state(WORKER_INSPECTED);
  return agent_host;
}

DevToolsAgentHost*
EmbeddedWorkerDevToolsManager::GetDevToolsAgentHostForServiceWorker(
    const ServiceWorkerIdentifier& service_worker_id) {
  WorkerInfoMap::iterator it = FindExistingServiceWorkerInfo(service_worker_id);
  if (it == workers_.end())
    return NULL;
  return GetDevToolsAgentHostForWorker(it->first.first, it->first.second);
}

EmbeddedWorkerDevToolsManager::EmbeddedWorkerDevToolsManager()
    : debug_service_worker_on_start_(false) {
}

EmbeddedWorkerDevToolsManager::~EmbeddedWorkerDevToolsManager() {
}

bool EmbeddedWorkerDevToolsManager::SharedWorkerCreated(
    int worker_process_id,
    int worker_route_id,
    const SharedWorkerInstance& instance) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const WorkerId id(worker_process_id, worker_route_id);
  WorkerInfoMap::iterator it = FindExistingSharedWorkerInfo(instance);
  if (it == workers_.end()) {
    scoped_ptr<WorkerInfo> info(new WorkerInfo(instance));
    workers_.set(id, info.Pass());
    return false;
  }
  MoveToPausedState(id, it);
  return true;
}

bool EmbeddedWorkerDevToolsManager::ServiceWorkerCreated(
    int worker_process_id,
    int worker_route_id,
    const ServiceWorkerIdentifier& service_worker_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const WorkerId id(worker_process_id, worker_route_id);
  WorkerInfoMap::iterator it = FindExistingServiceWorkerInfo(service_worker_id);
  if (it == workers_.end()) {
    scoped_ptr<WorkerInfo> info(new WorkerInfo(service_worker_id));
    if (debug_service_worker_on_start_)
      info->set_state(WORKER_PAUSED_FOR_DEBUG_ON_START);
    workers_.set(id, info.Pass());
    return debug_service_worker_on_start_;
  }
  MoveToPausedState(id, it);
  return true;
}

void EmbeddedWorkerDevToolsManager::WorkerDestroyed(int worker_process_id,
                                                    int worker_route_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const WorkerId id(worker_process_id, worker_route_id);
  WorkerInfoMap::iterator it = workers_.find(id);
  DCHECK(it != workers_.end());
  WorkerInfo* info = it->second;
  switch (info->state()) {
    case WORKER_UNINSPECTED:
    case WORKER_PAUSED_FOR_DEBUG_ON_START:
      workers_.erase(it);
      break;
    case WORKER_INSPECTED: {
      EmbeddedWorkerDevToolsAgentHost* agent_host = info->agent_host();
      info->set_state(WORKER_TERMINATED);
      if (!agent_host->IsAttached()) {
        agent_host->DetachFromWorker();
        return;
      }
      // Client host is debugging this worker agent host.
      std::string notification =
          DevToolsProtocol::CreateNotification(
              devtools::Worker::disconnectedFromWorker::kName, NULL)
              ->Serialize();
      DevToolsManagerImpl::GetInstance()->DispatchOnInspectorFrontend(
          agent_host, notification);
      agent_host->DetachFromWorker();
      break;
    }
    case WORKER_TERMINATED:
      NOTREACHED();
      break;
    case WORKER_PAUSED_FOR_REATTACH: {
      scoped_ptr<WorkerInfo> worker_info = workers_.take_and_erase(it);
      worker_info->set_state(WORKER_TERMINATED);
      const WorkerId old_id = worker_info->agent_host()->worker_id();
      workers_.set(old_id, worker_info.Pass());
      break;
    }
  }
}

void EmbeddedWorkerDevToolsManager::WorkerContextStarted(int worker_process_id,
                                                         int worker_route_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const WorkerId id(worker_process_id, worker_route_id);
  WorkerInfoMap::iterator it = workers_.find(id);
  DCHECK(it != workers_.end());
  WorkerInfo* info = it->second;
  if (info->state() == WORKER_PAUSED_FOR_DEBUG_ON_START) {
    RenderProcessHost* rph = RenderProcessHost::FromID(worker_process_id);
    scoped_refptr<DevToolsAgentHost> agent_host(
        GetDevToolsAgentHostForWorker(worker_process_id, worker_route_id));
    DevToolsManagerImpl::GetInstance()->Inspect(rph->GetBrowserContext(),
                                                agent_host.get());
  } else if (info->state() == WORKER_PAUSED_FOR_REATTACH) {
    info->agent_host()->ReattachToWorker(id);
    info->set_state(WORKER_INSPECTED);
  }
}

void EmbeddedWorkerDevToolsManager::RemoveInspectedWorkerData(
    EmbeddedWorkerDevToolsAgentHost* agent_host) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const WorkerId id(agent_host->worker_id());
  scoped_ptr<WorkerInfo> worker_info = workers_.take_and_erase(id);
  if (worker_info) {
    DCHECK_EQ(worker_info->agent_host(), agent_host);
    if (worker_info->state() == WORKER_TERMINATED)
      return;
    DCHECK_EQ(worker_info->state(), WORKER_INSPECTED);
    worker_info->set_agent_host(NULL);
    worker_info->set_state(WORKER_UNINSPECTED);
    workers_.set(id, worker_info.Pass());
    return;
  }
  for (WorkerInfoMap::iterator it = workers_.begin(); it != workers_.end();
       ++it) {
    if (it->second->agent_host() == agent_host) {
      DCHECK_EQ(WORKER_PAUSED_FOR_REATTACH, it->second->state());
      SendMessageToWorker(
          it->first,
          new DevToolsAgentMsg_ResumeWorkerContext(it->first.second));
      it->second->set_agent_host(NULL);
      it->second->set_state(WORKER_UNINSPECTED);
      return;
    }
  }
}

EmbeddedWorkerDevToolsManager::WorkerInfoMap::iterator
EmbeddedWorkerDevToolsManager::FindExistingSharedWorkerInfo(
    const SharedWorkerInstance& instance) {
  WorkerInfoMap::iterator it = workers_.begin();
  for (; it != workers_.end(); ++it) {
    if (it->second->Matches(instance))
      break;
  }
  return it;
}

EmbeddedWorkerDevToolsManager::WorkerInfoMap::iterator
EmbeddedWorkerDevToolsManager::FindExistingServiceWorkerInfo(
    const ServiceWorkerIdentifier& service_worker_id) {
  WorkerInfoMap::iterator it = workers_.begin();
  for (; it != workers_.end(); ++it) {
    if (it->second->Matches(service_worker_id))
      break;
  }
  return it;
}

void EmbeddedWorkerDevToolsManager::MoveToPausedState(
    const WorkerId& id,
    const WorkerInfoMap::iterator& it) {
  DCHECK_EQ(WORKER_TERMINATED, it->second->state());
  scoped_ptr<WorkerInfo> info = workers_.take_and_erase(it);
  info->set_state(WORKER_PAUSED_FOR_REATTACH);
  workers_.set(id, info.Pass());
}

void EmbeddedWorkerDevToolsManager::ResetForTesting() {
  workers_.clear();
}

}  // namespace content
