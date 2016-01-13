// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/worker_devtools_manager.h"

#include <list>
#include <map>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "content/browser/devtools/devtools_manager_impl.h"
#include "content/browser/devtools/devtools_protocol.h"
#include "content/browser/devtools/devtools_protocol_constants.h"
#include "content/browser/devtools/embedded_worker_devtools_manager.h"
#include "content/browser/devtools/ipc_devtools_agent_host.h"
#include "content/browser/devtools/worker_devtools_message_filter.h"
#include "content/browser/worker_host/worker_service_impl.h"
#include "content/common/devtools_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/common/process_type.h"

namespace content {

// Called on the UI thread.
// static
scoped_refptr<DevToolsAgentHost> DevToolsAgentHost::GetForWorker(
    int worker_process_id,
    int worker_route_id) {
  if (WorkerService::EmbeddedSharedWorkerEnabled()) {
    return EmbeddedWorkerDevToolsManager::GetInstance()
        ->GetDevToolsAgentHostForWorker(worker_process_id, worker_route_id);
  } else {
    return WorkerDevToolsManager::GetDevToolsAgentHostForWorker(
        worker_process_id, worker_route_id);
  }
}

namespace {

typedef std::map<WorkerDevToolsManager::WorkerId,
                 WorkerDevToolsManager::WorkerDevToolsAgentHost*> AgentHosts;
base::LazyInstance<AgentHosts>::Leaky g_agent_map = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<AgentHosts>::Leaky g_orphan_map = LAZY_INSTANCE_INITIALIZER;

}  // namespace

struct WorkerDevToolsManager::TerminatedInspectedWorker {
  TerminatedInspectedWorker(WorkerId id,
                            const GURL& url,
                            const base::string16& name)
      : old_worker_id(id),
        worker_url(url),
        worker_name(name) {}
  WorkerId old_worker_id;
  GURL worker_url;
  base::string16 worker_name;
};


class WorkerDevToolsManager::WorkerDevToolsAgentHost
    : public IPCDevToolsAgentHost {
 public:
  explicit WorkerDevToolsAgentHost(WorkerId worker_id)
      : has_worker_id_(false) {
    SetWorkerId(worker_id, false);
  }

  void SetWorkerId(WorkerId worker_id, bool reattach) {
    worker_id_ = worker_id;
    if (!has_worker_id_)
      AddRef();  //  Balanced in ResetWorkerId.
    has_worker_id_ = true;
    g_agent_map.Get()[worker_id_] = this;

    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(
            &ConnectToWorker,
            worker_id.first,
            worker_id.second));

    if (reattach)
      Reattach(state_);
  }

  void ResetWorkerId() {
    g_agent_map.Get().erase(worker_id_);
    has_worker_id_ = false;
    Release();  //  Balanced in SetWorkerId.
  }

  void SaveAgentRuntimeState(const std::string& state) {
    state_ = state;
  }

  void ConnectionFailed() {
    NotifyCloseListener();
    // Object can be deleted here.
  }

 private:
  virtual ~WorkerDevToolsAgentHost();

  static void ConnectToWorker(
      int worker_process_id,
      int worker_route_id) {
    WorkerDevToolsManager::GetInstance()->ConnectDevToolsAgentHostToWorker(
        worker_process_id, worker_route_id);
  }

  static void ForwardToWorkerDevToolsAgent(
      int worker_process_id,
      int worker_route_id,
      IPC::Message* message) {
    WorkerDevToolsManager::GetInstance()->ForwardToWorkerDevToolsAgent(
        worker_process_id, worker_route_id, *message);
  }

  // IPCDevToolsAgentHost implementation.
  virtual void SendMessageToAgent(IPC::Message* message) OVERRIDE {
    if (!has_worker_id_) {
      delete message;
      return;
    }
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(
            &WorkerDevToolsAgentHost::ForwardToWorkerDevToolsAgent,
            worker_id_.first,
            worker_id_.second,
            base::Owned(message)));
  }

  virtual void OnClientAttached() OVERRIDE {}
  virtual void OnClientDetached() OVERRIDE {}

  bool has_worker_id_;
  WorkerId worker_id_;
  std::string state_;

  DISALLOW_COPY_AND_ASSIGN(WorkerDevToolsAgentHost);
};


class WorkerDevToolsManager::DetachedClientHosts {
 public:
  static void WorkerReloaded(WorkerId old_id, WorkerId new_id) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    AgentHosts::iterator it = g_orphan_map.Get().find(old_id);
    if (it != g_orphan_map.Get().end()) {
      it->second->SetWorkerId(new_id, true);
      g_orphan_map.Get().erase(old_id);
      return;
    }
    RemovePendingWorkerData(old_id);
  }

  static void WorkerDestroyed(WorkerId id) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    AgentHosts::iterator it = g_agent_map.Get().find(id);
    if (it == g_agent_map.Get().end()) {
      RemovePendingWorkerData(id);
      return;
    }

    WorkerDevToolsAgentHost* agent = it->second;
    DevToolsManagerImpl* devtools_manager = DevToolsManagerImpl::GetInstance();
    if (!agent->IsAttached()) {
      // Agent has no client hosts -> delete it.
      RemovePendingWorkerData(id);
      return;
    }

    // Client host is debugging this worker agent host.
    std::string notification = DevToolsProtocol::CreateNotification(
        devtools::Worker::disconnectedFromWorker::kName, NULL)->Serialize();
    devtools_manager->DispatchOnInspectorFrontend(agent, notification);
    g_orphan_map.Get()[id] = agent;
    agent->ResetWorkerId();
  }

  static void RemovePendingWorkerData(WorkerId id) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&RemoveInspectedWorkerDataOnIOThread, id));
  }

 private:
  DetachedClientHosts() {}
  ~DetachedClientHosts() {}

  static void RemoveInspectedWorkerDataOnIOThread(WorkerId id) {
    WorkerDevToolsManager::GetInstance()->RemoveInspectedWorkerData(id);
  }
};

struct WorkerDevToolsManager::InspectedWorker {
  InspectedWorker(WorkerProcessHost* host, int route_id, const GURL& url,
                  const base::string16& name)
      : host(host),
        route_id(route_id),
        worker_url(url),
        worker_name(name) {}
  WorkerProcessHost* const host;
  int const route_id;
  GURL worker_url;
  base::string16 worker_name;
};

// static
WorkerDevToolsManager* WorkerDevToolsManager::GetInstance() {
  DCHECK(!WorkerService::EmbeddedSharedWorkerEnabled());
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return Singleton<WorkerDevToolsManager>::get();
}

// static
DevToolsAgentHost* WorkerDevToolsManager::GetDevToolsAgentHostForWorker(
    int worker_process_id,
    int worker_route_id) {
  DCHECK(!WorkerService::EmbeddedSharedWorkerEnabled());
  WorkerId id(worker_process_id, worker_route_id);
  AgentHosts::iterator it = g_agent_map.Get().find(id);
  if (it == g_agent_map.Get().end())
    return new WorkerDevToolsAgentHost(id);
  return it->second;
}

WorkerDevToolsManager::WorkerDevToolsManager() {
}

WorkerDevToolsManager::~WorkerDevToolsManager() {
}

bool WorkerDevToolsManager::WorkerCreated(
    WorkerProcessHost* worker,
    const WorkerProcessHost::WorkerInstance& instance) {
  for (TerminatedInspectedWorkers::iterator it = terminated_workers_.begin();
       it != terminated_workers_.end(); ++it) {
    if (instance.Matches(it->worker_url, it->worker_name,
                         instance.partition(),
                         instance.resource_context())) {
      WorkerId new_worker_id(worker->GetData().id, instance.worker_route_id());
      paused_workers_[new_worker_id] = it->old_worker_id;
      terminated_workers_.erase(it);
      return true;
    }
  }
  return false;
}

void WorkerDevToolsManager::WorkerDestroyed(
    WorkerProcessHost* worker,
    int worker_route_id) {
  InspectedWorkersList::iterator it = FindInspectedWorker(
      worker->GetData().id,
      worker_route_id);
  if (it == inspected_workers_.end())
    return;

  WorkerId worker_id(worker->GetData().id, worker_route_id);
  terminated_workers_.push_back(TerminatedInspectedWorker(
      worker_id,
      it->worker_url,
      it->worker_name));
  inspected_workers_.erase(it);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&DetachedClientHosts::WorkerDestroyed, worker_id));
}

void WorkerDevToolsManager::WorkerContextStarted(WorkerProcessHost* process,
                                                 int worker_route_id) {
  WorkerId new_worker_id(process->GetData().id, worker_route_id);
  PausedWorkers::iterator it = paused_workers_.find(new_worker_id);
  if (it == paused_workers_.end())
    return;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &DetachedClientHosts::WorkerReloaded,
          it->second,
          new_worker_id));
  paused_workers_.erase(it);
}

void WorkerDevToolsManager::RemoveInspectedWorkerData(
    const WorkerId& id) {
  for (TerminatedInspectedWorkers::iterator it = terminated_workers_.begin();
       it != terminated_workers_.end(); ++it) {
    if (it->old_worker_id == id) {
      terminated_workers_.erase(it);
      return;
    }
  }

  for (PausedWorkers::iterator it = paused_workers_.begin();
       it != paused_workers_.end(); ++it) {
    if (it->second == id) {
      SendResumeToWorker(it->first);
      paused_workers_.erase(it);
      return;
    }
  }
}

WorkerDevToolsManager::InspectedWorkersList::iterator
WorkerDevToolsManager::FindInspectedWorker(
    int host_id, int route_id) {
  InspectedWorkersList::iterator it = inspected_workers_.begin();
  while (it != inspected_workers_.end()) {
    if (it->host->GetData().id == host_id && it->route_id == route_id)
      break;
    ++it;
  }
  return it;
}

static WorkerProcessHost* FindWorkerProcess(int worker_process_id) {
  for (WorkerProcessHostIterator iter; !iter.Done(); ++iter) {
    if (iter.GetData().id == worker_process_id)
      return *iter;
  }
  return NULL;
}

void WorkerDevToolsManager::ConnectDevToolsAgentHostToWorker(
    int worker_process_id,
    int worker_route_id) {
  if (WorkerProcessHost* process = FindWorkerProcess(worker_process_id)) {
    const WorkerProcessHost::Instances& instances = process->instances();
    for (WorkerProcessHost::Instances::const_iterator i = instances.begin();
         i != instances.end(); ++i) {
      if (i->worker_route_id() == worker_route_id) {
        DCHECK(FindInspectedWorker(worker_process_id, worker_route_id) ==
               inspected_workers_.end());
        inspected_workers_.push_back(
            InspectedWorker(process, worker_route_id, i->url(), i->name()));
        return;
      }
    }
  }
  NotifyConnectionFailedOnIOThread(worker_process_id, worker_route_id);
}

void WorkerDevToolsManager::ForwardToDevToolsClient(
    int worker_process_id,
    int worker_route_id,
    const std::string& message) {
  if (FindInspectedWorker(worker_process_id, worker_route_id) ==
          inspected_workers_.end()) {
      NOTREACHED();
      return;
  }
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &ForwardToDevToolsClientOnUIThread,
          worker_process_id,
          worker_route_id,
          message));
}

void WorkerDevToolsManager::SaveAgentRuntimeState(int worker_process_id,
                                                  int worker_route_id,
                                                  const std::string& state) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &SaveAgentRuntimeStateOnUIThread,
          worker_process_id,
          worker_route_id,
          state));
}

void WorkerDevToolsManager::ForwardToWorkerDevToolsAgent(
    int worker_process_id,
    int worker_route_id,
    const IPC::Message& message) {
  InspectedWorkersList::iterator it = FindInspectedWorker(
      worker_process_id,
      worker_route_id);
  if (it == inspected_workers_.end())
    return;
  IPC::Message* msg = new IPC::Message(message);
  msg->set_routing_id(worker_route_id);
  it->host->Send(msg);
}

// static
void WorkerDevToolsManager::ForwardToDevToolsClientOnUIThread(
    int worker_process_id,
    int worker_route_id,
    const std::string& message) {
  AgentHosts::iterator it = g_agent_map.Get().find(WorkerId(worker_process_id,
                                                            worker_route_id));
  if (it == g_agent_map.Get().end())
    return;
  DevToolsManagerImpl::GetInstance()->DispatchOnInspectorFrontend(it->second,
                                                                  message);
}

// static
void WorkerDevToolsManager::SaveAgentRuntimeStateOnUIThread(
    int worker_process_id,
    int worker_route_id,
    const std::string& state) {
  AgentHosts::iterator it = g_agent_map.Get().find(WorkerId(worker_process_id,
                                                            worker_route_id));
  if (it == g_agent_map.Get().end())
    return;
  it->second->SaveAgentRuntimeState(state);
}

// static
void WorkerDevToolsManager::NotifyConnectionFailedOnIOThread(
    int worker_process_id,
    int worker_route_id) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &WorkerDevToolsManager::NotifyConnectionFailedOnUIThread,
          worker_process_id,
          worker_route_id));
}

// static
void WorkerDevToolsManager::NotifyConnectionFailedOnUIThread(
    int worker_process_id,
    int worker_route_id) {
  AgentHosts::iterator it = g_agent_map.Get().find(WorkerId(worker_process_id,
                                                            worker_route_id));
  if (it != g_agent_map.Get().end())
    it->second->ConnectionFailed();
}

// static
void WorkerDevToolsManager::SendResumeToWorker(const WorkerId& id) {
  if (WorkerProcessHost* process = FindWorkerProcess(id.first))
    process->Send(new DevToolsAgentMsg_ResumeWorkerContext(id.second));
}

WorkerDevToolsManager::WorkerDevToolsAgentHost::~WorkerDevToolsAgentHost() {
  DetachedClientHosts::RemovePendingWorkerData(worker_id_);
  g_agent_map.Get().erase(worker_id_);
  g_orphan_map.Get().erase(worker_id_);
}

}  // namespace content
