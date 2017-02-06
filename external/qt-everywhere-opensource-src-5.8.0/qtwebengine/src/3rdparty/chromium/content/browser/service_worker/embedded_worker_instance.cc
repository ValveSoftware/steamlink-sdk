// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/embedded_worker_instance.h"

#include <utility>

#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/non_thread_safe.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/common/content_switches_internal.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/common/service_worker/embedded_worker_settings.h"
#include "content/common/service_worker/embedded_worker_setup.mojom.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/child_process_host.h"
#include "ipc/ipc_message.h"
#include "services/shell/public/cpp/interface_provider.h"
#include "services/shell/public/cpp/interface_registry.h"
#include "url/gurl.h"

namespace content {

namespace {

// When a service worker version's failure count exceeds
// |kMaxSameProcessFailureCount|, the embedded worker is forced to start in a
// new process.
const int kMaxSameProcessFailureCount = 2;

void NotifyWorkerReadyForInspectionOnUI(int worker_process_id,
                                        int worker_route_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ServiceWorkerDevToolsManager::GetInstance()->WorkerReadyForInspection(
      worker_process_id, worker_route_id);
}

void NotifyWorkerDestroyedOnUI(int worker_process_id, int worker_route_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ServiceWorkerDevToolsManager::GetInstance()->WorkerDestroyed(
      worker_process_id, worker_route_id);
}

void NotifyWorkerStopIgnoredOnUI(int worker_process_id, int worker_route_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ServiceWorkerDevToolsManager::GetInstance()->WorkerStopIgnored(
      worker_process_id, worker_route_id);
}

void NotifyWorkerVersionInstalledOnUI(int worker_process_id,
                                      int worker_route_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ServiceWorkerDevToolsManager::GetInstance()->WorkerVersionInstalled(
      worker_process_id, worker_route_id);
}

void NotifyWorkerVersionDoomedOnUI(int worker_process_id, int worker_route_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ServiceWorkerDevToolsManager::GetInstance()->WorkerVersionDoomed(
      worker_process_id, worker_route_id);
}

void RegisterToWorkerDevToolsManagerOnUI(
    int process_id,
    const ServiceWorkerContextCore* service_worker_context,
    const base::WeakPtr<ServiceWorkerContextCore>& service_worker_context_weak,
    int64_t service_worker_version_id,
    const GURL& url,
    const GURL& scope,
    bool is_installed,
    const base::Callback<void(int worker_devtools_agent_route_id,
                              bool wait_for_debugger)>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  int worker_devtools_agent_route_id = MSG_ROUTING_NONE;
  bool wait_for_debugger = false;
  if (RenderProcessHost* rph = RenderProcessHost::FromID(process_id)) {
    // |rph| may be NULL in unit tests.
    worker_devtools_agent_route_id = rph->GetNextRoutingID();
    wait_for_debugger =
        ServiceWorkerDevToolsManager::GetInstance()->WorkerCreated(
            process_id, worker_devtools_agent_route_id,
            ServiceWorkerDevToolsManager::ServiceWorkerIdentifier(
                service_worker_context, service_worker_context_weak,
                service_worker_version_id, url, scope),
            is_installed);
  }
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(callback, worker_devtools_agent_route_id, wait_for_debugger));
}

void SetupMojoOnUIThread(
    int process_id,
    int thread_id,
    shell::mojom::InterfaceProviderRequest remote_interfaces,
    shell::mojom::InterfaceProviderPtrInfo exposed_interfaces) {
  RenderProcessHost* rph = RenderProcessHost::FromID(process_id);
  // |rph| or its InterfaceProvider may be NULL in unit tests.
  if (!rph || !rph->GetRemoteInterfaces())
    return;
  mojom::EmbeddedWorkerSetupPtr setup;
  rph->GetRemoteInterfaces()->GetInterface(&setup);
  setup->ExchangeInterfaceProviders(
      thread_id, std::move(remote_interfaces),
      mojo::MakeProxy(std::move(exposed_interfaces)));
}

}  // namespace

// Lives on IO thread, proxies notifications to DevToolsManager that lives on
// UI thread. Owned by EmbeddedWorkerInstance.
class EmbeddedWorkerInstance::DevToolsProxy : public base::NonThreadSafe {
 public:
  DevToolsProxy(int process_id, int agent_route_id)
      : process_id_(process_id),
        agent_route_id_(agent_route_id) {}

  ~DevToolsProxy() {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(NotifyWorkerDestroyedOnUI,
                   process_id_, agent_route_id_));
  }

  void NotifyWorkerReadyForInspection() {
    DCHECK(CalledOnValidThread());
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(NotifyWorkerReadyForInspectionOnUI,
                                       process_id_, agent_route_id_));
  }

  void NotifyWorkerStopIgnored() {
    DCHECK(CalledOnValidThread());
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(NotifyWorkerStopIgnoredOnUI,
                                       process_id_, agent_route_id_));
  }

  void NotifyWorkerVersionInstalled() {
    DCHECK(CalledOnValidThread());
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(NotifyWorkerVersionInstalledOnUI,
                                       process_id_, agent_route_id_));
  }

  void NotifyWorkerVersionDoomed() {
    DCHECK(CalledOnValidThread());
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(NotifyWorkerVersionDoomedOnUI,
                                       process_id_, agent_route_id_));
  }

  int agent_route_id() const { return agent_route_id_; }

 private:
  const int process_id_;
  const int agent_route_id_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsProxy);
};

// A handle for a worker process managed by ServiceWorkerProcessManager on the
// UI thread.
class EmbeddedWorkerInstance::WorkerProcessHandle {
 public:
  WorkerProcessHandle(const base::WeakPtr<ServiceWorkerContextCore>& context,
                      int embedded_worker_id,
                      int process_id,
                      bool is_new_process)
      : context_(context),
        embedded_worker_id_(embedded_worker_id),
        process_id_(process_id),
        is_new_process_(is_new_process) {
    DCHECK_NE(ChildProcessHost::kInvalidUniqueID, process_id_);
  }

  ~WorkerProcessHandle() {
    if (context_)
      context_->process_manager()->ReleaseWorkerProcess(embedded_worker_id_);
  }

  int process_id() const { return process_id_; }
  bool is_new_process() const { return is_new_process_; }

 private:
  base::WeakPtr<ServiceWorkerContextCore> context_;

  const int embedded_worker_id_;
  const int process_id_;
  const bool is_new_process_;

  DISALLOW_COPY_AND_ASSIGN(WorkerProcessHandle);
};

// A task to allocate a worker process and to send a start worker message. This
// is created on EmbeddedWorkerInstance::Start(), owned by the instance and
// destroyed on EmbeddedWorkerInstance::OnScriptEvaluated().
// We can abort starting worker by destroying this task anytime during the
// sequence.
class EmbeddedWorkerInstance::StartTask {
 public:
  enum class ProcessAllocationState { NOT_ALLOCATED, ALLOCATING, ALLOCATED };

  StartTask(EmbeddedWorkerInstance* instance, const GURL& script_url)
      : instance_(instance),
        state_(ProcessAllocationState::NOT_ALLOCATED),
        is_installed_(false),
        started_during_browser_startup_(false),
        weak_factory_(this) {
    TRACE_EVENT_ASYNC_BEGIN1("ServiceWorker", "EmbeddedWorkerInstance::Start",
                             this, "Script", script_url.spec());
  }

  ~StartTask() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    TRACE_EVENT_ASYNC_END0("ServiceWorker", "EmbeddedWorkerInstance::Start",
                           this);

    if (!instance_->context_)
      return;

    switch (state_) {
      case ProcessAllocationState::NOT_ALLOCATED:
        // Not necessary to release a process.
        break;
      case ProcessAllocationState::ALLOCATING:
        // Abort half-baked process allocation on the UI thread.
        instance_->context_->process_manager()->ReleaseWorkerProcess(
            instance_->embedded_worker_id());
        break;
      case ProcessAllocationState::ALLOCATED:
        // Otherwise, the process will be released by EmbeddedWorkerInstance.
        break;
    }

    // Don't have to abort |start_callback_| here. The caller of
    // EmbeddedWorkerInstance::Start(), that is, ServiceWorkerVersion does not
    // expect it when the start worker sequence is canceled by Stop() because
    // the callback, ServiceWorkerVersion::OnStartSentAndScriptEvaluated(),
    // could drain valid start requests queued in the version. After the worker
    // is stopped, the version attempts to restart the worker if there are
    // requests in the queue. See ServiceWorkerVersion::OnStoppedInternal() for
    // details.
    // TODO(nhiroki): Reconsider this bizarre layering.
  }

  void Start(std::unique_ptr<EmbeddedWorkerMsg_StartWorker_Params> params,
             const StatusCallback& callback) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    state_ = ProcessAllocationState::ALLOCATING;
    start_callback_ = callback;
    is_installed_ = params->is_installed;

    if (!GetContentClient()->browser()->IsBrowserStartupComplete())
      started_during_browser_startup_ = true;

    GURL scope(params->scope);
    GURL script_url(params->script_url);

    bool can_use_existing_process =
        instance_->context_->GetVersionFailureCount(
            params->service_worker_version_id) < kMaxSameProcessFailureCount;
    instance_->context_->process_manager()->AllocateWorkerProcess(
        instance_->embedded_worker_id_, scope, script_url,
        can_use_existing_process,
        base::Bind(&StartTask::OnProcessAllocated, weak_factory_.GetWeakPtr(),
                   base::Passed(&params)));
  }

  static void RunStartCallback(StartTask* task,
                               ServiceWorkerStatusCode status) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    StatusCallback callback = task->start_callback_;
    task->start_callback_.Reset();
    callback.Run(status);
    // |task| may be destroyed.
  }

  bool is_installed() const { return is_installed_; }

 private:
  void OnProcessAllocated(
      std::unique_ptr<EmbeddedWorkerMsg_StartWorker_Params> params,
      ServiceWorkerStatusCode status,
      int process_id,
      bool is_new_process,
      const EmbeddedWorkerSettings& settings) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    if (status != SERVICE_WORKER_OK) {
      TRACE_EVENT_ASYNC_STEP_PAST1(
          "ServiceWorker", "EmbeddedWorkerInstance::Start", this,
          "OnProcessAllocated", "Error", ServiceWorkerStatusToString(status));
      DCHECK_EQ(ChildProcessHost::kInvalidUniqueID, process_id);
      StatusCallback callback = start_callback_;
      start_callback_.Reset();
      instance_->OnStartFailed(callback, status);
      // |this| may be destroyed.
      return;
    }

    TRACE_EVENT_ASYNC_STEP_PAST1(
        "ServiceWorker", "EmbeddedWorkerInstance::Start", this,
        "OnProcessAllocated", "Is New Process", is_new_process);
    if (is_installed_)
      ServiceWorkerMetrics::RecordProcessCreated(is_new_process);

    ServiceWorkerMetrics::StartSituation start_situation =
        ServiceWorkerMetrics::StartSituation::UNKNOWN;
    if (started_during_browser_startup_)
      start_situation = ServiceWorkerMetrics::StartSituation::DURING_STARTUP;
    else if (is_new_process)
      start_situation = ServiceWorkerMetrics::StartSituation::NEW_PROCESS;
    else
      start_situation = ServiceWorkerMetrics::StartSituation::EXISTING_PROCESS;

    // Notify the instance that a process is allocated.
    state_ = ProcessAllocationState::ALLOCATED;
    instance_->OnProcessAllocated(
        base::WrapUnique(new WorkerProcessHandle(
            instance_->context_, instance_->embedded_worker_id(), process_id,
            is_new_process)),
        start_situation);

    // TODO(bengr): Support changes to this setting while the worker
    // is running.
    params->settings.data_saver_enabled = settings.data_saver_enabled;

    // Register the instance to DevToolsManager on UI thread.
    const int64_t service_worker_version_id = params->service_worker_version_id;
    const GURL& scope = params->scope;
    GURL script_url(params->script_url);
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(RegisterToWorkerDevToolsManagerOnUI, process_id,
                   instance_->context_.get(), instance_->context_,
                   service_worker_version_id, script_url, scope, is_installed_,
                   base::Bind(&StartTask::OnRegisteredToDevToolsManager,
                              weak_factory_.GetWeakPtr(), base::Passed(&params),
                              is_new_process)));
  }

  void OnRegisteredToDevToolsManager(
      std::unique_ptr<EmbeddedWorkerMsg_StartWorker_Params> params,
      bool is_new_process,
      int worker_devtools_agent_route_id,
      bool wait_for_debugger) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    TRACE_EVENT_ASYNC_STEP_PAST0("ServiceWorker",
                                 "EmbeddedWorkerInstance::Start", this,
                                 "OnRegisteredToDevToolsManager");

    // Notify the instance that it is registered to the devtools manager.
    instance_->OnRegisteredToDevToolsManager(
        is_new_process, worker_devtools_agent_route_id, wait_for_debugger);

    params->worker_devtools_agent_route_id = worker_devtools_agent_route_id;
    params->wait_for_debugger = wait_for_debugger;
    SendStartWorker(std::move(params));
  }

  void SendStartWorker(
      std::unique_ptr<EmbeddedWorkerMsg_StartWorker_Params> params) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    ServiceWorkerStatusCode status = instance_->registry_->SendStartWorker(
        std::move(params), instance_->process_id());
    TRACE_EVENT_ASYNC_STEP_PAST1(
        "ServiceWorker", "EmbeddedWorkerInstance::Start", this,
        "SendStartWorker", "Status", ServiceWorkerStatusToString(status));
    if (status != SERVICE_WORKER_OK) {
      StatusCallback callback = start_callback_;
      start_callback_.Reset();
      instance_->OnStartFailed(callback, status);
      // |this| may be destroyed.
      return;
    }
    instance_->OnStartWorkerMessageSent();

    // |start_callback_| will be called via RunStartCallback() when the script
    // is evaluated.
  }

  // |instance_| must outlive |this|.
  EmbeddedWorkerInstance* instance_;

  StatusCallback start_callback_;
  ProcessAllocationState state_;

  // Used for UMA.
  bool is_installed_;
  bool started_during_browser_startup_;

  base::WeakPtrFactory<StartTask> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(StartTask);
};

bool EmbeddedWorkerInstance::Listener::OnMessageReceived(
    const IPC::Message& message) {
  return false;
}

EmbeddedWorkerInstance::~EmbeddedWorkerInstance() {
  DCHECK(status_ == EmbeddedWorkerStatus::STOPPING ||
         status_ == EmbeddedWorkerStatus::STOPPED)
      << static_cast<int>(status_);
  devtools_proxy_.reset();
  if (registry_->GetWorker(embedded_worker_id_))
    registry_->RemoveWorker(process_id(), embedded_worker_id_);
  process_handle_.reset();
}

void EmbeddedWorkerInstance::Start(
    std::unique_ptr<EmbeddedWorkerMsg_StartWorker_Params> params,
    const StatusCallback& callback) {
  if (!context_) {
    callback.Run(SERVICE_WORKER_ERROR_ABORT);
    // |this| may be destroyed by the callback.
    return;
  }
  DCHECK(status_ == EmbeddedWorkerStatus::STOPPED);

  DCHECK(!params->pause_after_download || !params->is_installed);
  DCHECK_NE(kInvalidServiceWorkerVersionId, params->service_worker_version_id);
  step_time_ = base::TimeTicks::Now();
  status_ = EmbeddedWorkerStatus::STARTING;
  starting_phase_ = ALLOCATING_PROCESS;
  network_accessed_for_script_ = false;
  interface_registry_.reset(new shell::InterfaceRegistry(nullptr));
  remote_interfaces_.reset(new shell::InterfaceProvider);
  FOR_EACH_OBSERVER(Listener, listener_list_, OnStarting());

  params->embedded_worker_id = embedded_worker_id_;
  params->worker_devtools_agent_route_id = MSG_ROUTING_NONE;
  params->wait_for_debugger = false;
  params->settings.v8_cache_options = GetV8CacheOptions();

  inflight_start_task_.reset(new StartTask(this, params->script_url));
  inflight_start_task_->Start(std::move(params), callback);
}

ServiceWorkerStatusCode EmbeddedWorkerInstance::Stop() {
  DCHECK(status_ == EmbeddedWorkerStatus::STARTING ||
         status_ == EmbeddedWorkerStatus::RUNNING)
      << static_cast<int>(status_);

  // Abort an inflight start task.
  inflight_start_task_.reset();

  ServiceWorkerStatusCode status =
      registry_->StopWorker(process_id(), embedded_worker_id_);
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.SendStopWorker.Status", status,
                            SERVICE_WORKER_ERROR_MAX_VALUE);
  // StopWorker could fail if we were starting up and don't have a process yet,
  // or we can no longer communicate with the process. So just detach.
  if (status != SERVICE_WORKER_OK) {
    OnDetached();
    return status;
  }

  status_ = EmbeddedWorkerStatus::STOPPING;
  FOR_EACH_OBSERVER(Listener, listener_list_, OnStopping());
  return status;
}

void EmbeddedWorkerInstance::StopIfIdle() {
  if (devtools_attached_) {
    if (devtools_proxy_)
      devtools_proxy_->NotifyWorkerStopIgnored();
    return;
  }
  Stop();
}

ServiceWorkerStatusCode EmbeddedWorkerInstance::SendMessage(
    const IPC::Message& message) {
  DCHECK_NE(kInvalidEmbeddedWorkerThreadId, thread_id_);
  if (status_ != EmbeddedWorkerStatus::RUNNING &&
      status_ != EmbeddedWorkerStatus::STARTING) {
    return SERVICE_WORKER_ERROR_IPC_FAILED;
  }
  return registry_->Send(process_id(),
                         new EmbeddedWorkerContextMsg_MessageToWorker(
                             thread_id_, embedded_worker_id_, message));
}

void EmbeddedWorkerInstance::ResumeAfterDownload() {
  if (process_id() == ChildProcessHost::kInvalidUniqueID ||
      status_ != EmbeddedWorkerStatus::STARTING) {
    return;
  }
  registry_->Send(process_id(), new EmbeddedWorkerMsg_ResumeAfterDownload(
                                    embedded_worker_id_));
}

shell::InterfaceRegistry* EmbeddedWorkerInstance::GetInterfaceRegistry() {
  DCHECK(status_ == EmbeddedWorkerStatus::STARTING ||
         status_ == EmbeddedWorkerStatus::RUNNING)
      << static_cast<int>(status_);
  return interface_registry_.get();
}

shell::InterfaceProvider* EmbeddedWorkerInstance::GetRemoteInterfaces() {
  DCHECK(status_ == EmbeddedWorkerStatus::STARTING ||
         status_ == EmbeddedWorkerStatus::RUNNING)
      << static_cast<int>(status_);
  return remote_interfaces_.get();
}

EmbeddedWorkerInstance::EmbeddedWorkerInstance(
    base::WeakPtr<ServiceWorkerContextCore> context,
    int embedded_worker_id)
    : context_(context),
      registry_(context->embedded_worker_registry()),
      embedded_worker_id_(embedded_worker_id),
      status_(EmbeddedWorkerStatus::STOPPED),
      starting_phase_(NOT_STARTING),
      thread_id_(kInvalidEmbeddedWorkerThreadId),
      devtools_attached_(false),
      network_accessed_for_script_(false),
      weak_factory_(this) {}

void EmbeddedWorkerInstance::OnProcessAllocated(
    std::unique_ptr<WorkerProcessHandle> handle,
    ServiceWorkerMetrics::StartSituation start_situation) {
  DCHECK_EQ(EmbeddedWorkerStatus::STARTING, status_);
  DCHECK(!process_handle_);

  process_handle_ = std::move(handle);
  starting_phase_ = REGISTERING_TO_DEVTOOLS;
  start_situation_ = start_situation;
  FOR_EACH_OBSERVER(Listener, listener_list_, OnProcessAllocated());
}

void EmbeddedWorkerInstance::OnRegisteredToDevToolsManager(
    bool is_new_process,
    int worker_devtools_agent_route_id,
    bool wait_for_debugger) {
  if (worker_devtools_agent_route_id != MSG_ROUTING_NONE) {
    DCHECK(!devtools_proxy_);
    devtools_proxy_.reset(
        new DevToolsProxy(process_id(), worker_devtools_agent_route_id));
  }
  if (wait_for_debugger) {
    // We don't measure the start time when wait_for_debugger flag is set. So
    // we set the NULL time here.
    step_time_ = base::TimeTicks();
  }
}

void EmbeddedWorkerInstance::OnStartWorkerMessageSent() {
  if (!step_time_.is_null()) {
    base::TimeDelta duration = UpdateStepTime();
    if (inflight_start_task_->is_installed()) {
      ServiceWorkerMetrics::RecordTimeToSendStartWorker(duration,
                                                        start_situation_);
    }
  }

  starting_phase_ = SENT_START_WORKER;
  FOR_EACH_OBSERVER(Listener, listener_list_, OnStartWorkerMessageSent());
}

void EmbeddedWorkerInstance::OnReadyForInspection() {
  if (devtools_proxy_)
    devtools_proxy_->NotifyWorkerReadyForInspection();
}

void EmbeddedWorkerInstance::OnScriptReadStarted() {
  starting_phase_ = SCRIPT_READ_STARTED;
}

void EmbeddedWorkerInstance::OnScriptReadFinished() {
  starting_phase_ = SCRIPT_READ_FINISHED;
}

void EmbeddedWorkerInstance::OnScriptLoaded() {
  using LoadSource = ServiceWorkerMetrics::LoadSource;

  if (!inflight_start_task_)
    return;
  LoadSource source;
  if (network_accessed_for_script_) {
    DCHECK(!inflight_start_task_->is_installed());
    source = LoadSource::NETWORK;
  } else if (inflight_start_task_->is_installed()) {
    source = LoadSource::SERVICE_WORKER_STORAGE;
  } else {
    source = LoadSource::HTTP_CACHE;
  }
  TRACE_EVENT_ASYNC_STEP_PAST1(
      "ServiceWorker", "EmbeddedWorkerInstance::Start",
      inflight_start_task_.get(), "OnScriptLoaded", "Source",
      ServiceWorkerMetrics::LoadSourceToString(source));

  if (!step_time_.is_null()) {
    base::TimeDelta duration = UpdateStepTime();
    ServiceWorkerMetrics::RecordTimeToLoad(duration, source, start_situation_);
  }

  starting_phase_ = SCRIPT_LOADED;
  FOR_EACH_OBSERVER(Listener, listener_list_, OnScriptLoaded());
  // |this| may be destroyed by the callback.
}

void EmbeddedWorkerInstance::OnURLJobCreatedForMainScript() {
  if (!inflight_start_task_)
    return;
  TRACE_EVENT_ASYNC_STEP_PAST0("ServiceWorker", "EmbeddedWorkerInstance::Start",
                               inflight_start_task_.get(), "OnURLJobCreated");
  if (!step_time_.is_null()) {
    base::TimeDelta duration = UpdateStepTime();
    if (inflight_start_task_->is_installed())
      ServiceWorkerMetrics::RecordTimeToURLJob(duration, start_situation_);
  }
}

void EmbeddedWorkerInstance::OnWorkerVersionInstalled() {
  if (devtools_proxy_)
    devtools_proxy_->NotifyWorkerVersionInstalled();
}

void EmbeddedWorkerInstance::OnWorkerVersionDoomed() {
  if (devtools_proxy_)
    devtools_proxy_->NotifyWorkerVersionDoomed();
}

void EmbeddedWorkerInstance::OnThreadStarted(int thread_id) {
  if (!inflight_start_task_)
    return;
  TRACE_EVENT_ASYNC_STEP_PAST0("ServiceWorker", "EmbeddedWorkerInstance::Start",
                               inflight_start_task_.get(), "OnThreadStarted");

  starting_phase_ = THREAD_STARTED;
  if (!step_time_.is_null()) {
    base::TimeDelta duration = UpdateStepTime();
    if (inflight_start_task_->is_installed())
      ServiceWorkerMetrics::RecordTimeToStartThread(duration, start_situation_);
  }

  thread_id_ = thread_id;
  FOR_EACH_OBSERVER(Listener, listener_list_, OnThreadStarted());

  shell::mojom::InterfaceProviderPtr exposed_interfaces;
  interface_registry_->Bind(GetProxy(&exposed_interfaces));
  shell::mojom::InterfaceProviderPtr remote_interfaces;
  shell::mojom::InterfaceProviderRequest request = GetProxy(&remote_interfaces);
  remote_interfaces_->Bind(std::move(remote_interfaces));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(SetupMojoOnUIThread, process_id(), thread_id_,
                 base::Passed(&request),
                 base::Passed(exposed_interfaces.PassInterface())));
}

void EmbeddedWorkerInstance::OnScriptLoadFailed() {
  if (!inflight_start_task_)
    return;
  TRACE_EVENT_ASYNC_STEP_PAST0("ServiceWorker", "EmbeddedWorkerInstance::Start",
                               inflight_start_task_.get(),
                               "OnScriptLoadFailed");
  FOR_EACH_OBSERVER(Listener, listener_list_, OnScriptLoadFailed());
}

void EmbeddedWorkerInstance::OnScriptEvaluated(bool success) {
  if (!inflight_start_task_)
    return;
  DCHECK_EQ(EmbeddedWorkerStatus::STARTING, status_);

  TRACE_EVENT_ASYNC_STEP_PAST1("ServiceWorker", "EmbeddedWorkerInstance::Start",
                               inflight_start_task_.get(), "OnScriptEvaluated",
                               "Success", success);
  starting_phase_ = SCRIPT_EVALUATED;
  if (!step_time_.is_null()) {
    base::TimeDelta duration = UpdateStepTime();
    if (success && inflight_start_task_->is_installed())
      ServiceWorkerMetrics::RecordTimeToEvaluateScript(duration,
                                                       start_situation_);
  }

  base::WeakPtr<EmbeddedWorkerInstance> weak_this = weak_factory_.GetWeakPtr();
  StartTask::RunStartCallback(
      inflight_start_task_.get(),
      success ? SERVICE_WORKER_OK
              : SERVICE_WORKER_ERROR_SCRIPT_EVALUATE_FAILED);
  // |this| may be destroyed by the callback.
}

void EmbeddedWorkerInstance::OnStarted() {
  // Stop is requested before OnStarted is sent back from the worker.
  if (status_ == EmbeddedWorkerStatus::STOPPING)
    return;
  DCHECK(status_ == EmbeddedWorkerStatus::STARTING);
  status_ = EmbeddedWorkerStatus::RUNNING;
  inflight_start_task_.reset();
  FOR_EACH_OBSERVER(Listener, listener_list_, OnStarted());
}

void EmbeddedWorkerInstance::OnStopped() {
  EmbeddedWorkerStatus old_status = status_;
  ReleaseProcess();
  FOR_EACH_OBSERVER(Listener, listener_list_, OnStopped(old_status));
}

void EmbeddedWorkerInstance::OnDetached() {
  EmbeddedWorkerStatus old_status = status_;
  ReleaseProcess();
  FOR_EACH_OBSERVER(Listener, listener_list_, OnDetached(old_status));
}

void EmbeddedWorkerInstance::Detach() {
  registry_->RemoveWorker(process_id(), embedded_worker_id_);
  OnDetached();
}

base::WeakPtr<EmbeddedWorkerInstance> EmbeddedWorkerInstance::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool EmbeddedWorkerInstance::OnMessageReceived(const IPC::Message& message) {
  ListenerList::Iterator it(&listener_list_);
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

int EmbeddedWorkerInstance::process_id() const {
  if (process_handle_)
    return process_handle_->process_id();
  return ChildProcessHost::kInvalidUniqueID;
}

bool EmbeddedWorkerInstance::is_new_process() const {
  DCHECK(process_handle_);
  return process_handle_->is_new_process();
}

int EmbeddedWorkerInstance::worker_devtools_agent_route_id() const {
  if (devtools_proxy_)
    return devtools_proxy_->agent_route_id();
  return MSG_ROUTING_NONE;
}

MessagePortMessageFilter* EmbeddedWorkerInstance::message_port_message_filter()
    const {
  return registry_->MessagePortMessageFilterForProcess(process_id());
}

void EmbeddedWorkerInstance::AddListener(Listener* listener) {
  listener_list_.AddObserver(listener);
}

void EmbeddedWorkerInstance::RemoveListener(Listener* listener) {
  listener_list_.RemoveObserver(listener);
}

void EmbeddedWorkerInstance::OnNetworkAccessedForScriptLoad() {
  starting_phase_ = SCRIPT_DOWNLOADING;
  network_accessed_for_script_ = true;
}

void EmbeddedWorkerInstance::ReleaseProcess() {
  // Abort an inflight start task.
  inflight_start_task_.reset();

  devtools_proxy_.reset();
  process_handle_.reset();
  status_ = EmbeddedWorkerStatus::STOPPED;
  thread_id_ = kInvalidEmbeddedWorkerThreadId;
  interface_registry_.reset();
  remote_interfaces_.reset();
}

void EmbeddedWorkerInstance::OnStartFailed(const StatusCallback& callback,
                                           ServiceWorkerStatusCode status) {
  EmbeddedWorkerStatus old_status = status_;
  ReleaseProcess();
  base::WeakPtr<EmbeddedWorkerInstance> weak_this = weak_factory_.GetWeakPtr();
  callback.Run(status);
  if (weak_this && old_status != EmbeddedWorkerStatus::STOPPED)
    FOR_EACH_OBSERVER(Listener, weak_this->listener_list_,
                      OnStopped(old_status));
}

base::TimeDelta EmbeddedWorkerInstance::UpdateStepTime() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!step_time_.is_null());
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta duration = now - step_time_;
  step_time_ = now;
  return duration;
}

void EmbeddedWorkerInstance::AddMessageToConsole(ConsoleMessageLevel level,
                                                 const std::string& message) {
  if (status_ != EmbeddedWorkerStatus::RUNNING &&
      status_ != EmbeddedWorkerStatus::STARTING) {
    return;
  }
  registry_->Send(process_id(), new EmbeddedWorkerMsg_AddMessageToConsole(
                                    embedded_worker_id_, level, message));
}

// static
std::string EmbeddedWorkerInstance::StatusToString(
    EmbeddedWorkerStatus status) {
  switch (status) {
    case EmbeddedWorkerStatus::STOPPED:
      return "STOPPED";
    case EmbeddedWorkerStatus::STARTING:
      return "STARTING";
    case EmbeddedWorkerStatus::RUNNING:
      return "RUNNING";
    case EmbeddedWorkerStatus::STOPPING:
      return "STOPPING";
  }
  NOTREACHED() << static_cast<int>(status);
  return std::string();
}

// static
std::string EmbeddedWorkerInstance::StartingPhaseToString(StartingPhase phase) {
  switch (phase) {
    case NOT_STARTING:
      return "Not in STARTING status";
    case ALLOCATING_PROCESS:
      return "Allocating process";
    case REGISTERING_TO_DEVTOOLS:
      return "Registering to DevTools";
    case SENT_START_WORKER:
      return "Sent StartWorker message to renderer";
    case SCRIPT_DOWNLOADING:
      return "Script downloading";
    case SCRIPT_LOADED:
      return "Script loaded";
    case SCRIPT_EVALUATED:
      return "Script evaluated";
    case THREAD_STARTED:
      return "Thread started";
    case SCRIPT_READ_STARTED:
      return "Script read started";
    case SCRIPT_READ_FINISHED:
      return "Script read finished";
    case STARTING_PHASE_MAX_VALUE:
      NOTREACHED();
  }
  NOTREACHED() << phase;
  return std::string();
}

}  // namespace content
