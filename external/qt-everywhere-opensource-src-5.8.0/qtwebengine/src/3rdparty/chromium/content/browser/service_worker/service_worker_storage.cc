// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_storage.h"

#include <stddef.h>

#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_disk_cache.h"
#include "content/browser/service_worker/service_worker_info.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/special_storage_policy.h"

namespace content {

namespace {

void RunSoon(const tracked_objects::Location& from_here,
             const base::Closure& closure) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(from_here, closure);
}

void CompleteFindNow(
    const scoped_refptr<ServiceWorkerRegistration>& registration,
    ServiceWorkerStatusCode status,
    const ServiceWorkerStorage::FindRegistrationCallback& callback) {
  if (registration && registration->is_deleted()) {
    // It's past the point of no return and no longer findable.
    callback.Run(SERVICE_WORKER_ERROR_NOT_FOUND, nullptr);
    return;
  }
  callback.Run(status, registration);
}

void CompleteFindSoon(
    const tracked_objects::Location& from_here,
    const scoped_refptr<ServiceWorkerRegistration>& registration,
    ServiceWorkerStatusCode status,
    const ServiceWorkerStorage::FindRegistrationCallback& callback) {
  RunSoon(from_here,
          base::Bind(&CompleteFindNow, registration, status, callback));
}

const base::FilePath::CharType kDatabaseName[] =
    FILE_PATH_LITERAL("Database");
const base::FilePath::CharType kDiskCacheName[] =
    FILE_PATH_LITERAL("ScriptCache");

const int kMaxMemDiskCacheSize = 10 * 1024 * 1024;
const int kMaxDiskCacheSize = 250 * 1024 * 1024;

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
      next_resource_id(kInvalidServiceWorkerResourceId) {}

ServiceWorkerStorage::InitialData::~InitialData() {
}

ServiceWorkerStorage::
DidDeleteRegistrationParams::DidDeleteRegistrationParams()
    : registration_id(kInvalidServiceWorkerRegistrationId) {
}

ServiceWorkerStorage::DidDeleteRegistrationParams::DidDeleteRegistrationParams(
    const DidDeleteRegistrationParams& other) = default;

ServiceWorkerStorage::
DidDeleteRegistrationParams::~DidDeleteRegistrationParams() {
}

ServiceWorkerStorage::~ServiceWorkerStorage() {
  ClearSessionOnlyOrigins();
  weak_factory_.InvalidateWeakPtrs();
  database_task_manager_->GetTaskRunner()->DeleteSoon(FROM_HERE,
                                                      database_.release());
}

// static
std::unique_ptr<ServiceWorkerStorage> ServiceWorkerStorage::Create(
    const base::FilePath& path,
    const base::WeakPtr<ServiceWorkerContextCore>& context,
    std::unique_ptr<ServiceWorkerDatabaseTaskManager> database_task_manager,
    const scoped_refptr<base::SingleThreadTaskRunner>& disk_cache_thread,
    storage::QuotaManagerProxy* quota_manager_proxy,
    storage::SpecialStoragePolicy* special_storage_policy) {
  return base::WrapUnique(new ServiceWorkerStorage(
      path, context, std::move(database_task_manager), disk_cache_thread,
      quota_manager_proxy, special_storage_policy));
}

// static
std::unique_ptr<ServiceWorkerStorage> ServiceWorkerStorage::Create(
    const base::WeakPtr<ServiceWorkerContextCore>& context,
    ServiceWorkerStorage* old_storage) {
  return base::WrapUnique(new ServiceWorkerStorage(
      old_storage->path_, context, old_storage->database_task_manager_->Clone(),
      old_storage->disk_cache_thread_, old_storage->quota_manager_proxy_.get(),
      old_storage->special_storage_policy_.get()));
}

void ServiceWorkerStorage::FindRegistrationForDocument(
    const GURL& document_url,
    const FindRegistrationCallback& callback) {
  DCHECK(!document_url.has_ref());
  if (!LazyInitialize(base::Bind(
          &ServiceWorkerStorage::FindRegistrationForDocument,
          weak_factory_.GetWeakPtr(), document_url, callback))) {
    if (state_ != INITIALIZING) {
      CompleteFindNow(scoped_refptr<ServiceWorkerRegistration>(),
                      SERVICE_WORKER_ERROR_ABORT, callback);
    }
    TRACE_EVENT_INSTANT1(
        "ServiceWorker",
        "ServiceWorkerStorage::FindRegistrationForDocument:LazyInitialize",
        TRACE_EVENT_SCOPE_THREAD,
        "URL", document_url.spec());
    return;
  }
  DCHECK_EQ(INITIALIZED, state_);

  // See if there are any stored registrations for the origin.
  if (!ContainsKey(registered_origins_, document_url.GetOrigin())) {
    // Look for something currently being installed.
    scoped_refptr<ServiceWorkerRegistration> installing_registration =
        FindInstallingRegistrationForDocument(document_url);
    ServiceWorkerStatusCode status = installing_registration
                                         ? SERVICE_WORKER_OK
                                         : SERVICE_WORKER_ERROR_NOT_FOUND;
    TRACE_EVENT_INSTANT2(
        "ServiceWorker",
        "ServiceWorkerStorage::FindRegistrationForDocument:CheckInstalling",
        TRACE_EVENT_SCOPE_THREAD,
        "URL", document_url.spec(),
        "Status", ServiceWorkerStatusToString(status));
    CompleteFindNow(installing_registration,
                    status,
                    callback);
    return;
  }

  // To connect this TRACE_EVENT with the callback, TimeTicks is used for
  // callback id.
  int64_t callback_id = base::TimeTicks::Now().ToInternalValue();
  TRACE_EVENT_ASYNC_BEGIN1(
      "ServiceWorker",
      "ServiceWorkerStorage::FindRegistrationForDocument",
      callback_id,
      "URL", document_url.spec());
  database_task_manager_->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(
          &FindForDocumentInDB,
          database_.get(),
          base::ThreadTaskRunnerHandle::Get(),
          document_url,
          base::Bind(&ServiceWorkerStorage::DidFindRegistrationForDocument,
                     weak_factory_.GetWeakPtr(),
                     document_url,
                     callback,
                     callback_id)));
}

void ServiceWorkerStorage::FindRegistrationForPattern(
    const GURL& scope,
    const FindRegistrationCallback& callback) {
  if (!LazyInitialize(base::Bind(
          &ServiceWorkerStorage::FindRegistrationForPattern,
          weak_factory_.GetWeakPtr(), scope, callback))) {
    if (state_ != INITIALIZING) {
      CompleteFindSoon(FROM_HERE, scoped_refptr<ServiceWorkerRegistration>(),
                       SERVICE_WORKER_ERROR_ABORT, callback);
    }
    return;
  }
  DCHECK_EQ(INITIALIZED, state_);

  // See if there are any stored registrations for the origin.
  if (!ContainsKey(registered_origins_, scope.GetOrigin())) {
    // Look for something currently being installed.
    scoped_refptr<ServiceWorkerRegistration> installing_registration =
        FindInstallingRegistrationForPattern(scope);
    CompleteFindSoon(FROM_HERE, installing_registration,
                     installing_registration ? SERVICE_WORKER_OK
                                             : SERVICE_WORKER_ERROR_NOT_FOUND,
                     callback);
    return;
  }

  database_task_manager_->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(
          &FindForPatternInDB,
          database_.get(),
          base::ThreadTaskRunnerHandle::Get(),
          scope,
          base::Bind(&ServiceWorkerStorage::DidFindRegistrationForPattern,
                     weak_factory_.GetWeakPtr(),
                     scope,
                     callback)));
}

ServiceWorkerRegistration* ServiceWorkerStorage::GetUninstallingRegistration(
    const GURL& scope) {
  if (state_ != INITIALIZED)
    return nullptr;
  for (const auto& registration : uninstalling_registrations_) {
    if (registration.second->pattern() == scope) {
      DCHECK(registration.second->is_uninstalling());
      return registration.second.get();
    }
  }
  return nullptr;
}

void ServiceWorkerStorage::FindRegistrationForId(
    int64_t registration_id,
    const GURL& origin,
    const FindRegistrationCallback& callback) {
  if (!LazyInitialize(base::Bind(
          &ServiceWorkerStorage::FindRegistrationForId,
          weak_factory_.GetWeakPtr(), registration_id, origin, callback))) {
    if (state_ != INITIALIZING) {
      CompleteFindNow(scoped_refptr<ServiceWorkerRegistration>(),
                      SERVICE_WORKER_ERROR_ABORT, callback);
    }
    return;
  }
  DCHECK_EQ(INITIALIZED, state_);

  // See if there are any stored registrations for the origin.
  if (!ContainsKey(registered_origins_, origin)) {
    // Look for something currently being installed.
    scoped_refptr<ServiceWorkerRegistration> installing_registration =
        FindInstallingRegistrationForId(registration_id);
    CompleteFindNow(installing_registration,
                    installing_registration ? SERVICE_WORKER_OK
                                            : SERVICE_WORKER_ERROR_NOT_FOUND,
                    callback);
    return;
  }

  scoped_refptr<ServiceWorkerRegistration> registration =
      context_->GetLiveRegistration(registration_id);
  if (registration) {
    CompleteFindNow(registration, SERVICE_WORKER_OK, callback);
    return;
  }

  database_task_manager_->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&FindForIdInDB,
                 database_.get(),
                 base::ThreadTaskRunnerHandle::Get(),
                 registration_id,
                 origin,
                 base::Bind(&ServiceWorkerStorage::DidFindRegistrationForId,
                            weak_factory_.GetWeakPtr(),
                            callback)));
}

void ServiceWorkerStorage::FindRegistrationForIdOnly(
    int64_t registration_id,
    const FindRegistrationCallback& callback) {
  if (!LazyInitialize(
          base::Bind(&ServiceWorkerStorage::FindRegistrationForIdOnly,
                     weak_factory_.GetWeakPtr(), registration_id, callback))) {
    if (state_ != INITIALIZING) {
      CompleteFindNow(nullptr, SERVICE_WORKER_ERROR_ABORT, callback);
    }
    return;
  }
  DCHECK_EQ(INITIALIZED, state_);

  scoped_refptr<ServiceWorkerRegistration> registration =
      context_->GetLiveRegistration(registration_id);
  if (registration) {
    // Delegate to FindRegistrationForId to make sure the same subset of live
    // registrations is returned.
    // TODO(mek): CompleteFindNow should really do all the required checks, so
    // calling that directly here should be enough.
    FindRegistrationForId(registration_id, registration->pattern().GetOrigin(),
                          callback);
    return;
  }

  database_task_manager_->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&FindForIdOnlyInDB,
                 database_.get(),
                 base::ThreadTaskRunnerHandle::Get(),
                 registration_id,
                 base::Bind(&ServiceWorkerStorage::DidFindRegistrationForId,
                            weak_factory_.GetWeakPtr(),
                            callback)));
}

void ServiceWorkerStorage::GetRegistrationsForOrigin(
    const GURL& origin,
    const GetRegistrationsCallback& callback) {
  if (!LazyInitialize(base::Bind(
          &ServiceWorkerStorage::GetRegistrationsForOrigin,
          weak_factory_.GetWeakPtr(), origin, callback))) {
    if (state_ != INITIALIZING) {
      RunSoon(
          FROM_HERE,
          base::Bind(callback, SERVICE_WORKER_ERROR_ABORT,
                     std::vector<scoped_refptr<ServiceWorkerRegistration>>()));
    }
    return;
  }
  DCHECK_EQ(INITIALIZED, state_);

  RegistrationList* registrations = new RegistrationList;
  std::vector<ResourceList>* resource_lists = new std::vector<ResourceList>;
  PostTaskAndReplyWithResult(
      database_task_manager_->GetTaskRunner(), FROM_HERE,
      base::Bind(&ServiceWorkerDatabase::GetRegistrationsForOrigin,
                 base::Unretained(database_.get()), origin, registrations,
                 resource_lists),
      base::Bind(&ServiceWorkerStorage::DidGetRegistrations,
                 weak_factory_.GetWeakPtr(), callback,
                 base::Owned(registrations), base::Owned(resource_lists),
                 origin));
}

void ServiceWorkerStorage::GetAllRegistrationsInfos(
    const GetRegistrationsInfosCallback& callback) {
  if (!LazyInitialize(
          base::Bind(&ServiceWorkerStorage::GetAllRegistrationsInfos,
                     weak_factory_.GetWeakPtr(), callback))) {
    if (state_ != INITIALIZING) {
      RunSoon(FROM_HERE,
              base::Bind(callback, SERVICE_WORKER_ERROR_ABORT,
                         std::vector<ServiceWorkerRegistrationInfo>()));
    }
    return;
  }
  DCHECK_EQ(INITIALIZED, state_);

  RegistrationList* registrations = new RegistrationList;
  PostTaskAndReplyWithResult(
      database_task_manager_->GetTaskRunner(), FROM_HERE,
      base::Bind(&ServiceWorkerDatabase::GetAllRegistrations,
                 base::Unretained(database_.get()), registrations),
      base::Bind(&ServiceWorkerStorage::DidGetRegistrationsInfos,
                 weak_factory_.GetWeakPtr(), callback,
                 base::Owned(registrations), GURL()));
}

void ServiceWorkerStorage::StoreRegistration(
    ServiceWorkerRegistration* registration,
    ServiceWorkerVersion* version,
    const StatusCallback& callback) {
  DCHECK(registration);
  DCHECK(version);

  DCHECK(state_ == INITIALIZED || state_ == DISABLED) << state_;
  if (IsDisabled()) {
    RunSoon(FROM_HERE, base::Bind(callback, SERVICE_WORKER_ERROR_ABORT));
    return;
  }

  ServiceWorkerDatabase::RegistrationData data;
  data.registration_id = registration->id();
  data.scope = registration->pattern();
  data.script = version->script_url();
  data.has_fetch_handler = version->has_fetch_handler();
  data.version_id = version->version_id();
  data.last_update_check = registration->last_update_check();
  data.is_active = (version == registration->active_version());
  data.foreign_fetch_scopes = version->foreign_fetch_scopes();
  data.foreign_fetch_origins = version->foreign_fetch_origins();

  ResourceList resources;
  version->script_cache_map()->GetResources(&resources);

  if (resources.empty()) {
    RunSoon(FROM_HERE, base::Bind(callback, SERVICE_WORKER_ERROR_FAILED));
    return;
  }

  uint64_t resources_total_size_bytes = 0;
  for (const auto& resource : resources) {
    resources_total_size_bytes += resource.size_bytes;
  }
  data.resources_total_size_bytes = resources_total_size_bytes;

  if (!has_checked_for_stale_resources_)
    DeleteStaleResources();

  database_task_manager_->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&WriteRegistrationInDB,
                 database_.get(),
                 base::ThreadTaskRunnerHandle::Get(),
                 data,
                 resources,
                 base::Bind(&ServiceWorkerStorage::DidStoreRegistration,
                            weak_factory_.GetWeakPtr(),
                            callback,
                            data)));

  registration->set_is_deleted(false);
}

void ServiceWorkerStorage::UpdateToActiveState(
    ServiceWorkerRegistration* registration,
    const StatusCallback& callback) {
  DCHECK(registration);

  DCHECK(state_ == INITIALIZED || state_ == DISABLED) << state_;
  if (IsDisabled()) {
    RunSoon(FROM_HERE, base::Bind(callback, SERVICE_WORKER_ERROR_ABORT));
    return;
  }

  PostTaskAndReplyWithResult(
      database_task_manager_->GetTaskRunner(),
      FROM_HERE,
      base::Bind(&ServiceWorkerDatabase::UpdateVersionToActive,
                 base::Unretained(database_.get()),
                 registration->id(),
                 registration->pattern().GetOrigin()),
      base::Bind(&ServiceWorkerStorage::DidUpdateToActiveState,
                 weak_factory_.GetWeakPtr(),
                 callback));
}

void ServiceWorkerStorage::UpdateLastUpdateCheckTime(
    ServiceWorkerRegistration* registration) {
  DCHECK(registration);
  DCHECK(state_ == INITIALIZED || state_ == DISABLED) << state_;
  if (IsDisabled())
    return;

  database_task_manager_->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(
          base::IgnoreResult(&ServiceWorkerDatabase::UpdateLastCheckTime),
          base::Unretained(database_.get()),
          registration->id(),
          registration->pattern().GetOrigin(),
          registration->last_update_check()));
}

void ServiceWorkerStorage::DeleteRegistration(int64_t registration_id,
                                              const GURL& origin,
                                              const StatusCallback& callback) {
  DCHECK(state_ == INITIALIZED || state_ == DISABLED) << state_;
  if (IsDisabled()) {
    RunSoon(FROM_HERE, base::Bind(callback, SERVICE_WORKER_ERROR_ABORT));
    return;
  }

  if (!has_checked_for_stale_resources_)
    DeleteStaleResources();

  DidDeleteRegistrationParams params;
  params.registration_id = registration_id;
  params.origin = origin;
  params.callback = callback;

  database_task_manager_->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&DeleteRegistrationFromDB,
                 database_.get(),
                 base::ThreadTaskRunnerHandle::Get(),
                 registration_id,
                 origin,
                 base::Bind(&ServiceWorkerStorage::DidDeleteRegistration,
                            weak_factory_.GetWeakPtr(),
                            params)));

  // The registration should no longer be findable.
  pending_deletions_.insert(registration_id);
  ServiceWorkerRegistration* registration =
      context_->GetLiveRegistration(registration_id);
  if (registration)
    registration->set_is_deleted(true);
}

std::unique_ptr<ServiceWorkerResponseReader>
ServiceWorkerStorage::CreateResponseReader(int64_t resource_id) {
  return base::WrapUnique(
      new ServiceWorkerResponseReader(resource_id, disk_cache()->GetWeakPtr()));
}

std::unique_ptr<ServiceWorkerResponseWriter>
ServiceWorkerStorage::CreateResponseWriter(int64_t resource_id) {
  return base::WrapUnique(
      new ServiceWorkerResponseWriter(resource_id, disk_cache()->GetWeakPtr()));
}

std::unique_ptr<ServiceWorkerResponseMetadataWriter>
ServiceWorkerStorage::CreateResponseMetadataWriter(int64_t resource_id) {
  return base::WrapUnique(new ServiceWorkerResponseMetadataWriter(
      resource_id, disk_cache()->GetWeakPtr()));
}

void ServiceWorkerStorage::StoreUncommittedResourceId(int64_t resource_id) {
  DCHECK_NE(kInvalidServiceWorkerResourceId, resource_id);
  DCHECK(INITIALIZED == state_ || DISABLED == state_) << state_;
  if (IsDisabled())
    return;

  if (!has_checked_for_stale_resources_)
    DeleteStaleResources();

  PostTaskAndReplyWithResult(
      database_task_manager_->GetTaskRunner(), FROM_HERE,
      base::Bind(&ServiceWorkerDatabase::WriteUncommittedResourceIds,
                 base::Unretained(database_.get()),
                 std::set<int64_t>(&resource_id, &resource_id + 1)),
      base::Bind(&ServiceWorkerStorage::DidWriteUncommittedResourceIds,
                 weak_factory_.GetWeakPtr()));
}

void ServiceWorkerStorage::DoomUncommittedResource(int64_t resource_id) {
  DCHECK_NE(kInvalidServiceWorkerResourceId, resource_id);
  DCHECK(INITIALIZED == state_ || DISABLED == state_) << state_;
  if (IsDisabled())
    return;
  DoomUncommittedResources(std::set<int64_t>(&resource_id, &resource_id + 1));
}

void ServiceWorkerStorage::DoomUncommittedResources(
    const std::set<int64_t>& resource_ids) {
  DCHECK(INITIALIZED == state_ || DISABLED == state_) << state_;
  if (IsDisabled())
    return;

  PostTaskAndReplyWithResult(
      database_task_manager_->GetTaskRunner(), FROM_HERE,
      base::Bind(&ServiceWorkerDatabase::PurgeUncommittedResourceIds,
                 base::Unretained(database_.get()), resource_ids),
      base::Bind(&ServiceWorkerStorage::DidPurgeUncommittedResourceIds,
                 weak_factory_.GetWeakPtr(), resource_ids));
}

void ServiceWorkerStorage::StoreUserData(
    int64_t registration_id,
    const GURL& origin,
    const std::vector<std::pair<std::string, std::string>>& key_value_pairs,
    const StatusCallback& callback) {
  DCHECK(state_ == INITIALIZED || state_ == DISABLED) << state_;
  if (IsDisabled()) {
    RunSoon(FROM_HERE, base::Bind(callback, SERVICE_WORKER_ERROR_ABORT));
    return;
  }

  if (registration_id == kInvalidServiceWorkerRegistrationId ||
      key_value_pairs.empty()) {
    RunSoon(FROM_HERE, base::Bind(callback, SERVICE_WORKER_ERROR_FAILED));
    return;
  }
  for (const auto& kv : key_value_pairs) {
    if (kv.first.empty()) {
      RunSoon(FROM_HERE, base::Bind(callback, SERVICE_WORKER_ERROR_FAILED));
      return;
    }
  }

  PostTaskAndReplyWithResult(
      database_task_manager_->GetTaskRunner(), FROM_HERE,
      base::Bind(&ServiceWorkerDatabase::WriteUserData,
                 base::Unretained(database_.get()), registration_id, origin,
                 key_value_pairs),
      base::Bind(&ServiceWorkerStorage::DidStoreUserData,
                 weak_factory_.GetWeakPtr(), callback));
}

void ServiceWorkerStorage::GetUserData(int64_t registration_id,
                                       const std::vector<std::string>& keys,
                                       const GetUserDataCallback& callback) {
  DCHECK(state_ == INITIALIZED || state_ == DISABLED) << state_;
  if (IsDisabled()) {
    RunSoon(FROM_HERE, base::Bind(callback, std::vector<std::string>(),
                                  SERVICE_WORKER_ERROR_ABORT));
    return;
  }

  if (registration_id == kInvalidServiceWorkerRegistrationId || keys.empty()) {
    RunSoon(FROM_HERE, base::Bind(callback, std::vector<std::string>(),
                                  SERVICE_WORKER_ERROR_FAILED));
    return;
  }
  for (const std::string& key : keys) {
    if (key.empty()) {
      RunSoon(FROM_HERE, base::Bind(callback, std::vector<std::string>(),
                                    SERVICE_WORKER_ERROR_FAILED));
      return;
    }
  }

  database_task_manager_->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ServiceWorkerStorage::GetUserDataInDB, database_.get(),
                 base::ThreadTaskRunnerHandle::Get(), registration_id, keys,
                 base::Bind(&ServiceWorkerStorage::DidGetUserData,
                            weak_factory_.GetWeakPtr(), callback)));
}

void ServiceWorkerStorage::ClearUserData(int64_t registration_id,
                                         const std::vector<std::string>& keys,
                                         const StatusCallback& callback) {
  DCHECK(state_ == INITIALIZED || state_ == DISABLED) << state_;
  if (IsDisabled()) {
    RunSoon(FROM_HERE, base::Bind(callback, SERVICE_WORKER_ERROR_ABORT));
    return;
  }

  if (registration_id == kInvalidServiceWorkerRegistrationId || keys.empty()) {
    RunSoon(FROM_HERE, base::Bind(callback, SERVICE_WORKER_ERROR_FAILED));
    return;
  }
  for (const std::string& key : keys) {
    if (key.empty()) {
      RunSoon(FROM_HERE, base::Bind(callback, SERVICE_WORKER_ERROR_FAILED));
      return;
    }
  }

  PostTaskAndReplyWithResult(
      database_task_manager_->GetTaskRunner(), FROM_HERE,
      base::Bind(&ServiceWorkerDatabase::DeleteUserData,
                 base::Unretained(database_.get()), registration_id, keys),
      base::Bind(&ServiceWorkerStorage::DidDeleteUserData,
                 weak_factory_.GetWeakPtr(), callback));
}

void ServiceWorkerStorage::GetUserDataForAllRegistrations(
    const std::string& key,
    const ServiceWorkerStorage::GetUserDataForAllRegistrationsCallback&
        callback) {
  if (!LazyInitialize(
          base::Bind(&ServiceWorkerStorage::GetUserDataForAllRegistrations,
                     weak_factory_.GetWeakPtr(), key, callback))) {
    if (state_ != INITIALIZING) {
      RunSoon(
          FROM_HERE,
          base::Bind(callback, std::vector<std::pair<int64_t, std::string>>(),
                     SERVICE_WORKER_ERROR_ABORT));
    }
    return;
  }
  DCHECK_EQ(INITIALIZED, state_);

  if (key.empty()) {
    RunSoon(FROM_HERE,
            base::Bind(callback, std::vector<std::pair<int64_t, std::string>>(),
                       SERVICE_WORKER_ERROR_FAILED));
    return;
  }

  database_task_manager_->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(
          &ServiceWorkerStorage::GetUserDataForAllRegistrationsInDB,
          database_.get(),
          base::ThreadTaskRunnerHandle::Get(),
          key,
          base::Bind(&ServiceWorkerStorage::DidGetUserDataForAllRegistrations,
                     weak_factory_.GetWeakPtr(),
                     callback)));
}

bool ServiceWorkerStorage::OriginHasForeignFetchRegistrations(
    const GURL& origin) {
  return !IsDisabled() &&
         foreign_fetch_origins_.find(origin) != foreign_fetch_origins_.end();
}

void ServiceWorkerStorage::DeleteAndStartOver(const StatusCallback& callback) {
  Disable();

  // Delete the database on the database thread.
  PostTaskAndReplyWithResult(
      database_task_manager_->GetTaskRunner(), FROM_HERE,
      base::Bind(&ServiceWorkerDatabase::DestroyDatabase,
                 base::Unretained(database_.get())),
      base::Bind(&ServiceWorkerStorage::DidDeleteDatabase,
                 weak_factory_.GetWeakPtr(), callback));
}

int64_t ServiceWorkerStorage::NewRegistrationId() {
  if (state_ == DISABLED)
    return kInvalidServiceWorkerRegistrationId;
  DCHECK_EQ(INITIALIZED, state_);
  return next_registration_id_++;
}

int64_t ServiceWorkerStorage::NewVersionId() {
  if (state_ == DISABLED)
    return kInvalidServiceWorkerVersionId;
  DCHECK_EQ(INITIALIZED, state_);
  return next_version_id_++;
}

int64_t ServiceWorkerStorage::NewResourceId() {
  if (state_ == DISABLED)
    return kInvalidServiceWorkerResourceId;
  DCHECK_EQ(INITIALIZED, state_);
  return next_resource_id_++;
}

void ServiceWorkerStorage::NotifyInstallingRegistration(
      ServiceWorkerRegistration* registration) {
  DCHECK(installing_registrations_.find(registration->id()) ==
         installing_registrations_.end());
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

    std::set<int64_t> resource_ids;
    for (const auto& resource : resources)
      resource_ids.insert(resource.resource_id);
    DoomUncommittedResources(resource_ids);
  }
}

void ServiceWorkerStorage::NotifyUninstallingRegistration(
    ServiceWorkerRegistration* registration) {
  DCHECK(uninstalling_registrations_.find(registration->id()) ==
         uninstalling_registrations_.end());
  uninstalling_registrations_[registration->id()] = registration;
}

void ServiceWorkerStorage::NotifyDoneUninstallingRegistration(
    ServiceWorkerRegistration* registration) {
  uninstalling_registrations_.erase(registration->id());
}

void ServiceWorkerStorage::Disable() {
  state_ = DISABLED;
  if (disk_cache_)
    disk_cache_->Disable();
}

void ServiceWorkerStorage::PurgeResources(const ResourceList& resources) {
  if (!has_checked_for_stale_resources_)
    DeleteStaleResources();
  StartPurgingResources(resources);
}

ServiceWorkerStorage::ServiceWorkerStorage(
    const base::FilePath& path,
    base::WeakPtr<ServiceWorkerContextCore> context,
    std::unique_ptr<ServiceWorkerDatabaseTaskManager> database_task_manager,
    const scoped_refptr<base::SingleThreadTaskRunner>& disk_cache_thread,
    storage::QuotaManagerProxy* quota_manager_proxy,
    storage::SpecialStoragePolicy* special_storage_policy)
    : next_registration_id_(kInvalidServiceWorkerRegistrationId),
      next_version_id_(kInvalidServiceWorkerVersionId),
      next_resource_id_(kInvalidServiceWorkerResourceId),
      state_(UNINITIALIZED),
      path_(path),
      context_(context),
      database_task_manager_(std::move(database_task_manager)),
      disk_cache_thread_(disk_cache_thread),
      quota_manager_proxy_(quota_manager_proxy),
      special_storage_policy_(special_storage_policy),
      is_purge_pending_(false),
      has_checked_for_stale_resources_(false),
      weak_factory_(this) {
  DCHECK(context_);
  database_.reset(new ServiceWorkerDatabase(GetDatabasePath()));
}

base::FilePath ServiceWorkerStorage::GetDatabasePath() {
  if (path_.empty())
    return base::FilePath();
  return path_.Append(ServiceWorkerContextCore::kServiceWorkerDirectory)
      .Append(kDatabaseName);
}

base::FilePath ServiceWorkerStorage::GetDiskCachePath() {
  if (path_.empty())
    return base::FilePath();
  return path_.Append(ServiceWorkerContextCore::kServiceWorkerDirectory)
      .Append(kDiskCacheName);
}

bool ServiceWorkerStorage::LazyInitialize(const base::Closure& callback) {
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
  database_task_manager_->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ReadInitialDataFromDB,
                 database_.get(),
                 base::ThreadTaskRunnerHandle::Get(),
                 base::Bind(&ServiceWorkerStorage::DidReadInitialData,
                            weak_factory_.GetWeakPtr())));
  return false;
}

void ServiceWorkerStorage::DidReadInitialData(
    std::unique_ptr<InitialData> data,
    ServiceWorkerDatabase::Status status) {
  DCHECK(data);
  DCHECK_EQ(INITIALIZING, state_);

  if (status == ServiceWorkerDatabase::STATUS_OK) {
    next_registration_id_ = data->next_registration_id;
    next_version_id_ = data->next_version_id;
    next_resource_id_ = data->next_resource_id;
    registered_origins_.swap(data->origins);
    foreign_fetch_origins_.swap(data->foreign_fetch_origins);
    state_ = INITIALIZED;
  } else {
    DVLOG(2) << "Failed to initialize: "
             << ServiceWorkerDatabase::StatusToString(status);
    ScheduleDeleteAndStartOver();
  }

  for (const auto& task : pending_tasks_)
    RunSoon(FROM_HERE, task);
  pending_tasks_.clear();
}

void ServiceWorkerStorage::DidFindRegistrationForDocument(
    const GURL& document_url,
    const FindRegistrationCallback& callback,
    int64_t callback_id,
    const ServiceWorkerDatabase::RegistrationData& data,
    const ResourceList& resources,
    ServiceWorkerDatabase::Status status) {
  if (status == ServiceWorkerDatabase::STATUS_OK) {
    ReturnFoundRegistration(callback, data, resources);
    TRACE_EVENT_ASYNC_END1(
        "ServiceWorker",
        "ServiceWorkerStorage::FindRegistrationForDocument",
        callback_id,
        "Status", ServiceWorkerDatabase::StatusToString(status));
    return;
  }

  if (status == ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND) {
    // Look for something currently being installed.
    scoped_refptr<ServiceWorkerRegistration> installing_registration =
        FindInstallingRegistrationForDocument(document_url);
    ServiceWorkerStatusCode installing_status =
        installing_registration ? SERVICE_WORKER_OK
                                : SERVICE_WORKER_ERROR_NOT_FOUND;
    callback.Run(installing_status, installing_registration);
    TRACE_EVENT_ASYNC_END2(
        "ServiceWorker",
        "ServiceWorkerStorage::FindRegistrationForDocument",
        callback_id,
        "Status", ServiceWorkerDatabase::StatusToString(status),
        "Info",
        (installing_status == SERVICE_WORKER_OK) ?
            "Installing registration is found" :
            "Any registrations are not found");
    return;
  }

  ScheduleDeleteAndStartOver();
  callback.Run(DatabaseStatusToStatusCode(status),
               scoped_refptr<ServiceWorkerRegistration>());
  TRACE_EVENT_ASYNC_END1(
      "ServiceWorker",
      "ServiceWorkerStorage::FindRegistrationForDocument",
      callback_id,
      "Status", ServiceWorkerDatabase::StatusToString(status));
}

void ServiceWorkerStorage::DidFindRegistrationForPattern(
    const GURL& scope,
    const FindRegistrationCallback& callback,
    const ServiceWorkerDatabase::RegistrationData& data,
    const ResourceList& resources,
    ServiceWorkerDatabase::Status status) {
  if (status == ServiceWorkerDatabase::STATUS_OK) {
    ReturnFoundRegistration(callback, data, resources);
    return;
  }

  if (status == ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND) {
    scoped_refptr<ServiceWorkerRegistration> installing_registration =
        FindInstallingRegistrationForPattern(scope);
    callback.Run(installing_registration ? SERVICE_WORKER_OK
                                         : SERVICE_WORKER_ERROR_NOT_FOUND,
                 installing_registration);
    return;
  }

  ScheduleDeleteAndStartOver();
  callback.Run(DatabaseStatusToStatusCode(status),
               scoped_refptr<ServiceWorkerRegistration>());
}

void ServiceWorkerStorage::DidFindRegistrationForId(
    const FindRegistrationCallback& callback,
    const ServiceWorkerDatabase::RegistrationData& data,
    const ResourceList& resources,
    ServiceWorkerDatabase::Status status) {
  if (status == ServiceWorkerDatabase::STATUS_OK) {
    ReturnFoundRegistration(callback, data, resources);
    return;
  }

  if (status == ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND) {
    // TODO(nhiroki): Find a registration in |installing_registrations_|.
    callback.Run(DatabaseStatusToStatusCode(status),
                 scoped_refptr<ServiceWorkerRegistration>());
    return;
  }

  ScheduleDeleteAndStartOver();
  callback.Run(DatabaseStatusToStatusCode(status),
               scoped_refptr<ServiceWorkerRegistration>());
}

void ServiceWorkerStorage::ReturnFoundRegistration(
    const FindRegistrationCallback& callback,
    const ServiceWorkerDatabase::RegistrationData& data,
    const ResourceList& resources) {
  DCHECK(!resources.empty());
  scoped_refptr<ServiceWorkerRegistration> registration =
      GetOrCreateRegistration(data, resources);
  CompleteFindNow(registration, SERVICE_WORKER_OK, callback);
}

void ServiceWorkerStorage::DidGetRegistrations(
    const GetRegistrationsCallback& callback,
    RegistrationList* registration_data_list,
    std::vector<ResourceList>* resources_list,
    const GURL& origin_filter,
    ServiceWorkerDatabase::Status status) {
  DCHECK(registration_data_list);
  DCHECK(resources_list);

  if (status != ServiceWorkerDatabase::STATUS_OK &&
      status != ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND) {
    ScheduleDeleteAndStartOver();
    callback.Run(DatabaseStatusToStatusCode(status),
                 std::vector<scoped_refptr<ServiceWorkerRegistration>>());
    return;
  }

  // Add all stored registrations.
  std::set<int64_t> registration_ids;
  std::vector<scoped_refptr<ServiceWorkerRegistration>> registrations;
  size_t index = 0;
  for (const auto& registration_data : *registration_data_list) {
    registration_ids.insert(registration_data.registration_id);
    registrations.push_back(GetOrCreateRegistration(
        registration_data, resources_list->at(index++)));
  }

  // Add unstored registrations that are being installed.
  for (const auto& registration : installing_registrations_) {
    if ((!origin_filter.is_valid() ||
         registration.second->pattern().GetOrigin() == origin_filter) &&
        registration_ids.insert(registration.first).second) {
      registrations.push_back(registration.second);
    }
  }

  callback.Run(SERVICE_WORKER_OK, registrations);
}

void ServiceWorkerStorage::DidGetRegistrationsInfos(
    const GetRegistrationsInfosCallback& callback,
    RegistrationList* registration_data_list,
    const GURL& origin_filter,
    ServiceWorkerDatabase::Status status) {
  DCHECK(registration_data_list);
  if (status != ServiceWorkerDatabase::STATUS_OK &&
      status != ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND) {
    ScheduleDeleteAndStartOver();
    callback.Run(DatabaseStatusToStatusCode(status),
                 std::vector<ServiceWorkerRegistrationInfo>());
    return;
  }

  // Add all stored registrations.
  std::set<int64_t> pushed_registrations;
  std::vector<ServiceWorkerRegistrationInfo> infos;
  for (const auto& registration_data : *registration_data_list) {
    const bool inserted =
        pushed_registrations.insert(registration_data.registration_id).second;
    DCHECK(inserted);

    ServiceWorkerRegistration* registration =
        context_->GetLiveRegistration(registration_data.registration_id);
    if (registration) {
      infos.push_back(registration->GetInfo());
      continue;
    }

    ServiceWorkerRegistrationInfo info;
    info.pattern = registration_data.scope;
    info.registration_id = registration_data.registration_id;
    info.stored_version_size_bytes =
        registration_data.resources_total_size_bytes;
    if (ServiceWorkerVersion* version =
            context_->GetLiveVersion(registration_data.version_id)) {
      if (registration_data.is_active)
        info.active_version = version->GetInfo();
      else
        info.waiting_version = version->GetInfo();
      infos.push_back(info);
      continue;
    }

    if (registration_data.is_active) {
      info.active_version.status = ServiceWorkerVersion::ACTIVATED;
      info.active_version.script_url = registration_data.script;
      info.active_version.version_id = registration_data.version_id;
      info.active_version.registration_id = registration_data.registration_id;
    } else {
      info.waiting_version.status = ServiceWorkerVersion::INSTALLED;
      info.waiting_version.script_url = registration_data.script;
      info.waiting_version.version_id = registration_data.version_id;
      info.waiting_version.registration_id = registration_data.registration_id;
    }
    infos.push_back(info);
  }

  // Add unstored registrations that are being installed.
  for (const auto& registration : installing_registrations_) {
    if ((!origin_filter.is_valid() ||
         registration.second->pattern().GetOrigin() == origin_filter) &&
        pushed_registrations.insert(registration.first).second) {
      infos.push_back(registration.second->GetInfo());
    }
  }

  callback.Run(SERVICE_WORKER_OK, infos);
}

void ServiceWorkerStorage::DidStoreRegistration(
    const StatusCallback& callback,
    const ServiceWorkerDatabase::RegistrationData& new_version,
    const GURL& origin,
    const ServiceWorkerDatabase::RegistrationData& deleted_version,
    const std::vector<int64_t>& newly_purgeable_resources,
    ServiceWorkerDatabase::Status status) {
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    ScheduleDeleteAndStartOver();
    callback.Run(DatabaseStatusToStatusCode(status));
    return;
  }
  registered_origins_.insert(origin);
  if (!new_version.foreign_fetch_scopes.empty())
    foreign_fetch_origins_.insert(origin);

  scoped_refptr<ServiceWorkerRegistration> registration =
      context_->GetLiveRegistration(new_version.registration_id);
  if (registration) {
    registration->set_resources_total_size_bytes(
        new_version.resources_total_size_bytes);
  }
  if (quota_manager_proxy_) {
    // Can be nullptr in tests.
    quota_manager_proxy_->NotifyStorageModified(
        storage::QuotaClient::kServiceWorker,
        origin,
        storage::StorageType::kStorageTypeTemporary,
        new_version.resources_total_size_bytes -
            deleted_version.resources_total_size_bytes);
  }

  callback.Run(SERVICE_WORKER_OK);

  if (!context_->GetLiveVersion(deleted_version.version_id))
    StartPurgingResources(newly_purgeable_resources);
}

void ServiceWorkerStorage::DidUpdateToActiveState(
    const StatusCallback& callback,
    ServiceWorkerDatabase::Status status) {
  if (status != ServiceWorkerDatabase::STATUS_OK &&
      status != ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND) {
    ScheduleDeleteAndStartOver();
  }
  callback.Run(DatabaseStatusToStatusCode(status));
}

void ServiceWorkerStorage::DidDeleteRegistration(
    const DidDeleteRegistrationParams& params,
    OriginState origin_state,
    const ServiceWorkerDatabase::RegistrationData& deleted_version,
    const std::vector<int64_t>& newly_purgeable_resources,
    ServiceWorkerDatabase::Status status) {
  pending_deletions_.erase(params.registration_id);
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    ScheduleDeleteAndStartOver();
    params.callback.Run(DatabaseStatusToStatusCode(status));
    return;
  }
  if (quota_manager_proxy_) {
    // Can be nullptr in tests.
    quota_manager_proxy_->NotifyStorageModified(
        storage::QuotaClient::kServiceWorker,
        params.origin,
        storage::StorageType::kStorageTypeTemporary,
        -deleted_version.resources_total_size_bytes);
  }
  if (origin_state == OriginState::DELETE_FROM_ALL)
    registered_origins_.erase(params.origin);
  if (origin_state == OriginState::DELETE_FROM_ALL ||
      origin_state == OriginState::DELETE_FROM_FOREIGN_FETCH)
    foreign_fetch_origins_.erase(params.origin);
  params.callback.Run(SERVICE_WORKER_OK);

  if (!context_->GetLiveVersion(deleted_version.version_id))
    StartPurgingResources(newly_purgeable_resources);
}

void ServiceWorkerStorage::DidWriteUncommittedResourceIds(
    ServiceWorkerDatabase::Status status) {
  if (status != ServiceWorkerDatabase::STATUS_OK)
    ScheduleDeleteAndStartOver();
}

void ServiceWorkerStorage::DidPurgeUncommittedResourceIds(
    const std::set<int64_t>& resource_ids,
    ServiceWorkerDatabase::Status status) {
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    ScheduleDeleteAndStartOver();
    return;
  }
  StartPurgingResources(resource_ids);
}

void ServiceWorkerStorage::DidStoreUserData(
    const StatusCallback& callback,
    ServiceWorkerDatabase::Status status) {
  // |status| can be NOT_FOUND when the associated registration did not exist in
  // the database. In the case, we don't have to schedule the corruption
  // recovery.
  if (status != ServiceWorkerDatabase::STATUS_OK &&
      status != ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND) {
    ScheduleDeleteAndStartOver();
  }
  callback.Run(DatabaseStatusToStatusCode(status));
}

void ServiceWorkerStorage::DidGetUserData(
    const GetUserDataCallback& callback,
    const std::vector<std::string>& data,
    ServiceWorkerDatabase::Status status) {
  if (status != ServiceWorkerDatabase::STATUS_OK &&
      status != ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND) {
    ScheduleDeleteAndStartOver();
  }
  callback.Run(data, DatabaseStatusToStatusCode(status));
}

void ServiceWorkerStorage::DidDeleteUserData(
    const StatusCallback& callback,
    ServiceWorkerDatabase::Status status) {
  if (status != ServiceWorkerDatabase::STATUS_OK)
    ScheduleDeleteAndStartOver();
  callback.Run(DatabaseStatusToStatusCode(status));
}

void ServiceWorkerStorage::DidGetUserDataForAllRegistrations(
    const GetUserDataForAllRegistrationsCallback& callback,
    const std::vector<std::pair<int64_t, std::string>>& user_data,
    ServiceWorkerDatabase::Status status) {
  if (status != ServiceWorkerDatabase::STATUS_OK)
    ScheduleDeleteAndStartOver();
  callback.Run(user_data, DatabaseStatusToStatusCode(status));
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
      data.scope, data.registration_id, context_);
  registration->set_resources_total_size_bytes(data.resources_total_size_bytes);
  registration->set_last_update_check(data.last_update_check);
  if (pending_deletions_.find(data.registration_id) !=
      pending_deletions_.end()) {
    registration->set_is_deleted(true);
  }
  scoped_refptr<ServiceWorkerVersion> version =
      context_->GetLiveVersion(data.version_id);
  if (!version) {
    version = new ServiceWorkerVersion(
        registration.get(), data.script, data.version_id, context_);
    version->SetStatus(data.is_active ?
        ServiceWorkerVersion::ACTIVATED : ServiceWorkerVersion::INSTALLED);
    version->script_cache_map()->SetResources(resources);
    version->set_foreign_fetch_scopes(data.foreign_fetch_scopes);
    version->set_foreign_fetch_origins(data.foreign_fetch_origins);
    version->set_has_fetch_handler(data.has_fetch_handler);
  }

  if (version->status() == ServiceWorkerVersion::ACTIVATED)
    registration->SetActiveVersion(version);
  else if (version->status() == ServiceWorkerVersion::INSTALLED)
    registration->SetWaitingVersion(version);
  else
    NOTREACHED();

  return registration;
}

ServiceWorkerRegistration*
ServiceWorkerStorage::FindInstallingRegistrationForDocument(
    const GURL& document_url) {
  DCHECK(!document_url.has_ref());

  LongestScopeMatcher matcher(document_url);
  ServiceWorkerRegistration* match = nullptr;

  // TODO(nhiroki): This searches over installing registrations linearly and it
  // couldn't be scalable. Maybe the regs should be partitioned by origin.
  for (const auto& registration : installing_registrations_)
    if (matcher.MatchLongest(registration.second->pattern()))
      match = registration.second.get();
  return match;
}

ServiceWorkerRegistration*
ServiceWorkerStorage::FindInstallingRegistrationForPattern(const GURL& scope) {
  for (const auto& registration : installing_registrations_)
    if (registration.second->pattern() == scope)
      return registration.second.get();
  return nullptr;
}

ServiceWorkerRegistration*
ServiceWorkerStorage::FindInstallingRegistrationForId(int64_t registration_id) {
  RegistrationRefsById::const_iterator found =
      installing_registrations_.find(registration_id);
  if (found == installing_registrations_.end())
    return nullptr;
  return found->second.get();
}

ServiceWorkerDiskCache* ServiceWorkerStorage::disk_cache() {
  DCHECK(INITIALIZED == state_ || DISABLED == state_) << state_;
  if (disk_cache_)
    return disk_cache_.get();
  disk_cache_.reset(new ServiceWorkerDiskCache);

  if (IsDisabled()) {
    disk_cache_->Disable();
    return disk_cache_.get();
  }

  base::FilePath path = GetDiskCachePath();
  if (path.empty()) {
    int rv = disk_cache_->InitWithMemBackend(kMaxMemDiskCacheSize,
                                             net::CompletionCallback());
    DCHECK_EQ(net::OK, rv);
    return disk_cache_.get();
  }

  InitializeDiskCache();
  return disk_cache_.get();
}

void ServiceWorkerStorage::InitializeDiskCache() {
  disk_cache_->set_is_waiting_to_initialize(false);
  int rv = disk_cache_->InitWithDiskBackend(
      GetDiskCachePath(), kMaxDiskCacheSize, false, disk_cache_thread_,
      base::Bind(&ServiceWorkerStorage::OnDiskCacheInitialized,
                 weak_factory_.GetWeakPtr()));
  if (rv != net::ERR_IO_PENDING)
    OnDiskCacheInitialized(rv);
}

void ServiceWorkerStorage::OnDiskCacheInitialized(int rv) {
  if (rv != net::OK) {
    LOG(ERROR) << "Failed to open the serviceworker diskcache: "
               << net::ErrorToString(rv);
    ScheduleDeleteAndStartOver();
  }
  ServiceWorkerMetrics::CountInitDiskCacheResult(rv == net::OK);
}

void ServiceWorkerStorage::StartPurgingResources(
    const std::set<int64_t>& resource_ids) {
  DCHECK(has_checked_for_stale_resources_);
  for (int64_t resource_id : resource_ids)
    purgeable_resource_ids_.push_back(resource_id);
  ContinuePurgingResources();
}

void ServiceWorkerStorage::StartPurgingResources(
    const std::vector<int64_t>& resource_ids) {
  DCHECK(has_checked_for_stale_resources_);
  for (int64_t resource_id : resource_ids)
    purgeable_resource_ids_.push_back(resource_id);
  ContinuePurgingResources();
}

void ServiceWorkerStorage::StartPurgingResources(
    const ResourceList& resources) {
  DCHECK(has_checked_for_stale_resources_);
  for (const auto& resource : resources)
    purgeable_resource_ids_.push_back(resource.resource_id);
  ContinuePurgingResources();
}

void ServiceWorkerStorage::ContinuePurgingResources() {
  if (purgeable_resource_ids_.empty() || is_purge_pending_)
    return;

  // Do one at a time until we're done, use RunSoon to avoid recursion when
  // DoomEntry returns immediately.
  is_purge_pending_ = true;
  int64_t id = purgeable_resource_ids_.front();
  purgeable_resource_ids_.pop_front();
  RunSoon(FROM_HERE,
          base::Bind(&ServiceWorkerStorage::PurgeResource,
                     weak_factory_.GetWeakPtr(), id));
}

void ServiceWorkerStorage::PurgeResource(int64_t id) {
  DCHECK(is_purge_pending_);
  int rv = disk_cache()->DoomEntry(
      id, base::Bind(&ServiceWorkerStorage::OnResourcePurged,
                     weak_factory_.GetWeakPtr(), id));
  if (rv != net::ERR_IO_PENDING)
    OnResourcePurged(id, rv);
}

void ServiceWorkerStorage::OnResourcePurged(int64_t id, int rv) {
  DCHECK(is_purge_pending_);
  is_purge_pending_ = false;

  ServiceWorkerMetrics::RecordPurgeResourceResult(rv);

  database_task_manager_->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(
          base::IgnoreResult(&ServiceWorkerDatabase::ClearPurgeableResourceIds),
          base::Unretained(database_.get()), std::set<int64_t>(&id, &id + 1)));

  // Continue purging resources regardless of the previous result.
  ContinuePurgingResources();
}

void ServiceWorkerStorage::DeleteStaleResources() {
  DCHECK(!has_checked_for_stale_resources_);
  has_checked_for_stale_resources_ = true;
  database_task_manager_->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ServiceWorkerStorage::CollectStaleResourcesFromDB,
                 database_.get(),
                 base::ThreadTaskRunnerHandle::Get(),
                 base::Bind(&ServiceWorkerStorage::DidCollectStaleResources,
                            weak_factory_.GetWeakPtr())));
}

void ServiceWorkerStorage::DidCollectStaleResources(
    const std::vector<int64_t>& stale_resource_ids,
    ServiceWorkerDatabase::Status status) {
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    DCHECK_NE(ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND, status);
    ScheduleDeleteAndStartOver();
    return;
  }
  StartPurgingResources(stale_resource_ids);
}

void ServiceWorkerStorage::ClearSessionOnlyOrigins() {
  // Can be null in tests.
  if (!special_storage_policy_)
    return;

  if (!special_storage_policy_->HasSessionOnlyOrigins())
    return;

  std::set<GURL> session_only_origins;
  for (const GURL& origin : registered_origins_) {
    if (special_storage_policy_->IsStorageSessionOnly(origin))
      session_only_origins.insert(origin);
  }

  database_task_manager_->GetShutdownBlockingTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&DeleteAllDataForOriginsFromDB,
                 database_.get(),
                 session_only_origins));
}

void ServiceWorkerStorage::CollectStaleResourcesFromDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    const GetResourcesCallback& callback) {
  std::set<int64_t> ids;
  ServiceWorkerDatabase::Status status =
      database->GetUncommittedResourceIds(&ids);
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    original_task_runner->PostTask(
        FROM_HERE,
        base::Bind(callback, std::vector<int64_t>(ids.begin(), ids.end()),
                   status));
    return;
  }

  status = database->PurgeUncommittedResourceIds(ids);
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    original_task_runner->PostTask(
        FROM_HERE,
        base::Bind(callback, std::vector<int64_t>(ids.begin(), ids.end()),
                   status));
    return;
  }

  ids.clear();
  status = database->GetPurgeableResourceIds(&ids);
  original_task_runner->PostTask(
      FROM_HERE,
      base::Bind(callback, std::vector<int64_t>(ids.begin(), ids.end()),
                 status));
}

void ServiceWorkerStorage::ReadInitialDataFromDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    const InitializeCallback& callback) {
  DCHECK(database);
  std::unique_ptr<ServiceWorkerStorage::InitialData> data(
      new ServiceWorkerStorage::InitialData());

  ServiceWorkerDatabase::Status status =
      database->GetNextAvailableIds(&data->next_registration_id,
                                    &data->next_version_id,
                                    &data->next_resource_id);
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    original_task_runner->PostTask(
        FROM_HERE, base::Bind(callback, base::Passed(std::move(data)), status));
    return;
  }

  status = database->GetOriginsWithRegistrations(&data->origins);
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    original_task_runner->PostTask(
        FROM_HERE, base::Bind(callback, base::Passed(std::move(data)), status));
    return;
  }

  status = database->GetOriginsWithForeignFetchRegistrations(
      &data->foreign_fetch_origins);
  original_task_runner->PostTask(
      FROM_HERE, base::Bind(callback, base::Passed(std::move(data)), status));
}

void ServiceWorkerStorage::DeleteRegistrationFromDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    int64_t registration_id,
    const GURL& origin,
    const DeleteRegistrationCallback& callback) {
  DCHECK(database);

  ServiceWorkerDatabase::RegistrationData deleted_version;
  std::vector<int64_t> newly_purgeable_resources;
  ServiceWorkerDatabase::Status status = database->DeleteRegistration(
      registration_id, origin, &deleted_version, &newly_purgeable_resources);
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    original_task_runner->PostTask(
        FROM_HERE, base::Bind(callback, OriginState::KEEP_ALL, deleted_version,
                              std::vector<int64_t>(), status));
    return;
  }

  // TODO(nhiroki): Add convenient method to ServiceWorkerDatabase to check the
  // unique origin list.
  RegistrationList registrations;
  status = database->GetRegistrationsForOrigin(origin, &registrations, nullptr);
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    original_task_runner->PostTask(
        FROM_HERE, base::Bind(callback, OriginState::KEEP_ALL, deleted_version,
                              std::vector<int64_t>(), status));
    return;
  }

  OriginState origin_state = registrations.empty()
                                 ? OriginState::DELETE_FROM_ALL
                                 : OriginState::DELETE_FROM_FOREIGN_FETCH;

  // TODO(mek): Add convenient method to ServiceWorkerDatabase to check the
  // foreign fetch scope origin list.
  for (const auto& registration : registrations) {
    if (!registration.foreign_fetch_scopes.empty()) {
      origin_state = OriginState::KEEP_ALL;
      break;
    }
  }
  original_task_runner->PostTask(
      FROM_HERE, base::Bind(callback, origin_state, deleted_version,
                            newly_purgeable_resources, status));
}

void ServiceWorkerStorage::WriteRegistrationInDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    const ServiceWorkerDatabase::RegistrationData& data,
    const ResourceList& resources,
    const WriteRegistrationCallback& callback) {
  DCHECK(database);
  ServiceWorkerDatabase::RegistrationData deleted_version;
  std::vector<int64_t> newly_purgeable_resources;
  ServiceWorkerDatabase::Status status = database->WriteRegistration(
      data, resources, &deleted_version, &newly_purgeable_resources);
  original_task_runner->PostTask(FROM_HERE,
                                 base::Bind(callback,
                                            data.script.GetOrigin(),
                                            deleted_version,
                                            newly_purgeable_resources,
                                            status));
}

void ServiceWorkerStorage::FindForDocumentInDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    const GURL& document_url,
    const FindInDBCallback& callback) {
  GURL origin = document_url.GetOrigin();
  RegistrationList registration_data_list;
  ServiceWorkerDatabase::Status status = database->GetRegistrationsForOrigin(
      origin, &registration_data_list, nullptr);
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
  int64_t match = kInvalidServiceWorkerRegistrationId;
  for (const auto& registration_data : registration_data_list)
    if (matcher.MatchLongest(registration_data.scope))
      match = registration_data.registration_id;
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
  RegistrationList registration_data_list;
  ServiceWorkerDatabase::Status status = database->GetRegistrationsForOrigin(
      origin, &registration_data_list, nullptr);
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
  for (const auto& registration_data : registration_data_list) {
    if (scope != registration_data.scope)
      continue;
    status = database->ReadRegistration(registration_data.registration_id,
                                        origin, &data, &resources);
    break;  // We're done looping.
  }

  original_task_runner->PostTask(
      FROM_HERE,
      base::Bind(callback, data, resources, status));
}

void ServiceWorkerStorage::FindForIdInDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    int64_t registration_id,
    const GURL& origin,
    const FindInDBCallback& callback) {
  ServiceWorkerDatabase::RegistrationData data;
  ResourceList resources;
  ServiceWorkerDatabase::Status status =
      database->ReadRegistration(registration_id, origin, &data, &resources);
  original_task_runner->PostTask(
      FROM_HERE, base::Bind(callback, data, resources, status));
}

void ServiceWorkerStorage::FindForIdOnlyInDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    int64_t registration_id,
    const FindInDBCallback& callback) {
  GURL origin;
  ServiceWorkerDatabase::Status status =
      database->ReadRegistrationOrigin(registration_id, &origin);
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    original_task_runner->PostTask(
        FROM_HERE,
        base::Bind(callback, ServiceWorkerDatabase::RegistrationData(),
                   ResourceList(), status));
    return;
  }
  FindForIdInDB(database, original_task_runner, registration_id, origin,
                callback);
}

void ServiceWorkerStorage::GetUserDataInDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    int64_t registration_id,
    const std::vector<std::string>& keys,
    const GetUserDataInDBCallback& callback) {
  std::vector<std::string> values;
  ServiceWorkerDatabase::Status status =
      database->ReadUserData(registration_id, keys, &values);
  original_task_runner->PostTask(FROM_HERE,
                                 base::Bind(callback, values, status));
}

void ServiceWorkerStorage::GetUserDataForAllRegistrationsInDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    const std::string& key,
    const GetUserDataForAllRegistrationsInDBCallback& callback) {
  std::vector<std::pair<int64_t, std::string>> user_data;
  ServiceWorkerDatabase::Status status =
      database->ReadUserDataForAllRegistrations(key, &user_data);
  original_task_runner->PostTask(FROM_HERE,
                                 base::Bind(callback, user_data, status));
}

void ServiceWorkerStorage::DeleteAllDataForOriginsFromDB(
    ServiceWorkerDatabase* database,
    const std::set<GURL>& origins) {
  DCHECK(database);

  std::vector<int64_t> newly_purgeable_resources;
  database->DeleteAllDataForOrigins(origins, &newly_purgeable_resources);
}

bool ServiceWorkerStorage::IsDisabled() const {
  return state_ == DISABLED;
}

// TODO(nhiroki): The corruption recovery should not be scheduled if the error
// is transient and it can get healed soon (e.g. IO error). To do that, the
// database should not disable itself when an error occurs and the storage
// controls it instead.
void ServiceWorkerStorage::ScheduleDeleteAndStartOver() {
  // TODO(dmurph): Notify the quota manager somehow that all of our data is now
  // removed.
  if (state_ == DISABLED) {
    // Recovery process has already been scheduled.
    return;
  }
  Disable();

  DVLOG(1) << "Schedule to delete the context and start over.";
  context_->ScheduleDeleteAndStartOver();
}

void ServiceWorkerStorage::DidDeleteDatabase(
    const StatusCallback& callback,
    ServiceWorkerDatabase::Status status) {
  DCHECK_EQ(DISABLED, state_);
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    // Give up the corruption recovery until the browser restarts.
    LOG(ERROR) << "Failed to delete the database: "
               << ServiceWorkerDatabase::StatusToString(status);
    ServiceWorkerMetrics::RecordDeleteAndStartOverResult(
        ServiceWorkerMetrics::DELETE_DATABASE_ERROR);
    callback.Run(DatabaseStatusToStatusCode(status));
    return;
  }
  DVLOG(1) << "Deleted ServiceWorkerDatabase successfully.";

  // Delete the disk cache on the cache thread.
  // TODO(nhiroki): What if there is a bunch of files in the cache directory?
  // Deleting the directory could take a long time and restart could be delayed.
  // We should probably rename the directory and delete it later.
  PostTaskAndReplyWithResult(
      disk_cache_thread_.get(), FROM_HERE,
      base::Bind(&base::DeleteFile, GetDiskCachePath(), true),
      base::Bind(&ServiceWorkerStorage::DidDeleteDiskCache,
                 weak_factory_.GetWeakPtr(), callback));
}

void ServiceWorkerStorage::DidDeleteDiskCache(
    const StatusCallback& callback, bool result) {
  DCHECK_EQ(DISABLED, state_);
  if (!result) {
    // Give up the corruption recovery until the browser restarts.
    LOG(ERROR) << "Failed to delete the diskcache.";
    ServiceWorkerMetrics::RecordDeleteAndStartOverResult(
        ServiceWorkerMetrics::DELETE_DISK_CACHE_ERROR);
    callback.Run(SERVICE_WORKER_ERROR_FAILED);
    return;
  }
  DVLOG(1) << "Deleted ServiceWorkerDiskCache successfully.";
  ServiceWorkerMetrics::RecordDeleteAndStartOverResult(
      ServiceWorkerMetrics::DELETE_OK);
  callback.Run(SERVICE_WORKER_OK);
}

}  // namespace content
