// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_storage.h"

#include <string>

#include "base/bind_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_disk_cache.h"
#include "content/browser/service_worker/service_worker_histograms.h"
#include "content/browser/service_worker/service_worker_info.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "webkit/browser/quota/quota_manager_proxy.h"

namespace content {

namespace {

void RunSoon(const tracked_objects::Location& from_here,
             const base::Closure& closure) {
  base::MessageLoop::current()->PostTask(from_here, closure);
}

void CompleteFindNow(
    const scoped_refptr<ServiceWorkerRegistration>& registration,
    ServiceWorkerStatusCode status,
    const ServiceWorkerStorage::FindRegistrationCallback& callback) {
  callback.Run(status, registration);
}

void CompleteFindSoon(
    const tracked_objects::Location& from_here,
    const scoped_refptr<ServiceWorkerRegistration>& registration,
    ServiceWorkerStatusCode status,
    const ServiceWorkerStorage::FindRegistrationCallback& callback) {
  RunSoon(from_here, base::Bind(callback, status, registration));
}

const base::FilePath::CharType kServiceWorkerDirectory[] =
    FILE_PATH_LITERAL("Service Worker");
const base::FilePath::CharType kDatabaseName[] =
    FILE_PATH_LITERAL("Database");
const base::FilePath::CharType kDiskCacheName[] =
    FILE_PATH_LITERAL("Cache");

const int kMaxMemDiskCacheSize = 10 * 1024 * 1024;
const int kMaxDiskCacheSize = 250 * 1024 * 1024;

void EmptyCompletionCallback(int) {}

ServiceWorkerStatusCode DatabaseStatusToStatusCode(
    ServiceWorkerDatabase::Status status) {
  switch (status) {
    case ServiceWorkerDatabase::STATUS_OK:
      return SERVICE_WORKER_OK;
    case ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND:
      return SERVICE_WORKER_ERROR_NOT_FOUND;
    case ServiceWorkerDatabase::STATUS_ERROR_MAX:
      NOTREACHED();
    default:
      return SERVICE_WORKER_ERROR_FAILED;
  }
}

}  // namespace

ServiceWorkerStorage::InitialData::InitialData()
    : next_registration_id(kInvalidServiceWorkerRegistrationId),
      next_version_id(kInvalidServiceWorkerVersionId),
      next_resource_id(kInvalidServiceWorkerResourceId) {
}

ServiceWorkerStorage::InitialData::~InitialData() {
}

ServiceWorkerStorage::ServiceWorkerStorage(
    const base::FilePath& path,
    base::WeakPtr<ServiceWorkerContextCore> context,
    base::SequencedTaskRunner* database_task_runner,
    base::MessageLoopProxy* disk_cache_thread,
    quota::QuotaManagerProxy* quota_manager_proxy)
    : next_registration_id_(kInvalidServiceWorkerRegistrationId),
      next_version_id_(kInvalidServiceWorkerVersionId),
      next_resource_id_(kInvalidServiceWorkerResourceId),
      state_(UNINITIALIZED),
      context_(context),
      database_task_runner_(database_task_runner),
      disk_cache_thread_(disk_cache_thread),
      quota_manager_proxy_(quota_manager_proxy),
      is_purge_pending_(false),
      weak_factory_(this) {
  if (!path.empty())
    path_ = path.Append(kServiceWorkerDirectory);
  database_.reset(new ServiceWorkerDatabase(GetDatabasePath()));
}

ServiceWorkerStorage::~ServiceWorkerStorage() {
  weak_factory_.InvalidateWeakPtrs();
  database_task_runner_->DeleteSoon(FROM_HERE, database_.release());
}

void ServiceWorkerStorage::FindRegistrationForDocument(
    const GURL& document_url,
    const FindRegistrationCallback& callback) {
  DCHECK(!document_url.has_ref());
  if (!LazyInitialize(base::Bind(
          &ServiceWorkerStorage::FindRegistrationForDocument,
          weak_factory_.GetWeakPtr(), document_url, callback))) {
    if (state_ != INITIALIZING || !context_) {
      CompleteFindNow(scoped_refptr<ServiceWorkerRegistration>(),
                      SERVICE_WORKER_ERROR_FAILED, callback);
    }
    return;
  }
  DCHECK_EQ(INITIALIZED, state_);

  // See if there are any stored registrations for the origin.
  if (!ContainsKey(registered_origins_, document_url.GetOrigin())) {
    // Look for something currently being installed.
    scoped_refptr<ServiceWorkerRegistration> installing_registration =
        FindInstallingRegistrationForDocument(document_url);
    CompleteFindNow(
        installing_registration,
        installing_registration ?
            SERVICE_WORKER_OK : SERVICE_WORKER_ERROR_NOT_FOUND,
        callback);
    return;
  }

  database_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &FindForDocumentInDB,
          database_.get(),
          base::MessageLoopProxy::current(),
          document_url,
          base::Bind(&ServiceWorkerStorage::DidFindRegistrationForDocument,
                     weak_factory_.GetWeakPtr(), document_url, callback)));
}

void ServiceWorkerStorage::FindRegistrationForPattern(
    const GURL& scope,
    const FindRegistrationCallback& callback) {
  if (!LazyInitialize(base::Bind(
          &ServiceWorkerStorage::FindRegistrationForPattern,
          weak_factory_.GetWeakPtr(), scope, callback))) {
    if (state_ != INITIALIZING || !context_) {
      CompleteFindSoon(FROM_HERE, scoped_refptr<ServiceWorkerRegistration>(),
                       SERVICE_WORKER_ERROR_FAILED, callback);
    }
    return;
  }
  DCHECK_EQ(INITIALIZED, state_);

  // See if there are any stored registrations for the origin.
  if (!ContainsKey(registered_origins_, scope.GetOrigin())) {
    // Look for something currently being installed.
    scoped_refptr<ServiceWorkerRegistration> installing_registration =
        FindInstallingRegistrationForPattern(scope);
    CompleteFindSoon(
        FROM_HERE, installing_registration,
        installing_registration ?
            SERVICE_WORKER_OK : SERVICE_WORKER_ERROR_NOT_FOUND,
        callback);
    return;
  }

  database_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &FindForPatternInDB,
          database_.get(),
          base::MessageLoopProxy::current(),
          scope,
          base::Bind(&ServiceWorkerStorage::DidFindRegistrationForPattern,
                     weak_factory_.GetWeakPtr(), scope, callback)));
}

void ServiceWorkerStorage::FindRegistrationForId(
    int64 registration_id,
    const GURL& origin,
    const FindRegistrationCallback& callback) {
  if (!LazyInitialize(base::Bind(
          &ServiceWorkerStorage::FindRegistrationForId,
          weak_factory_.GetWeakPtr(), registration_id, origin, callback))) {
    if (state_ != INITIALIZING || !context_) {
      CompleteFindNow(scoped_refptr<ServiceWorkerRegistration>(),
                      SERVICE_WORKER_ERROR_FAILED, callback);
    }
    return;
  }
  DCHECK_EQ(INITIALIZED, state_);

  // See if there are any stored registrations for the origin.
  if (!ContainsKey(registered_origins_, origin)) {
    // Look for something currently being installed.
    scoped_refptr<ServiceWorkerRegistration> installing_registration =
        FindInstallingRegistrationForId(registration_id);
    CompleteFindNow(
        installing_registration,
        installing_registration ?
            SERVICE_WORKER_OK : SERVICE_WORKER_ERROR_NOT_FOUND,
        callback);
    return;
  }

  scoped_refptr<ServiceWorkerRegistration> registration =
      context_->GetLiveRegistration(registration_id);
  if (registration) {
    CompleteFindNow(registration, SERVICE_WORKER_OK, callback);
    return;
  }

  database_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&FindForIdInDB,
                 database_.get(),
                 base::MessageLoopProxy::current(),
                 registration_id, origin,
                 base::Bind(&ServiceWorkerStorage::DidFindRegistrationForId,
                            weak_factory_.GetWeakPtr(), callback)));
}

void ServiceWorkerStorage::GetAllRegistrations(
    const GetAllRegistrationInfosCallback& callback) {
  if (!LazyInitialize(base::Bind(
          &ServiceWorkerStorage::GetAllRegistrations,
          weak_factory_.GetWeakPtr(), callback))) {
    if (state_ != INITIALIZING || !context_) {
      RunSoon(FROM_HERE, base::Bind(
          callback, std::vector<ServiceWorkerRegistrationInfo>()));
    }
    return;
  }
  DCHECK_EQ(INITIALIZED, state_);

  RegistrationList* registrations = new RegistrationList;
  PostTaskAndReplyWithResult(
      database_task_runner_,
      FROM_HERE,
      base::Bind(&ServiceWorkerDatabase::GetAllRegistrations,
                 base::Unretained(database_.get()),
                 base::Unretained(registrations)),
      base::Bind(&ServiceWorkerStorage::DidGetAllRegistrations,
                 weak_factory_.GetWeakPtr(),
                 callback,
                 base::Owned(registrations)));
}

void ServiceWorkerStorage::StoreRegistration(
    ServiceWorkerRegistration* registration,
    ServiceWorkerVersion* version,
    const StatusCallback& callback) {
  DCHECK(registration);
  DCHECK(version);

  DCHECK(state_ == INITIALIZED || state_ == DISABLED);
  if (state_ != INITIALIZED || !context_) {
    RunSoon(FROM_HERE, base::Bind(callback, SERVICE_WORKER_ERROR_FAILED));
    return;
  }

  ServiceWorkerDatabase::RegistrationData data;
  data.registration_id = registration->id();
  data.scope = registration->pattern();
  data.script = registration->script_url();
  data.has_fetch_handler = true;
  data.version_id = version->version_id();
  data.last_update_check = base::Time::Now();
  data.is_active = false;  // initially stored in the waiting state

  ResourceList resources;
  version->script_cache_map()->GetResources(&resources);

  database_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&WriteRegistrationInDB,
                 database_.get(),
                 base::MessageLoopProxy::current(),
                 data, resources,
                 base::Bind(&ServiceWorkerStorage::DidStoreRegistration,
                            weak_factory_.GetWeakPtr(),
                            callback)));
}

void ServiceWorkerStorage::UpdateToActiveState(
    ServiceWorkerRegistration* registration,
    const StatusCallback& callback) {
  DCHECK(registration);

  DCHECK(state_ == INITIALIZED || state_ == DISABLED);
  if (state_ != INITIALIZED || !context_) {
    RunSoon(FROM_HERE, base::Bind(callback, SERVICE_WORKER_ERROR_FAILED));
    return;
  }

  PostTaskAndReplyWithResult(
      database_task_runner_,
      FROM_HERE,
      base::Bind(&ServiceWorkerDatabase::UpdateVersionToActive,
                 base::Unretained(database_.get()),
                 registration->id(),
                 registration->script_url().GetOrigin()),
      base::Bind(&ServiceWorkerStorage::DidUpdateToActiveState,
                 weak_factory_.GetWeakPtr(),
                 callback));
}

void ServiceWorkerStorage::DeleteRegistration(
    int64 registration_id,
    const GURL& origin,
    const StatusCallback& callback) {
  DCHECK(state_ == INITIALIZED || state_ == DISABLED);
  if (state_ != INITIALIZED || !context_) {
    RunSoon(FROM_HERE, base::Bind(callback, SERVICE_WORKER_ERROR_FAILED));
    return;
  }

  database_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DeleteRegistrationFromDB,
                 database_.get(),
                 base::MessageLoopProxy::current(),
                 registration_id, origin,
                 base::Bind(&ServiceWorkerStorage::DidDeleteRegistration,
                            weak_factory_.GetWeakPtr(), origin, callback)));

  // TODO(michaeln): Either its instance should also be
  // removed from liveregistrations map or the live object
  // should marked as deleted in some way and not 'findable'
  // thereafter.
}

scoped_ptr<ServiceWorkerResponseReader>
ServiceWorkerStorage::CreateResponseReader(int64 response_id) {
  return make_scoped_ptr(
      new ServiceWorkerResponseReader(response_id, disk_cache()));
}

scoped_ptr<ServiceWorkerResponseWriter>
ServiceWorkerStorage::CreateResponseWriter(int64 response_id) {
  return make_scoped_ptr(
      new ServiceWorkerResponseWriter(response_id, disk_cache()));
}

void ServiceWorkerStorage::StoreUncommittedReponseId(int64 id) {
  DCHECK_NE(kInvalidServiceWorkerResponseId, id);
  database_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(
          &ServiceWorkerDatabase::WriteUncommittedResourceIds),
          base::Unretained(database_.get()),
          std::set<int64>(&id, &id + 1)));
}

void ServiceWorkerStorage::DoomUncommittedResponse(int64 id) {
  DCHECK_NE(kInvalidServiceWorkerResponseId, id);
  database_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(
          &ServiceWorkerDatabase::PurgeUncommittedResourceIds),
          base::Unretained(database_.get()),
          std::set<int64>(&id, &id + 1)));
  StartPurgingResources(std::vector<int64>(1, id));
}

int64 ServiceWorkerStorage::NewRegistrationId() {
  if (state_ == DISABLED)
    return kInvalidServiceWorkerRegistrationId;
  DCHECK_EQ(INITIALIZED, state_);
  return next_registration_id_++;
}

int64 ServiceWorkerStorage::NewVersionId() {
  if (state_ == DISABLED)
    return kInvalidServiceWorkerVersionId;
  DCHECK_EQ(INITIALIZED, state_);
  return next_version_id_++;
}

int64 ServiceWorkerStorage::NewResourceId() {
  if (state_ == DISABLED)
    return kInvalidServiceWorkerResourceId;
  DCHECK_EQ(INITIALIZED, state_);
  return next_resource_id_++;
}

void ServiceWorkerStorage::NotifyInstallingRegistration(
      ServiceWorkerRegistration* registration) {
  installing_registrations_[registration->id()] = registration;
}

void ServiceWorkerStorage::NotifyDoneInstallingRegistration(
      ServiceWorkerRegistration* registration,
      ServiceWorkerVersion* version,
      ServiceWorkerStatusCode status) {
  installing_registrations_.erase(registration->id());
  if (status != SERVICE_WORKER_OK && version) {
    ResourceList resources;
    version->script_cache_map()->GetResources(&resources);

    std::set<int64> ids;
    for (size_t i = 0; i < resources.size(); ++i)
      ids.insert(resources[i].resource_id);

    database_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(base::IgnoreResult(
            &ServiceWorkerDatabase::PurgeUncommittedResourceIds),
            base::Unretained(database_.get()),
            ids));

    StartPurgingResources(resources);
  }
}

base::FilePath ServiceWorkerStorage::GetDatabasePath() {
  if (path_.empty())
    return base::FilePath();
  return path_.Append(kDatabaseName);
}

base::FilePath ServiceWorkerStorage::GetDiskCachePath() {
  if (path_.empty())
    return base::FilePath();
  return path_.Append(kDiskCacheName);
}

bool ServiceWorkerStorage::LazyInitialize(const base::Closure& callback) {
  if (!context_)
    return false;

  switch (state_) {
    case INITIALIZED:
      return true;
    case DISABLED:
      return false;
    case INITIALIZING:
      pending_tasks_.push_back(callback);
      return false;
    case UNINITIALIZED:
      pending_tasks_.push_back(callback);
      // Fall-through.
  }

  state_ = INITIALIZING;
  database_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ReadInitialDataFromDB,
                 database_.get(),
                 base::MessageLoopProxy::current(),
                 base::Bind(&ServiceWorkerStorage::DidReadInitialData,
                            weak_factory_.GetWeakPtr())));
  return false;
}

void ServiceWorkerStorage::DidReadInitialData(
    InitialData* data,
    ServiceWorkerDatabase::Status status) {
  DCHECK(data);
  DCHECK_EQ(INITIALIZING, state_);

  if (status == ServiceWorkerDatabase::STATUS_OK) {
    next_registration_id_ = data->next_registration_id;
    next_version_id_ = data->next_version_id;
    next_resource_id_ = data->next_resource_id;
    registered_origins_.swap(data->origins);
    state_ = INITIALIZED;
  } else {
    // TODO(nhiroki): If status==STATUS_ERROR_CORRUPTED, do corruption recovery
    // (http://crbug.com/371675).
    DLOG(WARNING) << "Failed to initialize: " << status;
    state_ = DISABLED;
  }

  for (std::vector<base::Closure>::const_iterator it = pending_tasks_.begin();
       it != pending_tasks_.end(); ++it) {
    RunSoon(FROM_HERE, *it);
  }
  pending_tasks_.clear();
}

void ServiceWorkerStorage::DidFindRegistrationForDocument(
    const GURL& document_url,
    const FindRegistrationCallback& callback,
    const ServiceWorkerDatabase::RegistrationData& data,
    const ResourceList& resources,
    ServiceWorkerDatabase::Status status) {
  if (status == ServiceWorkerDatabase::STATUS_OK) {
    callback.Run(SERVICE_WORKER_OK, GetOrCreateRegistration(data, resources));
    return;
  }

  if (status == ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND) {
    // Look for something currently being installed.
    scoped_refptr<ServiceWorkerRegistration> installing_registration =
        FindInstallingRegistrationForDocument(document_url);
    callback.Run(installing_registration ?
        SERVICE_WORKER_OK : SERVICE_WORKER_ERROR_NOT_FOUND,
        installing_registration);
    return;
  }

  // TODO(nhiroki): Handle database error (http://crbug.com/371675).
  callback.Run(DatabaseStatusToStatusCode(status),
               scoped_refptr<ServiceWorkerRegistration>());
}

void ServiceWorkerStorage::DidFindRegistrationForPattern(
    const GURL& scope,
    const FindRegistrationCallback& callback,
    const ServiceWorkerDatabase::RegistrationData& data,
    const ResourceList& resources,
    ServiceWorkerDatabase::Status status) {
  if (status == ServiceWorkerDatabase::STATUS_OK) {
    callback.Run(SERVICE_WORKER_OK, GetOrCreateRegistration(data, resources));
    return;
  }

  if (status == ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND) {
    scoped_refptr<ServiceWorkerRegistration> installing_registration =
        FindInstallingRegistrationForPattern(scope);
    callback.Run(installing_registration ?
        SERVICE_WORKER_OK : SERVICE_WORKER_ERROR_NOT_FOUND,
        installing_registration);
    return;
  }

  // TODO(nhiroki): Handle database error (http://crbug.com/371675).
  callback.Run(DatabaseStatusToStatusCode(status),
               scoped_refptr<ServiceWorkerRegistration>());
}

void ServiceWorkerStorage::DidFindRegistrationForId(
    const FindRegistrationCallback& callback,
    const ServiceWorkerDatabase::RegistrationData& data,
    const ResourceList& resources,
    ServiceWorkerDatabase::Status status) {
  if (status == ServiceWorkerDatabase::STATUS_OK) {
    callback.Run(SERVICE_WORKER_OK,
                 GetOrCreateRegistration(data, resources));
    return;
  }
  // TODO(nhiroki): Handle database error (http://crbug.com/371675).
  callback.Run(DatabaseStatusToStatusCode(status),
               scoped_refptr<ServiceWorkerRegistration>());
}

void ServiceWorkerStorage::DidGetAllRegistrations(
    const GetAllRegistrationInfosCallback& callback,
    RegistrationList* registrations,
    ServiceWorkerDatabase::Status status) {
  DCHECK(registrations);
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    // TODO(nhiroki): Handle database error (http://crbug.com/371675).
    callback.Run(std::vector<ServiceWorkerRegistrationInfo>());
    return;
  }

  // Add all stored registrations.
  std::set<int64> pushed_registrations;
  std::vector<ServiceWorkerRegistrationInfo> infos;
  for (RegistrationList::const_iterator it = registrations->begin();
       it != registrations->end(); ++it) {
    const bool inserted =
        pushed_registrations.insert(it->registration_id).second;
    DCHECK(inserted);

    ServiceWorkerRegistration* registration =
        context_->GetLiveRegistration(it->registration_id);
    if (registration) {
      infos.push_back(registration->GetInfo());
      continue;
    }

    ServiceWorkerRegistrationInfo info;
    info.pattern = it->scope;
    info.script_url = it->script;
    info.registration_id = it->registration_id;
    if (ServiceWorkerVersion* version =
            context_->GetLiveVersion(it->version_id)) {
      if (it->is_active)
        info.active_version = version->GetInfo();
      else
        info.waiting_version = version->GetInfo();
      infos.push_back(info);
      continue;
    }

    if (it->is_active) {
      info.active_version.is_null = false;
      info.active_version.status = ServiceWorkerVersion::ACTIVE;
      info.active_version.version_id = it->version_id;
    } else {
      info.waiting_version.is_null = false;
      info.waiting_version.status = ServiceWorkerVersion::INSTALLED;
      info.waiting_version.version_id = it->version_id;
    }
    infos.push_back(info);
  }

  // Add unstored registrations that are being installed.
  for (RegistrationRefsById::const_iterator it =
           installing_registrations_.begin();
       it != installing_registrations_.end(); ++it) {
    if (pushed_registrations.insert(it->first).second)
      infos.push_back(it->second->GetInfo());
  }

  callback.Run(infos);
}

void ServiceWorkerStorage::DidStoreRegistration(
    const StatusCallback& callback,
    const GURL& origin,
    const std::vector<int64>& newly_purgeable_resources,
    ServiceWorkerDatabase::Status status) {
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    // TODO(nhiroki): Handle database error (http://crbug.com/371675).
    callback.Run(DatabaseStatusToStatusCode(status));
    return;
  }
  registered_origins_.insert(origin);
  callback.Run(SERVICE_WORKER_OK);
  StartPurgingResources(newly_purgeable_resources);
}

void ServiceWorkerStorage::DidUpdateToActiveState(
    const StatusCallback& callback,
    ServiceWorkerDatabase::Status status) {
  // TODO(nhiroki): Handle database error (http://crbug.com/371675).
  callback.Run(DatabaseStatusToStatusCode(status));
}

void ServiceWorkerStorage::DidDeleteRegistration(
    const GURL& origin,
    const StatusCallback& callback,
    bool origin_is_deletable,
    const std::vector<int64>& newly_purgeable_resources,
    ServiceWorkerDatabase::Status status) {
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    // TODO(nhiroki): Handle database error (http://crbug.com/371675).
    callback.Run(DatabaseStatusToStatusCode(status));
    return;
  }
  if (origin_is_deletable)
    registered_origins_.erase(origin);
  callback.Run(SERVICE_WORKER_OK);
  StartPurgingResources(newly_purgeable_resources);
}

scoped_refptr<ServiceWorkerRegistration>
ServiceWorkerStorage::GetOrCreateRegistration(
    const ServiceWorkerDatabase::RegistrationData& data,
    const ResourceList& resources) {
  scoped_refptr<ServiceWorkerRegistration> registration =
      context_->GetLiveRegistration(data.registration_id);
  if (registration)
    return registration;

  registration = new ServiceWorkerRegistration(
      data.scope, data.script, data.registration_id, context_);
  scoped_refptr<ServiceWorkerVersion> version =
      context_->GetLiveVersion(data.version_id);
  if (!version) {
    version = new ServiceWorkerVersion(registration, data.version_id, context_);
    version->SetStatus(data.is_active ?
        ServiceWorkerVersion::ACTIVE : ServiceWorkerVersion::INSTALLED);
    version->script_cache_map()->SetResources(resources);
  }

  if (version->status() == ServiceWorkerVersion::ACTIVE)
    registration->set_active_version(version);
  else if (version->status() == ServiceWorkerVersion::INSTALLED)
    registration->set_waiting_version(version);
  else
    NOTREACHED();
  // TODO(michaeln): Hmmm, what if DeleteReg was invoked after
  // the Find result we're returning here? NOTREACHED condition?
  return registration;
}

ServiceWorkerRegistration*
ServiceWorkerStorage::FindInstallingRegistrationForDocument(
    const GURL& document_url) {
  DCHECK(!document_url.has_ref());

  LongestScopeMatcher matcher(document_url);
  ServiceWorkerRegistration* match = NULL;

  // TODO(nhiroki): This searches over installing registrations linearly and it
  // couldn't be scalable. Maybe the regs should be partitioned by origin.
  for (RegistrationRefsById::const_iterator it =
           installing_registrations_.begin();
       it != installing_registrations_.end(); ++it) {
    if (matcher.MatchLongest(it->second->pattern()))
      match = it->second;
  }
  return match;
}

ServiceWorkerRegistration*
ServiceWorkerStorage::FindInstallingRegistrationForPattern(
    const GURL& scope) {
  for (RegistrationRefsById::const_iterator it =
           installing_registrations_.begin();
       it != installing_registrations_.end(); ++it) {
    if (it->second->pattern() == scope)
      return it->second;
  }
  return NULL;
}

ServiceWorkerRegistration*
ServiceWorkerStorage::FindInstallingRegistrationForId(
    int64 registration_id) {
  RegistrationRefsById::const_iterator found =
      installing_registrations_.find(registration_id);
  if (found == installing_registrations_.end())
    return NULL;
  return found->second;
}

ServiceWorkerDiskCache* ServiceWorkerStorage::disk_cache() {
  if (disk_cache_)
    return disk_cache_.get();

  disk_cache_.reset(new ServiceWorkerDiskCache);

  base::FilePath path = GetDiskCachePath();
  if (path.empty()) {
    int rv = disk_cache_->InitWithMemBackend(
        kMaxMemDiskCacheSize,
        base::Bind(&EmptyCompletionCallback));
    DCHECK_EQ(net::OK, rv);
    return disk_cache_.get();
  }

  int rv = disk_cache_->InitWithDiskBackend(
      path, kMaxDiskCacheSize, false,
      disk_cache_thread_.get(),
      base::Bind(&ServiceWorkerStorage::OnDiskCacheInitialized,
                 weak_factory_.GetWeakPtr()));
  if (rv != net::ERR_IO_PENDING)
    OnDiskCacheInitialized(rv);

  return disk_cache_.get();
}

void ServiceWorkerStorage::OnDiskCacheInitialized(int rv) {
  if (rv != net::OK) {
    LOG(ERROR) << "Failed to open the serviceworker diskcache: "
               << net::ErrorToString(rv);
    // TODO(michaeln): DeleteAndStartOver()
    disk_cache_->Disable();
    state_ = DISABLED;
  }
  ServiceWorkerHistograms::CountInitDiskCacheResult(rv == net::OK);
}

void ServiceWorkerStorage::StartPurgingResources(
    const std::vector<int64>& ids) {
  for (size_t i = 0; i < ids.size(); ++i)
    purgeable_reource_ids_.push_back(ids[i]);
  ContinuePurgingResources();
}

void ServiceWorkerStorage::StartPurgingResources(
    const ResourceList& resources) {
  for (size_t i = 0; i < resources.size(); ++i)
    purgeable_reource_ids_.push_back(resources[i].resource_id);
  ContinuePurgingResources();
}

void ServiceWorkerStorage::ContinuePurgingResources() {
  if (purgeable_reource_ids_.empty() || is_purge_pending_)
    return;

  // Do one at a time until we're done, use RunSoon to avoid recursion when
  // DoomEntry returns immediately.
  is_purge_pending_ = true;
  int64 id = purgeable_reource_ids_.front();
  purgeable_reource_ids_.pop_front();
  RunSoon(FROM_HERE,
          base::Bind(&ServiceWorkerStorage::PurgeResource,
                     weak_factory_.GetWeakPtr(), id));
}

void ServiceWorkerStorage::PurgeResource(int64 id) {
  DCHECK(is_purge_pending_);
  int rv = disk_cache()->DoomEntry(
      id, base::Bind(&ServiceWorkerStorage::OnResourcePurged,
                     weak_factory_.GetWeakPtr(), id));
  if (rv != net::ERR_IO_PENDING)
    OnResourcePurged(id, rv);
}

void ServiceWorkerStorage::OnResourcePurged(int64 id, int rv) {
  DCHECK(is_purge_pending_);
  is_purge_pending_ = false;

  database_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(
          &ServiceWorkerDatabase::ClearPurgeableResourceIds),
          base::Unretained(database_.get()),
          std::set<int64>(&id, &id + 1)));

  ContinuePurgingResources();
}

void ServiceWorkerStorage::ReadInitialDataFromDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    const InitializeCallback& callback) {
  DCHECK(database);
  scoped_ptr<ServiceWorkerStorage::InitialData> data(
      new ServiceWorkerStorage::InitialData());

  ServiceWorkerDatabase::Status status =
      database->GetNextAvailableIds(&data->next_registration_id,
                                    &data->next_version_id,
                                    &data->next_resource_id);
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    original_task_runner->PostTask(
        FROM_HERE, base::Bind(callback, base::Owned(data.release()), status));
    return;
  }

  status = database->GetOriginsWithRegistrations(&data->origins);
  original_task_runner->PostTask(
      FROM_HERE, base::Bind(callback, base::Owned(data.release()), status));
}

void ServiceWorkerStorage::DeleteRegistrationFromDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    int64 registration_id,
    const GURL& origin,
    const DeleteRegistrationCallback& callback) {
  DCHECK(database);

  std::vector<int64> newly_purgeable_resources;
  ServiceWorkerDatabase::Status status =
      database->DeleteRegistration(registration_id, origin,
                                   &newly_purgeable_resources);
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    original_task_runner->PostTask(
        FROM_HERE, base::Bind(callback, false, std::vector<int64>(), status));
    return;
  }

  // TODO(nhiroki): Add convenient method to ServiceWorkerDatabase to check the
  // unique origin list.
  std::vector<ServiceWorkerDatabase::RegistrationData> registrations;
  status = database->GetRegistrationsForOrigin(origin, &registrations);
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    original_task_runner->PostTask(
        FROM_HERE, base::Bind(callback, false, std::vector<int64>(), status));
    return;
  }

  bool deletable = registrations.empty();
  original_task_runner->PostTask(
      FROM_HERE, base::Bind(callback, deletable,
                            newly_purgeable_resources, status));
}

void ServiceWorkerStorage::WriteRegistrationInDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    const ServiceWorkerDatabase::RegistrationData& data,
    const ResourceList& resources,
    const WriteRegistrationCallback& callback) {
  DCHECK(database);
  std::vector<int64> newly_purgeable_resources;
  ServiceWorkerDatabase::Status status =
      database->WriteRegistration(data, resources, &newly_purgeable_resources);
  original_task_runner->PostTask(
      FROM_HERE,
      base::Bind(callback, data.script.GetOrigin(),
                 newly_purgeable_resources, status));
}

void ServiceWorkerStorage::FindForDocumentInDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    const GURL& document_url,
    const FindInDBCallback& callback) {
  GURL origin = document_url.GetOrigin();
  RegistrationList registrations;
  ServiceWorkerDatabase::Status status =
      database->GetRegistrationsForOrigin(origin, &registrations);
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    original_task_runner->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   ServiceWorkerDatabase::RegistrationData(),
                   ResourceList(),
                   status));
    return;
  }

  ServiceWorkerDatabase::RegistrationData data;
  ResourceList resources;
  status = ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND;

  // Find one with a pattern match.
  LongestScopeMatcher matcher(document_url);
  int64 match = kInvalidServiceWorkerRegistrationId;
  for (size_t i = 0; i < registrations.size(); ++i) {
    if (matcher.MatchLongest(registrations[i].scope))
      match = registrations[i].registration_id;
  }

  if (match != kInvalidServiceWorkerRegistrationId)
    status = database->ReadRegistration(match, origin, &data, &resources);

  original_task_runner->PostTask(
      FROM_HERE,
      base::Bind(callback, data, resources, status));
}

void ServiceWorkerStorage::FindForPatternInDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    const GURL& scope,
    const FindInDBCallback& callback) {
  GURL origin = scope.GetOrigin();
  std::vector<ServiceWorkerDatabase::RegistrationData> registrations;
  ServiceWorkerDatabase::Status status =
      database->GetRegistrationsForOrigin(origin, &registrations);
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    original_task_runner->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   ServiceWorkerDatabase::RegistrationData(),
                   ResourceList(),
                   status));
    return;
  }

  // Find one with an exact matching scope.
  ServiceWorkerDatabase::RegistrationData data;
  ResourceList resources;
  status = ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND;
  for (RegistrationList::const_iterator it = registrations.begin();
       it != registrations.end(); ++it) {
    if (scope != it->scope)
      continue;
    status = database->ReadRegistration(it->registration_id, origin,
                                        &data, &resources);
    break;  // We're done looping.
  }

  original_task_runner->PostTask(
      FROM_HERE,
      base::Bind(callback, data, resources, status));
}

void ServiceWorkerStorage::FindForIdInDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    int64 registration_id,
    const GURL& origin,
    const FindInDBCallback& callback) {
  ServiceWorkerDatabase::RegistrationData data;
  ResourceList resources;
  ServiceWorkerDatabase::Status status =
      database->ReadRegistration(registration_id, origin, &data, &resources);
  original_task_runner->PostTask(
      FROM_HERE, base::Bind(callback, data, resources, status));
}

}  // namespace content
