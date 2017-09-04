// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_handle.h"

#include "base/memory/ptr_util.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_types.h"

namespace content {

namespace {

blink::WebServiceWorkerState
GetWebServiceWorkerState(ServiceWorkerVersion* version) {
  DCHECK(version);
  switch (version->status()) {
    case ServiceWorkerVersion::NEW:
      return blink::WebServiceWorkerStateUnknown;
    case ServiceWorkerVersion::INSTALLING:
      return blink::WebServiceWorkerStateInstalling;
    case ServiceWorkerVersion::INSTALLED:
      return blink::WebServiceWorkerStateInstalled;
    case ServiceWorkerVersion::ACTIVATING:
      return blink::WebServiceWorkerStateActivating;
    case ServiceWorkerVersion::ACTIVATED:
      return blink::WebServiceWorkerStateActivated;
    case ServiceWorkerVersion::REDUNDANT:
      return blink::WebServiceWorkerStateRedundant;
  }
  NOTREACHED() << version->status();
  return blink::WebServiceWorkerStateUnknown;
}

}  // namespace

std::unique_ptr<ServiceWorkerHandle> ServiceWorkerHandle::Create(
    base::WeakPtr<ServiceWorkerContextCore> context,
    base::WeakPtr<ServiceWorkerProviderHost> provider_host,
    ServiceWorkerVersion* version) {
  if (!context || !provider_host || !version)
    return std::unique_ptr<ServiceWorkerHandle>();
  DCHECK(context->GetLiveRegistration(version->registration_id()));
  return base::WrapUnique(
      new ServiceWorkerHandle(context, provider_host, version));
}

ServiceWorkerHandle::ServiceWorkerHandle(
    base::WeakPtr<ServiceWorkerContextCore> context,
    base::WeakPtr<ServiceWorkerProviderHost> provider_host,
    ServiceWorkerVersion* version)
    : context_(context),
      provider_host_(provider_host),
      provider_id_(provider_host ? provider_host->provider_id()
                                 : kInvalidServiceWorkerProviderId),
      handle_id_(context.get() ? context->GetNewServiceWorkerHandleId() : -1),
      ref_count_(1),
      version_(version) {
  version_->AddListener(this);
}

ServiceWorkerHandle::~ServiceWorkerHandle() {
  version_->RemoveListener(this);
}

void ServiceWorkerHandle::OnVersionStateChanged(ServiceWorkerVersion* version) {
  if (!provider_host_)
    return;
  provider_host_->SendServiceWorkerStateChangedMessage(
      handle_id_, GetWebServiceWorkerState(version));
}

ServiceWorkerObjectInfo ServiceWorkerHandle::GetObjectInfo() {
  ServiceWorkerObjectInfo info;
  info.handle_id = handle_id_;
  info.url = version_->script_url();
  info.state = GetWebServiceWorkerState(version_.get());
  info.version_id = version_->version_id();
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
