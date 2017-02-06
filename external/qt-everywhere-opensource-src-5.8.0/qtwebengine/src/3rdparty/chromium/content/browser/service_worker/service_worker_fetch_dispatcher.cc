// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_fetch_dispatcher.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_utils.h"

namespace content {

namespace {

using EventType = ServiceWorkerMetrics::EventType;
EventType ResourceTypeToEventType(ResourceType resource_type) {
  switch (resource_type) {
    case RESOURCE_TYPE_MAIN_FRAME:
      return EventType::FETCH_MAIN_FRAME;
    case RESOURCE_TYPE_SUB_FRAME:
      return EventType::FETCH_SUB_FRAME;
    case RESOURCE_TYPE_SHARED_WORKER:
      return EventType::FETCH_SHARED_WORKER;
    case RESOURCE_TYPE_SERVICE_WORKER:
    case RESOURCE_TYPE_LAST_TYPE:
      NOTREACHED() << resource_type;
      return EventType::FETCH_SUB_RESOURCE;
    default:
      return EventType::FETCH_SUB_RESOURCE;
  }
}

std::unique_ptr<base::Value> NetLogServiceWorkerStatusCallback(
    ServiceWorkerStatusCode status,
    net::NetLogCaptureMode) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  dict->SetString("status", ServiceWorkerStatusToString(status));
  return std::move(dict);
}

std::unique_ptr<base::Value> NetLogFetchEventCallback(
    ServiceWorkerStatusCode status,
    ServiceWorkerFetchEventResult result,
    net::NetLogCaptureMode) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  dict->SetString("status", ServiceWorkerStatusToString(status));
  dict->SetBoolean("has_response",
                   result == SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE);
  return std::move(dict);
}

void EndNetLogEventWithServiceWorkerStatus(const net::BoundNetLog& net_log,
                                           net::NetLog::EventType type,
                                           ServiceWorkerStatusCode status) {
  net_log.EndEvent(type,
                   base::Bind(&NetLogServiceWorkerStatusCallback, status));
}

ServiceWorkerMetrics::EventType FetchTypeToWaitUntilEventType(
    ServiceWorkerFetchType type) {
  if (type == ServiceWorkerFetchType::FOREIGN_FETCH)
    return ServiceWorkerMetrics::EventType::FOREIGN_FETCH_WAITUNTIL;
  return ServiceWorkerMetrics::EventType::FETCH_WAITUNTIL;
}

}  // namespace

// Helper to receive the fetch event response even if
// ServiceWorkerFetchDispatcher has been destroyed.
class ServiceWorkerFetchDispatcher::ResponseCallback {
 public:
  ResponseCallback(base::WeakPtr<ServiceWorkerFetchDispatcher> fetch_dispatcher,
                   ServiceWorkerVersion* version)
      : fetch_dispatcher_(fetch_dispatcher), version_(version) {}

  void Run(int request_id,
           ServiceWorkerFetchEventResult fetch_result,
           const ServiceWorkerResponse& response) {
    const bool handled =
        (fetch_result == SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE);
    if (!version_->FinishRequest(request_id, handled))
      NOTREACHED() << "Should only receive one reply per event";

    // |fetch_dispatcher| is null if the URLRequest was killed.
    if (fetch_dispatcher_)
      fetch_dispatcher_->DidFinish(request_id, fetch_result, response);
  }

 private:
  base::WeakPtr<ServiceWorkerFetchDispatcher> fetch_dispatcher_;
  // Owns |this|.
  ServiceWorkerVersion* version_;

  DISALLOW_COPY_AND_ASSIGN(ResponseCallback);
};

ServiceWorkerFetchDispatcher::ServiceWorkerFetchDispatcher(
    std::unique_ptr<ServiceWorkerFetchRequest> request,
    ServiceWorkerVersion* version,
    ResourceType resource_type,
    const net::BoundNetLog& net_log,
    const base::Closure& prepare_callback,
    const FetchCallback& fetch_callback)
    : version_(version),
      net_log_(net_log),
      prepare_callback_(prepare_callback),
      fetch_callback_(fetch_callback),
      request_(std::move(request)),
      resource_type_(resource_type),
      did_complete_(false),
      weak_factory_(this) {
  net_log_.BeginEvent(net::NetLog::TYPE_SERVICE_WORKER_DISPATCH_FETCH_EVENT,
                      net::NetLog::StringCallback(
                          "event_type", ServiceWorkerMetrics::EventTypeToString(
                                            GetEventType())));
}

ServiceWorkerFetchDispatcher::~ServiceWorkerFetchDispatcher() {
  if (!did_complete_)
    net_log_.EndEvent(net::NetLog::TYPE_SERVICE_WORKER_DISPATCH_FETCH_EVENT);
}

void ServiceWorkerFetchDispatcher::Run() {
  DCHECK(version_->status() == ServiceWorkerVersion::ACTIVATING ||
         version_->status() == ServiceWorkerVersion::ACTIVATED)
      << version_->status();

  if (version_->status() == ServiceWorkerVersion::ACTIVATING) {
    net_log_.BeginEvent(net::NetLog::TYPE_SERVICE_WORKER_WAIT_FOR_ACTIVATION);
    version_->RegisterStatusChangeCallback(
        base::Bind(&ServiceWorkerFetchDispatcher::DidWaitForActivation,
                   weak_factory_.GetWeakPtr()));
    return;
  }
  StartWorker();
}

void ServiceWorkerFetchDispatcher::DidWaitForActivation() {
  net_log_.EndEvent(net::NetLog::TYPE_SERVICE_WORKER_WAIT_FOR_ACTIVATION);
  StartWorker();
}

void ServiceWorkerFetchDispatcher::StartWorker() {
  // We might be REDUNDANT if a new worker started activating and kicked us out
  // before we could finish activation.
  if (version_->status() != ServiceWorkerVersion::ACTIVATED) {
    DCHECK_EQ(ServiceWorkerVersion::REDUNDANT, version_->status());
    DidFail(SERVICE_WORKER_ERROR_ACTIVATE_WORKER_FAILED);
    return;
  }

  if (version_->running_status() == EmbeddedWorkerStatus::RUNNING) {
    DispatchFetchEvent();
    return;
  }

  net_log_.BeginEvent(net::NetLog::TYPE_SERVICE_WORKER_START_WORKER);
  version_->RunAfterStartWorker(
      GetEventType(), base::Bind(&ServiceWorkerFetchDispatcher::DidStartWorker,
                                 weak_factory_.GetWeakPtr()),
      base::Bind(&ServiceWorkerFetchDispatcher::DidFailToStartWorker,
                 weak_factory_.GetWeakPtr()));
}

void ServiceWorkerFetchDispatcher::DidStartWorker() {
  net_log_.EndEvent(net::NetLog::TYPE_SERVICE_WORKER_START_WORKER);
  DispatchFetchEvent();
}

void ServiceWorkerFetchDispatcher::DidFailToStartWorker(
    ServiceWorkerStatusCode status) {
  EndNetLogEventWithServiceWorkerStatus(
      net_log_, net::NetLog::TYPE_SERVICE_WORKER_START_WORKER, status);
  DidFail(status);
}

void ServiceWorkerFetchDispatcher::DispatchFetchEvent() {
  DCHECK_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status())
      << "Worker stopped too soon after it was started.";

  DCHECK(!prepare_callback_.is_null());
  base::Closure prepare_callback = prepare_callback_;
  prepare_callback.Run();

  net_log_.BeginEvent(net::NetLog::TYPE_SERVICE_WORKER_FETCH_EVENT);
  int response_id = version_->StartRequest(
      GetEventType(),
      base::Bind(&ServiceWorkerFetchDispatcher::DidFailToDispatch,
                 weak_factory_.GetWeakPtr()));
  int event_finish_id = version_->StartRequest(
      FetchTypeToWaitUntilEventType(request_->fetch_type),
      base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));

  ResponseCallback* response_callback =
      new ResponseCallback(weak_factory_.GetWeakPtr(), version_.get());
  version_->RegisterRequestCallback<ServiceWorkerHostMsg_FetchEventResponse>(
      response_id,
      base::Bind(&ServiceWorkerFetchDispatcher::ResponseCallback::Run,
                 base::Owned(response_callback)));
  version_->RegisterSimpleRequest<ServiceWorkerHostMsg_FetchEventFinished>(
      event_finish_id);
  version_->DispatchEvent({response_id, event_finish_id},
                          ServiceWorkerMsg_FetchEvent(
                              response_id, event_finish_id, *request_.get()));
}

void ServiceWorkerFetchDispatcher::DidFailToDispatch(
    ServiceWorkerStatusCode status) {
  EndNetLogEventWithServiceWorkerStatus(
      net_log_, net::NetLog::TYPE_SERVICE_WORKER_FETCH_EVENT, status);
  DidFail(status);
}

void ServiceWorkerFetchDispatcher::DidFail(ServiceWorkerStatusCode status) {
  DCHECK_NE(SERVICE_WORKER_OK, status);
  Complete(status, SERVICE_WORKER_FETCH_EVENT_RESULT_FALLBACK,
           ServiceWorkerResponse());
}

void ServiceWorkerFetchDispatcher::DidFinish(
    int request_id,
    ServiceWorkerFetchEventResult fetch_result,
    const ServiceWorkerResponse& response) {
  net_log_.EndEvent(net::NetLog::TYPE_SERVICE_WORKER_FETCH_EVENT);
  Complete(SERVICE_WORKER_OK, fetch_result, response);
}

void ServiceWorkerFetchDispatcher::Complete(
    ServiceWorkerStatusCode status,
    ServiceWorkerFetchEventResult fetch_result,
    const ServiceWorkerResponse& response) {
  DCHECK(!fetch_callback_.is_null());

  did_complete_ = true;
  net_log_.EndEvent(
      net::NetLog::TYPE_SERVICE_WORKER_DISPATCH_FETCH_EVENT,
      base::Bind(&NetLogFetchEventCallback, status, fetch_result));

  FetchCallback fetch_callback = fetch_callback_;
  scoped_refptr<ServiceWorkerVersion> version = version_;
  fetch_callback.Run(status, fetch_result, response, version);
}

ServiceWorkerMetrics::EventType ServiceWorkerFetchDispatcher::GetEventType()
    const {
  if (request_->fetch_type == ServiceWorkerFetchType::FOREIGN_FETCH)
    return ServiceWorkerMetrics::EventType::FOREIGN_FETCH;
  return ResourceTypeToEventType(resource_type_);
}

}  // namespace content
