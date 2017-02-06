// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_context_core.h"

#include <limits>
#include <set>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_context_observer.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_database_task_manager.h"
#include "content/browser/service_worker/service_worker_info.h"
#include "content/browser/service_worker/service_worker_job_coordinator.h"
#include "content/browser/service_worker/service_worker_process_manager.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_register_job.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_thread.h"
#include "ipc/ipc_message.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "url/gurl.h"

namespace content {
namespace {

void SuccessCollectorCallback(const base::Closure& done_closure,
                              bool* overall_success,
                              ServiceWorkerStatusCode status) {
  if (status != ServiceWorkerStatusCode::SERVICE_WORKER_OK) {
    *overall_success = false;
  }
  done_closure.Run();
}

void SuccessReportingCallback(
    const bool* success,
    const ServiceWorkerContextCore::UnregistrationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  bool result = *success;
  callback.Run(result ? ServiceWorkerStatusCode::SERVICE_WORKER_OK
                      : ServiceWorkerStatusCode::SERVICE_WORKER_ERROR_FAILED);
}

bool IsSameOriginClientProviderHost(const GURL& origin,
                                    ServiceWorkerProviderHost* host) {
  return host->IsProviderForClient() &&
         host->document_url().GetOrigin() == origin;
}

bool IsSameOriginWindowProviderHost(const GURL& origin,
                                    ServiceWorkerProviderHost* host) {
  return host->provider_type() ==
             ServiceWorkerProviderType::SERVICE_WORKER_PROVIDER_FOR_WINDOW &&
         host->document_url().GetOrigin() == origin;
}

// Returns true if any of the frames specified by |frames| is a top-level frame.
// |frames| is a vector of (render process id, frame id) pairs.
bool FrameListContainsMainFrameOnUI(
    std::unique_ptr<std::vector<std::pair<int, int>>> frames) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (const auto& frame : *frames) {
    RenderFrameHostImpl* render_frame_host =
        RenderFrameHostImpl::FromID(frame.first, frame.second);
    if (!render_frame_host)
      continue;
    if (!render_frame_host->GetParent())
      return true;
  }
  return false;
}

class ClearAllServiceWorkersHelper
    : public base::RefCounted<ClearAllServiceWorkersHelper> {
 public:
  explicit ClearAllServiceWorkersHelper(const base::Closure& callback)
      : callback_(callback) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
  }

  void OnResult(ServiceWorkerStatusCode) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    // We do nothing in this method. We use this class to wait for all callbacks
    // to be called using the refcount.
  }

  void DidGetAllRegistrations(
      const base::WeakPtr<ServiceWorkerContextCore>& context,
      ServiceWorkerStatusCode status,
      const std::vector<ServiceWorkerRegistrationInfo>& registrations) {
    if (!context || status != SERVICE_WORKER_OK)
      return;
    // Make a copy of live versions map because StopWorker() removes the version
    // from it when we were starting up and don't have a process yet.
    const std::map<int64_t, ServiceWorkerVersion*> live_versions_copy =
        context->GetLiveVersions();
    for (const auto& version_itr : live_versions_copy) {
      ServiceWorkerVersion* version(version_itr.second);
      if (version->running_status() == EmbeddedWorkerStatus::STARTING ||
          version->running_status() == EmbeddedWorkerStatus::RUNNING) {
        version->StopWorker(
            base::Bind(&ClearAllServiceWorkersHelper::OnResult, this));
      }
    }
    for (const auto& registration_info : registrations) {
      context->UnregisterServiceWorker(
          registration_info.pattern,
          base::Bind(&ClearAllServiceWorkersHelper::OnResult, this));
    }
  }

 private:
  friend class base::RefCounted<ClearAllServiceWorkersHelper>;
  ~ClearAllServiceWorkersHelper() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, callback_);
  }

  const base::Closure callback_;
  DISALLOW_COPY_AND_ASSIGN(ClearAllServiceWorkersHelper);
};

}  // namespace

const base::FilePath::CharType
    ServiceWorkerContextCore::kServiceWorkerDirectory[] =
        FILE_PATH_LITERAL("Service Worker");

ServiceWorkerContextCore::ProviderHostIterator::~ProviderHostIterator() {}

ServiceWorkerProviderHost*
ServiceWorkerContextCore::ProviderHostIterator::GetProviderHost() {
  DCHECK(!IsAtEnd());
  return provider_host_iterator_->GetCurrentValue();
}

void ServiceWorkerContextCore::ProviderHostIterator::Advance() {
  DCHECK(!IsAtEnd());
  DCHECK(!provider_host_iterator_->IsAtEnd());
  DCHECK(!process_iterator_->IsAtEnd());

  // Advance the inner iterator. If an element is reached, we're done.
  provider_host_iterator_->Advance();
  if (ForwardUntilMatchingProviderHost())
    return;

  // Advance the outer iterator until an element is reached, or end is hit.
  while (true) {
    process_iterator_->Advance();
    if (process_iterator_->IsAtEnd())
      return;
    ProviderMap* provider_map = process_iterator_->GetCurrentValue();
    provider_host_iterator_.reset(new ProviderMap::iterator(provider_map));
    if (ForwardUntilMatchingProviderHost())
      return;
  }
}

bool ServiceWorkerContextCore::ProviderHostIterator::IsAtEnd() {
  return process_iterator_->IsAtEnd() &&
         (!provider_host_iterator_ || provider_host_iterator_->IsAtEnd());
}

ServiceWorkerContextCore::ProviderHostIterator::ProviderHostIterator(
    ProcessToProviderMap* map,
    const ProviderHostPredicate& predicate)
    : map_(map), predicate_(predicate) {
  DCHECK(map);
  Initialize();
}

void ServiceWorkerContextCore::ProviderHostIterator::Initialize() {
  process_iterator_.reset(new ProcessToProviderMap::iterator(map_));
  // Advance to the first element.
  while (!process_iterator_->IsAtEnd()) {
    ProviderMap* provider_map = process_iterator_->GetCurrentValue();
    provider_host_iterator_.reset(new ProviderMap::iterator(provider_map));
    if (ForwardUntilMatchingProviderHost())
      return;
    process_iterator_->Advance();
  }
}

bool ServiceWorkerContextCore::ProviderHostIterator::
    ForwardUntilMatchingProviderHost() {
  while (!provider_host_iterator_->IsAtEnd()) {
    if (predicate_.is_null() || predicate_.Run(GetProviderHost()))
      return true;
    provider_host_iterator_->Advance();
  }
  return false;
}

ServiceWorkerContextCore::ServiceWorkerContextCore(
    const base::FilePath& path,
    std::unique_ptr<ServiceWorkerDatabaseTaskManager> database_task_manager,
    const scoped_refptr<base::SingleThreadTaskRunner>& disk_cache_thread,
    storage::QuotaManagerProxy* quota_manager_proxy,
    storage::SpecialStoragePolicy* special_storage_policy,
    base::ObserverListThreadSafe<ServiceWorkerContextObserver>* observer_list,
    ServiceWorkerContextWrapper* wrapper)
    : wrapper_(wrapper),
      providers_(new ProcessToProviderMap),
      provider_by_uuid_(new ProviderByClientUUIDMap),
      force_update_on_page_load_(false),
      next_handle_id_(0),
      next_registration_handle_id_(0),
      was_service_worker_registered_(false),
      observer_list_(observer_list),
      weak_factory_(this) {
  // These get a WeakPtr from weak_factory_, so must be set after weak_factory_
  // is initialized.
  storage_ = ServiceWorkerStorage::Create(
      path, AsWeakPtr(), std::move(database_task_manager), disk_cache_thread,
      quota_manager_proxy, special_storage_policy);
  embedded_worker_registry_ = EmbeddedWorkerRegistry::Create(AsWeakPtr());
  job_coordinator_.reset(new ServiceWorkerJobCoordinator(AsWeakPtr()));
}

ServiceWorkerContextCore::ServiceWorkerContextCore(
    ServiceWorkerContextCore* old_context,
    ServiceWorkerContextWrapper* wrapper)
    : wrapper_(wrapper),
      providers_(old_context->providers_.release()),
      provider_by_uuid_(old_context->provider_by_uuid_.release()),
      next_handle_id_(old_context->next_handle_id_),
      next_registration_handle_id_(old_context->next_registration_handle_id_),
      was_service_worker_registered_(
          old_context->was_service_worker_registered_),
      observer_list_(old_context->observer_list_),
      weak_factory_(this) {
  // These get a WeakPtr from weak_factory_, so must be set after weak_factory_
  // is initialized.
  storage_ = ServiceWorkerStorage::Create(AsWeakPtr(), old_context->storage());
  embedded_worker_registry_ = EmbeddedWorkerRegistry::Create(
      AsWeakPtr(),
      old_context->embedded_worker_registry());
  job_coordinator_.reset(new ServiceWorkerJobCoordinator(AsWeakPtr()));
}

ServiceWorkerContextCore::~ServiceWorkerContextCore() {
  DCHECK(storage_);
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
    std::unique_ptr<ServiceWorkerProviderHost> host) {
  ServiceWorkerProviderHost* host_ptr = host.release();   // we take ownership
  ProviderMap* map = GetProviderMapForProcess(host_ptr->process_id());
  if (!map) {
    map = new ProviderMap;
    providers_->AddWithID(map, host_ptr->process_id());
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
  if (providers_->Lookup(process_id))
    providers_->Remove(process_id);
}

std::unique_ptr<ServiceWorkerContextCore::ProviderHostIterator>
ServiceWorkerContextCore::GetProviderHostIterator() {
  return base::WrapUnique(new ProviderHostIterator(
      providers_.get(), ProviderHostIterator::ProviderHostPredicate()));
}

std::unique_ptr<ServiceWorkerContextCore::ProviderHostIterator>
ServiceWorkerContextCore::GetClientProviderHostIterator(const GURL& origin) {
  return base::WrapUnique(new ProviderHostIterator(
      providers_.get(), base::Bind(IsSameOriginClientProviderHost, origin)));
}

void ServiceWorkerContextCore::HasMainFrameProviderHost(
    const GURL& origin,
    const BoolCallback& callback) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ProviderHostIterator provider_host_iterator(
      providers_.get(), base::Bind(IsSameOriginWindowProviderHost, origin));

  if (provider_host_iterator.IsAtEnd()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback, false));
    return;
  }

  std::unique_ptr<std::vector<std::pair<int, int>>> render_frames(
      new std::vector<std::pair<int, int>>());

  while (!provider_host_iterator.IsAtEnd()) {
    ServiceWorkerProviderHost* provider_host =
        provider_host_iterator.GetProviderHost();
    render_frames->push_back(
        std::make_pair(provider_host->process_id(), provider_host->frame_id()));
    provider_host_iterator.Advance();
  }

  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&FrameListContainsMainFrameOnUI,
                 base::Passed(std::move(render_frames))),
      callback);
}

void ServiceWorkerContextCore::RegisterProviderHostByClientID(
    const std::string& client_uuid,
    ServiceWorkerProviderHost* provider_host) {
  DCHECK(!ContainsKey(*provider_by_uuid_, client_uuid));
  (*provider_by_uuid_)[client_uuid] = provider_host;
}

void ServiceWorkerContextCore::UnregisterProviderHostByClientID(
    const std::string& client_uuid) {
  DCHECK(ContainsKey(*provider_by_uuid_, client_uuid));
  provider_by_uuid_->erase(client_uuid);
}

ServiceWorkerProviderHost* ServiceWorkerContextCore::GetProviderHostByClientID(
    const std::string& client_uuid) {
  auto found = provider_by_uuid_->find(client_uuid);
  if (found == provider_by_uuid_->end())
    return nullptr;
  return found->second;
}

void ServiceWorkerContextCore::RegisterServiceWorker(
    const GURL& pattern,
    const GURL& script_url,
    ServiceWorkerProviderHost* provider_host,
    const RegistrationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  was_service_worker_registered_ = true;
  job_coordinator_->Register(
      pattern,
      script_url,
      provider_host,
      base::Bind(&ServiceWorkerContextCore::RegistrationComplete,
                 AsWeakPtr(),
                 pattern,
                 callback));
}

void ServiceWorkerContextCore::UpdateServiceWorker(
    ServiceWorkerRegistration* registration,
    bool force_bypass_cache) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  job_coordinator_->Update(registration, force_bypass_cache);
}

void ServiceWorkerContextCore::UpdateServiceWorker(
    ServiceWorkerRegistration* registration,
    bool force_bypass_cache,
    bool skip_script_comparison,
    ServiceWorkerProviderHost* provider_host,
    const UpdateCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  job_coordinator_->Update(registration, force_bypass_cache,
                           skip_script_comparison, provider_host,
                           base::Bind(&ServiceWorkerContextCore::UpdateComplete,
                                      AsWeakPtr(), callback));
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

void ServiceWorkerContextCore::UnregisterServiceWorkers(
    const GURL& origin,
    const UnregistrationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  storage()->GetAllRegistrationsInfos(base::Bind(
      &ServiceWorkerContextCore::DidGetAllRegistrationsForUnregisterForOrigin,
      AsWeakPtr(), callback, origin));
}

void ServiceWorkerContextCore::DidGetAllRegistrationsForUnregisterForOrigin(
    const UnregistrationCallback& result,
    const GURL& origin,
    ServiceWorkerStatusCode status,
    const std::vector<ServiceWorkerRegistrationInfo>& registrations) {
  if (status != SERVICE_WORKER_OK) {
    result.Run(status);
    return;
  }
  std::set<GURL> scopes;
  for (const auto& registration_info : registrations) {
    if (origin == registration_info.pattern.GetOrigin()) {
      scopes.insert(registration_info.pattern);
    }
  }
  bool* overall_success = new bool(true);
  base::Closure barrier = base::BarrierClosure(
      scopes.size(),
      base::Bind(
          &SuccessReportingCallback, base::Owned(overall_success), result));

  for (const GURL& scope : scopes) {
    UnregisterServiceWorker(
        scope, base::Bind(&SuccessCollectorCallback, barrier, overall_success));
  }
}

void ServiceWorkerContextCore::RegistrationComplete(
    const GURL& pattern,
    const ServiceWorkerContextCore::RegistrationCallback& callback,
    ServiceWorkerStatusCode status,
    const std::string& status_message,
    ServiceWorkerRegistration* registration) {
  if (status != SERVICE_WORKER_OK) {
    DCHECK(!registration);
    callback.Run(status, status_message, kInvalidServiceWorkerRegistrationId);
    return;
  }

  DCHECK(registration);
  callback.Run(status, status_message, registration->id());
  // TODO(falken): At this point the registration promise is resolved, but we
  // haven't persisted anything to storage yet. So we should either call
  // OnRegistrationStored somewhere else or change its name.
  if (observer_list_.get()) {
    observer_list_->Notify(FROM_HERE,
                           &ServiceWorkerContextObserver::OnRegistrationStored,
                           registration->id(), pattern);
  }
}

void ServiceWorkerContextCore::UpdateComplete(
    const ServiceWorkerContextCore::UpdateCallback& callback,
    ServiceWorkerStatusCode status,
    const std::string& status_message,
    ServiceWorkerRegistration* registration) {
  if (status != SERVICE_WORKER_OK) {
    DCHECK(!registration);
    callback.Run(status, status_message, kInvalidServiceWorkerRegistrationId);
    return;
  }

  DCHECK(registration);
  callback.Run(status, status_message, registration->id());
}

void ServiceWorkerContextCore::UnregistrationComplete(
    const GURL& pattern,
    const ServiceWorkerContextCore::UnregistrationCallback& callback,
    int64_t registration_id,
    ServiceWorkerStatusCode status) {
  callback.Run(status);
  if (status == SERVICE_WORKER_OK && observer_list_.get()) {
    observer_list_->Notify(FROM_HERE,
                           &ServiceWorkerContextObserver::OnRegistrationDeleted,
                           registration_id, pattern);
  }
}

ServiceWorkerRegistration* ServiceWorkerContextCore::GetLiveRegistration(
    int64_t id) {
  RegistrationsMap::iterator it = live_registrations_.find(id);
  return (it != live_registrations_.end()) ? it->second : NULL;
}

void ServiceWorkerContextCore::AddLiveRegistration(
    ServiceWorkerRegistration* registration) {
  DCHECK(!GetLiveRegistration(registration->id()));
  live_registrations_[registration->id()] = registration;
  if (observer_list_.get()) {
    observer_list_->Notify(FROM_HERE,
                           &ServiceWorkerContextObserver::OnNewLiveRegistration,
                           registration->id(), registration->pattern());
  }
}

void ServiceWorkerContextCore::RemoveLiveRegistration(int64_t id) {
  live_registrations_.erase(id);
}

ServiceWorkerVersion* ServiceWorkerContextCore::GetLiveVersion(int64_t id) {
  VersionMap::iterator it = live_versions_.find(id);
  return (it != live_versions_.end()) ? it->second : NULL;
}

// PlzNavigate
void ServiceWorkerContextCore::AddNavigationHandleCore(
    int service_worker_provider_id,
    ServiceWorkerNavigationHandleCore* handle) {
  auto result = navigation_handle_cores_map_.insert(
      std::pair<int, ServiceWorkerNavigationHandleCore*>(
          service_worker_provider_id, handle));
  DCHECK(result.second)
      << "Inserting a duplicate ServiceWorkerNavigationHandleCore";
}

// PlzNavigate
void ServiceWorkerContextCore::RemoveNavigationHandleCore(
    int service_worker_provider_id) {
  navigation_handle_cores_map_.erase(service_worker_provider_id);
}

// PlzNavigate
ServiceWorkerNavigationHandleCore*
ServiceWorkerContextCore::GetNavigationHandleCore(
    int service_worker_provider_id) {
  auto result = navigation_handle_cores_map_.find(service_worker_provider_id);
  if (result == navigation_handle_cores_map_.end())
    return nullptr;
  return result->second;
}

void ServiceWorkerContextCore::AddLiveVersion(ServiceWorkerVersion* version) {
  // TODO(horo): If we will see crashes here, we have to find the root cause of
  // the version ID conflict. Otherwise change CHECK to DCHECK.
  CHECK(!GetLiveVersion(version->version_id()));
  live_versions_[version->version_id()] = version;
  version->AddListener(this);
  if (observer_list_.get()) {
    observer_list_->Notify(FROM_HERE,
                           &ServiceWorkerContextObserver::OnNewLiveVersion,
                           version->version_id(), version->registration_id(),
                           version->script_url());
  }
}

void ServiceWorkerContextCore::RemoveLiveVersion(int64_t id) {
  live_versions_.erase(id);
}

std::vector<ServiceWorkerRegistrationInfo>
ServiceWorkerContextCore::GetAllLiveRegistrationInfo() {
  std::vector<ServiceWorkerRegistrationInfo> infos;
  for (std::map<int64_t, ServiceWorkerRegistration*>::const_iterator iter =
           live_registrations_.begin();
       iter != live_registrations_.end(); ++iter) {
    infos.push_back(iter->second->GetInfo());
  }
  return infos;
}

std::vector<ServiceWorkerVersionInfo>
ServiceWorkerContextCore::GetAllLiveVersionInfo() {
  std::vector<ServiceWorkerVersionInfo> infos;
  for (std::map<int64_t, ServiceWorkerVersion*>::const_iterator iter =
           live_versions_.begin();
       iter != live_versions_.end(); ++iter) {
    infos.push_back(iter->second->GetInfo());
  }
  return infos;
}

void ServiceWorkerContextCore::ProtectVersion(
    const scoped_refptr<ServiceWorkerVersion>& version) {
  DCHECK(protected_versions_.find(version->version_id()) ==
         protected_versions_.end());
  protected_versions_[version->version_id()] = version;
}

void ServiceWorkerContextCore::UnprotectVersion(int64_t version_id) {
  DCHECK(protected_versions_.find(version_id) != protected_versions_.end());
  protected_versions_.erase(version_id);
}

int ServiceWorkerContextCore::GetNewServiceWorkerHandleId() {
  return next_handle_id_++;
}

int ServiceWorkerContextCore::GetNewRegistrationHandleId() {
  return next_registration_handle_id_++;
}

void ServiceWorkerContextCore::ScheduleDeleteAndStartOver() const {
  storage_->Disable();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&ServiceWorkerContextWrapper::DeleteAndStartOver, wrapper_));
}

void ServiceWorkerContextCore::DeleteAndStartOver(
    const StatusCallback& callback) {
  job_coordinator_->AbortAll();
  storage_->DeleteAndStartOver(callback);
}

std::unique_ptr<ServiceWorkerProviderHost>
ServiceWorkerContextCore::TransferProviderHostOut(int process_id,
                                                  int provider_id) {
  ProviderMap* map = GetProviderMapForProcess(process_id);
  ServiceWorkerProviderHost* transferee = map->Lookup(provider_id);
  ServiceWorkerProviderHost* replacement = new ServiceWorkerProviderHost(
      process_id, transferee->frame_id(), provider_id,
      transferee->provider_type(),
      transferee->is_parent_frame_secure()
          ? ServiceWorkerProviderHost::FrameSecurityLevel::SECURE
          : ServiceWorkerProviderHost::FrameSecurityLevel::INSECURE,
      AsWeakPtr(), transferee->dispatcher_host());
  map->Replace(provider_id, replacement);
  transferee->PrepareForCrossSiteTransfer();
  return base::WrapUnique(transferee);
}

void ServiceWorkerContextCore::TransferProviderHostIn(
    int new_process_id,
    int new_provider_id,
    std::unique_ptr<ServiceWorkerProviderHost> transferee) {
  ProviderMap* map = GetProviderMapForProcess(new_process_id);
  ServiceWorkerProviderHost* temp = map->Lookup(new_provider_id);
  if (!temp)
    return;

  DCHECK(temp->document_url().is_empty());
  transferee->CompleteCrossSiteTransfer(new_process_id,
                                        temp->frame_id(),
                                        new_provider_id,
                                        temp->provider_type(),
                                        temp->dispatcher_host());
  map->Replace(new_provider_id, transferee.release());
  delete temp;
}

void ServiceWorkerContextCore::ClearAllServiceWorkersForTest(
    const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // |callback| will be called in the destructor of |helper| on the UI thread.
  scoped_refptr<ClearAllServiceWorkersHelper> helper(
      new ClearAllServiceWorkersHelper(callback));
  if (!was_service_worker_registered_)
    return;
  was_service_worker_registered_ = false;
  storage()->GetAllRegistrationsInfos(
      base::Bind(&ClearAllServiceWorkersHelper::DidGetAllRegistrations, helper,
                 AsWeakPtr()));
}

void ServiceWorkerContextCore::CheckHasServiceWorker(
    const GURL& url,
    const GURL& other_url,
    const ServiceWorkerContext::CheckHasServiceWorkerCallback callback) {
  storage()->FindRegistrationForDocument(
      url, base::Bind(&ServiceWorkerContextCore::
                          DidFindRegistrationForCheckHasServiceWorker,
                      AsWeakPtr(), other_url, callback));
}

void ServiceWorkerContextCore::UpdateVersionFailureCount(
    int64_t version_id,
    ServiceWorkerStatusCode status) {
  // Don't count these, they aren't start worker failures.
  if (status == SERVICE_WORKER_ERROR_DISALLOWED)
    return;

  auto it = failure_counts_.find(version_id);
  if (it != failure_counts_.end()) {
    ServiceWorkerMetrics::RecordStartStatusAfterFailure(it->second.count,
                                                        status);
  }

  if (status == SERVICE_WORKER_OK) {
    if (it != failure_counts_.end())
      failure_counts_.erase(it);
    return;
  }

  if (it != failure_counts_.end()) {
    FailureInfo& info = it->second;
    DCHECK_GT(info.count, 0);
    if (info.count < std::numeric_limits<int>::max()) {
      ++info.count;
      info.last_failure = status;
    }
  } else {
    FailureInfo info;
    info.count = 1;
    info.last_failure = status;
    failure_counts_[version_id] = info;
  }
}

int ServiceWorkerContextCore::GetVersionFailureCount(int64_t version_id) {
  auto it = failure_counts_.find(version_id);
  if (it == failure_counts_.end())
    return 0;
  return it->second.count;
}

void ServiceWorkerContextCore::OnRunningStateChanged(
    ServiceWorkerVersion* version) {
  if (!observer_list_)
    return;
  observer_list_->Notify(FROM_HERE,
                         &ServiceWorkerContextObserver::OnRunningStateChanged,
                         version->version_id(), version->running_status());
}

void ServiceWorkerContextCore::OnVersionStateChanged(
    ServiceWorkerVersion* version) {
  if (!observer_list_)
    return;
  observer_list_->Notify(FROM_HERE,
                         &ServiceWorkerContextObserver::OnVersionStateChanged,
                         version->version_id(), version->status());
}

void ServiceWorkerContextCore::OnMainScriptHttpResponseInfoSet(
    ServiceWorkerVersion* version) {
  if (!observer_list_)
    return;
  const net::HttpResponseInfo* info = version->GetMainScriptHttpResponseInfo();
  DCHECK(info);
  base::Time lastModified;
  if (info->headers)
    info->headers->GetLastModifiedValue(&lastModified);
  observer_list_->Notify(
      FROM_HERE, &ServiceWorkerContextObserver::OnMainScriptHttpResponseInfoSet,
      version->version_id(), info->response_time, lastModified);
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
      FROM_HERE, &ServiceWorkerContextObserver::OnErrorReported,
      version->version_id(), version->embedded_worker()->process_id(),
      version->embedded_worker()->thread_id(),
      ServiceWorkerContextObserver::ErrorInfo(error_message, line_number,
                                              column_number, source_url));
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
      FROM_HERE, &ServiceWorkerContextObserver::OnReportConsoleMessage,
      version->version_id(), version->embedded_worker()->process_id(),
      version->embedded_worker()->thread_id(),
      ServiceWorkerContextObserver::ConsoleMessage(
          source_identifier, message_level, message, line_number, source_url));
}

void ServiceWorkerContextCore::OnControlleeAdded(
    ServiceWorkerVersion* version,
    ServiceWorkerProviderHost* provider_host) {
  if (!observer_list_)
    return;
  observer_list_->Notify(FROM_HERE,
                         &ServiceWorkerContextObserver::OnControlleeAdded,
                         version->version_id(), provider_host->client_uuid(),
                         provider_host->process_id(), provider_host->route_id(),
                         provider_host->provider_type());
}

void ServiceWorkerContextCore::OnControlleeRemoved(
    ServiceWorkerVersion* version,
    ServiceWorkerProviderHost* provider_host) {
  if (!observer_list_)
    return;
  observer_list_->Notify(FROM_HERE,
                         &ServiceWorkerContextObserver::OnControlleeRemoved,
                         version->version_id(), provider_host->client_uuid());
}

ServiceWorkerProcessManager* ServiceWorkerContextCore::process_manager() {
  return wrapper_->process_manager();
}

void ServiceWorkerContextCore::DidFindRegistrationForCheckHasServiceWorker(
    const GURL& other_url,
    const ServiceWorkerContext::CheckHasServiceWorkerCallback callback,
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  if (status != SERVICE_WORKER_OK) {
    callback.Run(false);
    return;
  }

  if (!ServiceWorkerUtils::ScopeMatches(registration->pattern(), other_url)) {
    callback.Run(false);
    return;
  }

  if (registration->is_uninstalling() || registration->is_uninstalled()) {
    callback.Run(false);
    return;
  }

  if (!registration->active_version() && !registration->waiting_version()) {
    registration->RegisterRegistrationFinishedCallback(
        base::Bind(&ServiceWorkerContextCore::
                       OnRegistrationFinishedForCheckHasServiceWorker,
                   AsWeakPtr(), callback, registration));
    return;
  }

  callback.Run(true);
}

void ServiceWorkerContextCore::OnRegistrationFinishedForCheckHasServiceWorker(
    const ServiceWorkerContext::CheckHasServiceWorkerCallback callback,
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  callback.Run(registration->active_version() ||
               registration->waiting_version());
}

}  // namespace content
