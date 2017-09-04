// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_registration_handle.h"

#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_handle.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_types.h"

namespace content {

ServiceWorkerRegistrationHandle::ServiceWorkerRegistrationHandle(
    base::WeakPtr<ServiceWorkerContextCore> context,
    base::WeakPtr<ServiceWorkerProviderHost> provider_host,
    ServiceWorkerRegistration* registration)
    : context_(context),
      provider_host_(provider_host),
      provider_id_(provider_host ? provider_host->provider_id()
                                 : kInvalidServiceWorkerProviderId),
      handle_id_(context ? context->GetNewRegistrationHandleId()
                         : kInvalidServiceWorkerRegistrationHandleId),
      ref_count_(1),
      registration_(registration) {
  DCHECK(registration_.get());
  ChangedVersionAttributesMask changed_mask;
  if (registration->installing_version())
    changed_mask.add(ChangedVersionAttributesMask::INSTALLING_VERSION);
  if (registration->waiting_version())
    changed_mask.add(ChangedVersionAttributesMask::WAITING_VERSION);
  if (registration->active_version())
    changed_mask.add(ChangedVersionAttributesMask::ACTIVE_VERSION);
  SetVersionAttributes(changed_mask,
                       registration->installing_version(),
                       registration->waiting_version(),
                       registration->active_version());
  registration_->AddListener(this);
}

ServiceWorkerRegistrationHandle::~ServiceWorkerRegistrationHandle() {
  DCHECK(registration_.get());
  registration_->RemoveListener(this);
}

ServiceWorkerRegistrationObjectInfo
ServiceWorkerRegistrationHandle::GetObjectInfo() {
  ServiceWorkerRegistrationObjectInfo info;
  info.handle_id = handle_id_;
  info.scope = registration_->pattern();
  info.registration_id = registration_->id();
  return info;
}

void ServiceWorkerRegistrationHandle::IncrementRefCount() {
  DCHECK_GT(ref_count_, 0);
  ++ref_count_;
}

void ServiceWorkerRegistrationHandle::DecrementRefCount() {
  DCHECK_GT(ref_count_, 0);
  --ref_count_;
}

void ServiceWorkerRegistrationHandle::OnVersionAttributesChanged(
    ServiceWorkerRegistration* registration,
    ChangedVersionAttributesMask changed_mask,
    const ServiceWorkerRegistrationInfo& info) {
  DCHECK_EQ(registration->id(), registration_->id());
  SetVersionAttributes(changed_mask,
                       registration->installing_version(),
                       registration->waiting_version(),
                       registration->active_version());
}

void ServiceWorkerRegistrationHandle::OnRegistrationFailed(
    ServiceWorkerRegistration* registration) {
  DCHECK_EQ(registration->id(), registration_->id());
  ChangedVersionAttributesMask changed_mask(
      ChangedVersionAttributesMask::INSTALLING_VERSION |
      ChangedVersionAttributesMask::WAITING_VERSION |
      ChangedVersionAttributesMask::ACTIVE_VERSION);
  SetVersionAttributes(changed_mask, nullptr, nullptr, nullptr);
}

void ServiceWorkerRegistrationHandle::OnUpdateFound(
    ServiceWorkerRegistration* registration) {
  if (!provider_host_)
    return;  // Could be nullptr in some tests.
  provider_host_->SendUpdateFoundMessage(handle_id_);
}

void ServiceWorkerRegistrationHandle::SetVersionAttributes(
    ChangedVersionAttributesMask changed_mask,
    ServiceWorkerVersion* installing_version,
    ServiceWorkerVersion* waiting_version,
    ServiceWorkerVersion* active_version) {
  if (!provider_host_)
    return;  // Could be nullptr in some tests.
  provider_host_->SendSetVersionAttributesMessage(handle_id_,
                                                  changed_mask,
                                                  installing_version,
                                                  waiting_version,
                                                  active_version);
}

}  // namespace content
