// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CORE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CORE_H_

#include <map>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/id_map.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_threadsafe.h"
#include "content/browser/service_worker/service_worker_info.h"
#include "content/browser/service_worker/service_worker_process_manager.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_registration_status.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/common/content_export.h"

class GURL;

namespace base {
class FilePath;
class MessageLoopProxy;
class SequencedTaskRunner;
}

namespace quota {
class QuotaManagerProxy;
}

namespace content {

class EmbeddedWorkerRegistry;
class ServiceWorkerContextObserver;
class ServiceWorkerContextWrapper;
class ServiceWorkerHandle;
class ServiceWorkerJobCoordinator;
class ServiceWorkerProviderHost;
class ServiceWorkerRegistration;
class ServiceWorkerStorage;

// This class manages data associated with service workers.
// The class is single threaded and should only be used on the IO thread.
// In chromium, there is one instance per storagepartition. This class
// is the root of the containment hierarchy for service worker data
// associated with a particular partition.
class CONTENT_EXPORT ServiceWorkerContextCore
    : public ServiceWorkerVersion::Listener {
 public:
  typedef base::Callback<void(ServiceWorkerStatusCode status,
                              int64 registration_id,
                              int64 version_id)> RegistrationCallback;
  typedef base::Callback<
      void(ServiceWorkerStatusCode status)> UnregistrationCallback;
  typedef IDMap<ServiceWorkerProviderHost, IDMapOwnPointer> ProviderMap;
  typedef IDMap<ProviderMap, IDMapOwnPointer> ProcessToProviderMap;

  // Iterates over ServiceWorkerProviderHost objects in a ProcessToProviderMap.
  class ProviderHostIterator {
   public:
    ~ProviderHostIterator();
    ServiceWorkerProviderHost* GetProviderHost();
    void Advance();
    bool IsAtEnd();

   private:
    friend class ServiceWorkerContextCore;
    explicit ProviderHostIterator(ProcessToProviderMap* map);
    void Initialize();

    ProcessToProviderMap* map_;
    scoped_ptr<ProcessToProviderMap::iterator> provider_iterator_;
    scoped_ptr<ProviderMap::iterator> provider_host_iterator_;

    DISALLOW_COPY_AND_ASSIGN(ProviderHostIterator);
  };

  // This is owned by the StoragePartition, which will supply it with
  // the local path on disk. Given an empty |user_data_directory|,
  // nothing will be stored on disk. |observer_list| is created in
  // ServiceWorkerContextWrapper. When Notify() of |observer_list| is called in
  // ServiceWorkerContextCore, the methods of ServiceWorkerContextObserver will
  // be called on the thread which called AddObserver() of |observer_list|.
  ServiceWorkerContextCore(
      const base::FilePath& user_data_directory,
      base::SequencedTaskRunner* database_task_runner,
      base::MessageLoopProxy* disk_cache_thread,
      quota::QuotaManagerProxy* quota_manager_proxy,
      ObserverListThreadSafe<ServiceWorkerContextObserver>* observer_list,
      ServiceWorkerContextWrapper* wrapper);
  virtual ~ServiceWorkerContextCore();

  // ServiceWorkerVersion::Listener overrides.
  virtual void OnWorkerStarted(ServiceWorkerVersion* version) OVERRIDE;
  virtual void OnWorkerStopped(ServiceWorkerVersion* version) OVERRIDE;
  virtual void OnVersionStateChanged(ServiceWorkerVersion* version) OVERRIDE;
  virtual void OnErrorReported(ServiceWorkerVersion* version,
                               const base::string16& error_message,
                               int line_number,
                               int column_number,
                               const GURL& source_url) OVERRIDE;
  virtual void OnReportConsoleMessage(ServiceWorkerVersion* version,
                                      int source_identifier,
                                      int message_level,
                                      const base::string16& message,
                                      int line_number,
                                      const GURL& source_url) OVERRIDE;

  ServiceWorkerStorage* storage() { return storage_.get(); }
  ServiceWorkerProcessManager* process_manager();
  EmbeddedWorkerRegistry* embedded_worker_registry() {
    return embedded_worker_registry_.get();
  }
  ServiceWorkerJobCoordinator* job_coordinator() {
    return job_coordinator_.get();
  }

  // The context class owns the set of ProviderHosts.
  ServiceWorkerProviderHost* GetProviderHost(int process_id, int provider_id);
  void AddProviderHost(scoped_ptr<ServiceWorkerProviderHost> provider_host);
  void RemoveProviderHost(int process_id, int provider_id);
  void RemoveAllProviderHostsForProcess(int process_id);
  scoped_ptr<ProviderHostIterator> GetProviderHostIterator();

  // The callback will be called on the IO thread.
  // A child process of |source_process_id| may be used to run the created
  // worker for initial installation.
  // Non-null |provider_host| must be given if this is called from a document,
  // whose process_id() must match with |source_process_id|.
  void RegisterServiceWorker(const GURL& pattern,
                             const GURL& script_url,
                             int source_process_id,
                             ServiceWorkerProviderHost* provider_host,
                             const RegistrationCallback& callback);

  // The callback will be called on the IO thread.
  void UnregisterServiceWorker(const GURL& pattern,
                               const UnregistrationCallback& callback);

  // This class maintains collections of live instances, this class
  // does not own these object or influence their lifetime.
  ServiceWorkerRegistration* GetLiveRegistration(int64 registration_id);
  void AddLiveRegistration(ServiceWorkerRegistration* registration);
  void RemoveLiveRegistration(int64 registration_id);
  ServiceWorkerVersion* GetLiveVersion(int64 version_id);
  void AddLiveVersion(ServiceWorkerVersion* version);
  void RemoveLiveVersion(int64 registration_id);

  std::vector<ServiceWorkerRegistrationInfo> GetAllLiveRegistrationInfo();
  std::vector<ServiceWorkerVersionInfo> GetAllLiveVersionInfo();

  // Returns new context-local unique ID for ServiceWorkerHandle.
  int GetNewServiceWorkerHandleId();

  base::WeakPtr<ServiceWorkerContextCore> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  typedef std::map<int64, ServiceWorkerRegistration*> RegistrationsMap;
  typedef std::map<int64, ServiceWorkerVersion*> VersionMap;

  ProviderMap* GetProviderMapForProcess(int process_id) {
    return providers_.Lookup(process_id);
  }

  void RegistrationComplete(const GURL& pattern,
                            const RegistrationCallback& callback,
                            ServiceWorkerStatusCode status,
                            ServiceWorkerRegistration* registration,
                            ServiceWorkerVersion* version);

  void UnregistrationComplete(const GURL& pattern,
                              const UnregistrationCallback& callback,
                              ServiceWorkerStatusCode status);

  base::WeakPtrFactory<ServiceWorkerContextCore> weak_factory_;
  // It's safe to store a raw pointer instead of a scoped_refptr to |wrapper_|
  // because the Wrapper::Shutdown call that hops threads to destroy |this| uses
  // Bind() to hold a reference to |wrapper_| until |this| is fully destroyed.
  ServiceWorkerContextWrapper* wrapper_;
  ProcessToProviderMap providers_;
  scoped_ptr<ServiceWorkerStorage> storage_;
  scoped_refptr<EmbeddedWorkerRegistry> embedded_worker_registry_;
  scoped_ptr<ServiceWorkerJobCoordinator> job_coordinator_;
  std::map<int64, ServiceWorkerRegistration*> live_registrations_;
  std::map<int64, ServiceWorkerVersion*> live_versions_;
  int next_handle_id_;
  scoped_refptr<ObserverListThreadSafe<ServiceWorkerContextObserver> >
      observer_list_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerContextCore);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CORE_H_
