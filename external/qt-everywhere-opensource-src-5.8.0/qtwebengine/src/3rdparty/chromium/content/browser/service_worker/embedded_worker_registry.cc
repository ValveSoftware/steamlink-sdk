// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/embedded_worker_registry.h"

#include "base/bind_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/public/browser/browser_thread.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sender.h"

namespace content {

// static
scoped_refptr<EmbeddedWorkerRegistry> EmbeddedWorkerRegistry::Create(
    const base::WeakPtr<ServiceWorkerContextCore>& context) {
  return make_scoped_refptr(new EmbeddedWorkerRegistry(context, 0));
}

// static
scoped_refptr<EmbeddedWorkerRegistry> EmbeddedWorkerRegistry::Create(
    const base::WeakPtr<ServiceWorkerContextCore>& context,
    EmbeddedWorkerRegistry* old_registry) {
  scoped_refptr<EmbeddedWorkerRegistry> registry =
      new EmbeddedWorkerRegistry(
          context,
          old_registry->next_embedded_worker_id_);
  registry->process_sender_map_.swap(old_registry->process_sender_map_);
  return registry;
}

std::unique_ptr<EmbeddedWorkerInstance> EmbeddedWorkerRegistry::CreateWorker() {
  std::unique_ptr<EmbeddedWorkerInstance> worker(
      new EmbeddedWorkerInstance(context_, next_embedded_worker_id_));
  worker_map_[next_embedded_worker_id_++] = worker.get();
  return worker;
}

ServiceWorkerStatusCode EmbeddedWorkerRegistry::StopWorker(
    int process_id, int embedded_worker_id) {
  return Send(process_id,
              new EmbeddedWorkerMsg_StopWorker(embedded_worker_id));
}

bool EmbeddedWorkerRegistry::OnMessageReceived(const IPC::Message& message,
                                               int process_id) {
  // TODO(kinuko): Move all EmbeddedWorker message handling from
  // ServiceWorkerDispatcherHost.

  EmbeddedWorkerInstance* worker =
      GetWorkerForMessage(process_id, message.routing_id());
  if (!worker) {
    // Assume this is from a detached worker, return true to indicate we're
    // purposely handling the message as no-op.
    return true;
  }
  bool handled = worker->OnMessageReceived(message);

  // Assume an unhandled message for a stopping worker is because the message
  // was timed out and its handler removed prior to stopping.
  // We might be more precise and record timed out request ids, but some
  // cumbersome bookkeeping is needed and the IPC messaging will soon migrate
  // to Mojo anyway.
  return handled || worker->status() == EmbeddedWorkerStatus::STOPPING;
}

void EmbeddedWorkerRegistry::Shutdown() {
  for (WorkerInstanceMap::iterator it = worker_map_.begin();
       it != worker_map_.end();
       ++it) {
    it->second->Stop();
  }
}

void EmbeddedWorkerRegistry::OnWorkerReadyForInspection(
    int process_id,
    int embedded_worker_id) {
  EmbeddedWorkerInstance* worker =
      GetWorkerForMessage(process_id, embedded_worker_id);
  if (!worker)
    return;
  worker->OnReadyForInspection();
}

void EmbeddedWorkerRegistry::OnWorkerScriptLoaded(int process_id,
                                                  int embedded_worker_id) {
  EmbeddedWorkerInstance* worker =
      GetWorkerForMessage(process_id, embedded_worker_id);
  if (!worker)
    return;
  worker->OnScriptLoaded();
}

void EmbeddedWorkerRegistry::OnWorkerThreadStarted(int process_id,
                                                   int thread_id,
                                                   int embedded_worker_id) {
  EmbeddedWorkerInstance* worker =
      GetWorkerForMessage(process_id, embedded_worker_id);
  if (!worker)
    return;
  worker->OnThreadStarted(thread_id);
}

void EmbeddedWorkerRegistry::OnWorkerScriptLoadFailed(int process_id,
                                                      int embedded_worker_id) {
  EmbeddedWorkerInstance* worker =
      GetWorkerForMessage(process_id, embedded_worker_id);
  if (!worker)
    return;
  worker->OnScriptLoadFailed();
}

void EmbeddedWorkerRegistry::OnWorkerScriptEvaluated(int process_id,
                                                     int embedded_worker_id,
                                                     bool success) {
  EmbeddedWorkerInstance* worker =
      GetWorkerForMessage(process_id, embedded_worker_id);
  if (!worker)
    return;
  worker->OnScriptEvaluated(success);
}

void EmbeddedWorkerRegistry::OnWorkerStarted(
    int process_id, int embedded_worker_id) {
  EmbeddedWorkerInstance* worker =
      GetWorkerForMessage(process_id, embedded_worker_id);
  if (!worker)
    return;

  if (!ContainsKey(worker_process_map_, process_id) ||
      worker_process_map_[process_id].count(embedded_worker_id) == 0) {
    return;
  }

  worker->OnStarted();
}

void EmbeddedWorkerRegistry::OnWorkerStopped(
    int process_id, int embedded_worker_id) {
  EmbeddedWorkerInstance* worker =
      GetWorkerForMessage(process_id, embedded_worker_id);
  if (!worker)
    return;
  worker_process_map_[process_id].erase(embedded_worker_id);
  worker->OnStopped();
}

void EmbeddedWorkerRegistry::OnReportException(
    int embedded_worker_id,
    const base::string16& error_message,
    int line_number,
    int column_number,
    const GURL& source_url) {
  EmbeddedWorkerInstance* worker = GetWorker(embedded_worker_id);
  if (!worker)
    return;
  worker->OnReportException(error_message, line_number, column_number,
                            source_url);
}

void EmbeddedWorkerRegistry::OnReportConsoleMessage(
    int embedded_worker_id,
    int source_identifier,
    int message_level,
    const base::string16& message,
    int line_number,
    const GURL& source_url) {
  EmbeddedWorkerInstance* worker = GetWorker(embedded_worker_id);
  if (!worker)
    return;
  worker->OnReportConsoleMessage(source_identifier, message_level, message,
                                 line_number, source_url);
}

void EmbeddedWorkerRegistry::AddChildProcessSender(
    int process_id,
    IPC::Sender* sender,
    MessagePortMessageFilter* message_port_message_filter) {
  process_sender_map_[process_id] = sender;
  process_message_port_message_filter_map_[process_id] =
      message_port_message_filter;
  DCHECK(!ContainsKey(worker_process_map_, process_id));
}

void EmbeddedWorkerRegistry::RemoveChildProcessSender(int process_id) {
  process_sender_map_.erase(process_id);
  process_message_port_message_filter_map_.erase(process_id);
  std::map<int, std::set<int> >::iterator found =
      worker_process_map_.find(process_id);
  if (found != worker_process_map_.end()) {
    const std::set<int>& worker_set = worker_process_map_[process_id];
    for (std::set<int>::const_iterator it = worker_set.begin();
         it != worker_set.end();
         ++it) {
      int embedded_worker_id = *it;
      DCHECK(ContainsKey(worker_map_, embedded_worker_id));
      // Somehow the worker thread has lost contact with the browser process.
      // The renderer may have been killed.  Set the worker's status to STOPPED
      // so a new thread can be created for this version. Use OnDetached rather
      // than OnStopped so UMA doesn't record it as a normal stoppage.
      worker_map_[embedded_worker_id]->OnDetached();
    }
    worker_process_map_.erase(found);
  }
}

EmbeddedWorkerInstance* EmbeddedWorkerRegistry::GetWorker(
    int embedded_worker_id) {
  WorkerInstanceMap::iterator found = worker_map_.find(embedded_worker_id);
  if (found == worker_map_.end())
    return nullptr;
  return found->second;
}

bool EmbeddedWorkerRegistry::CanHandle(int embedded_worker_id) const {
  if (embedded_worker_id < initial_embedded_worker_id_ ||
      next_embedded_worker_id_ <= embedded_worker_id) {
    return false;
  }
  return true;
}

MessagePortMessageFilter*
EmbeddedWorkerRegistry::MessagePortMessageFilterForProcess(int process_id) {
  return process_message_port_message_filter_map_[process_id];
}

EmbeddedWorkerRegistry::EmbeddedWorkerRegistry(
    const base::WeakPtr<ServiceWorkerContextCore>& context,
    int initial_embedded_worker_id)
    : context_(context),
      next_embedded_worker_id_(initial_embedded_worker_id),
      initial_embedded_worker_id_(initial_embedded_worker_id) {
}

EmbeddedWorkerRegistry::~EmbeddedWorkerRegistry() {
  Shutdown();
}

ServiceWorkerStatusCode EmbeddedWorkerRegistry::SendStartWorker(
    std::unique_ptr<EmbeddedWorkerMsg_StartWorker_Params> params,
    int process_id) {
  if (!context_)
    return SERVICE_WORKER_ERROR_ABORT;

  // The ServiceWorkerDispatcherHost is supposed to be created when the process
  // is created, and keep an entry in process_sender_map_ for its whole
  // lifetime.
  DCHECK(ContainsKey(process_sender_map_, process_id));

  int embedded_worker_id = params->embedded_worker_id;
  WorkerInstanceMap::iterator found = worker_map_.find(embedded_worker_id);
  DCHECK(found != worker_map_.end());
  DCHECK_EQ(found->second->process_id(), process_id);

  DCHECK(!ContainsKey(worker_process_map_, process_id) ||
         worker_process_map_[process_id].count(embedded_worker_id) == 0);

  ServiceWorkerStatusCode status =
      Send(process_id, new EmbeddedWorkerMsg_StartWorker(*params));
  if (status == SERVICE_WORKER_OK)
    worker_process_map_[process_id].insert(embedded_worker_id);
  return status;
}

ServiceWorkerStatusCode EmbeddedWorkerRegistry::Send(
    int process_id, IPC::Message* message_ptr) {
  std::unique_ptr<IPC::Message> message(message_ptr);
  if (!context_)
    return SERVICE_WORKER_ERROR_ABORT;
  ProcessToSenderMap::iterator found = process_sender_map_.find(process_id);
  if (found == process_sender_map_.end())
    return SERVICE_WORKER_ERROR_PROCESS_NOT_FOUND;
  if (!found->second->Send(message.release()))
    return SERVICE_WORKER_ERROR_IPC_FAILED;
  return SERVICE_WORKER_OK;
}

void EmbeddedWorkerRegistry::RemoveWorker(int process_id,
                                          int embedded_worker_id) {
  DCHECK(ContainsKey(worker_map_, embedded_worker_id));
  worker_map_.erase(embedded_worker_id);
  if (!ContainsKey(worker_process_map_, process_id))
    return;
  worker_process_map_[process_id].erase(embedded_worker_id);
  if (worker_process_map_[process_id].empty())
    worker_process_map_.erase(process_id);
}

EmbeddedWorkerInstance* EmbeddedWorkerRegistry::GetWorkerForMessage(
    int process_id,
    int embedded_worker_id) {
  EmbeddedWorkerInstance* worker = GetWorker(embedded_worker_id);
  if (!worker || worker->process_id() != process_id) {
    UMA_HISTOGRAM_BOOLEAN("ServiceWorker.WorkerForMessageFound", false);
    return nullptr;
  }
  UMA_HISTOGRAM_BOOLEAN("ServiceWorker.WorkerForMessageFound", true);
  return worker;
}

}  // namespace content
