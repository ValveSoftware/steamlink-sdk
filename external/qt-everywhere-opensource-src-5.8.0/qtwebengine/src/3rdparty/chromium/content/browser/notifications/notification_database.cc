// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/notification_database.h"

#include <string>

#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "content/browser/notifications/notification_database_data_conversions.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_database_data.h"
#include "storage/common/database/database_identifier.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/helpers/memenv/memenv.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"
#include "third_party/leveldatabase/src/include/leveldb/filter_policy.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"
#include "url/gurl.h"

// Notification LevelDB database schema (in alphabetized order)
// =======================
//
// key: "DATA:" <origin identifier> '\x00' <notification_id>
// value: String containing the NotificationDatabaseDataProto protocol buffer
//        in serialized form.
//
// key: "NEXT_NOTIFICATION_ID"
// value: Decimal string which fits into an int64_t.

namespace content {
namespace {

// Keys of the fields defined in the database.
const char kNextNotificationIdKey[] = "NEXT_NOTIFICATION_ID";
const char kDataKeyPrefix[] = "DATA:";

// Separates the components of compound keys.
const char kKeySeparator = '\x00';

// The first notification id which to be handed out by the database.
const int64_t kFirstNotificationId = 1;

// Converts the LevelDB |status| to one of the notification database's values.
NotificationDatabase::Status LevelDBStatusToStatus(
    const leveldb::Status& status) {
  if (status.ok())
    return NotificationDatabase::STATUS_OK;
  else if (status.IsNotFound())
    return NotificationDatabase::STATUS_ERROR_NOT_FOUND;
  else if (status.IsCorruption())
    return NotificationDatabase::STATUS_ERROR_CORRUPTED;
  else if (status.IsIOError())
    return NotificationDatabase::STATUS_IO_ERROR;
  else if (status.IsNotSupportedError())
    return NotificationDatabase::STATUS_NOT_SUPPORTED;
  else if (status.IsInvalidArgument())
    return NotificationDatabase::STATUS_INVALID_ARGUMENT;

  return NotificationDatabase::STATUS_ERROR_FAILED;
}

// Creates a prefix for the data entries based on |origin|.
std::string CreateDataPrefix(const GURL& origin) {
  if (!origin.is_valid())
    return kDataKeyPrefix;

  return base::StringPrintf("%s%s%c", kDataKeyPrefix,
                            storage::GetIdentifierFromOrigin(origin).c_str(),
                            kKeySeparator);
}

// Creates the compound data key in which notification data is stored.
std::string CreateDataKey(const GURL& origin, int64_t notification_id) {
  DCHECK(origin.is_valid());
  return CreateDataPrefix(origin) + base::Int64ToString(notification_id);
}

// Deserializes data in |serialized_data| to |notification_database_data|.
// Will return if the deserialization was successful.
NotificationDatabase::Status DeserializedNotificationData(
    const std::string& serialized_data,
    NotificationDatabaseData* notification_database_data) {
  DCHECK(notification_database_data);
  if (DeserializeNotificationDatabaseData(serialized_data,
                                          notification_database_data)) {
    return NotificationDatabase::STATUS_OK;
  }

  DLOG(ERROR) << "Unable to deserialize a notification's data.";
  return NotificationDatabase::STATUS_ERROR_CORRUPTED;
}

}  // namespace

NotificationDatabase::NotificationDatabase(const base::FilePath& path)
    : path_(path) {}

NotificationDatabase::~NotificationDatabase() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
}

NotificationDatabase::Status NotificationDatabase::Open(
    bool create_if_missing) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK_EQ(STATE_UNINITIALIZED, state_);

  if (!create_if_missing) {
    if (IsInMemoryDatabase() || !base::PathExists(path_) ||
        base::IsDirectoryEmpty(path_)) {
      return NotificationDatabase::STATUS_ERROR_NOT_FOUND;
    }
  }

  filter_policy_.reset(leveldb::NewBloomFilterPolicy(10));

  leveldb::Options options;
  options.create_if_missing = create_if_missing;
  options.paranoid_checks = true;
  options.reuse_logs = leveldb_env::kDefaultLogReuseOptionValue;
  options.filter_policy = filter_policy_.get();
  if (IsInMemoryDatabase()) {
    env_.reset(leveldb::NewMemEnv(leveldb::Env::Default()));
    options.env = env_.get();
  }

  leveldb::DB* db = nullptr;
  Status status = LevelDBStatusToStatus(
      leveldb::DB::Open(options, path_.AsUTF8Unsafe(), &db));
  if (status != STATUS_OK)
    return status;

  state_ = STATE_INITIALIZED;
  db_.reset(db);

  return ReadNextNotificationId();
}

NotificationDatabase::Status NotificationDatabase::ReadNotificationData(
    int64_t notification_id,
    const GURL& origin,
    NotificationDatabaseData* notification_database_data) const {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK_EQ(STATE_INITIALIZED, state_);
  DCHECK_GE(notification_id, kFirstNotificationId);
  DCHECK(origin.is_valid());
  DCHECK(notification_database_data);

  std::string key = CreateDataKey(origin, notification_id);
  std::string serialized_data;

  Status status = LevelDBStatusToStatus(
      db_->Get(leveldb::ReadOptions(), key, &serialized_data));
  if (status != STATUS_OK)
    return status;

  return DeserializedNotificationData(serialized_data,
                                      notification_database_data);
}

NotificationDatabase::Status NotificationDatabase::ReadAllNotificationData(
    std::vector<NotificationDatabaseData>* notification_data_vector) const {
  return ReadAllNotificationDataInternal(GURL() /* origin */,
                                         kInvalidServiceWorkerRegistrationId,
                                         notification_data_vector);
}

NotificationDatabase::Status
NotificationDatabase::ReadAllNotificationDataForOrigin(
    const GURL& origin,
    std::vector<NotificationDatabaseData>* notification_data_vector) const {
  return ReadAllNotificationDataInternal(
      origin, kInvalidServiceWorkerRegistrationId, notification_data_vector);
}

NotificationDatabase::Status
NotificationDatabase::ReadAllNotificationDataForServiceWorkerRegistration(
    const GURL& origin,
    int64_t service_worker_registration_id,
    std::vector<NotificationDatabaseData>* notification_data_vector) const {
  return ReadAllNotificationDataInternal(origin, service_worker_registration_id,
                                         notification_data_vector);
}

NotificationDatabase::Status NotificationDatabase::WriteNotificationData(
    const GURL& origin,
    const NotificationDatabaseData& notification_database_data,
    int64_t* notification_id) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK_EQ(STATE_INITIALIZED, state_);
  DCHECK(notification_id);
  DCHECK(origin.is_valid());

  DCHECK_GE(next_notification_id_, kFirstNotificationId);

  NotificationDatabaseData storage_data = notification_database_data;
  storage_data.notification_id = next_notification_id_;

  std::string serialized_data;
  if (!SerializeNotificationDatabaseData(storage_data, &serialized_data)) {
    DLOG(ERROR) << "Unable to serialize data for a notification belonging "
                << "to: " << origin;
    return STATUS_ERROR_FAILED;
  }

  leveldb::WriteBatch batch;
  batch.Put(CreateDataKey(origin, next_notification_id_), serialized_data);
  batch.Put(kNextNotificationIdKey,
            base::Int64ToString(next_notification_id_ + 1));

  Status status =
      LevelDBStatusToStatus(db_->Write(leveldb::WriteOptions(), &batch));
  if (status != STATUS_OK)
    return status;

  *notification_id = next_notification_id_++;
  return STATUS_OK;
}

NotificationDatabase::Status NotificationDatabase::DeleteNotificationData(
    int64_t notification_id,
    const GURL& origin) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK_EQ(STATE_INITIALIZED, state_);
  DCHECK_GE(notification_id, kFirstNotificationId);
  DCHECK(origin.is_valid());

  std::string key = CreateDataKey(origin, notification_id);
  return LevelDBStatusToStatus(db_->Delete(leveldb::WriteOptions(), key));
}

NotificationDatabase::Status
NotificationDatabase::DeleteAllNotificationDataForOrigin(
    const GURL& origin,
    std::set<int64_t>* deleted_notification_set) {
  return DeleteAllNotificationDataInternal(
      origin, kInvalidServiceWorkerRegistrationId, deleted_notification_set);
}

NotificationDatabase::Status
NotificationDatabase::DeleteAllNotificationDataForServiceWorkerRegistration(
    const GURL& origin,
    int64_t service_worker_registration_id,
    std::set<int64_t>* deleted_notification_set) {
  return DeleteAllNotificationDataInternal(
      origin, service_worker_registration_id, deleted_notification_set);
}

NotificationDatabase::Status NotificationDatabase::Destroy() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  leveldb::Options options;
  if (IsInMemoryDatabase()) {
    if (!env_)
      return STATUS_OK;  // The database has not been initialized.

    options.env = env_.get();
  }

  state_ = STATE_DISABLED;
  db_.reset();

  return LevelDBStatusToStatus(
      leveldb::DestroyDB(path_.AsUTF8Unsafe(), options));
}

NotificationDatabase::Status NotificationDatabase::ReadNextNotificationId() {
  std::string value;
  Status status = LevelDBStatusToStatus(
      db_->Get(leveldb::ReadOptions(), kNextNotificationIdKey, &value));

  if (status == STATUS_ERROR_NOT_FOUND) {
    next_notification_id_ = kFirstNotificationId;
    return STATUS_OK;
  }

  if (status != STATUS_OK)
    return status;

  if (!base::StringToInt64(value, &next_notification_id_) ||
      next_notification_id_ < kFirstNotificationId) {
    return STATUS_ERROR_CORRUPTED;
  }

  return STATUS_OK;
}

NotificationDatabase::Status
NotificationDatabase::ReadAllNotificationDataInternal(
    const GURL& origin,
    int64_t service_worker_registration_id,
    std::vector<NotificationDatabaseData>* notification_data_vector) const {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(notification_data_vector);

  const std::string prefix = CreateDataPrefix(origin);

  leveldb::Slice prefix_slice(prefix);

  NotificationDatabaseData notification_database_data;
  std::unique_ptr<leveldb::Iterator> iter(
      db_->NewIterator(leveldb::ReadOptions()));
  for (iter->Seek(prefix_slice); iter->Valid(); iter->Next()) {
    if (!iter->key().starts_with(prefix_slice))
      break;

    Status status = DeserializedNotificationData(iter->value().ToString(),
                                                 &notification_database_data);
    if (status != STATUS_OK)
      return status;

    if (service_worker_registration_id != kInvalidServiceWorkerRegistrationId &&
        notification_database_data.service_worker_registration_id !=
            service_worker_registration_id) {
      continue;
    }

    notification_data_vector->push_back(notification_database_data);
  }

  return LevelDBStatusToStatus(iter->status());
}

NotificationDatabase::Status
NotificationDatabase::DeleteAllNotificationDataInternal(
    const GURL& origin,
    int64_t service_worker_registration_id,
    std::set<int64_t>* deleted_notification_set) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(deleted_notification_set);
  DCHECK(origin.is_valid());

  const std::string prefix = CreateDataPrefix(origin);

  leveldb::Slice prefix_slice(prefix);
  leveldb::WriteBatch batch;

  NotificationDatabaseData notification_database_data;
  std::unique_ptr<leveldb::Iterator> iter(
      db_->NewIterator(leveldb::ReadOptions()));
  for (iter->Seek(prefix_slice); iter->Valid(); iter->Next()) {
    if (!iter->key().starts_with(prefix_slice))
      break;

    if (service_worker_registration_id != kInvalidServiceWorkerRegistrationId) {
      Status status = DeserializedNotificationData(iter->value().ToString(),
                                                   &notification_database_data);
      if (status != STATUS_OK)
        return status;

      if (notification_database_data.service_worker_registration_id !=
          service_worker_registration_id) {
        continue;
      }
    }

    leveldb::Slice notification_id_slice = iter->key();
    notification_id_slice.remove_prefix(prefix_slice.size());

    int64_t notification_id = 0;
    if (!base::StringToInt64(notification_id_slice.ToString(),
                             &notification_id)) {
      return STATUS_ERROR_CORRUPTED;
    }

    deleted_notification_set->insert(notification_id);
    batch.Delete(iter->key());
  }

  if (deleted_notification_set->empty())
    return STATUS_OK;

  return LevelDBStatusToStatus(db_->Write(leveldb::WriteOptions(), &batch));
}

}  // namespace content
