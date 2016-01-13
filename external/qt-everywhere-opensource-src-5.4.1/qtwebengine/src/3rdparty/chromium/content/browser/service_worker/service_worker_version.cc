// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_version.h"

#include "base/command_line.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_utils.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"

namespace content {

typedef ServiceWorkerVersion::StatusCallback StatusCallback;
typedef ServiceWorkerVersion::MessageCallback MessageCallback;

namespace {

// Default delay to stop the worker context after all documents that
// are associated to the worker are closed.
// (Note that if all references to the version is dropped the worker
// is also stopped without delay)
const int64 kStopWorkerDelay = 30;  // 30 secs.

void RunSoon(const base::Closure& callback) {
  if (!callback.is_null())
    base::MessageLoop::current()->PostTask(FROM_HERE, callback);
}

template <typename CallbackArray, typename Arg>
void RunCallbacks(ServiceWorkerVersion* version,
                  CallbackArray* callbacks_ptr,
                  const Arg& arg) {
  CallbackArray callbacks;
  callbacks.swap(*callbacks_ptr);
  scoped_refptr<ServiceWorkerVersion> protect(version);
  for (typename CallbackArray::const_iterator i = callbacks.begin();
       i != callbacks.end(); ++i)
    (*i).Run(arg);
}

template <typename IDMAP, typename Method, typename Params>
void RunIDMapCallbacks(IDMAP* callbacks, Method method, const Params& params) {
  typename IDMAP::iterator iter(callbacks);
  while (!iter.IsAtEnd()) {
    DispatchToMethod(iter.GetCurrentValue(), method, params);
    iter.Advance();
  }
  callbacks->Clear();
}

// A callback adapter to start a |task| after StartWorker.
void RunTaskAfterStartWorker(
    base::WeakPtr<ServiceWorkerVersion> version,
    const StatusCallback& error_callback,
    const base::Closure& task,
    ServiceWorkerStatusCode status) {
  if (status != SERVICE_WORKER_OK) {
    if (!error_callback.is_null())
      error_callback.Run(status);
    return;
  }
  if (version->running_status() != ServiceWorkerVersion::RUNNING) {
    // We've tried to start the worker (and it has succeeded), but
    // it looks it's not running yet.
    NOTREACHED() << "The worker's not running after successful StartWorker";
    if (!error_callback.is_null())
      error_callback.Run(SERVICE_WORKER_ERROR_START_WORKER_FAILED);
    return;
  }
  task.Run();
}

void RunErrorFetchCallback(const ServiceWorkerVersion::FetchCallback& callback,
                           ServiceWorkerStatusCode status) {
  callback.Run(status,
               SERVICE_WORKER_FETCH_EVENT_RESULT_FALLBACK,
               ServiceWorkerResponse());
}

}  // namespace

ServiceWorkerVersion::ServiceWorkerVersion(
    ServiceWorkerRegistration* registration,
    int64 version_id,
    base::WeakPtr<ServiceWorkerContextCore> context)
    : version_id_(version_id),
      registration_id_(kInvalidServiceWorkerVersionId),
      status_(NEW),
      context_(context),
      script_cache_map_(this, context),
      weak_factory_(this) {
  DCHECK(context_);
  DCHECK(registration);
  if (registration) {
    registration_id_ = registration->id();
    script_url_ = registration->script_url();
    scope_ = registration->pattern();
  }
  context_->AddLiveVersion(this);
  embedded_worker_ = context_->embedded_worker_registry()->CreateWorker();
  embedded_worker_->AddListener(this);
}

ServiceWorkerVersion::~ServiceWorkerVersion() {
  embedded_worker_->RemoveListener(this);
  if (context_)
    context_->RemoveLiveVersion(version_id_);
  // EmbeddedWorker's dtor sends StopWorker if it's still running.
}

void ServiceWorkerVersion::SetStatus(Status status) {
  if (status_ == status)
    return;

  status_ = status;

  std::vector<base::Closure> callbacks;
  callbacks.swap(status_change_callbacks_);
  for (std::vector<base::Closure>::const_iterator i = callbacks.begin();
       i != callbacks.end(); ++i) {
    (*i).Run();
  }

  FOR_EACH_OBSERVER(Listener, listeners_, OnVersionStateChanged(this));
}

void ServiceWorkerVersion::RegisterStatusChangeCallback(
    const base::Closure& callback) {
  status_change_callbacks_.push_back(callback);
}

ServiceWorkerVersionInfo ServiceWorkerVersion::GetInfo() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return ServiceWorkerVersionInfo(
      running_status(),
      status(),
      version_id(),
      embedded_worker()->process_id(),
      embedded_worker()->thread_id(),
      embedded_worker()->worker_devtools_agent_route_id());
}

void ServiceWorkerVersion::StartWorker(const StatusCallback& callback) {
  StartWorkerWithCandidateProcesses(std::vector<int>(), callback);
}

void ServiceWorkerVersion::StartWorkerWithCandidateProcesses(
    const std::vector<int>& possible_process_ids,
    const StatusCallback& callback) {
  switch (running_status()) {
    case RUNNING:
      RunSoon(base::Bind(callback, SERVICE_WORKER_OK));
      return;
    case STOPPING:
      RunSoon(base::Bind(callback, SERVICE_WORKER_ERROR_START_WORKER_FAILED));
      return;
    case STOPPED:
    case STARTING:
      start_callbacks_.push_back(callback);
      if (running_status() == STOPPED) {
        embedded_worker_->Start(
            version_id_,
            scope_,
            script_url_,
            possible_process_ids,
            base::Bind(&ServiceWorkerVersion::RunStartWorkerCallbacksOnError,
                       weak_factory_.GetWeakPtr()));
      }
      return;
  }
}

void ServiceWorkerVersion::StopWorker(const StatusCallback& callback) {
  if (running_status() == STOPPED) {
    RunSoon(base::Bind(callback, SERVICE_WORKER_OK));
    return;
  }
  if (stop_callbacks_.empty()) {
    ServiceWorkerStatusCode status = embedded_worker_->Stop();
    if (status != SERVICE_WORKER_OK) {
      RunSoon(base::Bind(callback, status));
      return;
    }
  }
  stop_callbacks_.push_back(callback);
}

void ServiceWorkerVersion::SendMessage(
    const IPC::Message& message, const StatusCallback& callback) {
  if (running_status() != RUNNING) {
    // Schedule calling this method after starting the worker.
    StartWorker(base::Bind(&RunTaskAfterStartWorker,
                           weak_factory_.GetWeakPtr(), callback,
                           base::Bind(&self::SendMessage,
                                      weak_factory_.GetWeakPtr(),
                                      message, callback)));
    return;
  }

  ServiceWorkerStatusCode status = embedded_worker_->SendMessage(message);
  RunSoon(base::Bind(callback, status));
}

void ServiceWorkerVersion::DispatchInstallEvent(
    int active_version_id,
    const StatusCallback& callback) {
  DCHECK_EQ(NEW, status()) << status();
  SetStatus(INSTALLING);

  if (running_status() != RUNNING) {
    // Schedule calling this method after starting the worker.
    StartWorker(
        base::Bind(&RunTaskAfterStartWorker,
                   weak_factory_.GetWeakPtr(),
                   callback,
                   base::Bind(&self::DispatchInstallEventAfterStartWorker,
                              weak_factory_.GetWeakPtr(),
                              active_version_id,
                              callback)));
  } else {
    DispatchInstallEventAfterStartWorker(active_version_id, callback);
  }
}

void ServiceWorkerVersion::DispatchActivateEvent(
    const StatusCallback& callback) {
  DCHECK_EQ(INSTALLED, status()) << status();
  SetStatus(ACTIVATING);

  if (running_status() != RUNNING) {
    // Schedule calling this method after starting the worker.
    StartWorker(
        base::Bind(&RunTaskAfterStartWorker,
                   weak_factory_.GetWeakPtr(),
                   callback,
                   base::Bind(&self::DispatchActivateEventAfterStartWorker,
                              weak_factory_.GetWeakPtr(),
                              callback)));
  } else {
    DispatchActivateEventAfterStartWorker(callback);
  }
}

void ServiceWorkerVersion::DispatchFetchEvent(
    const ServiceWorkerFetchRequest& request,
    const FetchCallback& callback) {
  DCHECK_EQ(ACTIVE, status()) << status();

  if (running_status() != RUNNING) {
    // Schedule calling this method after starting the worker.
    StartWorker(base::Bind(&RunTaskAfterStartWorker,
                           weak_factory_.GetWeakPtr(),
                           base::Bind(&RunErrorFetchCallback, callback),
                           base::Bind(&self::DispatchFetchEvent,
                                      weak_factory_.GetWeakPtr(),
                                      request, callback)));
    return;
  }

  int request_id = fetch_callbacks_.Add(new FetchCallback(callback));
  ServiceWorkerStatusCode status = embedded_worker_->SendMessage(
      ServiceWorkerMsg_FetchEvent(request_id, request));
  if (status != SERVICE_WORKER_OK) {
    fetch_callbacks_.Remove(request_id);
    RunSoon(base::Bind(&RunErrorFetchCallback,
                       callback,
                       SERVICE_WORKER_ERROR_FAILED));
  }
}

void ServiceWorkerVersion::DispatchSyncEvent(const StatusCallback& callback) {
  DCHECK_EQ(ACTIVE, status()) << status();

  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableServiceWorkerSync)) {
    callback.Run(SERVICE_WORKER_ERROR_ABORT);
    return;
  }

  if (running_status() != RUNNING) {
    // Schedule calling this method after starting the worker.
    StartWorker(base::Bind(&RunTaskAfterStartWorker,
                           weak_factory_.GetWeakPtr(), callback,
                           base::Bind(&self::DispatchSyncEvent,
                                      weak_factory_.GetWeakPtr(),
                                      callback)));
    return;
  }

  int request_id = sync_callbacks_.Add(new StatusCallback(callback));
  ServiceWorkerStatusCode status = embedded_worker_->SendMessage(
      ServiceWorkerMsg_SyncEvent(request_id));
  if (status != SERVICE_WORKER_OK) {
    sync_callbacks_.Remove(request_id);
    RunSoon(base::Bind(callback, status));
  }
}

void ServiceWorkerVersion::DispatchPushEvent(const StatusCallback& callback,
                                             const std::string& data) {
  DCHECK_EQ(ACTIVE, status()) << status();

  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableExperimentalWebPlatformFeatures)) {
    callback.Run(SERVICE_WORKER_ERROR_ABORT);
    return;
  }

  if (running_status() != RUNNING) {
    // Schedule calling this method after starting the worker.
    StartWorker(base::Bind(&RunTaskAfterStartWorker,
                           weak_factory_.GetWeakPtr(), callback,
                           base::Bind(&self::DispatchPushEvent,
                                      weak_factory_.GetWeakPtr(),
                                      callback, data)));
    return;
  }

  int request_id = push_callbacks_.Add(new StatusCallback(callback));
  ServiceWorkerStatusCode status = embedded_worker_->SendMessage(
      ServiceWorkerMsg_PushEvent(request_id, data));
  if (status != SERVICE_WORKER_OK) {
    push_callbacks_.Remove(request_id);
    RunSoon(base::Bind(callback, status));
  }
}

void ServiceWorkerVersion::AddProcessToWorker(int process_id) {
  embedded_worker_->AddProcessReference(process_id);
}

void ServiceWorkerVersion::RemoveProcessFromWorker(int process_id) {
  embedded_worker_->ReleaseProcessReference(process_id);
}

bool ServiceWorkerVersion::HasProcessToRun() const {
  return embedded_worker_->HasProcessToRun();
}

void ServiceWorkerVersion::AddControllee(
    ServiceWorkerProviderHost* provider_host) {
  DCHECK(!ContainsKey(controllee_map_, provider_host));
  int controllee_id = controllee_by_id_.Add(provider_host);
  controllee_map_[provider_host] = controllee_id;
  AddProcessToWorker(provider_host->process_id());
  if (stop_worker_timer_.IsRunning())
    stop_worker_timer_.Stop();
}

void ServiceWorkerVersion::RemoveControllee(
    ServiceWorkerProviderHost* provider_host) {
  ControlleeMap::iterator found = controllee_map_.find(provider_host);
  DCHECK(found != controllee_map_.end());
  controllee_by_id_.Remove(found->second);
  controllee_map_.erase(found);
  RemoveProcessFromWorker(provider_host->process_id());
  if (!HasControllee())
    ScheduleStopWorker();
  // TODO(kinuko): Fire NoControllees notification when the # of controllees
  // reaches 0, so that a new pending version can be activated (which will
  // deactivate this version).
  // TODO(michaeln): On no controllees call storage DeleteVersionResources
  // if this version has been deactivated. Probably storage can listen for
  // NoControllees for versions that have been deleted.
}

void ServiceWorkerVersion::AddWaitingControllee(
    ServiceWorkerProviderHost* provider_host) {
  AddProcessToWorker(provider_host->process_id());
}

void ServiceWorkerVersion::RemoveWaitingControllee(
    ServiceWorkerProviderHost* provider_host) {
  RemoveProcessFromWorker(provider_host->process_id());
}

void ServiceWorkerVersion::AddListener(Listener* listener) {
  listeners_.AddObserver(listener);
}

void ServiceWorkerVersion::RemoveListener(Listener* listener) {
  listeners_.RemoveObserver(listener);
}

void ServiceWorkerVersion::OnStarted() {
  DCHECK_EQ(RUNNING, running_status());
  // Fire all start callbacks.
  RunCallbacks(this, &start_callbacks_, SERVICE_WORKER_OK);
  FOR_EACH_OBSERVER(Listener, listeners_, OnWorkerStarted(this));
}

void ServiceWorkerVersion::OnStopped() {
  DCHECK_EQ(STOPPED, running_status());
  scoped_refptr<ServiceWorkerVersion> protect(this);

  // Fire all stop callbacks.
  RunCallbacks(this, &stop_callbacks_, SERVICE_WORKER_OK);

  // Let all start callbacks fail.
  RunCallbacks(
      this, &start_callbacks_, SERVICE_WORKER_ERROR_START_WORKER_FAILED);

  // Let all message callbacks fail (this will also fire and clear all
  // callbacks for events).
  // TODO(kinuko): Consider if we want to add queue+resend mechanism here.
  RunIDMapCallbacks(&activate_callbacks_,
                    &StatusCallback::Run,
                    MakeTuple(SERVICE_WORKER_ERROR_ACTIVATE_WORKER_FAILED));
  RunIDMapCallbacks(&install_callbacks_,
                    &StatusCallback::Run,
                    MakeTuple(SERVICE_WORKER_ERROR_INSTALL_WORKER_FAILED));
  RunIDMapCallbacks(&fetch_callbacks_,
                    &FetchCallback::Run,
                    MakeTuple(SERVICE_WORKER_ERROR_FAILED,
                              SERVICE_WORKER_FETCH_EVENT_RESULT_FALLBACK,
                              ServiceWorkerResponse()));
  RunIDMapCallbacks(&sync_callbacks_,
                    &StatusCallback::Run,
                    MakeTuple(SERVICE_WORKER_ERROR_FAILED));
  RunIDMapCallbacks(&push_callbacks_,
                    &StatusCallback::Run,
                    MakeTuple(SERVICE_WORKER_ERROR_FAILED));

  FOR_EACH_OBSERVER(Listener, listeners_, OnWorkerStopped(this));
}

void ServiceWorkerVersion::OnReportException(
    const base::string16& error_message,
    int line_number,
    int column_number,
    const GURL& source_url) {
  FOR_EACH_OBSERVER(
      Listener,
      listeners_,
      OnErrorReported(
          this, error_message, line_number, column_number, source_url));
}

void ServiceWorkerVersion::OnReportConsoleMessage(int source_identifier,
                                                  int message_level,
                                                  const base::string16& message,
                                                  int line_number,
                                                  const GURL& source_url) {
  FOR_EACH_OBSERVER(Listener,
                    listeners_,
                    OnReportConsoleMessage(this,
                                           source_identifier,
                                           message_level,
                                           message,
                                           line_number,
                                           source_url));
}

bool ServiceWorkerVersion::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ServiceWorkerVersion, message)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_GetClientDocuments,
                        OnGetClientDocuments)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_ActivateEventFinished,
                        OnActivateEventFinished)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_InstallEventFinished,
                        OnInstallEventFinished)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_FetchEventFinished,
                        OnFetchEventFinished)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_SyncEventFinished,
                        OnSyncEventFinished)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_PushEventFinished,
                        OnPushEventFinished)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_PostMessageToDocument,
                        OnPostMessageToDocument)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ServiceWorkerVersion::RunStartWorkerCallbacksOnError(
    ServiceWorkerStatusCode status) {
  if (status != SERVICE_WORKER_OK)
    RunCallbacks(this, &start_callbacks_, status);
}

void ServiceWorkerVersion::DispatchInstallEventAfterStartWorker(
    int active_version_id,
    const StatusCallback& callback) {
  DCHECK_EQ(RUNNING, running_status())
      << "Worker stopped too soon after it was started.";
  int request_id = install_callbacks_.Add(new StatusCallback(callback));
  ServiceWorkerStatusCode status = embedded_worker_->SendMessage(
      ServiceWorkerMsg_InstallEvent(request_id, active_version_id));
  if (status != SERVICE_WORKER_OK) {
    install_callbacks_.Remove(request_id);
    RunSoon(base::Bind(callback, status));
  }
}

void ServiceWorkerVersion::DispatchActivateEventAfterStartWorker(
    const StatusCallback& callback) {
  DCHECK_EQ(RUNNING, running_status())
      << "Worker stopped too soon after it was started.";
  int request_id = activate_callbacks_.Add(new StatusCallback(callback));
  ServiceWorkerStatusCode status =
      embedded_worker_->SendMessage(ServiceWorkerMsg_ActivateEvent(request_id));
  if (status != SERVICE_WORKER_OK) {
    activate_callbacks_.Remove(request_id);
    RunSoon(base::Bind(callback, status));
  }
}

void ServiceWorkerVersion::OnGetClientDocuments(int request_id) {
  std::vector<int> client_ids;
  ControlleeByIDMap::iterator it(&controllee_by_id_);
  while (!it.IsAtEnd()) {
    client_ids.push_back(it.GetCurrentKey());
    it.Advance();
  }
  // Don't bother if it's no longer running.
  if (running_status() == RUNNING) {
    embedded_worker_->SendMessage(
        ServiceWorkerMsg_DidGetClientDocuments(request_id, client_ids));
  }
}

void ServiceWorkerVersion::OnActivateEventFinished(
    int request_id,
    blink::WebServiceWorkerEventResult result) {
  StatusCallback* callback = activate_callbacks_.Lookup(request_id);
  if (!callback) {
    NOTREACHED() << "Got unexpected message: " << request_id;
    return;
  }
  ServiceWorkerStatusCode status = SERVICE_WORKER_OK;
  if (result == blink::WebServiceWorkerEventResultRejected)
    status = SERVICE_WORKER_ERROR_ACTIVATE_WORKER_FAILED;
  else
    SetStatus(ACTIVE);

  scoped_refptr<ServiceWorkerVersion> protect(this);
  callback->Run(status);
  activate_callbacks_.Remove(request_id);
}

void ServiceWorkerVersion::OnInstallEventFinished(
    int request_id,
    blink::WebServiceWorkerEventResult result) {
  StatusCallback* callback = install_callbacks_.Lookup(request_id);
  if (!callback) {
    NOTREACHED() << "Got unexpected message: " << request_id;
    return;
  }
  ServiceWorkerStatusCode status = SERVICE_WORKER_OK;
  if (result == blink::WebServiceWorkerEventResultRejected)
    status = SERVICE_WORKER_ERROR_INSTALL_WORKER_FAILED;
  else
    SetStatus(INSTALLED);

  scoped_refptr<ServiceWorkerVersion> protect(this);
  callback->Run(status);
  install_callbacks_.Remove(request_id);
}

void ServiceWorkerVersion::OnFetchEventFinished(
    int request_id,
    ServiceWorkerFetchEventResult result,
    const ServiceWorkerResponse& response) {
  FetchCallback* callback = fetch_callbacks_.Lookup(request_id);
  if (!callback) {
    NOTREACHED() << "Got unexpected message: " << request_id;
    return;
  }

  scoped_refptr<ServiceWorkerVersion> protect(this);
  callback->Run(SERVICE_WORKER_OK, result, response);
  fetch_callbacks_.Remove(request_id);
}

void ServiceWorkerVersion::OnSyncEventFinished(
    int request_id) {
  StatusCallback* callback = sync_callbacks_.Lookup(request_id);
  if (!callback) {
    NOTREACHED() << "Got unexpected message: " << request_id;
    return;
  }

  scoped_refptr<ServiceWorkerVersion> protect(this);
  callback->Run(SERVICE_WORKER_OK);
  sync_callbacks_.Remove(request_id);
}

void ServiceWorkerVersion::OnPushEventFinished(
    int request_id) {
  StatusCallback* callback = push_callbacks_.Lookup(request_id);
  if (!callback) {
    NOTREACHED() << "Got unexpected message: " << request_id;
    return;
  }

  scoped_refptr<ServiceWorkerVersion> protect(this);
  callback->Run(SERVICE_WORKER_OK);
  push_callbacks_.Remove(request_id);
}

void ServiceWorkerVersion::OnPostMessageToDocument(
    int client_id,
    const base::string16& message,
    const std::vector<int>& sent_message_port_ids) {
  ServiceWorkerProviderHost* provider_host =
      controllee_by_id_.Lookup(client_id);
  if (!provider_host) {
    // The client may already have been closed, just ignore.
    return;
  }
  provider_host->PostMessage(message, sent_message_port_ids);
}

void ServiceWorkerVersion::ScheduleStopWorker() {
  if (running_status() != RUNNING)
    return;
  if (stop_worker_timer_.IsRunning()) {
    stop_worker_timer_.Reset();
    return;
  }
  stop_worker_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kStopWorkerDelay),
      base::Bind(&ServiceWorkerVersion::StopWorker,
                 weak_factory_.GetWeakPtr(),
                 base::Bind(&ServiceWorkerUtils::NoOpStatusCallback)));
}

}  // namespace content
