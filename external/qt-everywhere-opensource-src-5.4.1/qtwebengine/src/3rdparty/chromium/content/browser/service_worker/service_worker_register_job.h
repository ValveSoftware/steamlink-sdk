// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTER_JOB_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTER_JOB_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_register_job_base.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "url/gurl.h"

namespace content {

class ServiceWorkerJobCoordinator;
class ServiceWorkerStorage;

// Handles the registration of a Service Worker.
//
// The registration flow includes most or all of the following,
// depending on what is already registered:
//  - creating a ServiceWorkerRegistration instance if there isn't
//    already something registered
//  - creating a ServiceWorkerVersion for the new registration instance.
//  - starting a worker for the ServiceWorkerVersion
//  - telling the Version to evaluate the script
//  - firing the 'install' event at the ServiceWorkerVersion
//  - firing the 'activate' event at the ServiceWorkerVersion
//  - waiting for older ServiceWorkerVersions to deactivate
//  - designating the new version to be the 'active' version
class ServiceWorkerRegisterJob : public ServiceWorkerRegisterJobBase {
 public:
  typedef base::Callback<void(ServiceWorkerStatusCode status,
                              ServiceWorkerRegistration* registration,
                              ServiceWorkerVersion* version)>
      RegistrationCallback;

  CONTENT_EXPORT ServiceWorkerRegisterJob(
      base::WeakPtr<ServiceWorkerContextCore> context,
      const GURL& pattern,
      const GURL& script_url);
  virtual ~ServiceWorkerRegisterJob();

  // Registers a callback to be called when the promise would resolve (whether
  // successfully or not). Multiple callbacks may be registered. If |process_id|
  // is not -1, it's added to the existing clients when deciding in which
  // process to create the Service Worker instance.  If there are no existing
  // clients, a new RenderProcessHost will be created.
  void AddCallback(const RegistrationCallback& callback, int process_id);

  // ServiceWorkerRegisterJobBase implementation:
  virtual void Start() OVERRIDE;
  virtual void Abort() OVERRIDE;
  virtual bool Equals(ServiceWorkerRegisterJobBase* job) OVERRIDE;
  virtual RegistrationJobType GetType() OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerProviderHostWaitingVersionTest,
                           AssociateWaitingVersionToDocuments);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerProviderHostWaitingVersionTest,
                           DisassociateWaitingVersionFromDocuments);

  enum Phase {
     INITIAL,
     START,
     REGISTER,
     UPDATE,
     INSTALL,
     STORE,
     ACTIVATE,
     COMPLETE,
     ABORT,
  };

  // Holds internal state of ServiceWorkerRegistrationJob, to compel use of the
  // getter/setter functions.
  struct Internal {
    Internal();
    ~Internal();
    scoped_refptr<ServiceWorkerRegistration> registration;

    // Holds 'installing' or 'waiting' version depending on the phase.
    scoped_refptr<ServiceWorkerVersion> pending_version;
  };

  void set_registration(ServiceWorkerRegistration* registration);
  ServiceWorkerRegistration* registration();
  void set_pending_version(ServiceWorkerVersion* version);
  ServiceWorkerVersion* pending_version();

  void SetPhase(Phase phase);

  void HandleExistingRegistrationAndContinue(
      ServiceWorkerStatusCode status,
      const scoped_refptr<ServiceWorkerRegistration>& registration);
  void RegisterAndContinue(ServiceWorkerStatusCode status);
  void UpdateAndContinue(ServiceWorkerStatusCode status);
  void OnStartWorkerFinished(ServiceWorkerStatusCode status);
  void OnStoreRegistrationComplete(ServiceWorkerStatusCode status);
  void InstallAndContinue();
  void OnInstallFinished(ServiceWorkerStatusCode status);
  void ActivateAndContinue();
  void OnActivateFinished(ServiceWorkerStatusCode status);
  void Complete(ServiceWorkerStatusCode status);
  void CompleteInternal(ServiceWorkerStatusCode status);

  void ResolvePromise(ServiceWorkerStatusCode status,
                      ServiceWorkerRegistration* registration,
                      ServiceWorkerVersion* version);

  // Associates a waiting version to documents matched with a scope of the
  // version.
  CONTENT_EXPORT static void AssociateWaitingVersionToDocuments(
      base::WeakPtr<ServiceWorkerContextCore> context,
      ServiceWorkerVersion* version);

  // Disassociates a waiting version specified by |version_id| from documents.
  CONTENT_EXPORT static void DisassociateWaitingVersionFromDocuments(
      base::WeakPtr<ServiceWorkerContextCore> context,
      int64 version_id);

  // The ServiceWorkerContextCore object should always outlive this.
  base::WeakPtr<ServiceWorkerContextCore> context_;

  const GURL pattern_;
  const GURL script_url_;
  std::vector<RegistrationCallback> callbacks_;
  std::vector<int> pending_process_ids_;
  Phase phase_;
  Internal internal_;
  bool is_promise_resolved_;
  ServiceWorkerStatusCode promise_resolved_status_;
  scoped_refptr<ServiceWorkerRegistration> promise_resolved_registration_;
  scoped_refptr<ServiceWorkerVersion> promise_resolved_version_;
  base::WeakPtrFactory<ServiceWorkerRegisterJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerRegisterJob);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTER_JOB_H_
