// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_register_job.h"

#include <vector>

#include "base/message_loop/message_loop.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_job_coordinator.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/browser/service_worker/service_worker_utils.h"

namespace content {

namespace {

void RunSoon(const base::Closure& closure) {
  base::MessageLoop::current()->PostTask(FROM_HERE, closure);
}

}

typedef ServiceWorkerRegisterJobBase::RegistrationJobType RegistrationJobType;

ServiceWorkerRegisterJob::ServiceWorkerRegisterJob(
    base::WeakPtr<ServiceWorkerContextCore> context,
    const GURL& pattern,
    const GURL& script_url)
    : context_(context),
      pattern_(pattern),
      script_url_(script_url),
      phase_(INITIAL),
      is_promise_resolved_(false),
      promise_resolved_status_(SERVICE_WORKER_OK),
      weak_factory_(this) {}

ServiceWorkerRegisterJob::~ServiceWorkerRegisterJob() {
  DCHECK(!context_ ||
         phase_ == INITIAL || phase_ == COMPLETE || phase_ == ABORT)
      << "Jobs should only be interrupted during shutdown.";
}

void ServiceWorkerRegisterJob::AddCallback(const RegistrationCallback& callback,
                                           int process_id) {
  if (!is_promise_resolved_) {
    callbacks_.push_back(callback);
    if (process_id != -1 && (phase_ < UPDATE || !pending_version()))
      pending_process_ids_.push_back(process_id);
    return;
  }
  RunSoon(base::Bind(
      callback, promise_resolved_status_,
      promise_resolved_registration_, promise_resolved_version_));
}

void ServiceWorkerRegisterJob::Start() {
  SetPhase(START);
  context_->storage()->FindRegistrationForPattern(
      pattern_,
      base::Bind(
          &ServiceWorkerRegisterJob::HandleExistingRegistrationAndContinue,
          weak_factory_.GetWeakPtr()));
}

void ServiceWorkerRegisterJob::Abort() {
  SetPhase(ABORT);
  CompleteInternal(SERVICE_WORKER_ERROR_ABORT);
  // Don't have to call FinishJob() because the caller takes care of removing
  // the jobs from the queue.
}

bool ServiceWorkerRegisterJob::Equals(ServiceWorkerRegisterJobBase* job) {
  if (job->GetType() != GetType())
    return false;
  ServiceWorkerRegisterJob* register_job =
      static_cast<ServiceWorkerRegisterJob*>(job);
  return register_job->pattern_ == pattern_ &&
         register_job->script_url_ == script_url_;
}

RegistrationJobType ServiceWorkerRegisterJob::GetType() {
  return REGISTRATION;
}

ServiceWorkerRegisterJob::Internal::Internal() {}

ServiceWorkerRegisterJob::Internal::~Internal() {}

void ServiceWorkerRegisterJob::set_registration(
    ServiceWorkerRegistration* registration) {
  DCHECK(phase_ == START || phase_ == REGISTER) << phase_;
  DCHECK(!internal_.registration);
  internal_.registration = registration;
}

ServiceWorkerRegistration* ServiceWorkerRegisterJob::registration() {
  DCHECK(phase_ >= REGISTER) << phase_;
  return internal_.registration;
}

void ServiceWorkerRegisterJob::set_pending_version(
    ServiceWorkerVersion* version) {
  DCHECK(phase_ == UPDATE) << phase_;
  DCHECK(!internal_.pending_version);
  internal_.pending_version = version;
}

ServiceWorkerVersion* ServiceWorkerRegisterJob::pending_version() {
  DCHECK(phase_ >= UPDATE) << phase_;
  return internal_.pending_version;
}

void ServiceWorkerRegisterJob::SetPhase(Phase phase) {
  switch (phase) {
    case INITIAL:
      NOTREACHED();
      break;
    case START:
      DCHECK(phase_ == INITIAL) << phase_;
      break;
    case REGISTER:
      DCHECK(phase_ == START) << phase_;
      break;
    case UPDATE:
      DCHECK(phase_ == START || phase_ == REGISTER) << phase_;
      break;
    case INSTALL:
      DCHECK(phase_ == UPDATE) << phase_;
      break;
    case STORE:
      DCHECK(phase_ == INSTALL) << phase_;
      break;
    case ACTIVATE:
      DCHECK(phase_ == STORE) << phase_;
      break;
    case COMPLETE:
      DCHECK(phase_ != INITIAL && phase_ != COMPLETE) << phase_;
      break;
    case ABORT:
      break;
  }
  phase_ = phase;
}

// This function corresponds to the steps in Register following
// "Let serviceWorkerRegistration be _GetRegistration(scope)"
// |existing_registration| corresponds to serviceWorkerRegistration.
// Throughout this file, comments in quotes are excerpts from the spec.
void ServiceWorkerRegisterJob::HandleExistingRegistrationAndContinue(
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& existing_registration) {
  // On unexpected error, abort this registration job.
  if (status != SERVICE_WORKER_ERROR_NOT_FOUND && status != SERVICE_WORKER_OK) {
    Complete(status);
    return;
  }

  // "If serviceWorkerRegistration is not null and script is equal to
  // serviceWorkerRegistration.scriptUrl..." resolve with the existing
  // registration and abort.
  if (existing_registration.get() &&
      existing_registration->script_url() == script_url_) {
    set_registration(existing_registration);
    // If there's no active version, go ahead to Update (this isn't in the spec
    // but seems reasonable, and without SoftUpdate implemented we can never
    // Update otherwise).
    if (!existing_registration->active_version()) {
      UpdateAndContinue(status);
      return;
    }
    ResolvePromise(
        status, existing_registration, existing_registration->active_version());
    Complete(SERVICE_WORKER_OK);
    return;
  }

  // "If serviceWorkerRegistration is null..." create a new registration.
  if (!existing_registration.get()) {
    RegisterAndContinue(SERVICE_WORKER_OK);
    return;
  }

  // On script URL mismatch, "set serviceWorkerRegistration.scriptUrl to
  // script." We accomplish this by deleting the existing registration and
  // registering a new one.
  // TODO(falken): Match the spec. We now throw away the active_version_ and
  // waiting_version_ of the existing registration, which isn't in the spec.
  // TODO(michaeln): Deactivate the live existing_registration object and
  // eventually call storage->DeleteVersionResources()
  // when it no longer has any controllees.
  context_->storage()->DeleteRegistration(
      existing_registration->id(),
      existing_registration->script_url().GetOrigin(),
      base::Bind(&ServiceWorkerRegisterJob::RegisterAndContinue,
                 weak_factory_.GetWeakPtr()));
}

// Creates a new ServiceWorkerRegistration.
void ServiceWorkerRegisterJob::RegisterAndContinue(
    ServiceWorkerStatusCode status) {
  SetPhase(REGISTER);
  if (status != SERVICE_WORKER_OK) {
    // Abort this registration job.
    Complete(status);
    return;
  }

  set_registration(new ServiceWorkerRegistration(
      pattern_, script_url_, context_->storage()->NewRegistrationId(),
      context_));
  context_->storage()->NotifyInstallingRegistration(registration());
  UpdateAndContinue(SERVICE_WORKER_OK);
}

// This function corresponds to the spec's _Update algorithm.
void ServiceWorkerRegisterJob::UpdateAndContinue(
    ServiceWorkerStatusCode status) {
  SetPhase(UPDATE);
  if (status != SERVICE_WORKER_OK) {
    // Abort this registration job.
    Complete(status);
    return;
  }

  // TODO(falken): "If serviceWorkerRegistration.installingWorker is not null.."
  // then terminate the installing worker. It doesn't make sense to implement
  // yet since we always activate the worker if install completed, so there can
  // be no installing worker at this point.
  // TODO(nhiroki): Check 'installing_version()' instead when it's supported.
  DCHECK(!registration()->waiting_version());

  // "Let serviceWorker be a newly-created ServiceWorker object..." and start
  // the worker.
  set_pending_version(new ServiceWorkerVersion(
      registration(), context_->storage()->NewVersionId(), context_));

  // TODO(michaeln): Start the worker into a paused state where the
  // script resource is downloaded but not yet evaluated.
  pending_version()->StartWorkerWithCandidateProcesses(
      pending_process_ids_,
      base::Bind(&ServiceWorkerRegisterJob::OnStartWorkerFinished,
                 weak_factory_.GetWeakPtr()));
}

void ServiceWorkerRegisterJob::OnStartWorkerFinished(
    ServiceWorkerStatusCode status) {
  // "If serviceWorker fails to start up..." then reject the promise with an
  // error and abort.
  if (status != SERVICE_WORKER_OK) {
    Complete(status);
    return;
  }

  // TODO(michaeln): Compare the old and new script.
  // If different unpause the worker and continue with
  // the job. If the same ResolvePromise with the current
  // version and complete the job, throwing away the new version
  // since there's nothing new.

  // "Resolve promise with serviceWorker."
  // Although the spec doesn't set waitingWorker until after resolving the
  // promise, our system's resolving works by passing ServiceWorkerRegistration
  // to the callbacks, so waitingWorker must be set first.
  DCHECK(!registration()->waiting_version());
  registration()->set_waiting_version(pending_version());
  ResolvePromise(status, registration(), pending_version());

  AssociateWaitingVersionToDocuments(context_, pending_version());

  InstallAndContinue();
}

// This function corresponds to the spec's _Install algorithm.
void ServiceWorkerRegisterJob::InstallAndContinue() {
  SetPhase(INSTALL);
  // "Set serviceWorkerRegistration.installingWorker._state to installing."
  // "Fire install event on the associated ServiceWorkerGlobalScope object."
  pending_version()->DispatchInstallEvent(
      -1,
      base::Bind(&ServiceWorkerRegisterJob::OnInstallFinished,
                 weak_factory_.GetWeakPtr()));
}

void ServiceWorkerRegisterJob::OnInstallFinished(
    ServiceWorkerStatusCode status) {
  // "If any handler called waitUntil()..." and the resulting promise
  // is rejected, abort.
  // TODO(kinuko,falken): For some error cases (e.g. ServiceWorker is
  // unexpectedly terminated) we may want to retry sending the event again.
  if (status != SERVICE_WORKER_OK) {
    Complete(status);
    return;
  }

  SetPhase(STORE);
  context_->storage()->StoreRegistration(
      registration(),
      pending_version(),
      base::Bind(&ServiceWorkerRegisterJob::OnStoreRegistrationComplete,
                 weak_factory_.GetWeakPtr()));
}

void ServiceWorkerRegisterJob::OnStoreRegistrationComplete(
    ServiceWorkerStatusCode status) {
  if (status != SERVICE_WORKER_OK) {
    Complete(status);
    return;
  }

  ActivateAndContinue();
}

// This function corresponds to the spec's _Activate algorithm.
void ServiceWorkerRegisterJob::ActivateAndContinue() {
  SetPhase(ACTIVATE);

  // "If existingWorker is not null, then: wait for exitingWorker to finish
  // handling any in-progress requests."
  // See if we already have an active_version for the scope and it has
  // controllee documents (if so activating the new version should wait
  // until we have no documents controlled by the version).
  if (registration()->active_version() &&
      registration()->active_version()->HasControllee()) {
    // TODO(kinuko,falken): Currently we immediately return if the existing
    // registration already has an active version, so we shouldn't come
    // this way.
    NOTREACHED();
    // TODO(falken): Register an continuation task to wait for NoControllees
    // notification so that we can resume activation later (see comments
    // in ServiceWorkerVersion::RemoveControllee).
    Complete(SERVICE_WORKER_OK);
    return;
  }

  // "Set serviceWorkerRegistration.waitingWorker to null."
  // "Set serviceWorkerRegistration.activeWorker to activatingWorker."
  DisassociateWaitingVersionFromDocuments(
      context_, pending_version()->version_id());
  registration()->set_waiting_version(NULL);
  DCHECK(!registration()->active_version());
  registration()->set_active_version(pending_version());

  // "Set serviceWorkerRegistration.activeWorker._state to activating."
  // "Fire activate event on the associated ServiceWorkerGlobalScope object."
  // "Set serviceWorkerRegistration.activeWorker._state to active."
  pending_version()->DispatchActivateEvent(
      base::Bind(&ServiceWorkerRegisterJob::OnActivateFinished,
                 weak_factory_.GetWeakPtr()));
}

void ServiceWorkerRegisterJob::OnActivateFinished(
    ServiceWorkerStatusCode status) {
  // "If any handler called waitUntil()..." and the resulting promise
  // is rejected, abort.
  // TODO(kinuko,falken): For some error cases (e.g. ServiceWorker is
  // unexpectedly terminated) we may want to retry sending the event again.
  if (status != SERVICE_WORKER_OK) {
    registration()->set_active_version(NULL);
    Complete(status);
    return;
  }
  context_->storage()->UpdateToActiveState(
      registration(),
      base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
  Complete(SERVICE_WORKER_OK);
}

void ServiceWorkerRegisterJob::Complete(ServiceWorkerStatusCode status) {
  CompleteInternal(status);
  context_->job_coordinator()->FinishJob(pattern_, this);
}

void ServiceWorkerRegisterJob::CompleteInternal(
    ServiceWorkerStatusCode status) {
  SetPhase(COMPLETE);
  if (status != SERVICE_WORKER_OK) {
    if (registration() && registration()->waiting_version()) {
      DisassociateWaitingVersionFromDocuments(
          context_, registration()->waiting_version()->version_id());
      registration()->set_waiting_version(NULL);
    }
    if (registration() && !registration()->active_version()) {
      context_->storage()->DeleteRegistration(
          registration()->id(),
          registration()->script_url().GetOrigin(),
          base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
    }
    if (!is_promise_resolved_)
      ResolvePromise(status, NULL, NULL);
  }
  DCHECK(callbacks_.empty());
  if (registration()) {
    context_->storage()->NotifyDoneInstallingRegistration(
        registration(), pending_version(), status);
  }
}

void ServiceWorkerRegisterJob::ResolvePromise(
    ServiceWorkerStatusCode status,
    ServiceWorkerRegistration* registration,
    ServiceWorkerVersion* version) {
  DCHECK(!is_promise_resolved_);
  is_promise_resolved_ = true;
  promise_resolved_status_ = status;
  promise_resolved_registration_ = registration;
  promise_resolved_version_ = version;
  for (std::vector<RegistrationCallback>::iterator it = callbacks_.begin();
       it != callbacks_.end();
       ++it) {
    it->Run(status, registration, version);
  }
  callbacks_.clear();
}

// static
void ServiceWorkerRegisterJob::AssociateWaitingVersionToDocuments(
    base::WeakPtr<ServiceWorkerContextCore> context,
    ServiceWorkerVersion* version) {
  DCHECK(context);
  DCHECK(version);

  for (scoped_ptr<ServiceWorkerContextCore::ProviderHostIterator> it =
           context->GetProviderHostIterator();
       !it->IsAtEnd();
       it->Advance()) {
    ServiceWorkerProviderHost* host = it->GetProviderHost();
    if (!host->IsContextAlive())
      continue;
    if (ServiceWorkerUtils::ScopeMatches(version->scope(),
                                         host->document_url())) {
      // The spec's _Update algorithm says, "upgrades active version to a new
      // version for the same URL scope.", so skip if the scope (registration)
      // of |version| is different from that of the current active/waiting
      // version.
      if (!host->ValidateVersionForAssociation(version))
        continue;

      // TODO(nhiroki): Keep |host->waiting_version()| to be replaced and set
      // status of them to 'redandunt' after breaking the loop.

      host->SetWaitingVersion(version);
      // TODO(nhiroki): Set |host|'s installing version to null.
    }
  }
}

// static
void ServiceWorkerRegisterJob::DisassociateWaitingVersionFromDocuments(
    base::WeakPtr<ServiceWorkerContextCore> context,
    int64 version_id) {
  DCHECK(context);
  for (scoped_ptr<ServiceWorkerContextCore::ProviderHostIterator> it =
           context->GetProviderHostIterator();
       !it->IsAtEnd();
       it->Advance()) {
    ServiceWorkerProviderHost* host = it->GetProviderHost();
    if (!host->IsContextAlive())
      continue;
    if (host->waiting_version() &&
        host->waiting_version()->version_id() == version_id) {
      host->SetWaitingVersion(NULL);
    }
  }
}

}  // namespace content
