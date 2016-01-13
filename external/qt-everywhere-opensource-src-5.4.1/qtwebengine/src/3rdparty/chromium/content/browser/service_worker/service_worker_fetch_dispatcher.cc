// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_fetch_dispatcher.h"

#include "base/bind.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "net/url_request/url_request.h"

namespace content {

ServiceWorkerFetchDispatcher::ServiceWorkerFetchDispatcher(
    net::URLRequest* request,
    ServiceWorkerVersion* version,
    const FetchCallback& callback)
    : version_(version),
      callback_(callback),
      weak_factory_(this) {
  request_.url = request->url();
  request_.method = request->method();
  const net::HttpRequestHeaders& headers = request->extra_request_headers();
  for (net::HttpRequestHeaders::Iterator it(headers); it.GetNext();)
    request_.headers[it.name()] = it.value();
}

ServiceWorkerFetchDispatcher::~ServiceWorkerFetchDispatcher() {}

void ServiceWorkerFetchDispatcher::Run() {
  DCHECK(version_->status() == ServiceWorkerVersion::ACTIVATING ||
         version_->status() == ServiceWorkerVersion::ACTIVE)
      << version_->status();

  if (version_->status() == ServiceWorkerVersion::ACTIVATING) {
    version_->RegisterStatusChangeCallback(
        base::Bind(&ServiceWorkerFetchDispatcher::DidWaitActivation,
                   weak_factory_.GetWeakPtr()));
    return;
  }
  DispatchFetchEvent();
}

void ServiceWorkerFetchDispatcher::DidWaitActivation() {
  if (version_->status() != ServiceWorkerVersion::ACTIVE) {
    DCHECK_EQ(ServiceWorkerVersion::INSTALLED, version_->status());
    DidFailActivation();
    return;
  }
  DispatchFetchEvent();
}

void ServiceWorkerFetchDispatcher::DidFailActivation() {
  // The previous activation seems to have failed, abort the step
  // with activate error. (The error should be separately reported
  // to the associated documents and association must be dropped
  // at this point)
  DidFinish(SERVICE_WORKER_ERROR_ACTIVATE_WORKER_FAILED,
            SERVICE_WORKER_FETCH_EVENT_RESULT_FALLBACK,
            ServiceWorkerResponse());
}

void ServiceWorkerFetchDispatcher::DispatchFetchEvent() {
  version_->DispatchFetchEvent(
      request_,
      base::Bind(&ServiceWorkerFetchDispatcher::DidFinish,
                 weak_factory_.GetWeakPtr()));
}

void ServiceWorkerFetchDispatcher::DidFinish(
    ServiceWorkerStatusCode status,
    ServiceWorkerFetchEventResult fetch_result,
    const ServiceWorkerResponse& response) {
  DCHECK(!callback_.is_null());
  FetchCallback callback = callback_;
  callback.Run(status, fetch_result, response);
}

}  // namespace content
