// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_VERSION_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_VERSION_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/id_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/service_worker_script_cache_map.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/common/service_worker/service_worker_types.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerEventResult.h"

class GURL;

namespace content {

class EmbeddedWorkerRegistry;
class ServiceWorkerContextCore;
class ServiceWorkerProviderHost;
class ServiceWorkerRegistration;
class ServiceWorkerVersionInfo;

// This class corresponds to a specific version of a ServiceWorker
// script for a given pattern. When a script is upgraded, there may be
// more than one ServiceWorkerVersion "running" at a time, but only
// one of them is active. This class connects the actual script with a
// running worker.
//
// is_shutdown_ detects the live-ness of the object itself. If the object is
// shut down, then it is in the process of being deleted from memory.
// This happens when a version is replaced as well as at browser shutdown.
class CONTENT_EXPORT ServiceWorkerVersion
    : NON_EXPORTED_BASE(public base::RefCounted<ServiceWorkerVersion>),
      public EmbeddedWorkerInstance::Listener {
 public:
  typedef base::Callback<void(ServiceWorkerStatusCode)> StatusCallback;
  typedef base::Callback<void(ServiceWorkerStatusCode,
                              const IPC::Message& message)> MessageCallback;
  typedef base::Callback<void(ServiceWorkerStatusCode,
                              ServiceWorkerFetchEventResult,
                              const ServiceWorkerResponse&)> FetchCallback;

  enum RunningStatus {
    STOPPED = EmbeddedWorkerInstance::STOPPED,
    STARTING = EmbeddedWorkerInstance::STARTING,
    RUNNING = EmbeddedWorkerInstance::RUNNING,
    STOPPING = EmbeddedWorkerInstance::STOPPING,
  };

  // Current version status; some of the status (e.g. INSTALLED and ACTIVE)
  // should be persisted unlike running status.
  enum Status {
    NEW,         // The version is just created.
    INSTALLING,  // Install event is dispatched and being handled.
    INSTALLED,   // Install event is finished and is ready to be activated.
    ACTIVATING,  // Activate event is dispatched and being handled.
    ACTIVE,      // Activation is finished and can run as active.
    DEACTIVATED, // The version is no longer running as active, due to
                 // unregistration or replace. (TODO(kinuko): we may need
                 // different states for different termination sequences)
  };

  class Listener {
   public:
    virtual void OnWorkerStarted(ServiceWorkerVersion* version) = 0;
    virtual void OnWorkerStopped(ServiceWorkerVersion* version) = 0;
    virtual void OnVersionStateChanged(ServiceWorkerVersion* version) = 0;
    virtual void OnErrorReported(ServiceWorkerVersion* version,
                                 const base::string16& error_message,
                                 int line_number,
                                 int column_number,
                                 const GURL& source_url) = 0;
    virtual void OnReportConsoleMessage(ServiceWorkerVersion* version,
                                        int source_identifier,
                                        int message_level,
                                        const base::string16& message,
                                        int line_number,
                                        const GURL& source_url) = 0;
  };

  ServiceWorkerVersion(
      ServiceWorkerRegistration* registration,
      int64 version_id,
      base::WeakPtr<ServiceWorkerContextCore> context);

  int64 version_id() const { return version_id_; }
  int64 registration_id() const { return registration_id_; }
  const GURL& script_url() const { return script_url_; }
  const GURL& scope() const { return scope_; }
  RunningStatus running_status() const {
    return static_cast<RunningStatus>(embedded_worker_->status());
  }
  ServiceWorkerVersionInfo GetInfo();
  Status status() const { return status_; }

  // This sets the new status and also run status change callbacks
  // if there're any (see RegisterStatusChangeCallback).
  void SetStatus(Status status);

  // Registers status change callback. (This is for one-off observation,
  // the consumer needs to re-register if it wants to continue observing
  // status changes)
  void RegisterStatusChangeCallback(const base::Closure& callback);

  // Starts an embedded worker for this version.
  // This returns OK (success) if the worker is already running.
  void StartWorker(const StatusCallback& callback);

  // Starts an embedded worker for this version.
  // |potential_process_ids| is a list of processes in which to start the
  // worker.
  // This returns OK (success) if the worker is already running.
  void StartWorkerWithCandidateProcesses(
      const std::vector<int>& potential_process_ids,
      const StatusCallback& callback);

  // Starts an embedded worker for this version.
  // This returns OK (success) if the worker is already stopped.
  void StopWorker(const StatusCallback& callback);

  // Sends an IPC message to the worker.
  // If the worker is not running this first tries to start it by
  // calling StartWorker internally.
  // |callback| can be null if the sender does not need to know if the
  // message is successfully sent or not.
  void SendMessage(const IPC::Message& message, const StatusCallback& callback);

  // Sends install event to the associated embedded worker and asynchronously
  // calls |callback| when it errors out or it gets response from the worker
  // to notify install completion.
  // |active_version_id| must be a valid positive ID
  // if there's an active (previous) version running.
  //
  // This must be called when the status() is NEW. Calling this changes
  // the version's status to INSTALLING.
  // Upon completion, the version's status will be changed to INSTALLED
  // on success, or back to NEW on failure.
  void DispatchInstallEvent(int active_version_id,
                            const StatusCallback& callback);

  // Sends activate event to the associated embedded worker and asynchronously
  // calls |callback| when it errors out or it gets response from the worker
  // to notify activation completion.
  //
  // This must be called when the status() is INSTALLED. Calling this changes
  // the version's status to ACTIVATING.
  // Upon completion, the version's status will be changed to ACTIVE
  // on success, or back to INSTALLED on failure.
  void DispatchActivateEvent(const StatusCallback& callback);

  // Sends fetch event to the associated embedded worker and calls
  // |callback| with the response from the worker.
  //
  // This must be called when the status() is ACTIVE. Calling this in other
  // statuses will result in an error SERVICE_WORKER_ERROR_FAILED.
  void DispatchFetchEvent(const ServiceWorkerFetchRequest& request,
                          const FetchCallback& callback);

  // Sends sync event to the associated embedded worker and asynchronously calls
  // |callback| when it errors out or it gets response from the worker to notify
  // completion.
  //
  // This must be called when the status() is ACTIVE.
  void DispatchSyncEvent(const StatusCallback& callback);

  // Sends push event to the associated embedded worker and asynchronously calls
  // |callback| when it errors out or it gets response from the worker to notify
  // completion.
  //
  // This must be called when the status() is ACTIVE.
  void DispatchPushEvent(const StatusCallback& callback,
                         const std::string& data);

  // These are expected to be called when a renderer process host for the
  // same-origin as for this ServiceWorkerVersion is created.  The added
  // processes are used to run an in-renderer embedded worker.
  void AddProcessToWorker(int process_id);
  void RemoveProcessFromWorker(int process_id);

  // Returns true if this has at least one process to run.
  bool HasProcessToRun() const;

  // Adds and removes |provider_host| as a controllee of this ServiceWorker.
  void AddControllee(ServiceWorkerProviderHost* provider_host);
  void RemoveControllee(ServiceWorkerProviderHost* provider_host);
  void AddWaitingControllee(ServiceWorkerProviderHost* provider_host);
  void RemoveWaitingControllee(ServiceWorkerProviderHost* provider_host);

  // Returns if it has controllee.
  bool HasControllee() const { return !controllee_map_.empty(); }

  // Adds and removes Listeners.
  void AddListener(Listener* listener);
  void RemoveListener(Listener* listener);

  ServiceWorkerScriptCacheMap* script_cache_map() { return &script_cache_map_; }
  EmbeddedWorkerInstance* embedded_worker() { return embedded_worker_.get(); }

 private:
  typedef ServiceWorkerVersion self;
  typedef std::map<ServiceWorkerProviderHost*, int> ControlleeMap;
  typedef IDMap<ServiceWorkerProviderHost> ControlleeByIDMap;
  friend class base::RefCounted<ServiceWorkerVersion>;

  virtual ~ServiceWorkerVersion();

  // EmbeddedWorkerInstance::Listener overrides:
  virtual void OnStarted() OVERRIDE;
  virtual void OnStopped() OVERRIDE;
  virtual void OnReportException(const base::string16& error_message,
                                 int line_number,
                                 int column_number,
                                 const GURL& source_url) OVERRIDE;
  virtual void OnReportConsoleMessage(int source_identifier,
                                      int message_level,
                                      const base::string16& message,
                                      int line_number,
                                      const GURL& source_url) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void RunStartWorkerCallbacksOnError(ServiceWorkerStatusCode status);

  void DispatchInstallEventAfterStartWorker(int active_version_id,
                                            const StatusCallback& callback);
  void DispatchActivateEventAfterStartWorker(const StatusCallback& callback);

  // Message handlers.
  void OnGetClientDocuments(int request_id);
  void OnActivateEventFinished(int request_id,
                               blink::WebServiceWorkerEventResult result);
  void OnInstallEventFinished(int request_id,
                              blink::WebServiceWorkerEventResult result);
  void OnFetchEventFinished(int request_id,
                            ServiceWorkerFetchEventResult result,
                            const ServiceWorkerResponse& response);
  void OnSyncEventFinished(int request_id);
  void OnPushEventFinished(int request_id);
  void OnPostMessageToDocument(int client_id,
                               const base::string16& message,
                               const std::vector<int>& sent_message_port_ids);

  void ScheduleStopWorker();

  const int64 version_id_;
  int64 registration_id_;
  GURL script_url_;
  GURL scope_;
  Status status_;
  scoped_ptr<EmbeddedWorkerInstance> embedded_worker_;
  std::vector<StatusCallback> start_callbacks_;
  std::vector<StatusCallback> stop_callbacks_;
  std::vector<base::Closure> status_change_callbacks_;

  // Message callbacks.
  IDMap<StatusCallback, IDMapOwnPointer> activate_callbacks_;
  IDMap<StatusCallback, IDMapOwnPointer> install_callbacks_;
  IDMap<FetchCallback, IDMapOwnPointer> fetch_callbacks_;
  IDMap<StatusCallback, IDMapOwnPointer> sync_callbacks_;
  IDMap<StatusCallback, IDMapOwnPointer> push_callbacks_;

  ControlleeMap controllee_map_;
  ControlleeByIDMap controllee_by_id_;
  base::WeakPtr<ServiceWorkerContextCore> context_;
  ObserverList<Listener> listeners_;
  ServiceWorkerScriptCacheMap script_cache_map_;
  base::OneShotTimer<ServiceWorkerVersion> stop_worker_timer_;

  base::WeakPtrFactory<ServiceWorkerVersion> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerVersion);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_VERSION_H_
