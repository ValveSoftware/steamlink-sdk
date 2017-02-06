// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_types.h"
#include "url/gurl.h"

namespace content {

class ServiceWorkerVersion;
struct ServiceWorkerRegistrationInfo;

// Represents the core of a service worker registration object. Other
// registration derivatives (WebServiceWorkerRegistration etc) ultimately refer
// to this class. This is refcounted via ServiceWorkerRegistrationHandle to
// facilitate multiple controllees being associated with the same registration.
class CONTENT_EXPORT ServiceWorkerRegistration
    : public NON_EXPORTED_BASE(base::RefCounted<ServiceWorkerRegistration>),
      public NON_EXPORTED_BASE(ServiceWorkerVersion::Listener) {
 public:
  typedef base::Callback<void(ServiceWorkerStatusCode status)> StatusCallback;

  class Listener {
   public:
    virtual void OnVersionAttributesChanged(
        ServiceWorkerRegistration* registration,
        ChangedVersionAttributesMask changed_mask,
        const ServiceWorkerRegistrationInfo& info) {}
    virtual void OnRegistrationFailed(
        ServiceWorkerRegistration* registration) {}
    virtual void OnRegistrationFinishedUninstalling(
        ServiceWorkerRegistration* registration) {}
    virtual void OnUpdateFound(
        ServiceWorkerRegistration* registration) {}
    virtual void OnSkippedWaiting(ServiceWorkerRegistration* registation) {}
  };

  ServiceWorkerRegistration(const GURL& pattern,
                            int64_t registration_id,
                            base::WeakPtr<ServiceWorkerContextCore> context);

  int64_t id() const { return registration_id_; }
  const GURL& pattern() const { return pattern_; }

  bool is_deleted() const { return is_deleted_; }
  void set_is_deleted(bool deleted) { is_deleted_ = deleted; }

  bool is_uninstalling() const { return is_uninstalling_; }

  void set_is_uninstalled(bool uninstalled) { is_uninstalled_ = uninstalled; }
  bool is_uninstalled() const { return is_uninstalled_; }

  int64_t resources_total_size_bytes() const {
    return resources_total_size_bytes_;
  }

  void set_resources_total_size_bytes(int64_t resources_total_size_bytes) {
    resources_total_size_bytes_ = resources_total_size_bytes;
  }

  // Returns the active version. This version may be in ACTIVATING or ACTIVATED
  // state. If you require an ACTIVATED version, use
  // ServiceWorkerContextWrapper::FindReadyRegistration* to get a registration
  // with such a version. Alternatively, use
  // ServiceWorkerVersion::Listener::OnVersionStateChanged to wait for the
  // ACTIVATING version to become ACTIVATED.
  ServiceWorkerVersion* active_version() const {
    return active_version_.get();
  }

  ServiceWorkerVersion* waiting_version() const {
    return waiting_version_.get();
  }

  ServiceWorkerVersion* installing_version() const {
    return installing_version_.get();
  }

  bool has_installed_version() const {
    return active_version() || waiting_version();
  }

  ServiceWorkerVersion* GetNewestVersion() const;

  void AddListener(Listener* listener);
  void RemoveListener(Listener* listener);
  void NotifyRegistrationFailed();
  void NotifyUpdateFound();
  void NotifyVersionAttributesChanged(ChangedVersionAttributesMask mask);

  ServiceWorkerRegistrationInfo GetInfo();

  // Sets the corresposding version attribute and resets the position
  // (if any) left vacant (ie. by a waiting version being promoted).
  // Also notifies listeners via OnVersionAttributesChanged.
  void SetActiveVersion(const scoped_refptr<ServiceWorkerVersion>& version);
  void SetWaitingVersion(const scoped_refptr<ServiceWorkerVersion>& version);
  void SetInstallingVersion(const scoped_refptr<ServiceWorkerVersion>& version);

  // If version is the installing, waiting, active version of this
  // registation, the method will reset that field to NULL, and notify
  // listeners via OnVersionAttributesChanged.
  void UnsetVersion(ServiceWorkerVersion* version);

  // Triggers the [[Activate]] algorithm when the currently active version
  // has no controllees. If there are no controllees at the time the method
  // is called or when version's skip waiting flag is set, activation is
  // initiated immediately.
  void ActivateWaitingVersionWhenReady();

  // Takes over control of provider hosts which are currently not controlled or
  // controlled by other registrations.
  void ClaimClients();

  // Triggers the [[ClearRegistration]] algorithm when the currently
  // active version has no controllees. Deletes this registration
  // from storage immediately.
  void ClearWhenReady();

  // Restores this registration in storage and cancels the pending
  // [[ClearRegistration]] algorithm.
  void AbortPendingClear(const StatusCallback& callback);

  // The time of the most recent update check.
  base::Time last_update_check() const { return last_update_check_; }
  void set_last_update_check(base::Time last) { last_update_check_ = last; }

  // Unsets the version and deletes its resources. Also deletes this
  // registration from storage if there is no longer a stored version.
  void DeleteVersion(const scoped_refptr<ServiceWorkerVersion>& version);

  void RegisterRegistrationFinishedCallback(const base::Closure& callback);
  void NotifyRegistrationFinished();

  void SetTaskRunnerForTest(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

 private:
  friend class base::RefCounted<ServiceWorkerRegistration>;

  ~ServiceWorkerRegistration() override;

  void UnsetVersionInternal(
      ServiceWorkerVersion* version,
      ChangedVersionAttributesMask* mask);

  // ServiceWorkerVersion::Listener override.
  void OnNoControllees(ServiceWorkerVersion* version) override;
  void OnNoWork(ServiceWorkerVersion* version) override;

  bool IsReadyToActivate() const;

  // Promotes the waiting version to active version. If |delay| is true, waits
  // a short time before attempting to start and dispatch the activate event
  // to the version.
  void ActivateWaitingVersion(bool delay);
  void ContinueActivation(
      scoped_refptr<ServiceWorkerVersion> activating_version);
  void DispatchActivateEvent(
      scoped_refptr<ServiceWorkerVersion> activating_version);
  void OnActivateEventFinished(
      scoped_refptr<ServiceWorkerVersion> activating_version,
      ServiceWorkerStatusCode status);

  void OnDeleteFinished(ServiceWorkerStatusCode status);

  // This method corresponds to the [[ClearRegistration]] algorithm.
  void Clear();

  void OnRestoreFinished(const StatusCallback& callback,
                         scoped_refptr<ServiceWorkerVersion> version,
                         ServiceWorkerStatusCode status);

  const GURL pattern_;
  const int64_t registration_id_;
  bool is_deleted_;
  bool is_uninstalling_;
  bool is_uninstalled_;
  bool should_activate_when_ready_;
  base::Time last_update_check_;
  int64_t resources_total_size_bytes_;

  // This registration is the primary owner of these versions.
  scoped_refptr<ServiceWorkerVersion> active_version_;
  scoped_refptr<ServiceWorkerVersion> waiting_version_;
  scoped_refptr<ServiceWorkerVersion> installing_version_;

  base::ObserverList<Listener> listeners_;
  std::vector<base::Closure> registration_finished_callbacks_;
  base::WeakPtr<ServiceWorkerContextCore> context_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerRegistration);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_H_
