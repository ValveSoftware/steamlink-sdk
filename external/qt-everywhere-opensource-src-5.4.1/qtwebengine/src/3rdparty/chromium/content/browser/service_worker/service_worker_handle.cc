// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_handle.h"

#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "ipc/ipc_sender.h"

namespace content {

namespace {

blink::WebServiceWorkerState
GetWebServiceWorkerState(ServiceWorkerVersion* version) {
  DCHECK(version);
  switch (version->status()) {
    case ServiceWorkerVersion::NEW:
      if (version->running_status() == ServiceWorkerVersion::RUNNING)
        return blink::WebServiceWorkerStateParsed;
      else
        return blink::WebServiceWorkerStateUnknown;
    case ServiceWorkerVersion::INSTALLING:
      return blink::WebServiceWorkerStateInstalling;
    case ServiceWorkerVersion::INSTALLED:
      return blink::WebServiceWorkerStateInstalled;
    case ServiceWorkerVersion::ACTIVATING:
      return blink::WebServiceWorkerStateActivating;
    case ServiceWorkerVersion::ACTIVE:
      return blink::WebServiceWorkerStateActive;
    case ServiceWorkerVersion::DEACTIVATED:
      return blink::WebServiceWorkerStateDeactivated;
  }
  NOTREACHED() << version->status();
  return blink::WebServiceWorkerStateUnknown;
}

}  // namespace

scoped_ptr<ServiceWorkerHandle> ServiceWorkerHandle::Create(
    base::WeakPtr<ServiceWorkerContextCore> context,
    IPC::Sender* sender,
    int thread_id,
    ServiceWorkerVersion* version) {
  if (!context || !version)
    return scoped_ptr<ServiceWorkerHandle>();
  ServiceWorkerRegistration* registration =
      context->GetLiveRegistration(version->registration_id());
  return make_scoped_ptr(
      new ServiceWorkerHandle(context, sender, thread_id,
                              registration, version));
}

ServiceWorkerHandle::ServiceWorkerHandle(
    base::WeakPtr<ServiceWorkerContextCore> context,
    IPC::Sender* sender,
    int thread_id,
    ServiceWorkerRegistration* registration,
    ServiceWorkerVersion* version)
    : context_(context),
      sender_(sender),
      thread_id_(thread_id),
      handle_id_(context.get() ? context->GetNewServiceWorkerHandleId() : -1),
      ref_count_(1),
      registration_(registration),
      version_(version) {
  version_->AddListener(this);
}

ServiceWorkerHandle::~ServiceWorkerHandle() {
  version_->RemoveListener(this);
  // TODO(kinuko): At this point we can discard the registration if
  // all documents/handles that have a reference to the registration is
  // closed or freed up, but could also keep it alive in cache
  // (e.g. in context_) for a while with some timer so that we don't
  // need to re-load the same registration from disk over and over.
}

void ServiceWorkerHandle::OnWorkerStarted(ServiceWorkerVersion* version) {
}

void ServiceWorkerHandle::OnWorkerStopped(ServiceWorkerVersion* version) {
}

void ServiceWorkerHandle::OnErrorReported(ServiceWorkerVersion* version,
                                          const base::string16& error_message,
                                          int line_number,
                                          int column_number,
                                          const GURL& source_url) {
}

void ServiceWorkerHandle::OnReportConsoleMessage(ServiceWorkerVersion* version,
                                                 int source_identifier,
                                                 int message_level,
                                                 const base::string16& message,
                                                 int line_number,
                                                 const GURL& source_url) {
}

void ServiceWorkerHandle::OnVersionStateChanged(ServiceWorkerVersion* version) {
  sender_->Send(new ServiceWorkerMsg_ServiceWorkerStateChanged(
      thread_id_, handle_id_, GetWebServiceWorkerState(version)));
}

ServiceWorkerObjectInfo ServiceWorkerHandle::GetObjectInfo() {
  ServiceWorkerObjectInfo info;
  info.handle_id = handle_id_;
  info.scope = registration_->pattern();
  info.url = registration_->script_url();
  info.state = GetWebServiceWorkerState(version_);
  return info;
}

void ServiceWorkerHandle::IncrementRefCount() {
  DCHECK_GT(ref_count_, 0);
  ++ref_count_;
}

void ServiceWorkerHandle::DecrementRefCount() {
  DCHECK_GE(ref_count_, 0);
  --ref_count_;
}

}  // namespace content
