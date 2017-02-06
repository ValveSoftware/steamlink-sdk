// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_controllee_request_handler.h"

#include <memory>
#include <set>
#include <string>

#include "base/trace_event/trace_event.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_response_info.h"
#include "content/browser/service_worker/service_worker_url_request_job.h"
#include "content/common/resource_request_body_impl.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/resource_response_info.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/url_request/url_request.h"

namespace content {

ServiceWorkerControlleeRequestHandler::ServiceWorkerControlleeRequestHandler(
    base::WeakPtr<ServiceWorkerContextCore> context,
    base::WeakPtr<ServiceWorkerProviderHost> provider_host,
    base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
    FetchRequestMode request_mode,
    FetchCredentialsMode credentials_mode,
    FetchRedirectMode redirect_mode,
    ResourceType resource_type,
    RequestContextType request_context_type,
    RequestContextFrameType frame_type,
    scoped_refptr<ResourceRequestBodyImpl> body)
    : ServiceWorkerRequestHandler(context,
                                  provider_host,
                                  blob_storage_context,
                                  resource_type),
      is_main_resource_load_(
          ServiceWorkerUtils::IsMainResourceType(resource_type)),
      is_main_frame_load_(resource_type == RESOURCE_TYPE_MAIN_FRAME),
      request_mode_(request_mode),
      credentials_mode_(credentials_mode),
      redirect_mode_(redirect_mode),
      request_context_type_(request_context_type),
      frame_type_(frame_type),
      body_(body),
      force_update_started_(false),
      use_network_(false),
      weak_factory_(this) {}

ServiceWorkerControlleeRequestHandler::
    ~ServiceWorkerControlleeRequestHandler() {
  // Navigation triggers an update to occur shortly after the page and
  // its initial subresources load.
  if (provider_host_ && provider_host_->active_version()) {
    if (is_main_resource_load_ && !force_update_started_)
      provider_host_->active_version()->ScheduleUpdate();
    else
      provider_host_->active_version()->DeferScheduledUpdate();
  }

  if (is_main_resource_load_ && provider_host_)
    provider_host_->SetAllowAssociation(true);
}

net::URLRequestJob* ServiceWorkerControlleeRequestHandler::MaybeCreateJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    ResourceContext* resource_context) {
  ClearJob();
  ServiceWorkerResponseInfo::ResetDataForRequest(request);

  if (!context_ || !provider_host_) {
    // We can't do anything other than to fall back to network.
    return NULL;
  }

  // This may get called multiple times for original and redirect requests:
  // A. original request case: use_network_ is false, no previous location info.
  // B. redirect or restarted request case:
  //  a) use_network_ is false if the previous location was forwarded to SW.
  //  b) use_network_ is false if the previous location was fallback.
  //  c) use_network_ is true if additional restart was required to fall back.

  // Fall back to network. (Case B-c)
  if (use_network_) {
    // Once a subresource request has fallen back to the network once, it will
    // never be handled by a service worker. This is not true of main frame
    // requests.
    if (is_main_resource_load_)
      use_network_ = false;
    return NULL;
  }

  // It's for original request (A) or redirect case (B-a or B-b).
  std::unique_ptr<ServiceWorkerURLRequestJob> job(
      new ServiceWorkerURLRequestJob(
          request, network_delegate, provider_host_->client_uuid(),
          blob_storage_context_, resource_context, request_mode_,
          credentials_mode_, redirect_mode_, resource_type_,
          request_context_type_, frame_type_, body_,
          ServiceWorkerFetchType::FETCH, this));
  job_ = job->GetWeakPtr();

  resource_context_ = resource_context;

  if (is_main_resource_load_)
    PrepareForMainResource(request);
  else
    PrepareForSubResource();

  if (job_->ShouldFallbackToNetwork()) {
    // If we know we can fallback to network at this point (in case
    // the storage lookup returned immediately), just destroy the job and return
    // NULL here to fallback to network.

    // If this is a subresource request, all subsequent requests should also use
    // the network.
    if (!is_main_resource_load_)
      use_network_ = true;

    job.reset();
    ClearJob();
  }

  return job.release();
}

void ServiceWorkerControlleeRequestHandler::PrepareForMainResource(
    const net::URLRequest* request) {
  DCHECK(job_.get());
  DCHECK(context_);
  DCHECK(provider_host_);
  TRACE_EVENT_ASYNC_BEGIN1(
      "ServiceWorker",
      "ServiceWorkerControlleeRequestHandler::PrepareForMainResource",
      job_.get(),
      "URL", request->url().spec());
  // The corresponding provider_host may already have associated a registration
  // in redirect case, unassociate it now.
  provider_host_->DisassociateRegistration();

  // Also prevent a registrater job for establishing an association to a new
  // registration while we're finding an existing registration.
  provider_host_->SetAllowAssociation(false);

  stripped_url_ = net::SimplifyUrlForRequest(request->url());
  provider_host_->SetDocumentUrl(stripped_url_);
  provider_host_->SetTopmostFrameUrl(request->first_party_for_cookies());
  context_->storage()->FindRegistrationForDocument(
      stripped_url_, base::Bind(&self::DidLookupRegistrationForMainResource,
                                weak_factory_.GetWeakPtr()));
}

void
ServiceWorkerControlleeRequestHandler::DidLookupRegistrationForMainResource(
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  // The job may have been canceled and then destroyed before this was invoked.
  if (!job_)
    return;

  const bool need_to_update = !force_update_started_ && registration &&
                              context_->force_update_on_page_load();

  if (provider_host_ && !need_to_update)
    provider_host_->SetAllowAssociation(true);
  if (status != SERVICE_WORKER_OK || !provider_host_ || !context_) {
    job_->FallbackToNetwork();
    TRACE_EVENT_ASYNC_END1(
        "ServiceWorker",
        "ServiceWorkerControlleeRequestHandler::PrepareForMainResource",
        job_.get(),
        "Status", status);
    return;
  }
  DCHECK(registration.get());

  if (!GetContentClient()->browser()->AllowServiceWorker(
          registration->pattern(), provider_host_->topmost_frame_url(),
          resource_context_, provider_host_->process_id(),
          provider_host_->frame_id())) {
    job_->FallbackToNetwork();
    TRACE_EVENT_ASYNC_END2(
        "ServiceWorker",
        "ServiceWorkerControlleeRequestHandler::PrepareForMainResource",
        job_.get(),
        "Status", status,
        "Info", "ServiceWorker is blocked");
    return;
  }

  if (!provider_host_->IsContextSecureForServiceWorker()) {
    // TODO(falken): Figure out a way to surface in the page's DevTools
    // console that the service worker was blocked for security.
    job_->FallbackToNetwork();
    TRACE_EVENT_ASYNC_END1(
        "ServiceWorker",
        "ServiceWorkerControlleeRequestHandler::PrepareForMainResource",
        job_.get(), "Info", "Insecure context");
    return;
  }

  if (need_to_update) {
    force_update_started_ = true;
    context_->UpdateServiceWorker(
        registration.get(), true /* force_bypass_cache */,
        true /* skip_script_comparison */, provider_host_.get(),
        base::Bind(&self::DidUpdateRegistration, weak_factory_.GetWeakPtr(),
                   registration));
    return;
  }

  // Initiate activation of a waiting version.
  // Usually a register job initiates activation but that
  // doesn't happen if the browser exits prior to activation
  // having occurred. This check handles that case.
  if (registration->waiting_version())
    registration->ActivateWaitingVersionWhenReady();

  scoped_refptr<ServiceWorkerVersion> active_version =
      registration->active_version();

  // Wait until it's activated before firing fetch events.
  if (active_version.get() &&
      active_version->status() == ServiceWorkerVersion::ACTIVATING) {
    provider_host_->SetAllowAssociation(false);
    registration->active_version()->RegisterStatusChangeCallback(base::Bind(
        &self::OnVersionStatusChanged, weak_factory_.GetWeakPtr(),
        base::RetainedRef(registration), base::RetainedRef(active_version)));
    TRACE_EVENT_ASYNC_END2(
        "ServiceWorker",
        "ServiceWorkerControlleeRequestHandler::PrepareForMainResource",
        job_.get(),
        "Status", status,
        "Info", "Wait until finished SW activation");
    return;
  }

  // A registration exists, so associate it. Note that the controller is only
  // set if there's an active version. If there's no active version, we should
  // still associate so the provider host can use .ready.
  provider_host_->AssociateRegistration(registration.get(),
                                        false /* notify_controllerchange */);

  if (!active_version.get() ||
      active_version->status() != ServiceWorkerVersion::ACTIVATED) {
    job_->FallbackToNetwork();
    TRACE_EVENT_ASYNC_END2(
        "ServiceWorker",
        "ServiceWorkerControlleeRequestHandler::PrepareForMainResource",
        job_.get(),
        "Status", status,
        "Info",
        "ServiceWorkerVersion is not available, so falling back to network");
    return;
  }

  ServiceWorkerMetrics::CountControlledPageLoad(
      stripped_url_, active_version->has_fetch_handler(), is_main_frame_load_);

  job_->ForwardToServiceWorker();
  TRACE_EVENT_ASYNC_END2(
      "ServiceWorker",
      "ServiceWorkerControlleeRequestHandler::PrepareForMainResource",
      job_.get(), "Status", status, "Info", "Forwarded to the ServiceWorker");
}

void ServiceWorkerControlleeRequestHandler::OnVersionStatusChanged(
    ServiceWorkerRegistration* registration,
    ServiceWorkerVersion* version) {
  // The job may have been canceled and then destroyed before this was invoked.
  if (!job_)
    return;

  if (provider_host_)
    provider_host_->SetAllowAssociation(true);
  if (version != registration->active_version() ||
      version->status() != ServiceWorkerVersion::ACTIVATED ||
      !provider_host_) {
    job_->FallbackToNetwork();
    return;
  }

  ServiceWorkerMetrics::CountControlledPageLoad(
      stripped_url_, version->has_fetch_handler(), is_main_frame_load_);

  provider_host_->AssociateRegistration(registration,
                                        false /* notify_controllerchange */);
  job_->ForwardToServiceWorker();
}

void ServiceWorkerControlleeRequestHandler::DidUpdateRegistration(
    const scoped_refptr<ServiceWorkerRegistration>& original_registration,
    ServiceWorkerStatusCode status,
    const std::string& status_message,
    int64_t registration_id) {
  DCHECK(force_update_started_);

  // The job may have been canceled and then destroyed before this was invoked.
  if (!job_)
    return;

  if (!context_) {
    job_->FallbackToNetwork();
    return;
  }
  if (status != SERVICE_WORKER_OK ||
      !original_registration->installing_version()) {
    // Update failed. Look up the registration again since the original
    // registration was possibly unregistered in the meantime.
    context_->storage()->FindRegistrationForDocument(
        stripped_url_, base::Bind(&self::DidLookupRegistrationForMainResource,
                                  weak_factory_.GetWeakPtr()));
    return;
  }
  DCHECK_EQ(original_registration->id(), registration_id);
  scoped_refptr<ServiceWorkerVersion> new_version =
      original_registration->installing_version();
  new_version->ReportError(
      SERVICE_WORKER_OK,
      "ServiceWorker was updated because \"Force update on page load\" was "
      "checked in DevTools Source tab.");
  new_version->set_skip_waiting(true);
  new_version->RegisterStatusChangeCallback(base::Bind(
      &self::OnUpdatedVersionStatusChanged, weak_factory_.GetWeakPtr(),
      original_registration, new_version));
}

void ServiceWorkerControlleeRequestHandler::OnUpdatedVersionStatusChanged(
    const scoped_refptr<ServiceWorkerRegistration>& registration,
    const scoped_refptr<ServiceWorkerVersion>& version) {
  // The job may have been canceled and then destroyed before this was invoked.
  if (!job_)
    return;

  if (!context_) {
    job_->FallbackToNetwork();
    return;
  }
  if (version->status() == ServiceWorkerVersion::ACTIVATED ||
      version->status() == ServiceWorkerVersion::REDUNDANT) {
    // When the status is REDUNDANT, the update failed (eg: script error), we
    // continue with the incumbent version.
    // In case unregister job may have run, look up the registration again.
    context_->storage()->FindRegistrationForDocument(
        stripped_url_, base::Bind(&self::DidLookupRegistrationForMainResource,
                                  weak_factory_.GetWeakPtr()));
    return;
  }
  version->RegisterStatusChangeCallback(
      base::Bind(&self::OnUpdatedVersionStatusChanged,
                 weak_factory_.GetWeakPtr(), registration, version));
}

void ServiceWorkerControlleeRequestHandler::PrepareForSubResource() {
  DCHECK(job_.get());
  DCHECK(context_);
  DCHECK(provider_host_->active_version());
  job_->ForwardToServiceWorker();
}

void ServiceWorkerControlleeRequestHandler::OnPrepareToRestart() {
  use_network_ = true;
  ClearJob();
}

ServiceWorkerVersion*
ServiceWorkerControlleeRequestHandler::GetServiceWorkerVersion(
    ServiceWorkerMetrics::URLRequestJobResult* result) {
  if (!provider_host_) {
    *result = ServiceWorkerMetrics::REQUEST_JOB_ERROR_NO_PROVIDER_HOST;
    return nullptr;
  }
  if (!provider_host_->active_version()) {
    *result = ServiceWorkerMetrics::REQUEST_JOB_ERROR_NO_ACTIVE_VERSION;
    return nullptr;
  }
  return provider_host_->active_version();
}

bool ServiceWorkerControlleeRequestHandler::RequestStillValid(
    ServiceWorkerMetrics::URLRequestJobResult* result) {
  // A null |provider_host_| probably means the tab was closed. The null value
  // would cause problems down the line, so bail out.
  if (!provider_host_) {
    *result = ServiceWorkerMetrics::REQUEST_JOB_ERROR_NO_PROVIDER_HOST;
    return false;
  }
  return true;
}

void ServiceWorkerControlleeRequestHandler::MainResourceLoadFailed() {
  DCHECK(provider_host_);
  // Detach the controller so subresource requests also skip the worker.
  provider_host_->NotifyControllerLost();
}

void ServiceWorkerControlleeRequestHandler::ClearJob() {
  job_.reset();
}

}  // namespace content
