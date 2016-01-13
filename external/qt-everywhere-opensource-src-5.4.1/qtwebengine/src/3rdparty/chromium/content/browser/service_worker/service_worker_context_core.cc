// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_context_core.h"

#include "base/files/file_path.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/strings/string_util.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/service_worker_context_observer.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_info.h"
#include "content/browser/service_worker/service_worker_job_coordinator.h"
#include "content/browser/service_worker/service_worker_process_manager.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_register_job.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace content {

ServiceWorkerContextCore::ProviderHostIterator::~ProviderHostIterator() {}

ServiceWorkerProviderHost*
ServiceWorkerContextCore::ProviderHostIterator::GetProviderHost() {
  DCHECK(!IsAtEnd());
  return provider_host_iterator_->GetCurrentValue();
}

void ServiceWorkerContextCore::ProviderHostIterator::Advance() {
  DCHECK(!IsAtEnd());
  DCHECK(!provider_host_iterator_->IsAtEnd());
  DCHECK(!provider_iterator_->IsAtEnd());

  // Advance the inner iterator. If an element is reached, we're done.
  provider_host_iterator_->Advance();
  if (!provider_host_iterator_->IsAtEnd())
    return;

  // Advance the outer iterator until an element is reached, or end is hit.
  while (true) {
    provider_iterator_->Advance();
    if (provider_iterator_->IsAtEnd())
      return;
    ProviderMap* provider_map = provider_iterator_->GetCurrentValue();
    provider_host_iterator_.reset(new ProviderMap::iterator(provider_map));
    if (!provider_host_iterator_->IsAtEnd())
      return;
  }
}

bool ServiceWorkerContextCore::ProviderHostIterator::IsAtEnd() {
  return provider_iterator_->IsAtEnd() &&
         (!provider_host_iterator_ || provider_host_iterator_->IsAtEnd());
}

ServiceWorkerContextCore::ProviderHostIterator::ProviderHostIterator(
    ProcessToProviderMap* map)
    : map_(map) {
  DCHECK(map);
  Initialize();
}

void ServiceWorkerContextCore::ProviderHostIterator::Initialize() {
  provider_iterator_.reset(new ProcessToProviderMap::iterator(map_));
  // Advance to the first element.
  while (!provider_iterator_->IsAtEnd()) {
    ProviderMap* provider_map = provider_iterator_->GetCurrentValue();
    provider_host_iterator_.reset(new ProviderMap::iterator(provider_map));
    if (!provider_host_iterator_->IsAtEnd())
      return;
    provider_iterator_->Advance();
  }
}

ServiceWorkerContextCore::ServiceWorkerContextCore(
    const base::FilePath& path,
    base::SequencedTaskRunner* database_task_runner,
    base::MessageLoopProxy* disk_cache_thread,
    quota::QuotaManagerProxy* quota_manager_proxy,
    ObserverListThreadSafe<ServiceWorkerContextObserver>* observer_list,
    ServiceWorkerContextWrapper* wrapper)
    : weak_factory_(this),
      wrapper_(wrapper),
      storage_(new ServiceWorkerStorage(path,
                                        AsWeakPtr(),
                                        database_task_runner,
                                        disk_cache_thread,
                                        quota_manager_proxy)),
      embedded_worker_registry_(new EmbeddedWorkerRegistry(AsWeakPtr())),
      job_coordinator_(new ServiceWorkerJobCoordinator(AsWeakPtr())),
      next_handle_id_(0),
      observer_list_(observer_list) {
}

ServiceWorkerContextCore::~ServiceWorkerContextCore() {
  for (VersionMap::iterator it = live_versions_.begin();
       it != live_versions_.end();
       ++it) {
    it->second->RemoveListener(this);
  }
  weak_factory_.InvalidateWeakPtrs();
}

ServiceWorkerProviderHost* ServiceWorkerContextCore::GetProviderHost(
    int process_id, int provider_id) {
  ProviderMap* map = GetProviderMapForProcess(process_id);
  if (!map)
    return NULL;
  return map->Lookup(provider_id);
}

void ServiceWorkerContextCore::AddProviderHost(
    scoped_ptr<ServiceWorkerProviderHost> host) {
  ServiceWorkerProviderHost* host_ptr = host.release();   // we take ownership
  ProviderMap* map = GetProviderMapForProcess(host_ptr->process_id());
  if (!map) {
    map = new ProviderMap;
    providers_.AddWithID(map, host_ptr->process_id());
  }
  map->AddWithID(host_ptr, host_ptr->provider_id());
}

void ServiceWorkerContextCore::RemoveProviderHost(
    int process_id, int provider_id) {
  ProviderMap* map = GetProviderMapForProcess(process_id);
  DCHECK(map);
  map->Remove(provider_id);
}

void ServiceWorkerContextCore::RemoveAllProviderHostsForProcess(
    int process_id) {
  if (providers_.Lookup(process_id))
    providers_.Remove(process_id);
}

scoped_ptr<ServiceWorkerContextCore::ProviderHostIterator>
ServiceWorkerContextCore::GetProviderHostIterator() {
  return make_scoped_ptr(new ProviderHostIterator(&providers_));
}

void ServiceWorkerContextCore::RegisterServiceWorker(
    const GURL& pattern,
    const GURL& script_url,
    int source_process_id,
    ServiceWorkerProviderHost* provider_host,
    const RegistrationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // TODO(kinuko): Wire the provider_host so that we can tell which document
  // is calling .register.

  job_coordinator_->Register(
      pattern,
      script_url,
      source_process_id,
      base::Bind(&ServiceWorkerContextCore::RegistrationComplete,
                 AsWeakPtr(),
                 pattern,
                 callback));
}

void ServiceWorkerContextCore::UnregisterServiceWorker(
    const GURL& pattern,
    const UnregistrationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  job_coordinator_->Unregister(
      pattern,
      base::Bind(&ServiceWorkerContextCore::UnregistrationComplete,
                 AsWeakPtr(),
                 pattern,
                 callback));
}

void ServiceWorkerContextCore::RegistrationComplete(
    const GURL& pattern,
    const ServiceWorkerContextCore::RegistrationCallback& callback,
    ServiceWorkerStatusCode status,
    ServiceWorkerRegistration* registration,
    ServiceWorkerVersion* version) {
  if (status != SERVICE_WORKER_OK) {
    DCHECK(!version);
    callback.Run(status,
                 kInvalidServiceWorkerRegistrationId,
                 kInvalidServiceWorkerVersionId);
    return;
  }

  DCHECK(version);
  DCHECK_EQ(version->registration_id(), registration->id());
  callback.Run(status,
               registration->id(),
               version->version_id());
  if (observer_list_) {
    observer_list_->Notify(&ServiceWorkerContextObserver::OnRegistrationStored,
                           pattern);
  }
}

void ServiceWorkerContextCore::UnregistrationComplete(
    const GURL& pattern,
    const ServiceWorkerContextCore::UnregistrationCallback& callback,
    ServiceWorkerStatusCode status) {
  callback.Run(status);
  if (observer_list_) {
    observer_list_->Notify(&ServiceWorkerContextObserver::OnRegistrationDeleted,
                           pattern);
  }
}

ServiceWorkerRegistration* ServiceWorkerContextCore::GetLiveRegistration(
    int64 id) {
  RegistrationsMap::iterator it = live_registrations_.find(id);
  return (it != live_registrations_.end()) ? it->second : NULL;
}

void ServiceWorkerContextCore::AddLiveRegistration(
    ServiceWorkerRegistration* registration) {
  DCHECK(!GetLiveRegistration(registration->id()));
  live_registrations_[registration->id()] = registration;
}

void ServiceWorkerContextCore::RemoveLiveRegistration(int64 id) {
  live_registrations_.erase(id);
}

ServiceWorkerVersion* ServiceWorkerContextCore::GetLiveVersion(
    int64 id) {
  VersionMap::iterator it = live_versions_.find(id);
  return (it != live_versions_.end()) ? it->second : NULL;
}

void ServiceWorkerContextCore::AddLiveVersion(ServiceWorkerVersion* version) {
  DCHECK(!GetLiveVersion(version->version_id()));
  live_versions_[version->version_id()] = version;
  version->AddListener(this);
}

void ServiceWorkerContextCore::RemoveLiveVersion(int64 id) {
  live_versions_.erase(id);
}

std::vector<ServiceWorkerRegistrationInfo>
ServiceWorkerContextCore::GetAllLiveRegistrationInfo() {
  std::vector<ServiceWorkerRegistrationInfo> infos;
  for (std::map<int64, ServiceWorkerRegistration*>::const_iterator iter =
           live_registrations_.begin();
       iter != live_registrations_.end();
       ++iter) {
    infos.push_back(iter->second->GetInfo());
  }
  return infos;
}

std::vector<ServiceWorkerVersionInfo>
ServiceWorkerContextCore::GetAllLiveVersionInfo() {
  std::vector<ServiceWorkerVersionInfo> infos;
  for (std::map<int64, ServiceWorkerVersion*>::const_iterator iter =
           live_versions_.begin();
       iter != live_versions_.end();
       ++iter) {
    infos.push_back(iter->second->GetInfo());
  }
  return infos;
}

int ServiceWorkerContextCore::GetNewServiceWorkerHandleId() {
  return next_handle_id_++;
}

void ServiceWorkerContextCore::OnWorkerStarted(ServiceWorkerVersion* version) {
  if (!observer_list_)
    return;
  observer_list_->Notify(&ServiceWorkerContextObserver::OnWorkerStarted,
                         version->version_id(),
                         version->embedded_worker()->process_id(),
                         version->embedded_worker()->thread_id());
}

void ServiceWorkerContextCore::OnWorkerStopped(ServiceWorkerVersion* version) {
  if (!observer_list_)
    return;
  observer_list_->Notify(&ServiceWorkerContextObserver::OnWorkerStopped,
                         version->version_id(),
                         version->embedded_worker()->process_id(),
                         version->embedded_worker()->thread_id());
}

void ServiceWorkerContextCore::OnVersionStateChanged(
    ServiceWorkerVersion* version) {
  if (!observer_list_)
    return;
  observer_list_->Notify(&ServiceWorkerContextObserver::OnVersionStateChanged,
                         version->version_id());
}

void ServiceWorkerContextCore::OnErrorReported(
    ServiceWorkerVersion* version,
    const base::string16& error_message,
    int line_number,
    int column_number,
    const GURL& source_url) {
  if (!observer_list_)
    return;
  observer_list_->Notify(
      &ServiceWorkerContextObserver::OnErrorReported,
      version->version_id(),
      version->embedded_worker()->process_id(),
      version->embedded_worker()->thread_id(),
      ServiceWorkerContextObserver::ErrorInfo(
          error_message, line_number, column_number, source_url));
}

void ServiceWorkerContextCore::OnReportConsoleMessage(
    ServiceWorkerVersion* version,
    int source_identifier,
    int message_level,
    const base::string16& message,
    int line_number,
    const GURL& source_url) {
  if (!observer_list_)
    return;
  observer_list_->Notify(
      &ServiceWorkerContextObserver::OnReportConsoleMessage,
      version->version_id(),
      version->embedded_worker()->process_id(),
      version->embedded_worker()->thread_id(),
      ServiceWorkerContextObserver::ConsoleMessage(
          source_identifier, message_level, message, line_number, source_url));
}

ServiceWorkerProcessManager* ServiceWorkerContextCore::process_manager() {
  return wrapper_->process_manager();
}

}  // namespace content
