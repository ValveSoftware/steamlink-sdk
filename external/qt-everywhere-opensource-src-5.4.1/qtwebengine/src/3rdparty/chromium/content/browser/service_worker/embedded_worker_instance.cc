// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/embedded_worker_instance.h"

#include "base/bind_helpers.h"
#include "content/browser/devtools/embedded_worker_devtools_manager.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "ipc/ipc_message.h"
#include "url/gurl.h"

namespace content {

namespace {

// Functor to sort by the .second element of a struct.
struct SecondGreater {
  template <typename Value>
  bool operator()(const Value& lhs, const Value& rhs) {
    return lhs.second > rhs.second;
  }
};

void NotifyWorkerContextStarted(int worker_process_id, int worker_route_id) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(
            NotifyWorkerContextStarted, worker_process_id, worker_route_id));
    return;
  }
  EmbeddedWorkerDevToolsManager::GetInstance()->WorkerContextStarted(
      worker_process_id, worker_route_id);
}

void NotifyWorkerDestroyed(int worker_process_id, int worker_route_id) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(NotifyWorkerDestroyed, worker_process_id, worker_route_id));
    return;
  }
  EmbeddedWorkerDevToolsManager::GetInstance()->WorkerDestroyed(
      worker_process_id, worker_route_id);
}

void RegisterToWorkerDevToolsManager(
    int process_id,
    const ServiceWorkerContextCore* const service_worker_context,
    int64 service_worker_version_id,
    const base::Callback<void(int worker_devtools_agent_route_id,
                              bool pause_on_start)>& callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(RegisterToWorkerDevToolsManager,
                                       process_id,
                                       service_worker_context,
                                       service_worker_version_id,
                                       callback));
    return;
  }
  int worker_devtools_agent_route_id = MSG_ROUTING_NONE;
  bool pause_on_start = false;
  if (RenderProcessHost* rph = RenderProcessHost::FromID(process_id)) {
    // |rph| may be NULL in unit tests.
    worker_devtools_agent_route_id = rph->GetNextRoutingID();
    pause_on_start =
        EmbeddedWorkerDevToolsManager::GetInstance()->ServiceWorkerCreated(
            process_id,
            worker_devtools_agent_route_id,
            EmbeddedWorkerDevToolsManager::ServiceWorkerIdentifier(
                service_worker_context, service_worker_version_id));
  }
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(callback, worker_devtools_agent_route_id, pause_on_start));
}

}  // namespace

EmbeddedWorkerInstance::~EmbeddedWorkerInstance() {
  if (status_ == STARTING || status_ == RUNNING)
    Stop();
  if (worker_devtools_agent_route_id_ != MSG_ROUTING_NONE)
    NotifyWorkerDestroyed(process_id_, worker_devtools_agent_route_id_);
  if (context_ && process_id_ != -1)
    context_->process_manager()->ReleaseWorkerProcess(embedded_worker_id_);
  registry_->RemoveWorker(process_id_, embedded_worker_id_);
}

void EmbeddedWorkerInstance::Start(int64 service_worker_version_id,
                                   const GURL& scope,
                                   const GURL& script_url,
                                   const std::vector<int>& possible_process_ids,
                                   const StatusCallback& callback) {
  if (!context_) {
    callback.Run(SERVICE_WORKER_ERROR_ABORT);
    return;
  }
  DCHECK(status_ == STOPPED);
  status_ = STARTING;
  scoped_ptr<EmbeddedWorkerMsg_StartWorker_Params> params(
      new EmbeddedWorkerMsg_StartWorker_Params());
  params->embedded_worker_id = embedded_worker_id_;
  params->service_worker_version_id = service_worker_version_id;
  params->scope = scope;
  params->script_url = script_url;
  params->worker_devtools_agent_route_id = MSG_ROUTING_NONE;
  params->pause_on_start = false;
  context_->process_manager()->AllocateWorkerProcess(
      embedded_worker_id_,
      SortProcesses(possible_process_ids),
      script_url,
      base::Bind(&EmbeddedWorkerInstance::RunProcessAllocated,
                 weak_factory_.GetWeakPtr(),
                 context_,
                 base::Passed(&params),
                 callback));
}

ServiceWorkerStatusCode EmbeddedWorkerInstance::Stop() {
  DCHECK(status_ == STARTING || status_ == RUNNING);
  ServiceWorkerStatusCode status =
      registry_->StopWorker(process_id_, embedded_worker_id_);
  if (status == SERVICE_WORKER_OK)
    status_ = STOPPING;
  return status;
}

ServiceWorkerStatusCode EmbeddedWorkerInstance::SendMessage(
    const IPC::Message& message) {
  DCHECK(status_ == RUNNING);
  return registry_->Send(process_id_,
                         new EmbeddedWorkerContextMsg_MessageToWorker(
                             thread_id_, embedded_worker_id_, message));
}

void EmbeddedWorkerInstance::AddProcessReference(int process_id) {
  ProcessRefMap::iterator found = process_refs_.find(process_id);
  if (found == process_refs_.end())
    found = process_refs_.insert(std::make_pair(process_id, 0)).first;
  ++found->second;
}

void EmbeddedWorkerInstance::ReleaseProcessReference(int process_id) {
  ProcessRefMap::iterator found = process_refs_.find(process_id);
  if (found == process_refs_.end()) {
    NOTREACHED() << "Releasing unknown process ref " << process_id;
    return;
  }
  if (--found->second == 0)
    process_refs_.erase(found);
}

EmbeddedWorkerInstance::EmbeddedWorkerInstance(
    base::WeakPtr<ServiceWorkerContextCore> context,
    int embedded_worker_id)
    : context_(context),
      registry_(context->embedded_worker_registry()),
      embedded_worker_id_(embedded_worker_id),
      status_(STOPPED),
      process_id_(-1),
      thread_id_(-1),
      worker_devtools_agent_route_id_(MSG_ROUTING_NONE),
      weak_factory_(this) {
}

// static
void EmbeddedWorkerInstance::RunProcessAllocated(
    base::WeakPtr<EmbeddedWorkerInstance> instance,
    base::WeakPtr<ServiceWorkerContextCore> context,
    scoped_ptr<EmbeddedWorkerMsg_StartWorker_Params> params,
    const EmbeddedWorkerInstance::StatusCallback& callback,
    ServiceWorkerStatusCode status,
    int process_id) {
  if (!context) {
    callback.Run(SERVICE_WORKER_ERROR_ABORT);
    return;
  }
  if (!instance) {
    if (status == SERVICE_WORKER_OK) {
      // We only have a process allocated if the status is OK.
      context->process_manager()->ReleaseWorkerProcess(
          params->embedded_worker_id);
    }
    callback.Run(SERVICE_WORKER_ERROR_ABORT);
    return;
  }
  instance->ProcessAllocated(params.Pass(), callback, process_id, status);
}

void EmbeddedWorkerInstance::ProcessAllocated(
    scoped_ptr<EmbeddedWorkerMsg_StartWorker_Params> params,
    const StatusCallback& callback,
    int process_id,
    ServiceWorkerStatusCode status) {
  DCHECK_EQ(process_id_, -1);
  if (status != SERVICE_WORKER_OK) {
    status_ = STOPPED;
    callback.Run(status);
    return;
  }
  const int64 service_worker_version_id = params->service_worker_version_id;
  process_id_ = process_id;
  RegisterToWorkerDevToolsManager(
      process_id,
      context_.get(),
      service_worker_version_id,
      base::Bind(&EmbeddedWorkerInstance::SendStartWorker,
                 weak_factory_.GetWeakPtr(),
                 base::Passed(&params),
                 callback));
}

void EmbeddedWorkerInstance::SendStartWorker(
    scoped_ptr<EmbeddedWorkerMsg_StartWorker_Params> params,
    const StatusCallback& callback,
    int worker_devtools_agent_route_id,
    bool pause_on_start) {
  worker_devtools_agent_route_id_ = worker_devtools_agent_route_id;
  params->worker_devtools_agent_route_id = worker_devtools_agent_route_id;
  params->pause_on_start = pause_on_start;
  registry_->SendStartWorker(params.Pass(), callback, process_id_);
}

void EmbeddedWorkerInstance::OnScriptLoaded() {
  if (worker_devtools_agent_route_id_ != MSG_ROUTING_NONE)
    NotifyWorkerContextStarted(process_id_, worker_devtools_agent_route_id_);
}

void EmbeddedWorkerInstance::OnScriptLoadFailed() {
}

void EmbeddedWorkerInstance::OnStarted(int thread_id) {
  // Stop is requested before OnStarted is sent back from the worker.
  if (status_ == STOPPING)
    return;
  DCHECK(status_ == STARTING);
  status_ = RUNNING;
  thread_id_ = thread_id;
  FOR_EACH_OBSERVER(Listener, listener_list_, OnStarted());
}

void EmbeddedWorkerInstance::OnStopped() {
  if (worker_devtools_agent_route_id_ != MSG_ROUTING_NONE)
    NotifyWorkerDestroyed(process_id_, worker_devtools_agent_route_id_);
  if (context_)
    context_->process_manager()->ReleaseWorkerProcess(embedded_worker_id_);
  status_ = STOPPED;
  process_id_ = -1;
  thread_id_ = -1;
  worker_devtools_agent_route_id_ = MSG_ROUTING_NONE;
  FOR_EACH_OBSERVER(Listener, listener_list_, OnStopped());
}

bool EmbeddedWorkerInstance::OnMessageReceived(const IPC::Message& message) {
  ListenerList::Iterator it(listener_list_);
  while (Listener* listener = it.GetNext()) {
    if (listener->OnMessageReceived(message))
      return true;
  }
  return false;
}

void EmbeddedWorkerInstance::OnReportException(
    const base::string16& error_message,
    int line_number,
    int column_number,
    const GURL& source_url) {
  FOR_EACH_OBSERVER(
      Listener,
      listener_list_,
      OnReportException(error_message, line_number, column_number, source_url));
}

void EmbeddedWorkerInstance::OnReportConsoleMessage(
    int source_identifier,
    int message_level,
    const base::string16& message,
    int line_number,
    const GURL& source_url) {
  FOR_EACH_OBSERVER(
      Listener,
      listener_list_,
      OnReportConsoleMessage(
          source_identifier, message_level, message, line_number, source_url));
}

void EmbeddedWorkerInstance::AddListener(Listener* listener) {
  listener_list_.AddObserver(listener);
}

void EmbeddedWorkerInstance::RemoveListener(Listener* listener) {
  listener_list_.RemoveObserver(listener);
}

std::vector<int> EmbeddedWorkerInstance::SortProcesses(
    const std::vector<int>& possible_process_ids) const {
  // Add the |possible_process_ids| to the existing process_refs_ since each one
  // is likely to take a reference once the SW starts up.
  ProcessRefMap refs_with_new_ids = process_refs_;
  for (std::vector<int>::const_iterator it = possible_process_ids.begin();
       it != possible_process_ids.end();
       ++it) {
    refs_with_new_ids[*it]++;
  }

  std::vector<std::pair<int, int> > counted(refs_with_new_ids.begin(),
                                            refs_with_new_ids.end());
  // Sort descending by the reference count.
  std::sort(counted.begin(), counted.end(), SecondGreater());

  std::vector<int> result(counted.size());
  for (size_t i = 0; i < counted.size(); ++i)
    result[i] = counted[i].first;
  return result;
}

}  // namespace content
