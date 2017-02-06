// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_database.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "content/browser/service_worker/service_worker_database.pb.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/helpers/memenv/memenv.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"
#include "url/origin.h"

// LevelDB database schema
// =======================
//
// NOTE
// - int64_t value is serialized as a string by base::Int64ToString().
// - GURL value is serialized as a string by GURL::spec().
//
// Version 1 (in sorted order)
//   key: "INITDATA_DB_VERSION"
//   value: "1"
//
//   key: "INITDATA_NEXT_REGISTRATION_ID"
//   value: <int64_t 'next_available_registration_id'>
//
//   key: "INITDATA_NEXT_RESOURCE_ID"
//   value: <int64_t 'next_available_resource_id'>
//
//   key: "INITDATA_NEXT_VERSION_ID"
//   value: <int64_t 'next_available_version_id'>
//
//   key: "INITDATA_UNIQUE_ORIGIN:" + <GURL 'origin'>
//   value: <empty>
//
//   key: "PRES:" + <int64_t 'purgeable_resource_id'>
//   value: <empty>
//
//   key: "REG:" + <GURL 'origin'> + '\x00' + <int64_t 'registration_id'>
//     (ex. "REG:http://example.com\x00123456")
//   value: <ServiceWorkerRegistrationData serialized as a string>
//
//   key: "REG_HAS_USER_DATA:" + <std::string 'user_data_name'> + '\x00'
//            + <int64_t 'registration_id'>
//   value: <empty>
//
//   key: "REG_USER_DATA:" + <int64_t 'registration_id'> + '\x00'
//            + <std::string user_data_name>
//     (ex. "REG_USER_DATA:123456\x00foo_bar")
//   value: <std::string user_data>
//
//   key: "RES:" + <int64_t 'version_id'> + '\x00' + <int64_t 'resource_id'>
//     (ex. "RES:123456\x00654321")
//   value: <ServiceWorkerResourceRecord serialized as a string>
//
//   key: "URES:" + <int64_t 'uncommitted_resource_id'>
//   value: <empty>
//
// Version 2
//
//   key: "REGID_TO_ORIGIN:" + <int64_t 'registration_id'>
//   value: <GURL 'origin'>
//
//   OBSOLETE: http://crbug.com/539713
//   key: "INITDATA_DISKCACHE_MIGRATION_NOT_NEEDED"
//   value: <empty>
//     - This entry represents that the diskcache uses the Simple backend and
//       does not have to do diskcache migration (http://crbug.com/487482).
//
//   OBSOLETE: http://crbug.com/539713
//   key: "INITDATA_OLD_DISKCACHE_DELETION_NOT_NEEDED"
//   value: <empty>
//     - This entry represents that the old BlockFile diskcache was deleted
//       after diskcache migration (http://crbug.com/487482).
//
//   key: "INITDATA_FOREIGN_FETCH_ORIGIN:" + <GURL 'origin'>
//   value: <empty>
namespace content {

namespace {

const char kDatabaseVersionKey[] = "INITDATA_DB_VERSION";
const char kNextRegIdKey[] = "INITDATA_NEXT_REGISTRATION_ID";
const char kNextResIdKey[] = "INITDATA_NEXT_RESOURCE_ID";
const char kNextVerIdKey[] = "INITDATA_NEXT_VERSION_ID";
const char kUniqueOriginKey[] = "INITDATA_UNIQUE_ORIGIN:";
const char kForeignFetchOriginKey[] = "INITDATA_FOREIGN_FETCH_ORIGIN:";

const char kRegKeyPrefix[] = "REG:";
const char kRegUserDataKeyPrefix[] = "REG_USER_DATA:";
const char kRegHasUserDataKeyPrefix[] = "REG_HAS_USER_DATA:";
const char kRegIdToOriginKeyPrefix[] = "REGID_TO_ORIGIN:";
const char kResKeyPrefix[] = "RES:";
const char kKeySeparator = '\x00';

const char kUncommittedResIdKeyPrefix[] = "URES:";
const char kPurgeableResIdKeyPrefix[] = "PRES:";

const int64_t kCurrentSchemaVersion = 2;

class ServiceWorkerEnv : public leveldb_env::ChromiumEnv {
 public:
  ServiceWorkerEnv()
      : ChromiumEnv("LevelDBEnv.ServiceWorker", false /* make_backup */) {}
};

base::LazyInstance<ServiceWorkerEnv>::Leaky g_service_worker_env =
    LAZY_INSTANCE_INITIALIZER;

bool RemovePrefix(const std::string& str,
                  const std::string& prefix,
                  std::string* out) {
  if (!base::StartsWith(str, prefix, base::CompareCase::SENSITIVE))
    return false;
  if (out)
    *out = str.substr(prefix.size());
  return true;
}

std::string CreateRegistrationKeyPrefix(const GURL& origin) {
  return base::StringPrintf("%s%s%c", kRegKeyPrefix,
                            origin.GetOrigin().spec().c_str(), kKeySeparator);
}

std::string CreateRegistrationKey(int64_t registration_id, const GURL& origin) {
  return CreateRegistrationKeyPrefix(origin)
      .append(base::Int64ToString(registration_id));
}

std::string CreateResourceRecordKeyPrefix(int64_t version_id) {
  return base::StringPrintf("%s%s%c",
                            kResKeyPrefix,
                            base::Int64ToString(version_id).c_str(),
                            kKeySeparator);
}

std::string CreateResourceRecordKey(int64_t version_id, int64_t resource_id) {
  return CreateResourceRecordKeyPrefix(version_id).append(
      base::Int64ToString(resource_id));
}

std::string CreateUniqueOriginKey(const GURL& origin) {
  return base::StringPrintf("%s%s", kUniqueOriginKey,
                            origin.GetOrigin().spec().c_str());
}

std::string CreateForeignFetchOriginKey(const GURL& origin) {
  return base::StringPrintf("%s%s", kForeignFetchOriginKey,
                            origin.GetOrigin().spec().c_str());
}

std::string CreateResourceIdKey(const char* key_prefix, int64_t resource_id) {
  return base::StringPrintf(
      "%s%s", key_prefix, base::Int64ToString(resource_id).c_str());
}

std::string CreateUserDataKeyPrefix(int64_t registration_id) {
  return base::StringPrintf("%s%s%c",
                            kRegUserDataKeyPrefix,
                            base::Int64ToString(registration_id).c_str(),
                            kKeySeparator);
}

std::string CreateUserDataKey(int64_t registration_id,
                              const std::string& user_data_name) {
  return CreateUserDataKeyPrefix(registration_id).append(user_data_name);
}

std::string CreateHasUserDataKeyPrefix(const std::string& user_data_name) {
  return base::StringPrintf("%s%s%c", kRegHasUserDataKeyPrefix,
                            user_data_name.c_str(), kKeySeparator);
}

std::string CreateHasUserDataKey(int64_t registration_id,
                                 const std::string& user_data_name) {
  return CreateHasUserDataKeyPrefix(user_data_name)
      .append(base::Int64ToString(registration_id));
}

std::string CreateRegistrationIdToOriginKey(int64_t registration_id) {
  return base::StringPrintf("%s%s", kRegIdToOriginKeyPrefix,
                            base::Int64ToString(registration_id).c_str());
}

void PutUniqueOriginToBatch(const GURL& origin,
                            leveldb::WriteBatch* batch) {
  // Value should be empty.
  batch->Put(CreateUniqueOriginKey(origin), "");
}

void PutPurgeableResourceIdToBatch(int64_t resource_id,
                                   leveldb::WriteBatch* batch) {
  // Value should be empty.
  batch->Put(CreateResourceIdKey(kPurgeableResIdKeyPrefix, resource_id), "");
}

void PutForeignFetchOriginToBatch(const GURL& origin,
                                  leveldb::WriteBatch* batch) {
  // Value should be empty.
  batch->Put(CreateForeignFetchOriginKey(origin), "");
}

ServiceWorkerDatabase::Status ParseId(const std::string& serialized,
                                      int64_t* out) {
  DCHECK(out);
  int64_t id;
  if (!base::StringToInt64(serialized, &id) || id < 0)
    return ServiceWorkerDatabase::STATUS_ERROR_CORRUPTED;
  *out = id;
  return ServiceWorkerDatabase::STATUS_OK;
}

ServiceWorkerDatabase::Status LevelDBStatusToStatus(
    const leveldb::Status& status) {
  if (status.ok())
    return ServiceWorkerDatabase::STATUS_OK;
  else if (status.IsNotFound())
    return ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND;
  else if (status.IsIOError())
    return ServiceWorkerDatabase::STATUS_ERROR_IO_ERROR;
  else if (status.IsCorruption())
    return ServiceWorkerDatabase::STATUS_ERROR_CORRUPTED;
  else if (status.IsNotSupportedError())
    return ServiceWorkerDatabase::STATUS_ERROR_NOT_SUPPORTED;
  else
    return ServiceWorkerDatabase::STATUS_ERROR_FAILED;
}

int64_t AccumulateResourceSizeInBytes(
    const std::vector<ServiceWorkerDatabase::ResourceRecord>& resources) {
  int64_t total_size_bytes = 0;
  for (const auto& resource : resources)
    total_size_bytes += resource.size_bytes;
  return total_size_bytes;
}

}  // namespace

const char* ServiceWorkerDatabase::StatusToString(
  ServiceWorkerDatabase::Status status) {
  switch (status) {
    case ServiceWorkerDatabase::STATUS_OK:
      return "Database OK";
    case ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND:
      return "Database not found";
    case ServiceWorkerDatabase::STATUS_ERROR_IO_ERROR:
      return "Database IO error";
    case ServiceWorkerDatabase::STATUS_ERROR_CORRUPTED:
      return "Database corrupted";
    case ServiceWorkerDatabase::STATUS_ERROR_FAILED:
      return "Database operation failed";
    case ServiceWorkerDatabase::STATUS_ERROR_NOT_SUPPORTED:
      return "Database operation not supported";
    case ServiceWorkerDatabase::STATUS_ERROR_MAX:
      NOTREACHED();
      return "Database unknown error";
  }
  NOTREACHED();
  return "Database unknown error";
}

ServiceWorkerDatabase::RegistrationData::RegistrationData()
    : registration_id(kInvalidServiceWorkerRegistrationId),
      version_id(kInvalidServiceWorkerVersionId),
      is_active(false),
      has_fetch_handler(false),
      resources_total_size_bytes(0) {
}

ServiceWorkerDatabase::RegistrationData::RegistrationData(
    const RegistrationData& other) = default;

ServiceWorkerDatabase::RegistrationData::~RegistrationData() {
}

ServiceWorkerDatabase::ServiceWorkerDatabase(const base::FilePath& path)
    : path_(path),
      next_avail_registration_id_(0),
      next_avail_resource_id_(0),
      next_avail_version_id_(0),
      state_(UNINITIALIZED) {
  sequence_checker_.DetachFromSequence();
}

ServiceWorkerDatabase::~ServiceWorkerDatabase() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  db_.reset();
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::GetNextAvailableIds(
    int64_t* next_avail_registration_id,
    int64_t* next_avail_version_id,
    int64_t* next_avail_resource_id) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(next_avail_registration_id);
  DCHECK(next_avail_version_id);
  DCHECK(next_avail_resource_id);

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status)) {
    *next_avail_registration_id = 0;
    *next_avail_version_id = 0;
    *next_avail_resource_id = 0;
    return STATUS_OK;
  }
  if (status != STATUS_OK)
    return status;

  status = ReadNextAvailableId(kNextRegIdKey, &next_avail_registration_id_);
  if (status != STATUS_OK)
    return status;
  status = ReadNextAvailableId(kNextVerIdKey, &next_avail_version_id_);
  if (status != STATUS_OK)
    return status;
  status = ReadNextAvailableId(kNextResIdKey, &next_avail_resource_id_);
  if (status != STATUS_OK)
    return status;

  *next_avail_registration_id = next_avail_registration_id_;
  *next_avail_version_id = next_avail_version_id_;
  *next_avail_resource_id = next_avail_resource_id_;
  return STATUS_OK;
}

ServiceWorkerDatabase::Status
ServiceWorkerDatabase::GetOriginsWithRegistrations(std::set<GURL>* origins) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(origins->empty());

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return STATUS_OK;
  if (status != STATUS_OK)
    return status;

  {
    std::unique_ptr<leveldb::Iterator> itr(
        db_->NewIterator(leveldb::ReadOptions()));
    for (itr->Seek(kUniqueOriginKey); itr->Valid(); itr->Next()) {
      status = LevelDBStatusToStatus(itr->status());
      if (status != STATUS_OK) {
        origins->clear();
        break;
      }

      std::string origin_str;
      if (!RemovePrefix(itr->key().ToString(), kUniqueOriginKey, &origin_str))
        break;

      GURL origin(origin_str);
      if (!origin.is_valid()) {
        status = STATUS_ERROR_CORRUPTED;
        origins->clear();
        break;
      }

      origins->insert(origin);
    }
  }

  HandleReadResult(FROM_HERE, status);
  return status;
}

ServiceWorkerDatabase::Status
ServiceWorkerDatabase::GetOriginsWithForeignFetchRegistrations(
    std::set<GURL>* origins) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(origins->empty());

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return STATUS_OK;
  if (status != STATUS_OK)
    return status;

  {
    std::unique_ptr<leveldb::Iterator> itr(
        db_->NewIterator(leveldb::ReadOptions()));
    for (itr->Seek(kForeignFetchOriginKey); itr->Valid(); itr->Next()) {
      status = LevelDBStatusToStatus(itr->status());
      if (status != STATUS_OK) {
        origins->clear();
        break;
      }

      std::string origin_str;
      if (!RemovePrefix(itr->key().ToString(), kForeignFetchOriginKey,
                        &origin_str))
        break;

      GURL origin(origin_str);
      if (!origin.is_valid()) {
        status = STATUS_ERROR_CORRUPTED;
        origins->clear();
        break;
      }

      origins->insert(origin);
    }
  }

  HandleReadResult(FROM_HERE, status);
  return status;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::GetRegistrationsForOrigin(
    const GURL& origin,
    std::vector<RegistrationData>* registrations,
    std::vector<std::vector<ResourceRecord>>* opt_resources_list) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(registrations->empty());

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return STATUS_OK;
  if (status != STATUS_OK)
    return status;

  std::string prefix = CreateRegistrationKeyPrefix(origin);
  {
    std::unique_ptr<leveldb::Iterator> itr(
        db_->NewIterator(leveldb::ReadOptions()));
    for (itr->Seek(prefix); itr->Valid(); itr->Next()) {
      status = LevelDBStatusToStatus(itr->status());
      if (status != STATUS_OK) {
        registrations->clear();
        if (opt_resources_list)
          opt_resources_list->clear();
        break;
      }

      if (!RemovePrefix(itr->key().ToString(), prefix, NULL))
        break;

      RegistrationData registration;
      status = ParseRegistrationData(itr->value().ToString(), &registration);
      if (status != STATUS_OK) {
        registrations->clear();
        if (opt_resources_list)
          opt_resources_list->clear();
        break;
      }
      registrations->push_back(registration);

      if (opt_resources_list) {
        std::vector<ResourceRecord> resources;
        status = ReadResourceRecords(registration.version_id, &resources);
        if (status != STATUS_OK) {
          registrations->clear();
          opt_resources_list->clear();
          break;
        }
        opt_resources_list->push_back(resources);
      }
    }
  }

  HandleReadResult(FROM_HERE, status);
  return status;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::GetAllRegistrations(
    std::vector<RegistrationData>* registrations) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(registrations->empty());

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return STATUS_OK;
  if (status != STATUS_OK)
    return status;

  {
    std::unique_ptr<leveldb::Iterator> itr(
        db_->NewIterator(leveldb::ReadOptions()));
    for (itr->Seek(kRegKeyPrefix); itr->Valid(); itr->Next()) {
      status = LevelDBStatusToStatus(itr->status());
      if (status != STATUS_OK) {
        registrations->clear();
        break;
      }

      if (!RemovePrefix(itr->key().ToString(), kRegKeyPrefix, NULL))
        break;

      RegistrationData registration;
      status = ParseRegistrationData(itr->value().ToString(), &registration);
      if (status != STATUS_OK) {
        registrations->clear();
        break;
      }
      registrations->push_back(registration);
    }
  }

  HandleReadResult(FROM_HERE, status);
  return status;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::ReadRegistration(
    int64_t registration_id,
    const GURL& origin,
    RegistrationData* registration,
    std::vector<ResourceRecord>* resources) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(registration);
  DCHECK(resources);

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return STATUS_ERROR_NOT_FOUND;
  if (status != STATUS_OK)
    return status;

  RegistrationData value;
  status = ReadRegistrationData(registration_id, origin, &value);
  if (status != STATUS_OK)
    return status;

  status = ReadResourceRecords(value.version_id, resources);
  if (status != STATUS_OK)
    return status;

  // ResourceRecord must contain the ServiceWorker's main script.
  if (resources->empty())
    return ServiceWorkerDatabase::STATUS_ERROR_CORRUPTED;

  *registration = value;
  return STATUS_OK;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::ReadRegistrationOrigin(
    int64_t registration_id,
    GURL* origin) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(origin);

  Status status = LazyOpen(true);
  if (IsNewOrNonexistentDatabase(status))
    return STATUS_ERROR_NOT_FOUND;
  if (status != STATUS_OK)
    return status;

  std::string value;
  status = LevelDBStatusToStatus(
      db_->Get(leveldb::ReadOptions(),
               CreateRegistrationIdToOriginKey(registration_id), &value));
  if (status != STATUS_OK) {
    HandleReadResult(FROM_HERE,
                     status == STATUS_ERROR_NOT_FOUND ? STATUS_OK : status);
    return status;
  }

  GURL parsed(value);
  if (!parsed.is_valid()) {
    status = STATUS_ERROR_CORRUPTED;
    HandleReadResult(FROM_HERE, status);
    return status;
  }

  *origin = parsed;
  HandleReadResult(FROM_HERE, STATUS_OK);
  return STATUS_OK;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::WriteRegistration(
    const RegistrationData& registration,
    const std::vector<ResourceRecord>& resources,
    RegistrationData* old_registration,
    std::vector<int64_t>* newly_purgeable_resources) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(old_registration);
  DCHECK(!resources.empty());
  Status status = LazyOpen(true);
  old_registration->version_id = kInvalidServiceWorkerVersionId;
  if (status != STATUS_OK)
    return status;

  leveldb::WriteBatch batch;
  BumpNextRegistrationIdIfNeeded(registration.registration_id, &batch);
  BumpNextVersionIdIfNeeded(registration.version_id, &batch);

  PutUniqueOriginToBatch(registration.scope.GetOrigin(), &batch);

  if (!registration.foreign_fetch_scopes.empty())
    PutForeignFetchOriginToBatch(registration.scope.GetOrigin(), &batch);

  DCHECK_EQ(AccumulateResourceSizeInBytes(resources),
            registration.resources_total_size_bytes)
      << "The total size in the registration must match the cumulative "
      << "sizes of the resources.";

  WriteRegistrationDataInBatch(registration, &batch);
  batch.Put(CreateRegistrationIdToOriginKey(registration.registration_id),
            registration.scope.GetOrigin().spec());

  // Used for avoiding multiple writes for the same resource id or url.
  std::set<int64_t> pushed_resources;
  std::set<GURL> pushed_urls;
  for (std::vector<ResourceRecord>::const_iterator itr = resources.begin();
       itr != resources.end(); ++itr) {
    if (!itr->url.is_valid())
      return STATUS_ERROR_FAILED;

    // Duplicated resource id or url should not exist.
    DCHECK(pushed_resources.insert(itr->resource_id).second);
    DCHECK(pushed_urls.insert(itr->url).second);

    WriteResourceRecordInBatch(*itr, registration.version_id, &batch);

    // Delete a resource from the uncommitted list.
    batch.Delete(CreateResourceIdKey(
        kUncommittedResIdKeyPrefix, itr->resource_id));
    // Delete from the purgeable list in case this version was once deleted.
    batch.Delete(
        CreateResourceIdKey(kPurgeableResIdKeyPrefix, itr->resource_id));
  }

  // Retrieve a previous version to sweep purgeable resources.
  status = ReadRegistrationData(registration.registration_id,
                                registration.scope.GetOrigin(),
                                old_registration);
  if (status != STATUS_OK && status != STATUS_ERROR_NOT_FOUND)
    return status;
  if (status == STATUS_OK) {
    DCHECK_LT(old_registration->version_id, registration.version_id);
    status = DeleteResourceRecords(
        old_registration->version_id, newly_purgeable_resources, &batch);
    if (status != STATUS_OK)
      return status;

    // Currently resource sharing across versions and registrations is not
    // supported, so resource ids should not be overlapped between
    // |registration| and |old_registration|.
    std::set<int64_t> deleted_resources(newly_purgeable_resources->begin(),
                                        newly_purgeable_resources->end());
    DCHECK(base::STLSetIntersection<std::set<int64_t>>(pushed_resources,
                                                       deleted_resources)
               .empty());

    // If old registration had foreign fetch scopes, but new registration
    // doesn't, the origin might have to be removed from the list of origins
    // with foreign fetch scopes.
    // TODO(mek): Like the similar check in DeleteRegistration, ideally this
    // could be done more efficiently.
    if (!old_registration->foreign_fetch_scopes.empty() &&
        registration.foreign_fetch_scopes.empty()) {
      std::vector<RegistrationData> registrations;
      status = GetRegistrationsForOrigin(registration.scope.GetOrigin(),
                                         &registrations, nullptr);
      if (status != STATUS_OK)
        return status;
      bool remaining_ff_scopes = false;
      for (const auto& existing_reg : registrations) {
        if (existing_reg.registration_id != registration.registration_id &&
            !existing_reg.foreign_fetch_scopes.empty()) {
          remaining_ff_scopes = true;
          break;
        }
      }
      if (!remaining_ff_scopes)
        batch.Delete(
            CreateForeignFetchOriginKey(registration.scope.GetOrigin()));
    }
  }

  return WriteBatch(&batch);
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::UpdateVersionToActive(
    int64_t registration_id,
    const GURL& origin) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return STATUS_ERROR_NOT_FOUND;
  if (status != STATUS_OK)
    return status;
  if (!origin.is_valid())
    return STATUS_ERROR_FAILED;

  RegistrationData registration;
  status = ReadRegistrationData(registration_id, origin, &registration);
  if (status != STATUS_OK)
    return status;

  registration.is_active = true;

  leveldb::WriteBatch batch;
  WriteRegistrationDataInBatch(registration, &batch);
  return WriteBatch(&batch);
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::UpdateLastCheckTime(
    int64_t registration_id,
    const GURL& origin,
    const base::Time& time) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return STATUS_ERROR_NOT_FOUND;
  if (status != STATUS_OK)
    return status;
  if (!origin.is_valid())
    return STATUS_ERROR_FAILED;

  RegistrationData registration;
  status = ReadRegistrationData(registration_id, origin, &registration);
  if (status != STATUS_OK)
    return status;

  registration.last_update_check = time;

  leveldb::WriteBatch batch;
  WriteRegistrationDataInBatch(registration, &batch);
  return WriteBatch(&batch);
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::DeleteRegistration(
    int64_t registration_id,
    const GURL& origin,
    RegistrationData* deleted_version,
    std::vector<int64_t>* newly_purgeable_resources) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(deleted_version);
  deleted_version->version_id = kInvalidServiceWorkerVersionId;
  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return STATUS_OK;
  if (status != STATUS_OK)
    return status;
  if (!origin.is_valid())
    return STATUS_ERROR_FAILED;

  leveldb::WriteBatch batch;

  // Remove |origin| from unique origins if a registration specified by
  // |registration_id| is the only one for |origin|.
  // TODO(nhiroki): Check the uniqueness by more efficient way.
  std::vector<RegistrationData> registrations;
  status = GetRegistrationsForOrigin(origin, &registrations, nullptr);
  if (status != STATUS_OK)
    return status;

  if (registrations.size() == 1 &&
      registrations[0].registration_id == registration_id) {
    batch.Delete(CreateUniqueOriginKey(origin));
  }

  // Remove |origin| from foreign fetch origins if a registration specified by
  // |registration_id| is the only one with foreign fetch scopes for |origin|.
  bool remaining_ff_scopes = false;
  for (const auto& registration : registrations) {
    if (registration.registration_id != registration_id &&
        !registration.foreign_fetch_scopes.empty()) {
      remaining_ff_scopes = true;
      break;
    }
  }
  if (!remaining_ff_scopes)
    batch.Delete(CreateForeignFetchOriginKey(origin));

  // Delete a registration specified by |registration_id|.
  batch.Delete(CreateRegistrationKey(registration_id, origin));
  batch.Delete(CreateRegistrationIdToOriginKey(registration_id));

  // Delete resource records and user data associated with the registration.
  for (const auto& registration : registrations) {
    if (registration.registration_id == registration_id) {
      *deleted_version = registration;
      status = DeleteResourceRecords(
          registration.version_id, newly_purgeable_resources, &batch);
      if (status != STATUS_OK)
        return status;

      status = DeleteUserDataForRegistration(registration_id, &batch);
      if (status != STATUS_OK)
        return status;
      break;
    }
  }

  return WriteBatch(&batch);
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::ReadUserData(
    int64_t registration_id,
    const std::vector<std::string>& user_data_names,
    std::vector<std::string>* user_data_values) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK_NE(kInvalidServiceWorkerRegistrationId, registration_id);
  DCHECK(!user_data_names.empty());
  DCHECK(user_data_values);

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return STATUS_ERROR_NOT_FOUND;
  if (status != STATUS_OK)
    return status;

  user_data_values->resize(user_data_names.size());
  for (size_t i = 0; i < user_data_names.size(); i++) {
    const std::string key =
        CreateUserDataKey(registration_id, user_data_names[i]);
    status = LevelDBStatusToStatus(
        db_->Get(leveldb::ReadOptions(), key, &(*user_data_values)[i]));
    if (status != STATUS_OK) {
      user_data_values->clear();
      break;
    }
  }
  HandleReadResult(FROM_HERE,
                   status == STATUS_ERROR_NOT_FOUND ? STATUS_OK : status);
  return status;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::WriteUserData(
    int64_t registration_id,
    const GURL& origin,
    const std::vector<std::pair<std::string, std::string>>& name_value_pairs) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK_NE(kInvalidServiceWorkerRegistrationId, registration_id);
  DCHECK(!name_value_pairs.empty());

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return STATUS_ERROR_NOT_FOUND;
  if (status != STATUS_OK)
    return status;

  // There should be the registration specified by |registration_id|.
  RegistrationData registration;
  status = ReadRegistrationData(registration_id, origin, &registration);
  if (status != STATUS_OK)
    return status;

  leveldb::WriteBatch batch;
  for (const auto& pair : name_value_pairs) {
    DCHECK(!pair.first.empty());
    batch.Put(CreateUserDataKey(registration_id, pair.first), pair.second);
    batch.Put(CreateHasUserDataKey(registration_id, pair.first), "");
  }
  return WriteBatch(&batch);
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::DeleteUserData(
    int64_t registration_id,
    const std::vector<std::string>& user_data_names) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK_NE(kInvalidServiceWorkerRegistrationId, registration_id);
  DCHECK(!user_data_names.empty());

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return STATUS_OK;
  if (status != STATUS_OK)
    return status;

  leveldb::WriteBatch batch;
  for (const std::string& name : user_data_names) {
    DCHECK(!name.empty());
    batch.Delete(CreateUserDataKey(registration_id, name));
    batch.Delete(CreateHasUserDataKey(registration_id, name));
  }
  return WriteBatch(&batch);
}

ServiceWorkerDatabase::Status
ServiceWorkerDatabase::ReadUserDataForAllRegistrations(
    const std::string& user_data_name,
    std::vector<std::pair<int64_t, std::string>>* user_data) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(user_data->empty());

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return STATUS_OK;
  if (status != STATUS_OK)
    return status;

  std::string key_prefix = CreateHasUserDataKeyPrefix(user_data_name);
  {
    std::unique_ptr<leveldb::Iterator> itr(
        db_->NewIterator(leveldb::ReadOptions()));
    for (itr->Seek(key_prefix); itr->Valid(); itr->Next()) {
      status = LevelDBStatusToStatus(itr->status());
      if (status != STATUS_OK) {
        user_data->clear();
        break;
      }

      std::string registration_id_string;
      if (!RemovePrefix(itr->key().ToString(), key_prefix,
                        &registration_id_string)) {
        break;
      }

      int64_t registration_id;
      status = ParseId(registration_id_string, &registration_id);
      if (status != STATUS_OK) {
        user_data->clear();
        break;
      }

      std::string value;
      status = LevelDBStatusToStatus(
          db_->Get(leveldb::ReadOptions(),
                   CreateUserDataKey(registration_id, user_data_name), &value));
      if (status != STATUS_OK) {
        user_data->clear();
        break;
      }
      user_data->push_back(std::make_pair(registration_id, value));
    }
  }

  HandleReadResult(FROM_HERE, status);
  return status;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::GetUncommittedResourceIds(
    std::set<int64_t>* ids) {
  return ReadResourceIds(kUncommittedResIdKeyPrefix, ids);
}

ServiceWorkerDatabase::Status
ServiceWorkerDatabase::WriteUncommittedResourceIds(
    const std::set<int64_t>& ids) {
  leveldb::WriteBatch batch;
  Status status =
      WriteResourceIdsInBatch(kUncommittedResIdKeyPrefix, ids, &batch);
  if (status != STATUS_OK)
    return status;
  return WriteBatch(&batch);
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::GetPurgeableResourceIds(
    std::set<int64_t>* ids) {
  return ReadResourceIds(kPurgeableResIdKeyPrefix, ids);
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::ClearPurgeableResourceIds(
    const std::set<int64_t>& ids) {
  leveldb::WriteBatch batch;
  Status status =
      DeleteResourceIdsInBatch(kPurgeableResIdKeyPrefix, ids, &batch);
  if (status != STATUS_OK)
    return status;
  return WriteBatch(&batch);
}

ServiceWorkerDatabase::Status
ServiceWorkerDatabase::PurgeUncommittedResourceIds(
    const std::set<int64_t>& ids) {
  leveldb::WriteBatch batch;
  Status status = DeleteResourceIdsInBatch(
      kUncommittedResIdKeyPrefix, ids, &batch);
  if (status != STATUS_OK)
    return status;
  status = WriteResourceIdsInBatch(kPurgeableResIdKeyPrefix, ids, &batch);
  if (status != STATUS_OK)
    return status;
  return WriteBatch(&batch);
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::DeleteAllDataForOrigins(
    const std::set<GURL>& origins,
    std::vector<int64_t>* newly_purgeable_resources) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return STATUS_OK;
  if (status != STATUS_OK)
    return status;
  leveldb::WriteBatch batch;

  for (const GURL& origin : origins) {
    if (!origin.is_valid())
      return STATUS_ERROR_FAILED;

    // Delete from the unique origin list.
    batch.Delete(CreateUniqueOriginKey(origin));

    // Delete from the foreign fetch origin list.
    batch.Delete(CreateForeignFetchOriginKey(origin));

    std::vector<RegistrationData> registrations;
    status = GetRegistrationsForOrigin(origin, &registrations, nullptr);
    if (status != STATUS_OK)
      return status;

    // Delete registrations, resource records and user data.
    for (const RegistrationData& data : registrations) {
      batch.Delete(CreateRegistrationKey(data.registration_id, origin));
      batch.Delete(CreateRegistrationIdToOriginKey(data.registration_id));

      status = DeleteResourceRecords(
          data.version_id, newly_purgeable_resources, &batch);
      if (status != STATUS_OK)
        return status;

      status = DeleteUserDataForRegistration(data.registration_id, &batch);
      if (status != STATUS_OK)
        return status;
    }
  }

  return WriteBatch(&batch);
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::DestroyDatabase() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  Disable(FROM_HERE, STATUS_OK);

  if (IsDatabaseInMemory()) {
    env_.reset();
    return STATUS_OK;
  }

  // Directly delete the database directory instead of leveldb::DestroyDB()
  // because the API does not delete the directory if there are unrelated files.
  // (https://code.google.com/p/chromium/issues/detail?id=468926#c24)
  Status status = base::DeleteFile(path_, true /* recursive */)
                      ? STATUS_OK
                      : STATUS_ERROR_FAILED;
  ServiceWorkerMetrics::RecordDestroyDatabaseResult(status);
  return status;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::LazyOpen(
    bool create_if_missing) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  // Do not try to open a database if we tried and failed once.
  if (state_ == DISABLED)
    return STATUS_ERROR_FAILED;
  if (IsOpen())
    return STATUS_OK;

  if (!create_if_missing) {
    // Avoid opening a database if it does not exist at the |path_|.
    if (IsDatabaseInMemory() || !base::PathExists(path_) ||
        base::IsDirectoryEmpty(path_)) {
      return STATUS_ERROR_NOT_FOUND;
    }
  }

  leveldb::Options options;
  options.create_if_missing = create_if_missing;
  options.reuse_logs = leveldb_env::kDefaultLogReuseOptionValue;
  if (IsDatabaseInMemory()) {
    env_.reset(leveldb::NewMemEnv(leveldb::Env::Default()));
    options.env = env_.get();
  } else {
    options.env = g_service_worker_env.Pointer();
  }

  leveldb::DB* db = NULL;
  Status status = LevelDBStatusToStatus(
      leveldb::DB::Open(options, path_.AsUTF8Unsafe(), &db));
  HandleOpenResult(FROM_HERE, status);
  if (status != STATUS_OK) {
    DCHECK(!db);
    // TODO(nhiroki): Should we retry to open the database?
    return status;
  }
  db_.reset(db);

  int64_t db_version;
  status = ReadDatabaseVersion(&db_version);
  if (status != STATUS_OK)
    return status;

  switch (db_version) {
    case 0:
      // This database is new. It will be initialized when something is written.
      DCHECK_EQ(UNINITIALIZED, state_);
      return STATUS_OK;
    case 1:
      // This database has an obsolete schema version. ServiceWorkerStorage
      // should recreate it.
      status = STATUS_ERROR_FAILED;
      Disable(FROM_HERE, status);
      return status;
    case 2:
      DCHECK_EQ(db_version, kCurrentSchemaVersion);
      state_ = INITIALIZED;
      return STATUS_OK;
    default:
      // Other cases should be handled in ReadDatabaseVersion.
      NOTREACHED();
      return STATUS_ERROR_CORRUPTED;
  }
}

bool ServiceWorkerDatabase::IsNewOrNonexistentDatabase(
    ServiceWorkerDatabase::Status status) {
  if (status == STATUS_ERROR_NOT_FOUND)
    return true;
  if (status == STATUS_OK && state_ == UNINITIALIZED)
    return true;
  return false;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::ReadNextAvailableId(
    const char* id_key,
    int64_t* next_avail_id) {
  DCHECK(id_key);
  DCHECK(next_avail_id);

  std::string value;
  Status status = LevelDBStatusToStatus(
      db_->Get(leveldb::ReadOptions(), id_key, &value));
  if (status == STATUS_ERROR_NOT_FOUND) {
    // Nobody has gotten the next resource id for |id_key|.
    *next_avail_id = 0;
    HandleReadResult(FROM_HERE, STATUS_OK);
    return STATUS_OK;
  } else if (status != STATUS_OK) {
    HandleReadResult(FROM_HERE, status);
    return status;
  }

  status = ParseId(value, next_avail_id);
  HandleReadResult(FROM_HERE, status);
  return status;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::ReadRegistrationData(
    int64_t registration_id,
    const GURL& origin,
    RegistrationData* registration) {
  DCHECK(registration);

  const std::string key = CreateRegistrationKey(registration_id, origin);
  std::string value;
  Status status = LevelDBStatusToStatus(
      db_->Get(leveldb::ReadOptions(), key, &value));
  if (status != STATUS_OK) {
    HandleReadResult(
        FROM_HERE,
        status == STATUS_ERROR_NOT_FOUND ? STATUS_OK : status);
    return status;
  }

  status = ParseRegistrationData(value, registration);
  HandleReadResult(FROM_HERE, status);
  return status;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::ParseRegistrationData(
    const std::string& serialized,
    RegistrationData* out) {
  DCHECK(out);
  ServiceWorkerRegistrationData data;
  if (!data.ParseFromString(serialized))
    return ServiceWorkerDatabase::STATUS_ERROR_CORRUPTED;

  GURL scope_url(data.scope_url());
  GURL script_url(data.script_url());
  if (!scope_url.is_valid() || !script_url.is_valid() ||
      scope_url.GetOrigin() != script_url.GetOrigin()) {
    DLOG(ERROR) << "Scope URL '" << data.scope_url() << "' and/or script url '"
                << data.script_url()
                << "' are invalid or have mismatching origins.";
    return ServiceWorkerDatabase::STATUS_ERROR_CORRUPTED;
  }

  if (data.registration_id() >= next_avail_registration_id_ ||
      data.version_id() >= next_avail_version_id_) {
    // The stored registration should not have the higher registration id or
    // version id than the next available id.
    DLOG(ERROR) << "Registration id " << data.registration_id()
                << " and/or version id " << data.version_id()
                << " is higher than the next available id.";
    return ServiceWorkerDatabase::STATUS_ERROR_CORRUPTED;
  }

  // Convert ServiceWorkerRegistrationData to RegistrationData.
  out->registration_id = data.registration_id();
  out->scope = scope_url;
  out->script = script_url;
  out->version_id = data.version_id();
  out->is_active = data.is_active();
  out->has_fetch_handler = data.has_fetch_handler();
  out->last_update_check =
      base::Time::FromInternalValue(data.last_update_check_time());
  out->resources_total_size_bytes = data.resources_total_size_bytes();
  for (int i = 0; i < data.foreign_fetch_scope_size(); ++i) {
    GURL sub_scope_url(data.foreign_fetch_scope(i));
    if (!sub_scope_url.is_valid() ||
        !ServiceWorkerUtils::ScopeMatches(scope_url, sub_scope_url)) {
      DLOG(ERROR) << "Foreign fetch scope '" << data.foreign_fetch_scope(i)
                  << "' is not valid or does not match Scope URL '" << scope_url
                  << "'.";
      return ServiceWorkerDatabase::STATUS_ERROR_CORRUPTED;
    }
    out->foreign_fetch_scopes.push_back(sub_scope_url);
  }
  for (int i = 0; i < data.foreign_fetch_origin_size(); ++i) {
    url::Origin parsed_origin(GURL(data.foreign_fetch_origin(i)));
    if (parsed_origin.unique()) {
      DLOG(ERROR) << "Foreign fetch origin '" << data.foreign_fetch_origin(i)
                  << "' is not valid.";
      return ServiceWorkerDatabase::STATUS_ERROR_CORRUPTED;
    }
    out->foreign_fetch_origins.push_back(parsed_origin);
  }

  return ServiceWorkerDatabase::STATUS_OK;
}

void ServiceWorkerDatabase::WriteRegistrationDataInBatch(
    const RegistrationData& registration,
    leveldb::WriteBatch* batch) {
  DCHECK(batch);

  // The registration id and version id should be bumped before this.
  DCHECK_GT(next_avail_registration_id_, registration.registration_id);
  DCHECK_GT(next_avail_version_id_, registration.version_id);

  // Convert RegistrationData to ServiceWorkerRegistrationData.
  ServiceWorkerRegistrationData data;
  data.set_registration_id(registration.registration_id);
  data.set_scope_url(registration.scope.spec());
  data.set_script_url(registration.script.spec());
  data.set_version_id(registration.version_id);
  data.set_is_active(registration.is_active);
  data.set_has_fetch_handler(registration.has_fetch_handler);
  data.set_last_update_check_time(
      registration.last_update_check.ToInternalValue());
  data.set_resources_total_size_bytes(registration.resources_total_size_bytes);
  for (const GURL& url : registration.foreign_fetch_scopes) {
    DCHECK(ServiceWorkerUtils::ScopeMatches(registration.scope, url))
        << "Foreign fetch scope '" << url
        << "' does not match service worker scope '" << registration.scope
        << "'.";
    data.add_foreign_fetch_scope(url.spec());
  }
  for (const url::Origin& origin : registration.foreign_fetch_origins)
    data.add_foreign_fetch_origin(origin.Serialize());

  std::string value;
  bool success = data.SerializeToString(&value);
  DCHECK(success);
  GURL origin = registration.scope.GetOrigin();
  batch->Put(CreateRegistrationKey(data.registration_id(), origin), value);
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::ReadResourceRecords(
    int64_t version_id,
    std::vector<ResourceRecord>* resources) {
  DCHECK(resources->empty());

  Status status = STATUS_OK;
  const std::string prefix = CreateResourceRecordKeyPrefix(version_id);

  {
    std::unique_ptr<leveldb::Iterator> itr(
        db_->NewIterator(leveldb::ReadOptions()));
    for (itr->Seek(prefix); itr->Valid(); itr->Next()) {
      Status status = LevelDBStatusToStatus(itr->status());
      if (status != STATUS_OK) {
        resources->clear();
        break;
      }

      if (!RemovePrefix(itr->key().ToString(), prefix, NULL))
        break;

      ResourceRecord resource;
      status = ParseResourceRecord(itr->value().ToString(), &resource);
      if (status != STATUS_OK) {
        resources->clear();
        break;
      }
      resources->push_back(resource);
    }
  }

  HandleReadResult(FROM_HERE, status);
  return status;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::ParseResourceRecord(
    const std::string& serialized,
    ServiceWorkerDatabase::ResourceRecord* out) {
  DCHECK(out);
  ServiceWorkerResourceRecord record;
  if (!record.ParseFromString(serialized))
    return ServiceWorkerDatabase::STATUS_ERROR_CORRUPTED;

  GURL url(record.url());
  if (!url.is_valid())
    return ServiceWorkerDatabase::STATUS_ERROR_CORRUPTED;

  if (record.resource_id() >= next_avail_resource_id_) {
    // The stored resource should not have a higher resource id than the next
    // available resource id.
    return ServiceWorkerDatabase::STATUS_ERROR_CORRUPTED;
  }

  // Convert ServiceWorkerResourceRecord to ResourceRecord.
  out->resource_id = record.resource_id();
  out->url = url;
  out->size_bytes = record.size_bytes();
  return ServiceWorkerDatabase::STATUS_OK;
}

void ServiceWorkerDatabase::WriteResourceRecordInBatch(
    const ResourceRecord& resource,
    int64_t version_id,
    leveldb::WriteBatch* batch) {
  DCHECK(batch);
  DCHECK_GE(resource.size_bytes, 0);

  // The next available resource id should be bumped when a resource is recorded
  // in the uncommitted list and this should be nop. However, we attempt it here
  // for some unit tests that bypass WriteUncommittedResourceIds() when setting
  // up a dummy registration. Otherwise, tests fail due to a corruption error in
  // ParseResourceRecord().
  // TODO(nhiroki): Remove this hack by making tests appropriately set up
  // uncommitted resource ids before writing a registration.
  BumpNextResourceIdIfNeeded(resource.resource_id, batch);

  // Convert ResourceRecord to ServiceWorkerResourceRecord.
  ServiceWorkerResourceRecord data;
  data.set_resource_id(resource.resource_id);
  data.set_url(resource.url.spec());
  data.set_size_bytes(resource.size_bytes);

  std::string value;
  bool success = data.SerializeToString(&value);
  DCHECK(success);
  batch->Put(CreateResourceRecordKey(version_id, data.resource_id()), value);
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::DeleteResourceRecords(
    int64_t version_id,
    std::vector<int64_t>* newly_purgeable_resources,
    leveldb::WriteBatch* batch) {
  DCHECK(batch);

  Status status = STATUS_OK;
  const std::string prefix = CreateResourceRecordKeyPrefix(version_id);

  {
    std::unique_ptr<leveldb::Iterator> itr(
        db_->NewIterator(leveldb::ReadOptions()));
    for (itr->Seek(prefix); itr->Valid(); itr->Next()) {
      status = LevelDBStatusToStatus(itr->status());
      if (status != STATUS_OK)
        break;

      const std::string key = itr->key().ToString();
      std::string unprefixed;
      if (!RemovePrefix(key, prefix, &unprefixed))
        break;

      int64_t resource_id;
      status = ParseId(unprefixed, &resource_id);
      if (status != STATUS_OK)
        break;

      // Remove a resource record.
      batch->Delete(key);

      // Currently resource sharing across versions and registrations is not
      // supported, so we can purge this without caring about it.
      PutPurgeableResourceIdToBatch(resource_id, batch);
      newly_purgeable_resources->push_back(resource_id);
    }
  }

  HandleReadResult(FROM_HERE, status);
  return status;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::ReadResourceIds(
    const char* id_key_prefix,
    std::set<int64_t>* ids) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(id_key_prefix);
  DCHECK(ids->empty());

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return STATUS_OK;
  if (status != STATUS_OK)
    return status;

  {
    std::unique_ptr<leveldb::Iterator> itr(
        db_->NewIterator(leveldb::ReadOptions()));
    for (itr->Seek(id_key_prefix); itr->Valid(); itr->Next()) {
      status = LevelDBStatusToStatus(itr->status());
      if (status != STATUS_OK) {
        ids->clear();
        break;
      }

      std::string unprefixed;
      if (!RemovePrefix(itr->key().ToString(), id_key_prefix, &unprefixed))
        break;

      int64_t resource_id;
      status = ParseId(unprefixed, &resource_id);
      if (status != STATUS_OK) {
        ids->clear();
        break;
      }
      ids->insert(resource_id);
    }
  }

  HandleReadResult(FROM_HERE, status);
  return status;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::WriteResourceIdsInBatch(
    const char* id_key_prefix,
    const std::set<int64_t>& ids,
    leveldb::WriteBatch* batch) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(id_key_prefix);

  Status status = LazyOpen(true);
  if (status != STATUS_OK)
    return status;

  if (ids.empty())
    return STATUS_OK;
  for (std::set<int64_t>::const_iterator itr = ids.begin(); itr != ids.end();
       ++itr) {
    // Value should be empty.
    batch->Put(CreateResourceIdKey(id_key_prefix, *itr), "");
  }
  // std::set is sorted, so the last element is the largest.
  BumpNextResourceIdIfNeeded(*ids.rbegin(), batch);
  return STATUS_OK;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::DeleteResourceIdsInBatch(
    const char* id_key_prefix,
    const std::set<int64_t>& ids,
    leveldb::WriteBatch* batch) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(id_key_prefix);

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return STATUS_OK;
  if (status != STATUS_OK)
    return status;

  for (std::set<int64_t>::const_iterator itr = ids.begin(); itr != ids.end();
       ++itr) {
    batch->Delete(CreateResourceIdKey(id_key_prefix, *itr));
  }
  return STATUS_OK;
}

ServiceWorkerDatabase::Status
ServiceWorkerDatabase::DeleteUserDataForRegistration(
    int64_t registration_id,
    leveldb::WriteBatch* batch) {
  DCHECK(batch);
  Status status = STATUS_OK;
  const std::string prefix = CreateUserDataKeyPrefix(registration_id);

  {
    std::unique_ptr<leveldb::Iterator> itr(
        db_->NewIterator(leveldb::ReadOptions()));
    for (itr->Seek(prefix); itr->Valid(); itr->Next()) {
      status = LevelDBStatusToStatus(itr->status());
      if (status != STATUS_OK)
        break;

      const std::string key = itr->key().ToString();
      std::string user_data_name;
      if (!RemovePrefix(key, prefix, &user_data_name))
        break;
      batch->Delete(key);
      batch->Delete(CreateHasUserDataKey(registration_id, user_data_name));
    }
  }

  HandleReadResult(FROM_HERE, status);
  return status;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::ReadDatabaseVersion(
    int64_t* db_version) {
  std::string value;
  Status status = LevelDBStatusToStatus(
      db_->Get(leveldb::ReadOptions(), kDatabaseVersionKey, &value));
  if (status == STATUS_ERROR_NOT_FOUND) {
    // The database hasn't been initialized yet.
    *db_version = 0;
    HandleReadResult(FROM_HERE, STATUS_OK);
    return STATUS_OK;
  }

  if (status != STATUS_OK) {
    HandleReadResult(FROM_HERE, status);
    return status;
  }

  const int kFirstValidVersion = 1;
  if (!base::StringToInt64(value, db_version) ||
      *db_version < kFirstValidVersion || kCurrentSchemaVersion < *db_version) {
    status = STATUS_ERROR_CORRUPTED;
    HandleReadResult(FROM_HERE, status);
    return status;
  }

  status = STATUS_OK;
  HandleReadResult(FROM_HERE, status);
  return status;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::WriteBatch(
    leveldb::WriteBatch* batch) {
  DCHECK(batch);
  DCHECK_NE(DISABLED, state_);

  if (state_ == UNINITIALIZED) {
    // Write database default values.
    batch->Put(kDatabaseVersionKey, base::Int64ToString(kCurrentSchemaVersion));
    state_ = INITIALIZED;
  }

  Status status = LevelDBStatusToStatus(
      db_->Write(leveldb::WriteOptions(), batch));
  HandleWriteResult(FROM_HERE, status);
  return status;
}

void ServiceWorkerDatabase::BumpNextRegistrationIdIfNeeded(
    int64_t used_id,
    leveldb::WriteBatch* batch) {
  DCHECK(batch);
  if (next_avail_registration_id_ <= used_id) {
    next_avail_registration_id_ = used_id + 1;
    batch->Put(kNextRegIdKey, base::Int64ToString(next_avail_registration_id_));
  }
}

void ServiceWorkerDatabase::BumpNextResourceIdIfNeeded(
    int64_t used_id,
    leveldb::WriteBatch* batch) {
  DCHECK(batch);
  if (next_avail_resource_id_ <= used_id) {
    next_avail_resource_id_ = used_id + 1;
    batch->Put(kNextResIdKey, base::Int64ToString(next_avail_resource_id_));
  }
}

void ServiceWorkerDatabase::BumpNextVersionIdIfNeeded(
    int64_t used_id,
    leveldb::WriteBatch* batch) {
  DCHECK(batch);
  if (next_avail_version_id_ <= used_id) {
    next_avail_version_id_ = used_id + 1;
    batch->Put(kNextVerIdKey, base::Int64ToString(next_avail_version_id_));
  }
}

bool ServiceWorkerDatabase::IsOpen() {
  return db_ != NULL;
}

void ServiceWorkerDatabase::Disable(
    const tracked_objects::Location& from_here,
    Status status) {
  if (status != STATUS_OK) {
    DLOG(ERROR) << "Failed at: " << from_here.ToString()
                << " with error: " << StatusToString(status);
    DLOG(ERROR) << "ServiceWorkerDatabase is disabled.";
  }
  state_ = DISABLED;
  db_.reset();
}

void ServiceWorkerDatabase::HandleOpenResult(
    const tracked_objects::Location& from_here,
    Status status) {
  if (status != STATUS_OK)
    Disable(from_here, status);
  ServiceWorkerMetrics::CountOpenDatabaseResult(status);
}

void ServiceWorkerDatabase::HandleReadResult(
    const tracked_objects::Location& from_here,
    Status status) {
  if (status != STATUS_OK)
    Disable(from_here, status);
  ServiceWorkerMetrics::CountReadDatabaseResult(status);
}

void ServiceWorkerDatabase::HandleWriteResult(
    const tracked_objects::Location& from_here,
    Status status) {
  if (status != STATUS_OK)
    Disable(from_here, status);
  ServiceWorkerMetrics::CountWriteDatabaseResult(status);
}

bool ServiceWorkerDatabase::IsDatabaseInMemory() const {
  return path_.empty();
}

}  // namespace content
