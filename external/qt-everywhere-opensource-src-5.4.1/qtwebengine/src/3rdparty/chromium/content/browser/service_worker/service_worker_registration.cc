// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_registration.h"

#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_info.h"
#include "content/public/browser/browser_thread.h"

namespace content {

ServiceWorkerRegistration::ServiceWorkerRegistration(
    const GURL& pattern,
    const GURL& script_url,
    int64 registration_id,
    base::WeakPtr<ServiceWorkerContextCore> context)
    : pattern_(pattern),
      script_url_(script_url),
      registration_id_(registration_id),
      is_shutdown_(false),
      context_(context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(context_);
  context_->AddLiveRegistration(this);
}

ServiceWorkerRegistration::~ServiceWorkerRegistration() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (context_)
    context_->RemoveLiveRegistration(registration_id_);
}

ServiceWorkerRegistrationInfo ServiceWorkerRegistration::GetInfo() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return ServiceWorkerRegistrationInfo(
      script_url(),
      pattern(),
      registration_id_,
      active_version_ ? active_version_->GetInfo() : ServiceWorkerVersionInfo(),
      waiting_version_ ? waiting_version_->GetInfo()
                       : ServiceWorkerVersionInfo());
}

ServiceWorkerVersion* ServiceWorkerRegistration::GetNewestVersion() {
  if (active_version())
    return active_version();
  return waiting_version();
}

void ServiceWorkerRegistration::ActivateWaitingVersion() {
  active_version_->SetStatus(ServiceWorkerVersion::DEACTIVATED);
  active_version_ = waiting_version_;
  // TODO(kinuko): This should be set to ACTIVATING until activation finishes.
  active_version_->SetStatus(ServiceWorkerVersion::ACTIVE);
  waiting_version_ = NULL;
}

}  // namespace content
