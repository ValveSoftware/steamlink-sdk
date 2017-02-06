// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_version.h"

#include <stddef.h>

#include <limits>
#include <map>
#include <string>

#include "base/command_line.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "content/browser/bad_message.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/message_port_message_filter.h"
#include "content/browser/message_port_service.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_client_utils.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_type_converters.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "mojo/common/common_type_converters.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"

namespace content {

using StatusCallback = ServiceWorkerVersion::StatusCallback;

namespace {

// Time to wait until stopping an idle worker.
const int kIdleWorkerTimeoutSeconds = 30;

// Default delay for scheduled update.
const int kUpdateDelaySeconds = 1;

// Timeout for waiting for a response to a ping.
const int kPingTimeoutSeconds = 30;

const char kClaimClientsStateErrorMesage[] =
    "Only the active worker can claim clients.";

const char kClaimClientsShutdownErrorMesage[] =
    "Failed to claim clients due to Service Worker system shutdown.";

void RunSoon(const base::Closure& callback) {
  if (!callback.is_null())
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
}

template <typename CallbackArray, typename Arg>
void RunCallbacks(ServiceWorkerVersion* version,
                  CallbackArray* callbacks_ptr,
                  const Arg& arg) {
  CallbackArray callbacks;
  callbacks.swap(*callbacks_ptr);
  for (const auto& callback : callbacks)
    callback.Run(arg);
}

void RunStartWorkerCallback(
    const StatusCallback& callback,
    scoped_refptr<ServiceWorkerRegistration> protect,
    ServiceWorkerStatusCode status) {
  callback.Run(status);
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
  if (version->running_status() != EmbeddedWorkerStatus::RUNNING) {
    // We've tried to start the worker (and it has succeeded), but
    // it looks it's not running yet.
    NOTREACHED() << "The worker's not running after successful StartWorker";
    if (!error_callback.is_null())
      error_callback.Run(SERVICE_WORKER_ERROR_START_WORKER_FAILED);
    return;
  }
  task.Run();
}

void KillEmbeddedWorkerProcess(int process_id, ResultCode code) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderProcessHost* render_process_host =
      RenderProcessHost::FromID(process_id);
  if (render_process_host->GetHandle() != base::kNullProcessHandle) {
    bad_message::ReceivedBadMessage(render_process_host,
                                    bad_message::SERVICE_WORKER_BAD_URL);
  }
}

void ClearTick(base::TimeTicks* time) {
  *time = base::TimeTicks();
}

void RestartTick(base::TimeTicks* time) {
  *time = base::TimeTicks().Now();
}

bool RequestExpired(const base::TimeTicks& expiration) {
  if (expiration.is_null())
    return false;
  return base::TimeTicks().Now() >= expiration;
}

base::TimeDelta GetTickDuration(const base::TimeTicks& time) {
  if (time.is_null())
    return base::TimeDelta();
  return base::TimeTicks().Now() - time;
}

bool IsInstalled(ServiceWorkerVersion::Status status) {
  switch (status) {
    case ServiceWorkerVersion::NEW:
    case ServiceWorkerVersion::INSTALLING:
    case ServiceWorkerVersion::REDUNDANT:
      return false;
    case ServiceWorkerVersion::INSTALLED:
    case ServiceWorkerVersion::ACTIVATING:
    case ServiceWorkerVersion::ACTIVATED:
      return true;
  }
  NOTREACHED() << "Unexpected status: " << status;
  return false;
}

std::string VersionStatusToString(ServiceWorkerVersion::Status status) {
  switch (status) {
    case ServiceWorkerVersion::NEW:
      return "new";
    case ServiceWorkerVersion::INSTALLING:
      return "installing";
    case ServiceWorkerVersion::INSTALLED:
      return "installed";
    case ServiceWorkerVersion::ACTIVATING:
      return "activating";
    case ServiceWorkerVersion::ACTIVATED:
      return "activated";
    case ServiceWorkerVersion::REDUNDANT:
      return "redundant";
  }
  NOTREACHED() << status;
  return std::string();
}

const int kInvalidTraceId = -1;

int NextTraceId() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  static int trace_id = 0;
  if (trace_id == std::numeric_limits<int>::max())
    trace_id = 0;
  else
    ++trace_id;
  DCHECK_NE(kInvalidTraceId, trace_id);
  return trace_id;
}

}  // namespace

const int ServiceWorkerVersion::kTimeoutTimerDelaySeconds = 30;
const int ServiceWorkerVersion::kStartInstalledWorkerTimeoutSeconds = 60;
const int ServiceWorkerVersion::kStartNewWorkerTimeoutMinutes = 5;
const int ServiceWorkerVersion::kRequestTimeoutMinutes = 5;
const int ServiceWorkerVersion::kStopWorkerTimeoutSeconds = 5;

class ServiceWorkerVersion::Metrics {
 public:
  using EventType = ServiceWorkerMetrics::EventType;
  explicit Metrics(ServiceWorkerVersion* owner) : owner_(owner) {}
  ~Metrics() {
    if (owner_->should_exclude_from_uma_)
      return;
    for (const auto& ev : event_stats_) {
      ServiceWorkerMetrics::RecordEventHandledRatio(
          ev.first, ev.second.handled_events, ev.second.fired_events);
    }
  }

  void RecordEventHandledStatus(EventType event, bool handled) {
    event_stats_[event].fired_events++;
    if (handled)
      event_stats_[event].handled_events++;
  }

 private:
  struct EventStat {
    size_t fired_events = 0;
    size_t handled_events = 0;
  };

  ServiceWorkerVersion* owner_;
  std::map<EventType, EventStat> event_stats_;

  DISALLOW_COPY_AND_ASSIGN(Metrics);
};

// A controller for periodically sending a ping to the worker to see
// if the worker is not stalling.
class ServiceWorkerVersion::PingController {
 public:
  explicit PingController(ServiceWorkerVersion* version) : version_(version) {}
  ~PingController() {}

  void Activate() { ping_state_ = PINGING; }

  void Deactivate() {
    ClearTick(&ping_time_);
    ping_state_ = NOT_PINGING;
  }

  void OnPongReceived() { ClearTick(&ping_time_); }

  bool IsTimedOut() { return ping_state_ == PING_TIMED_OUT; }

  // Checks ping status. This is supposed to be called periodically.
  // This may call:
  // - OnPingTimeout() if the worker hasn't reponded within a certain period.
  // - PingWorker() if we're running ping timer and can send next ping.
  void CheckPingStatus() {
    if (GetTickDuration(ping_time_) >
        base::TimeDelta::FromSeconds(kPingTimeoutSeconds)) {
      ping_state_ = PING_TIMED_OUT;
      version_->OnPingTimeout();
      return;
    }

    // Check if we want to send a next ping.
    if (ping_state_ != PINGING || !ping_time_.is_null())
      return;

    if (version_->PingWorker() != SERVICE_WORKER_OK) {
      // TODO(falken): Maybe try resending Ping a few times first?
      ping_state_ = PING_TIMED_OUT;
      version_->OnPingTimeout();
      return;
    }
    RestartTick(&ping_time_);
  }

  void SimulateTimeoutForTesting() {
    version_->PingWorker();
    ping_state_ = PING_TIMED_OUT;
    version_->OnPingTimeout();
  }

 private:
  enum PingState { NOT_PINGING, PINGING, PING_TIMED_OUT };
  ServiceWorkerVersion* version_;  // Not owned.
  base::TimeTicks ping_time_;
  PingState ping_state_ = NOT_PINGING;
  DISALLOW_COPY_AND_ASSIGN(PingController);
};

ServiceWorkerVersion::ServiceWorkerVersion(
    ServiceWorkerRegistration* registration,
    const GURL& script_url,
    int64_t version_id,
    base::WeakPtr<ServiceWorkerContextCore> context)
    : version_id_(version_id),
      registration_id_(registration->id()),
      script_url_(script_url),
      scope_(registration->pattern()),
      context_(context),
      script_cache_map_(this, context),
      ping_controller_(new PingController(this)),
      should_exclude_from_uma_(
          ServiceWorkerMetrics::ShouldExcludeURLFromHistogram(scope_)),
      weak_factory_(this) {
  DCHECK_NE(kInvalidServiceWorkerVersionId, version_id);
  DCHECK(context_);
  DCHECK(registration);
  DCHECK(script_url_.is_valid());
  context_->AddLiveVersion(this);
  embedded_worker_ = context_->embedded_worker_registry()->CreateWorker();
  embedded_worker_->AddListener(this);
}

ServiceWorkerVersion::~ServiceWorkerVersion() {
  in_dtor_ = true;

  // Record UMA if the worker was trying to start. One way we get here is if the
  // user closed the tab before the SW could start up.
  if (!start_callbacks_.empty()) {
    // RecordStartWorkerResult must be the first element of start_callbacks_.
    StatusCallback record_start_worker_result = start_callbacks_[0];
    start_callbacks_.clear();
    record_start_worker_result.Run(SERVICE_WORKER_ERROR_ABORT);
  }

  if (context_)
    context_->RemoveLiveVersion(version_id_);

  if (running_status() == EmbeddedWorkerStatus::STARTING ||
      running_status() == EmbeddedWorkerStatus::RUNNING) {
    embedded_worker_->Stop();
  }
  embedded_worker_->RemoveListener(this);
}

void ServiceWorkerVersion::SetStatus(Status status) {
  if (status_ == status)
    return;

  TRACE_EVENT2("ServiceWorker", "ServiceWorkerVersion::SetStatus", "Script URL",
               script_url_.spec(), "New Status", VersionStatusToString(status));

  status_ = status;
  if (skip_waiting_ && status_ == ACTIVATED) {
    for (int request_id : pending_skip_waiting_requests_)
      DidSkipWaiting(request_id);
    pending_skip_waiting_requests_.clear();
  }

  // OnVersionStateChanged() invokes updates of the status using state
  // change IPC at ServiceWorkerHandle (for JS-land on renderer process) and
  // ServiceWorkerContextCore (for devtools and serviceworker-internals).
  // This should be done before using the new status by
  // |status_change_callbacks_| which sends the IPC for resolving the .ready
  // property.
  // TODO(shimazu): Clarify the dependency of OnVersionStateChanged and
  // |status_change_callbacks_|
  FOR_EACH_OBSERVER(Listener, listeners_, OnVersionStateChanged(this));

  std::vector<base::Closure> callbacks;
  callbacks.swap(status_change_callbacks_);
  for (const auto& callback : callbacks)
    callback.Run();

  if (status == INSTALLED)
    embedded_worker_->OnWorkerVersionInstalled();
  else if (status == REDUNDANT)
    embedded_worker_->OnWorkerVersionDoomed();
}

void ServiceWorkerVersion::RegisterStatusChangeCallback(
    const base::Closure& callback) {
  status_change_callbacks_.push_back(callback);
}

ServiceWorkerVersionInfo ServiceWorkerVersion::GetInfo() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ServiceWorkerVersionInfo info(
      running_status(), status(), script_url(), registration_id(), version_id(),
      embedded_worker()->process_id(), embedded_worker()->thread_id(),
      embedded_worker()->worker_devtools_agent_route_id());
  for (const auto& controllee : controllee_map_) {
    const ServiceWorkerProviderHost* host = controllee.second;
    info.clients.insert(std::make_pair(
        host->client_uuid(),
        ServiceWorkerVersionInfo::ClientInfo(
            host->process_id(), host->route_id(), host->provider_type())));
  }
  if (!main_script_http_info_)
    return info;
  info.script_response_time = main_script_http_info_->response_time;
  if (main_script_http_info_->headers)
    main_script_http_info_->headers->GetLastModifiedValue(
        &info.script_last_modified);
  return info;
}

void ServiceWorkerVersion::StartWorker(ServiceWorkerMetrics::EventType purpose,
                                       const StatusCallback& callback) {
  TRACE_EVENT_INSTANT2(
      "ServiceWorker", "ServiceWorkerVersion::StartWorker (instant)",
      TRACE_EVENT_SCOPE_THREAD, "Script", script_url_.spec(), "Purpose",
      ServiceWorkerMetrics::EventTypeToString(purpose));

  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  const bool is_browser_startup_complete =
      GetContentClient()->browser()->IsBrowserStartupComplete();
  if (!context_) {
    RecordStartWorkerResult(purpose, status_, kInvalidTraceId,
                            is_browser_startup_complete,
                            SERVICE_WORKER_ERROR_ABORT);
    RunSoon(base::Bind(callback, SERVICE_WORKER_ERROR_ABORT));
    return;
  }
  if (is_redundant()) {
    RecordStartWorkerResult(purpose, status_, kInvalidTraceId,
                            is_browser_startup_complete,
                            SERVICE_WORKER_ERROR_REDUNDANT);
    RunSoon(base::Bind(callback, SERVICE_WORKER_ERROR_REDUNDANT));
    return;
  }

  // Check that the worker is allowed to start on the given scope. Since this
  // worker might not be used for a specific frame/process, use -1.
  // resource_context() can return null in unit tests.
  if (context_->wrapper()->resource_context() &&
      !GetContentClient()->browser()->AllowServiceWorker(
          scope_, scope_, context_->wrapper()->resource_context(), -1, -1)) {
    RecordStartWorkerResult(purpose, status_, kInvalidTraceId,
                            is_browser_startup_complete,
                            SERVICE_WORKER_ERROR_DISALLOWED);
    RunSoon(base::Bind(callback, SERVICE_WORKER_ERROR_DISALLOWED));
    return;
  }

  // Ensure the live registration during starting worker so that the worker can
  // get associated with it in SWDispatcherHost::OnSetHostedVersionId().
  context_->storage()->FindRegistrationForId(
      registration_id_, scope_.GetOrigin(),
      base::Bind(&ServiceWorkerVersion::DidEnsureLiveRegistrationForStartWorker,
                 weak_factory_.GetWeakPtr(), purpose, status_,
                 is_browser_startup_complete, callback));
}

void ServiceWorkerVersion::StopWorker(const StatusCallback& callback) {
  TRACE_EVENT_INSTANT2("ServiceWorker",
                       "ServiceWorkerVersion::StopWorker (instant)",
                       TRACE_EVENT_SCOPE_THREAD, "Script", script_url_.spec(),
                       "Status", VersionStatusToString(status_));

  if (running_status() == EmbeddedWorkerStatus::STOPPED) {
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

void ServiceWorkerVersion::ScheduleUpdate() {
  if (!context_)
    return;
  if (update_timer_.IsRunning()) {
    update_timer_.Reset();
    return;
  }
  if (is_update_scheduled_)
    return;
  is_update_scheduled_ = true;

  // Protect |this| until the timer fires, since we may be stopping
  // and soon no one might hold a reference to us.
  context_->ProtectVersion(make_scoped_refptr(this));
  update_timer_.Start(FROM_HERE,
                      base::TimeDelta::FromSeconds(kUpdateDelaySeconds),
                      base::Bind(&ServiceWorkerVersion::StartUpdate,
                                 weak_factory_.GetWeakPtr()));
}

void ServiceWorkerVersion::StartUpdate() {
  if (!context_)
    return;
  context_->storage()->FindRegistrationForId(
      registration_id_, scope_.GetOrigin(),
      base::Bind(&ServiceWorkerVersion::FoundRegistrationForUpdate,
                 weak_factory_.GetWeakPtr()));
}

void ServiceWorkerVersion::DeferScheduledUpdate() {
  if (update_timer_.IsRunning())
    update_timer_.Reset();
}

int ServiceWorkerVersion::StartRequest(
    ServiceWorkerMetrics::EventType event_type,
    const StatusCallback& error_callback) {
  return StartRequestWithCustomTimeout(
      event_type, error_callback,
      base::TimeDelta::FromMinutes(kRequestTimeoutMinutes), KILL_ON_TIMEOUT);
}

int ServiceWorkerVersion::StartRequestWithCustomTimeout(
    ServiceWorkerMetrics::EventType event_type,
    const StatusCallback& error_callback,
    const base::TimeDelta& timeout,
    TimeoutBehavior timeout_behavior) {
  OnBeginEvent();
  DCHECK_EQ(EmbeddedWorkerStatus::RUNNING, running_status())
      << "Can only start a request with a running worker.";
  DCHECK(event_type == ServiceWorkerMetrics::EventType::INSTALL ||
         event_type == ServiceWorkerMetrics::EventType::ACTIVATE ||
         event_type == ServiceWorkerMetrics::EventType::MESSAGE ||
         status() == ACTIVATED)
      << "Event of type " << static_cast<int>(event_type)
      << " can only be dispatched to an active worker: " << status();

  PendingRequest<StatusCallback>* request = new PendingRequest<StatusCallback>(
      error_callback, base::TimeTicks::Now(), event_type);
  int request_id = custom_requests_.Add(request);
  TRACE_EVENT_ASYNC_BEGIN2("ServiceWorker", "ServiceWorkerVersion::Request",
                           request, "Request id", request_id, "Event type",
                           ServiceWorkerMetrics::EventTypeToString(event_type));
  base::TimeTicks expiration_time = base::TimeTicks::Now() + timeout;
  requests_.push(
      RequestInfo(request_id, event_type, expiration_time, timeout_behavior));
  return request_id;
}

bool ServiceWorkerVersion::FinishRequest(int request_id, bool was_handled) {
  PendingRequest<StatusCallback>* request = custom_requests_.Lookup(request_id);
  if (!request)
    return false;
  // TODO(kinuko): Record other event statuses too.
  metrics_->RecordEventHandledStatus(request->event_type, was_handled);
  ServiceWorkerMetrics::RecordEventDuration(
      request->event_type, base::TimeTicks::Now() - request->start_time,
      was_handled);

  RestartTick(&idle_time_);
  TRACE_EVENT_ASYNC_END1("ServiceWorker", "ServiceWorkerVersion::Request",
                         request, "Handled", was_handled);
  custom_requests_.Remove(request_id);
  if (!HasWork())
    FOR_EACH_OBSERVER(Listener, listeners_, OnNoWork(this));

  return true;
}

void ServiceWorkerVersion::RunAfterStartWorker(
    ServiceWorkerMetrics::EventType purpose,
    const base::Closure& task,
    const StatusCallback& error_callback) {
  if (running_status() == EmbeddedWorkerStatus::RUNNING) {
    DCHECK(start_callbacks_.empty());
    task.Run();
    return;
  }
  StartWorker(purpose,
              base::Bind(&RunTaskAfterStartWorker, weak_factory_.GetWeakPtr(),
                         error_callback, task));
}

void ServiceWorkerVersion::DispatchEvent(const std::vector<int>& request_ids,
                                         const IPC::Message& message) {
  DCHECK_EQ(EmbeddedWorkerStatus::RUNNING, running_status());

  const ServiceWorkerStatusCode status = embedded_worker_->SendMessage(message);

  for (int request_id : request_ids) {
    PendingRequest<StatusCallback>* request =
        custom_requests_.Lookup(request_id);
    DCHECK(request) << "Invalid request id";
    DCHECK(!request->is_dispatched)
        << "Request already dispatched an IPC event";
    if (status != SERVICE_WORKER_OK) {
      RunSoon(base::Bind(request->callback, status));
      custom_requests_.Remove(request_id);
    } else {
      request->is_dispatched = true;
    }
  }
}

void ServiceWorkerVersion::AddControllee(
    ServiceWorkerProviderHost* provider_host) {
  const std::string& uuid = provider_host->client_uuid();
  CHECK(!provider_host->client_uuid().empty());
  DCHECK(!ContainsKey(controllee_map_, uuid));
  controllee_map_[uuid] = provider_host;
  // Keep the worker alive a bit longer right after a new controllee is added.
  RestartTick(&idle_time_);
  FOR_EACH_OBSERVER(Listener, listeners_,
                    OnControlleeAdded(this, provider_host));
}

void ServiceWorkerVersion::RemoveControllee(
    ServiceWorkerProviderHost* provider_host) {
  const std::string& uuid = provider_host->client_uuid();
  DCHECK(ContainsKey(controllee_map_, uuid));
  controllee_map_.erase(uuid);
  FOR_EACH_OBSERVER(Listener, listeners_,
                    OnControlleeRemoved(this, provider_host));
  if (!HasControllee())
    FOR_EACH_OBSERVER(Listener, listeners_, OnNoControllees(this));
}

void ServiceWorkerVersion::AddStreamingURLRequestJob(
    const ServiceWorkerURLRequestJob* request_job) {
  DCHECK(streaming_url_request_jobs_.find(request_job) ==
         streaming_url_request_jobs_.end());
  streaming_url_request_jobs_.insert(request_job);
}

void ServiceWorkerVersion::RemoveStreamingURLRequestJob(
    const ServiceWorkerURLRequestJob* request_job) {
  streaming_url_request_jobs_.erase(request_job);
  if (!HasWork())
    FOR_EACH_OBSERVER(Listener, listeners_, OnNoWork(this));
}

void ServiceWorkerVersion::AddListener(Listener* listener) {
  listeners_.AddObserver(listener);
}

void ServiceWorkerVersion::RemoveListener(Listener* listener) {
  listeners_.RemoveObserver(listener);
}

void ServiceWorkerVersion::ReportError(ServiceWorkerStatusCode status,
                                       const std::string& status_message) {
  if (status_message.empty()) {
    OnReportException(base::UTF8ToUTF16(ServiceWorkerStatusToString(status)),
                      -1, -1, GURL());
  } else {
    OnReportException(base::UTF8ToUTF16(status_message), -1, -1, GURL());
  }
}

void ServiceWorkerVersion::SetStartWorkerStatusCode(
    ServiceWorkerStatusCode status) {
  start_worker_status_ = status;
}

void ServiceWorkerVersion::Doom() {
  DCHECK(!HasControllee());
  SetStatus(REDUNDANT);
  if (running_status() == EmbeddedWorkerStatus::STARTING ||
      running_status() == EmbeddedWorkerStatus::RUNNING) {
    if (embedded_worker()->devtools_attached())
      stop_when_devtools_detached_ = true;
    else
      embedded_worker_->Stop();
  }
  if (!context_)
    return;
  std::vector<ServiceWorkerDatabase::ResourceRecord> resources;
  script_cache_map_.GetResources(&resources);
  context_->storage()->PurgeResources(resources);
}

void ServiceWorkerVersion::SetDevToolsAttached(bool attached) {
  embedded_worker()->set_devtools_attached(attached);
  if (stop_when_devtools_detached_ && !attached) {
    DCHECK_EQ(REDUNDANT, status());
    if (running_status() == EmbeddedWorkerStatus::STARTING ||
        running_status() == EmbeddedWorkerStatus::RUNNING) {
      embedded_worker_->Stop();
    }
    return;
  }
  if (attached) {
    // TODO(falken): Canceling the timeouts when debugging could cause
    // heisenbugs; we should instead run them as normal show an educational
    // message in DevTools when they occur. crbug.com/470419

    // Don't record the startup time metric once DevTools is attached.
    ClearTick(&start_time_);
    skip_recording_startup_time_ = true;

    // Cancel request timeouts.
    SetAllRequestExpirations(base::TimeTicks());
    return;
  }
  if (!start_callbacks_.empty()) {
    // Reactivate the timer for start timeout.
    DCHECK(timeout_timer_.IsRunning());
    DCHECK(running_status() == EmbeddedWorkerStatus::STARTING ||
           running_status() == EmbeddedWorkerStatus::STOPPING)
        << static_cast<int>(running_status());
    RestartTick(&start_time_);
  }

  // Reactivate request timeouts, setting them all to the same expiration time.
  SetAllRequestExpirations(
      base::TimeTicks::Now() +
      base::TimeDelta::FromMinutes(kRequestTimeoutMinutes));
}

void ServiceWorkerVersion::SetMainScriptHttpResponseInfo(
    const net::HttpResponseInfo& http_info) {
  main_script_http_info_.reset(new net::HttpResponseInfo(http_info));
  FOR_EACH_OBSERVER(Listener, listeners_,
                    OnMainScriptHttpResponseInfoSet(this));
}

void ServiceWorkerVersion::SimulatePingTimeoutForTesting() {
  ping_controller_->SimulateTimeoutForTesting();
}

const net::HttpResponseInfo*
ServiceWorkerVersion::GetMainScriptHttpResponseInfo() {
  return main_script_http_info_.get();
}

ServiceWorkerVersion::RequestInfo::RequestInfo(
    int id,
    ServiceWorkerMetrics::EventType event_type,
    const base::TimeTicks& expiration,
    TimeoutBehavior timeout_behavior)
    : id(id),
      event_type(event_type),
      expiration(expiration),
      timeout_behavior(timeout_behavior) {}

ServiceWorkerVersion::RequestInfo::~RequestInfo() {
}

bool ServiceWorkerVersion::RequestInfo::operator>(
    const RequestInfo& other) const {
  return expiration > other.expiration;
}

template <typename CallbackType>
ServiceWorkerVersion::PendingRequest<CallbackType>::PendingRequest(
    const CallbackType& callback,
    const base::TimeTicks& time,
    ServiceWorkerMetrics::EventType event_type)
    : callback(callback), start_time(time), event_type(event_type) {}

ServiceWorkerVersion::BaseMojoServiceWrapper::BaseMojoServiceWrapper(
    ServiceWorkerVersion* worker,
    const char* service_name)
    : worker_(worker), service_name_(service_name) {}

ServiceWorkerVersion::BaseMojoServiceWrapper::~BaseMojoServiceWrapper() {
  IDMap<PendingRequest<StatusCallback>, IDMapOwnPointer>::iterator iter(
      &worker_->custom_requests_);
  while (!iter.IsAtEnd()) {
    PendingRequest<StatusCallback>* request = iter.GetCurrentValue();
    if (request->mojo_service == service_name_) {
      TRACE_EVENT_ASYNC_END1("ServiceWorker", "ServiceWorkerVersion::Request",
                             request, "Error", "Service Disconnected");
      request->callback.Run(SERVICE_WORKER_ERROR_FAILED);
      worker_->custom_requests_.Remove(iter.GetCurrentKey());
    }
    iter.Advance();
  }
}

void ServiceWorkerVersion::OnThreadStarted() {
  DCHECK_EQ(EmbeddedWorkerStatus::STARTING, running_status());
  // Activate ping/pong now that JavaScript execution will start.
  ping_controller_->Activate();
}

void ServiceWorkerVersion::OnStarting() {
  FOR_EACH_OBSERVER(Listener, listeners_, OnRunningStateChanged(this));
}

void ServiceWorkerVersion::OnStarted() {
  DCHECK_EQ(EmbeddedWorkerStatus::RUNNING, running_status());
  RestartTick(&idle_time_);

  // Fire all start callbacks.
  scoped_refptr<ServiceWorkerVersion> protect(this);
  RunCallbacks(this, &start_callbacks_, SERVICE_WORKER_OK);
  FOR_EACH_OBSERVER(Listener, listeners_, OnRunningStateChanged(this));
}

void ServiceWorkerVersion::OnStopping() {
  DCHECK(stop_time_.is_null());
  RestartTick(&stop_time_);
  TRACE_EVENT_ASYNC_BEGIN2("ServiceWorker", "ServiceWorkerVersion::StopWorker",
                           stop_time_.ToInternalValue(), "Script",
                           script_url_.spec(), "Version Status",
                           VersionStatusToString(status_));

  // Shorten the interval so stalling in stopped can be fixed quickly. Once the
  // worker stops, the timer is disabled. The interval will be reset to normal
  // when the worker starts up again.
  SetTimeoutTimerInterval(
      base::TimeDelta::FromSeconds(kStopWorkerTimeoutSeconds));
  FOR_EACH_OBSERVER(Listener, listeners_, OnRunningStateChanged(this));
}

void ServiceWorkerVersion::OnStopped(EmbeddedWorkerStatus old_status) {
  if (IsInstalled(status())) {
    ServiceWorkerMetrics::RecordWorkerStopped(
        ServiceWorkerMetrics::StopStatus::NORMAL);
  }
  if (!stop_time_.is_null())
    ServiceWorkerMetrics::RecordStopWorkerTime(GetTickDuration(stop_time_));

  OnStoppedInternal(old_status);
}

void ServiceWorkerVersion::OnDetached(EmbeddedWorkerStatus old_status) {
  if (IsInstalled(status())) {
    ServiceWorkerMetrics::RecordWorkerStopped(
        ServiceWorkerMetrics::StopStatus::DETACH_BY_REGISTRY);
  }
  OnStoppedInternal(old_status);
}

void ServiceWorkerVersion::OnScriptLoaded() {
  if (IsInstalled(status()))
    UMA_HISTOGRAM_BOOLEAN("ServiceWorker.ScriptLoadSuccess", true);
}

void ServiceWorkerVersion::OnScriptLoadFailed() {
  if (IsInstalled(status()))
    UMA_HISTOGRAM_BOOLEAN("ServiceWorker.ScriptLoadSuccess", false);
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
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_GetClient, OnGetClient)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_GetClients,
                        OnGetClients)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_OpenWindow,
                        OnOpenWindow)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_SetCachedMetadata,
                        OnSetCachedMetadata)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_ClearCachedMetadata,
                        OnClearCachedMetadata)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_PostMessageToClient,
                        OnPostMessageToClient)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_FocusClient,
                        OnFocusClient)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_NavigateClient, OnNavigateClient)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_SkipWaiting,
                        OnSkipWaiting)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_ClaimClients,
                        OnClaimClients)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_Pong, OnPongFromWorker)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_RegisterForeignFetchScopes,
                        OnRegisterForeignFetchScopes)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ServiceWorkerVersion::OnStartSentAndScriptEvaluated(
    ServiceWorkerStatusCode status) {
  if (status != SERVICE_WORKER_OK) {
    scoped_refptr<ServiceWorkerVersion> protect(this);
    RunCallbacks(this, &start_callbacks_,
                 DeduceStartWorkerFailureReason(status));
  }
}

void ServiceWorkerVersion::OnGetClient(int request_id,
                                       const std::string& client_uuid) {
  if (!context_)
    return;
  TRACE_EVENT_ASYNC_BEGIN1("ServiceWorker", "ServiceWorkerVersion::OnGetClient",
                           request_id, "client_uuid", client_uuid);
  ServiceWorkerProviderHost* provider_host =
      context_->GetProviderHostByClientID(client_uuid);
  if (!provider_host ||
      provider_host->document_url().GetOrigin() != script_url_.GetOrigin()) {
    // The promise will be resolved to 'undefined'.
    OnGetClientFinished(request_id, ServiceWorkerClientInfo());
    return;
  }
  service_worker_client_utils::GetClient(
      provider_host, base::Bind(&ServiceWorkerVersion::OnGetClientFinished,
                                weak_factory_.GetWeakPtr(), request_id));
}

void ServiceWorkerVersion::OnGetClientFinished(
    int request_id,
    const ServiceWorkerClientInfo& client_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT_ASYNC_END1("ServiceWorker", "ServiceWorkerVersion::OnGetClient",
                         request_id, "client_type", client_info.client_type);

  // When Clients.get() is called on the script evaluation phase, the running
  // status can be STARTING here.
  if (running_status() != EmbeddedWorkerStatus::STARTING &&
      running_status() != EmbeddedWorkerStatus::RUNNING) {
    return;
  }

  embedded_worker_->SendMessage(
      ServiceWorkerMsg_DidGetClient(request_id, client_info));
}

void ServiceWorkerVersion::OnGetClients(
    int request_id,
    const ServiceWorkerClientQueryOptions& options) {
  TRACE_EVENT_ASYNC_BEGIN2(
      "ServiceWorker", "ServiceWorkerVersion::OnGetClients", request_id,
      "client_type", options.client_type, "include_uncontrolled",
      options.include_uncontrolled);
  service_worker_client_utils::GetClients(
      weak_factory_.GetWeakPtr(), options,
      base::Bind(&ServiceWorkerVersion::OnGetClientsFinished,
                 weak_factory_.GetWeakPtr(), request_id));
}

void ServiceWorkerVersion::OnGetClientsFinished(int request_id,
                                                ServiceWorkerClients* clients) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT_ASYNC_END1("ServiceWorker", "ServiceWorkerVersion::OnGetClients",
                         request_id, "The number of clients", clients->size());

  // When Clients.matchAll() is called on the script evaluation phase, the
  // running status can be STARTING here.
  if (running_status() != EmbeddedWorkerStatus::STARTING &&
      running_status() != EmbeddedWorkerStatus::RUNNING) {
    return;
  }

  embedded_worker_->SendMessage(
      ServiceWorkerMsg_DidGetClients(request_id, *clients));
}

void ServiceWorkerVersion::OnSimpleEventResponse(
    int request_id,
    blink::WebServiceWorkerEventResult result) {
  // Copy error callback before calling FinishRequest.
  PendingRequest<StatusCallback>* request = custom_requests_.Lookup(request_id);
  DCHECK(request) << "Invalid request id";
  StatusCallback callback = request->callback;

  FinishRequest(request_id,
                result == blink::WebServiceWorkerEventResultCompleted);

  ServiceWorkerStatusCode status = SERVICE_WORKER_OK;
  if (result == blink::WebServiceWorkerEventResultRejected)
    status = SERVICE_WORKER_ERROR_EVENT_WAITUNTIL_REJECTED;
  callback.Run(status);
}

void ServiceWorkerVersion::OnOpenWindow(int request_id, GURL url) {
  // Just abort if we are shutting down.
  if (!context_)
    return;

  if (!url.is_valid()) {
    DVLOG(1) << "Received unexpected invalid URL from renderer process.";
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(&KillEmbeddedWorkerProcess,
                                       embedded_worker_->process_id(),
                                       RESULT_CODE_KILLED_BAD_MESSAGE));
    return;
  }

  // The renderer treats all URLs in the about: scheme as being about:blank.
  // Canonicalize about: URLs to about:blank.
  if (url.SchemeIs(url::kAboutScheme))
    url = GURL(url::kAboutBlankURL);

  // Reject requests for URLs that the process is not allowed to access. It's
  // possible to receive such requests since the renderer-side checks are
  // slightly different. For example, the view-source scheme will not be
  // filtered out by Blink.
  if (!ChildProcessSecurityPolicyImpl::GetInstance()->CanRequestURL(
          embedded_worker_->process_id(), url)) {
    embedded_worker_->SendMessage(ServiceWorkerMsg_OpenWindowError(
        request_id, url.spec() + " cannot be opened."));
    return;
  }

  service_worker_client_utils::OpenWindow(
      url, script_url_, embedded_worker_->process_id(), context_,
      base::Bind(&ServiceWorkerVersion::OnOpenWindowFinished,
                 weak_factory_.GetWeakPtr(), request_id));
}

void ServiceWorkerVersion::OnOpenWindowFinished(
    int request_id,
    ServiceWorkerStatusCode status,
    const ServiceWorkerClientInfo& client_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (running_status() != EmbeddedWorkerStatus::RUNNING)
    return;

  if (status != SERVICE_WORKER_OK) {
    embedded_worker_->SendMessage(ServiceWorkerMsg_OpenWindowError(
        request_id, "Something went wrong while trying to open the window."));
    return;
  }

  embedded_worker_->SendMessage(
      ServiceWorkerMsg_OpenWindowResponse(request_id, client_info));
}

void ServiceWorkerVersion::OnSetCachedMetadata(const GURL& url,
                                               const std::vector<char>& data) {
  int64_t callback_id = base::TimeTicks::Now().ToInternalValue();
  TRACE_EVENT_ASYNC_BEGIN1("ServiceWorker",
                           "ServiceWorkerVersion::OnSetCachedMetadata",
                           callback_id, "URL", url.spec());
  script_cache_map_.WriteMetadata(
      url, data, base::Bind(&ServiceWorkerVersion::OnSetCachedMetadataFinished,
                            weak_factory_.GetWeakPtr(), callback_id));
}

void ServiceWorkerVersion::OnSetCachedMetadataFinished(int64_t callback_id,
                                                       int result) {
  TRACE_EVENT_ASYNC_END1("ServiceWorker",
                         "ServiceWorkerVersion::OnSetCachedMetadata",
                         callback_id, "result", result);
  FOR_EACH_OBSERVER(Listener, listeners_, OnCachedMetadataUpdated(this));
}

void ServiceWorkerVersion::OnClearCachedMetadata(const GURL& url) {
  int64_t callback_id = base::TimeTicks::Now().ToInternalValue();
  TRACE_EVENT_ASYNC_BEGIN1("ServiceWorker",
                           "ServiceWorkerVersion::OnClearCachedMetadata",
                           callback_id, "URL", url.spec());
  script_cache_map_.ClearMetadata(
      url, base::Bind(&ServiceWorkerVersion::OnClearCachedMetadataFinished,
                      weak_factory_.GetWeakPtr(), callback_id));
}

void ServiceWorkerVersion::OnClearCachedMetadataFinished(int64_t callback_id,
                                                         int result) {
  TRACE_EVENT_ASYNC_END1("ServiceWorker",
                         "ServiceWorkerVersion::OnClearCachedMetadata",
                         callback_id, "result", result);
  FOR_EACH_OBSERVER(Listener, listeners_, OnCachedMetadataUpdated(this));
}

void ServiceWorkerVersion::OnPostMessageToClient(
    const std::string& client_uuid,
    const base::string16& message,
    const std::vector<int>& sent_message_ports) {
  if (!context_)
    return;
  TRACE_EVENT1("ServiceWorker",
               "ServiceWorkerVersion::OnPostMessageToDocument",
               "Client id", client_uuid);
  ServiceWorkerProviderHost* provider_host =
      context_->GetProviderHostByClientID(client_uuid);
  if (!provider_host) {
    // The client may already have been closed, just ignore.
    return;
  }
  if (provider_host->document_url().GetOrigin() != script_url_.GetOrigin()) {
    // The client does not belong to the same origin as this ServiceWorker,
    // possibly due to timing issue or bad message.
    return;
  }
  provider_host->PostMessageToClient(this, message, sent_message_ports);
}

void ServiceWorkerVersion::OnFocusClient(int request_id,
                                         const std::string& client_uuid) {
  if (!context_)
    return;
  TRACE_EVENT2("ServiceWorker",
               "ServiceWorkerVersion::OnFocusClient",
               "Request id", request_id,
               "Client id", client_uuid);
  ServiceWorkerProviderHost* provider_host =
      context_->GetProviderHostByClientID(client_uuid);
  if (!provider_host) {
    // The client may already have been closed, just ignore.
    return;
  }
  if (provider_host->document_url().GetOrigin() != script_url_.GetOrigin()) {
    // The client does not belong to the same origin as this ServiceWorker,
    // possibly due to timing issue or bad message.
    return;
  }
  if (provider_host->client_type() != blink::WebServiceWorkerClientTypeWindow) {
    // focus() should be called only for WindowClient. This may happen due to
    // bad message.
    return;
  }

  service_worker_client_utils::FocusWindowClient(
      provider_host, base::Bind(&ServiceWorkerVersion::OnFocusClientFinished,
                                weak_factory_.GetWeakPtr(), request_id));
}

void ServiceWorkerVersion::OnFocusClientFinished(
    int request_id,
    const ServiceWorkerClientInfo& client_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (running_status() != EmbeddedWorkerStatus::RUNNING)
    return;

  embedded_worker_->SendMessage(
      ServiceWorkerMsg_FocusClientResponse(request_id, client_info));
}

void ServiceWorkerVersion::OnNavigateClient(int request_id,
                                            const std::string& client_uuid,
                                            const GURL& url) {
  if (!context_)
    return;

  TRACE_EVENT2("ServiceWorker", "ServiceWorkerVersion::OnNavigateClient",
               "Request id", request_id, "Client id", client_uuid);

  if (!url.is_valid() || !base::IsValidGUID(client_uuid)) {
    DVLOG(1) << "Received unexpected invalid URL/UUID from renderer process.";
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&KillEmbeddedWorkerProcess, embedded_worker_->process_id(),
                   RESULT_CODE_KILLED_BAD_MESSAGE));
    return;
  }

  // Reject requests for URLs that the process is not allowed to access. It's
  // possible to receive such requests since the renderer-side checks are
  // slightly different. For example, the view-source scheme will not be
  // filtered out by Blink.
  if (!ChildProcessSecurityPolicyImpl::GetInstance()->CanRequestURL(
          embedded_worker_->process_id(), url)) {
    embedded_worker_->SendMessage(
        ServiceWorkerMsg_NavigateClientError(request_id, url));
    return;
  }

  ServiceWorkerProviderHost* provider_host =
      context_->GetProviderHostByClientID(client_uuid);
  if (!provider_host || provider_host->active_version() != this) {
    embedded_worker_->SendMessage(
        ServiceWorkerMsg_NavigateClientError(request_id, url));
    return;
  }

  service_worker_client_utils::NavigateClient(
      url, script_url_, provider_host->process_id(), provider_host->frame_id(),
      context_, base::Bind(&ServiceWorkerVersion::OnNavigateClientFinished,
                           weak_factory_.GetWeakPtr(), request_id));
}

void ServiceWorkerVersion::OnNavigateClientFinished(
    int request_id,
    ServiceWorkerStatusCode status,
    const ServiceWorkerClientInfo& client_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (running_status() != EmbeddedWorkerStatus::RUNNING)
    return;

  if (status != SERVICE_WORKER_OK) {
    embedded_worker_->SendMessage(
        ServiceWorkerMsg_NavigateClientError(request_id, GURL()));
    return;
  }

  embedded_worker_->SendMessage(
      ServiceWorkerMsg_NavigateClientResponse(request_id, client_info));
}

void ServiceWorkerVersion::OnSkipWaiting(int request_id) {
  skip_waiting_ = true;
  if (status_ != INSTALLED)
    return DidSkipWaiting(request_id);

  if (!context_)
    return;
  ServiceWorkerRegistration* registration =
      context_->GetLiveRegistration(registration_id_);
  if (!registration)
    return;
  pending_skip_waiting_requests_.push_back(request_id);
  if (pending_skip_waiting_requests_.size() == 1)
    registration->ActivateWaitingVersionWhenReady();
}

void ServiceWorkerVersion::DidSkipWaiting(int request_id) {
  if (running_status() == EmbeddedWorkerStatus::STARTING ||
      running_status() == EmbeddedWorkerStatus::RUNNING) {
    embedded_worker_->SendMessage(ServiceWorkerMsg_DidSkipWaiting(request_id));
  }
}

void ServiceWorkerVersion::OnClaimClients(int request_id) {
  if (status_ != ACTIVATING && status_ != ACTIVATED) {
    embedded_worker_->SendMessage(ServiceWorkerMsg_ClaimClientsError(
        request_id, blink::WebServiceWorkerError::ErrorTypeState,
        base::ASCIIToUTF16(kClaimClientsStateErrorMesage)));
    return;
  }
  if (context_) {
    if (ServiceWorkerRegistration* registration =
            context_->GetLiveRegistration(registration_id_)) {
      registration->ClaimClients();
      embedded_worker_->SendMessage(
          ServiceWorkerMsg_DidClaimClients(request_id));
      return;
    }
  }

  embedded_worker_->SendMessage(ServiceWorkerMsg_ClaimClientsError(
      request_id, blink::WebServiceWorkerError::ErrorTypeAbort,
      base::ASCIIToUTF16(kClaimClientsShutdownErrorMesage)));
}

void ServiceWorkerVersion::OnPongFromWorker() {
  ping_controller_->OnPongReceived();
}

void ServiceWorkerVersion::OnRegisterForeignFetchScopes(
    const std::vector<GURL>& sub_scopes,
    const std::vector<url::Origin>& origins) {
  DCHECK(status() == INSTALLING || status() == REDUNDANT) << status();
  // Renderer should have already verified all these urls are inside the
  // worker's scope, but verify again here on the browser process side.
  GURL origin = scope_.GetOrigin();
  std::string scope_path = scope_.path();
  for (const GURL& url : sub_scopes) {
    if (!url.is_valid() || url.GetOrigin() != origin ||
        !base::StartsWith(url.path(), scope_path,
                          base::CompareCase::SENSITIVE)) {
      DVLOG(1) << "Received unexpected invalid URL from renderer process.";
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&KillEmbeddedWorkerProcess, embedded_worker_->process_id(),
                     RESULT_CODE_KILLED_BAD_MESSAGE));
      return;
    }
  }
  for (const url::Origin& url : origins) {
    if (url.unique()) {
      DVLOG(1) << "Received unexpected unique origin from renderer process.";
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&KillEmbeddedWorkerProcess, embedded_worker_->process_id(),
                     RESULT_CODE_KILLED_BAD_MESSAGE));
      return;
    }
  }
  set_foreign_fetch_scopes(sub_scopes);
  set_foreign_fetch_origins(origins);
}

void ServiceWorkerVersion::DidEnsureLiveRegistrationForStartWorker(
    ServiceWorkerMetrics::EventType purpose,
    Status prestart_status,
    bool is_browser_startup_complete,
    const StatusCallback& callback,
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  scoped_refptr<ServiceWorkerRegistration> protect = registration;
  if (status == SERVICE_WORKER_ERROR_NOT_FOUND) {
    // When the registration has already been deleted from the storage but its
    // active worker is still controlling clients, the event should be
    // dispatched on the worker. However, the storage cannot find the
    // registration. To handle the case, check the live registrations here.
    protect = context_->GetLiveRegistration(registration_id_);
    if (protect) {
      DCHECK(protect->is_deleted());
      status = SERVICE_WORKER_OK;
    }
  }
  if (status != SERVICE_WORKER_OK) {
    RecordStartWorkerResult(purpose, prestart_status, kInvalidTraceId,
                            is_browser_startup_complete, status);
    RunSoon(base::Bind(callback, SERVICE_WORKER_ERROR_START_WORKER_FAILED));
    return;
  }
  if (is_redundant()) {
    RecordStartWorkerResult(purpose, prestart_status, kInvalidTraceId,
                            is_browser_startup_complete,
                            SERVICE_WORKER_ERROR_REDUNDANT);
    RunSoon(base::Bind(callback, SERVICE_WORKER_ERROR_REDUNDANT));
    return;
  }

  MarkIfStale();

  switch (running_status()) {
    case EmbeddedWorkerStatus::RUNNING:
      RunSoon(base::Bind(callback, SERVICE_WORKER_OK));
      return;
    case EmbeddedWorkerStatus::STARTING:
      DCHECK(!start_callbacks_.empty());
      break;
    case EmbeddedWorkerStatus::STOPPING:
    case EmbeddedWorkerStatus::STOPPED:
      if (start_callbacks_.empty()) {
        int trace_id = NextTraceId();
        TRACE_EVENT_ASYNC_BEGIN2(
            "ServiceWorker", "ServiceWorkerVersion::StartWorker", trace_id,
            "Script", script_url_.spec(), "Purpose",
            ServiceWorkerMetrics::EventTypeToString(purpose));
        start_callbacks_.push_back(
            base::Bind(&ServiceWorkerVersion::RecordStartWorkerResult,
                       weak_factory_.GetWeakPtr(), purpose, prestart_status,
                       trace_id, is_browser_startup_complete));
      }
      break;
  }

  // Keep the live registration while starting the worker.
  start_callbacks_.push_back(
      base::Bind(&RunStartWorkerCallback, callback, protect));

  if (running_status() == EmbeddedWorkerStatus::STOPPED)
    StartWorkerInternal();
  DCHECK(timeout_timer_.IsRunning());
}

void ServiceWorkerVersion::StartWorkerInternal() {
  DCHECK_EQ(EmbeddedWorkerStatus::STOPPED, running_status());

  DCHECK(!metrics_);
  metrics_.reset(new Metrics(this));

  StartTimeoutTimer();

  std::unique_ptr<EmbeddedWorkerMsg_StartWorker_Params> params(
      new EmbeddedWorkerMsg_StartWorker_Params());
  params->service_worker_version_id = version_id_;
  params->scope = scope_;
  params->script_url = script_url_;
  params->is_installed = IsInstalled(status_);
  params->pause_after_download = pause_after_download_;

  embedded_worker_->Start(
      std::move(params),
      base::Bind(&ServiceWorkerVersion::OnStartSentAndScriptEvaluated,
                 weak_factory_.GetWeakPtr()));
}

void ServiceWorkerVersion::StartTimeoutTimer() {
  DCHECK(!timeout_timer_.IsRunning());

  if (embedded_worker_->devtools_attached()) {
    // Don't record the startup time metric once DevTools is attached.
    ClearTick(&start_time_);
    skip_recording_startup_time_ = true;
  } else {
    RestartTick(&start_time_);
    skip_recording_startup_time_ = false;
  }

  // The worker is starting up and not yet idle.
  ClearTick(&idle_time_);

  // Ping will be activated in OnScriptLoaded.
  ping_controller_->Deactivate();

  timeout_timer_.Start(FROM_HERE,
                       base::TimeDelta::FromSeconds(kTimeoutTimerDelaySeconds),
                       this, &ServiceWorkerVersion::OnTimeoutTimer);
}

void ServiceWorkerVersion::StopTimeoutTimer() {
  timeout_timer_.Stop();
  ClearTick(&idle_time_);

  // Trigger update if worker is stale.
  if (!in_dtor_ && !stale_time_.is_null()) {
    ClearTick(&stale_time_);
    if (!update_timer_.IsRunning())
      ScheduleUpdate();
  }
}

void ServiceWorkerVersion::SetTimeoutTimerInterval(base::TimeDelta interval) {
  DCHECK(timeout_timer_.IsRunning());
  if (timeout_timer_.GetCurrentDelay() != interval) {
    timeout_timer_.Stop();
    timeout_timer_.Start(FROM_HERE, interval, this,
                         &ServiceWorkerVersion::OnTimeoutTimer);
  }
}

void ServiceWorkerVersion::OnTimeoutTimer() {
  DCHECK(running_status() == EmbeddedWorkerStatus::STARTING ||
         running_status() == EmbeddedWorkerStatus::RUNNING ||
         running_status() == EmbeddedWorkerStatus::STOPPING)
      << static_cast<int>(running_status());

  if (!context_)
    return;

  MarkIfStale();

  // Stopping the worker hasn't finished within a certain period.
  if (GetTickDuration(stop_time_) >
      base::TimeDelta::FromSeconds(kStopWorkerTimeoutSeconds)) {
    DCHECK_EQ(EmbeddedWorkerStatus::STOPPING, running_status());
    if (IsInstalled(status())) {
      ServiceWorkerMetrics::RecordWorkerStopped(
          ServiceWorkerMetrics::StopStatus::TIMEOUT);
    }
    ReportError(SERVICE_WORKER_ERROR_TIMEOUT, "DETACH_STALLED_IN_STOPPING");

    // Detach the worker. Remove |this| as a listener first; otherwise
    // OnStoppedInternal might try to restart before the new worker
    // is created.
    embedded_worker_->RemoveListener(this);
    embedded_worker_->Detach();
    embedded_worker_ = context_->embedded_worker_registry()->CreateWorker();
    embedded_worker_->AddListener(this);

    // Call OnStoppedInternal to fail callbacks and possibly restart.
    OnStoppedInternal(EmbeddedWorkerStatus::STOPPING);
    return;
  }

  // Trigger update if worker is stale and we waited long enough for it to go
  // idle.
  if (GetTickDuration(stale_time_) >
      base::TimeDelta::FromMinutes(kRequestTimeoutMinutes)) {
    ClearTick(&stale_time_);
    if (!update_timer_.IsRunning())
      ScheduleUpdate();
  }

  // Starting a worker hasn't finished within a certain period.
  const base::TimeDelta start_limit =
      IsInstalled(status())
          ? base::TimeDelta::FromSeconds(kStartInstalledWorkerTimeoutSeconds)
          : base::TimeDelta::FromMinutes(kStartNewWorkerTimeoutMinutes);
  if (GetTickDuration(start_time_) > start_limit) {
    DCHECK(running_status() == EmbeddedWorkerStatus::STARTING ||
           running_status() == EmbeddedWorkerStatus::STOPPING)
        << static_cast<int>(running_status());
    scoped_refptr<ServiceWorkerVersion> protect(this);
    RunCallbacks(this, &start_callbacks_, SERVICE_WORKER_ERROR_TIMEOUT);
    if (running_status() == EmbeddedWorkerStatus::STARTING)
      embedded_worker_->Stop();
    return;
  }

  // Requests have not finished before their expiration.
  bool stop_for_timeout = false;
  while (!requests_.empty()) {
    RequestInfo info = requests_.top();
    if (!RequestExpired(info.expiration))
      break;
    if (MaybeTimeOutRequest(info)) {
      stop_for_timeout =
          stop_for_timeout || info.timeout_behavior == KILL_ON_TIMEOUT;
      ServiceWorkerMetrics::RecordEventTimeout(info.event_type);
    }
    requests_.pop();
  }
  if (stop_for_timeout && running_status() != EmbeddedWorkerStatus::STOPPING)
    embedded_worker_->Stop();

  // For the timeouts below, there are no callbacks to timeout so there is
  // nothing more to do if the worker is already stopping.
  if (running_status() == EmbeddedWorkerStatus::STOPPING)
    return;

  // The worker has been idle for longer than a certain period.
  if (GetTickDuration(idle_time_) >
      base::TimeDelta::FromSeconds(kIdleWorkerTimeoutSeconds)) {
    StopWorkerIfIdle();
    return;
  }

  // Check ping status.
  ping_controller_->CheckPingStatus();
}

ServiceWorkerStatusCode ServiceWorkerVersion::PingWorker() {
  DCHECK(running_status() == EmbeddedWorkerStatus::STARTING ||
         running_status() == EmbeddedWorkerStatus::RUNNING);
  return embedded_worker_->SendMessage(ServiceWorkerMsg_Ping());
}

void ServiceWorkerVersion::OnPingTimeout() {
  DCHECK(running_status() == EmbeddedWorkerStatus::STARTING ||
         running_status() == EmbeddedWorkerStatus::RUNNING);
  // TODO(falken): Show a message to the developer that the SW was stopped due
  // to timeout (crbug.com/457968). Also, change the error code to
  // SERVICE_WORKER_ERROR_TIMEOUT.
  StopWorkerIfIdle();
}

void ServiceWorkerVersion::StopWorkerIfIdle() {
  if (HasWork() && !ping_controller_->IsTimedOut())
    return;
  if (running_status() == EmbeddedWorkerStatus::STOPPED ||
      running_status() == EmbeddedWorkerStatus::STOPPING ||
      !stop_callbacks_.empty()) {
    return;
  }

  embedded_worker_->StopIfIdle();
}

bool ServiceWorkerVersion::HasWork() const {
  return !custom_requests_.IsEmpty() || !streaming_url_request_jobs_.empty() ||
         !start_callbacks_.empty();
}

void ServiceWorkerVersion::RecordStartWorkerResult(
    ServiceWorkerMetrics::EventType purpose,
    Status prestart_status,
    int trace_id,
    bool is_browser_startup_complete,
    ServiceWorkerStatusCode status) {
  if (trace_id != kInvalidTraceId) {
    TRACE_EVENT_ASYNC_END1("ServiceWorker", "ServiceWorkerVersion::StartWorker",
                           trace_id, "Status",
                           ServiceWorkerStatusToString(status));
  }
  base::TimeTicks start_time = start_time_;
  ClearTick(&start_time_);

  if (context_ && IsInstalled(prestart_status))
    context_->UpdateVersionFailureCount(version_id_, status);

  ServiceWorkerMetrics::RecordStartWorkerStatus(status, purpose,
                                                IsInstalled(prestart_status));

  if (status == SERVICE_WORKER_OK && !start_time.is_null() &&
      !skip_recording_startup_time_) {
    ServiceWorkerMetrics::RecordStartWorkerTime(
        GetTickDuration(start_time), IsInstalled(prestart_status),
        ServiceWorkerMetrics::GetStartSituation(
            is_browser_startup_complete, embedded_worker_->is_new_process()),
        purpose);
  }

  if (status != SERVICE_WORKER_ERROR_TIMEOUT)
    return;
  EmbeddedWorkerInstance::StartingPhase phase =
      EmbeddedWorkerInstance::NOT_STARTING;
  EmbeddedWorkerStatus running_status = embedded_worker_->status();
  // Build an artifical JavaScript exception to show in the ServiceWorker
  // log for developers; it's not user-facing so it's not a localized resource.
  std::string message = "ServiceWorker startup timed out. ";
  if (running_status != EmbeddedWorkerStatus::STARTING) {
    message.append("The worker had unexpected status: ");
    message.append(EmbeddedWorkerInstance::StatusToString(running_status));
  } else {
    phase = embedded_worker_->starting_phase();
    message.append("The worker was in startup phase: ");
    message.append(EmbeddedWorkerInstance::StartingPhaseToString(phase));
  }
  message.append(".");
  OnReportException(base::UTF8ToUTF16(message), -1, -1, GURL());
  DVLOG(1) << message;
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.StartWorker.TimeoutPhase",
                            phase,
                            EmbeddedWorkerInstance::STARTING_PHASE_MAX_VALUE);
}

bool ServiceWorkerVersion::MaybeTimeOutRequest(const RequestInfo& info) {
  PendingRequest<StatusCallback>* request = custom_requests_.Lookup(info.id);
  if (!request)
    return false;

  TRACE_EVENT_ASYNC_END1("ServiceWorker", "ServiceWorkerVersion::Request",
                         request, "Error", "Timeout");
  request->callback.Run(SERVICE_WORKER_ERROR_TIMEOUT);
  custom_requests_.Remove(info.id);
  return true;
}

void ServiceWorkerVersion::SetAllRequestExpirations(
    const base::TimeTicks& expiration) {
  RequestInfoPriorityQueue new_requests;
  while (!requests_.empty()) {
    RequestInfo info = requests_.top();
    info.expiration = expiration;
    new_requests.push(info);
    requests_.pop();
  }
  requests_ = new_requests;
}

ServiceWorkerStatusCode ServiceWorkerVersion::DeduceStartWorkerFailureReason(
    ServiceWorkerStatusCode default_code) {
  if (ping_controller_->IsTimedOut())
    return SERVICE_WORKER_ERROR_TIMEOUT;

  if (start_worker_status_ != SERVICE_WORKER_OK)
    return start_worker_status_;

  const net::URLRequestStatus& main_script_status =
      script_cache_map()->main_script_status();
  if (main_script_status.status() != net::URLRequestStatus::SUCCESS) {
    switch (main_script_status.error()) {
      case net::ERR_INSECURE_RESPONSE:
      case net::ERR_UNSAFE_REDIRECT:
        return SERVICE_WORKER_ERROR_SECURITY;
      case net::ERR_ABORTED:
        return SERVICE_WORKER_ERROR_ABORT;
      default:
        return SERVICE_WORKER_ERROR_NETWORK;
    }
  }

  return default_code;
}

void ServiceWorkerVersion::MarkIfStale() {
  if (!context_)
    return;
  if (update_timer_.IsRunning() || !stale_time_.is_null())
    return;
  ServiceWorkerRegistration* registration =
      context_->GetLiveRegistration(registration_id_);
  if (!registration || registration->active_version() != this)
    return;
  base::TimeDelta time_since_last_check =
      base::Time::Now() - registration->last_update_check();
  if (time_since_last_check >
      base::TimeDelta::FromHours(kServiceWorkerScriptMaxCacheAgeInHours))
    RestartTick(&stale_time_);
}

void ServiceWorkerVersion::FoundRegistrationForUpdate(
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  if (!context_)
    return;

  const scoped_refptr<ServiceWorkerVersion> protect = this;
  if (is_update_scheduled_) {
    context_->UnprotectVersion(version_id_);
    is_update_scheduled_ = false;
  }

  if (status != SERVICE_WORKER_OK || registration->active_version() != this)
    return;
  context_->UpdateServiceWorker(registration.get(),
                                false /* force_bypass_cache */);
}

void ServiceWorkerVersion::OnStoppedInternal(EmbeddedWorkerStatus old_status) {
  DCHECK_EQ(EmbeddedWorkerStatus::STOPPED, running_status());
  scoped_refptr<ServiceWorkerVersion> protect;
  if (!in_dtor_)
    protect = this;

  DCHECK(metrics_);
  metrics_.reset();

  bool should_restart = !is_redundant() && !start_callbacks_.empty() &&
                        (old_status != EmbeddedWorkerStatus::STARTING) &&
                        !in_dtor_ && !ping_controller_->IsTimedOut();

  if (!stop_time_.is_null()) {
    TRACE_EVENT_ASYNC_END1("ServiceWorker", "ServiceWorkerVersion::StopWorker",
                           stop_time_.ToInternalValue(), "Restart",
                           should_restart);
    ClearTick(&stop_time_);
  }
  StopTimeoutTimer();

  // Fire all stop callbacks.
  RunCallbacks(this, &stop_callbacks_, SERVICE_WORKER_OK);

  if (!should_restart) {
    // Let all start callbacks fail.
    RunCallbacks(this, &start_callbacks_,
                 DeduceStartWorkerFailureReason(
                     SERVICE_WORKER_ERROR_START_WORKER_FAILED));
  }

  // Let all message callbacks fail (this will also fire and clear all
  // callbacks for events).
  // TODO(kinuko): Consider if we want to add queue+resend mechanism here.
  IDMap<PendingRequest<StatusCallback>, IDMapOwnPointer>::iterator iter(
      &custom_requests_);
  while (!iter.IsAtEnd()) {
    TRACE_EVENT_ASYNC_END1("ServiceWorker", "ServiceWorkerVersion::Request",
                           iter.GetCurrentValue(), "Error", "Worker Stopped");
    iter.GetCurrentValue()->callback.Run(SERVICE_WORKER_ERROR_FAILED);
    iter.Advance();
  }
  custom_requests_.Clear();

  // Close all mojo services. This will also fire and clear all callbacks
  // for messages that are still outstanding for those services.
  mojo_services_.clear();

  // TODO(falken): Call SWURLRequestJob::ClearStream here?
  streaming_url_request_jobs_.clear();

  FOR_EACH_OBSERVER(Listener, listeners_, OnRunningStateChanged(this));
  if (should_restart)
    StartWorkerInternal();
  else if (!HasWork())
    FOR_EACH_OBSERVER(Listener, listeners_, OnNoWork(this));
}

void ServiceWorkerVersion::OnMojoConnectionError(const char* service_name) {
  // Simply deleting the service will cause error callbacks to be called from
  // the destructor of the MojoServiceWrapper instance.
  mojo_services_.erase(service_name);
}

void ServiceWorkerVersion::OnBeginEvent() {
  if (should_exclude_from_uma_ ||
      running_status() != EmbeddedWorkerStatus::RUNNING ||
      idle_time_.is_null()) {
    return;
  }
  ServiceWorkerMetrics::RecordTimeBetweenEvents(base::TimeTicks::Now() -
                                                idle_time_);
}

}  // namespace content
