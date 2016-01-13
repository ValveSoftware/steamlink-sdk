// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DATABASE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DATABASE_H_

#include <map>
#include <set>
#include <vector>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "url/gurl.h"

namespace leveldb {
class DB;
class Env;
class Status;
class WriteBatch;
}

namespace content {

// Class to persist serviceworker registration data in a database.
// Should NOT be used on the IO thread since this does blocking
// file io. The ServiceWorkerStorage class owns this class and
// is responsible for only calling it serially on background
// non-IO threads (ala SequencedWorkerPool).
class CONTENT_EXPORT ServiceWorkerDatabase {
 public:
  // We do leveldb stuff in |path| or in memory if |path| is empty.
  explicit ServiceWorkerDatabase(const base::FilePath& path);
  ~ServiceWorkerDatabase();

  // Used in UMA. A new value must be appended only.
  enum Status {
    STATUS_OK,
    STATUS_ERROR_NOT_FOUND,
    STATUS_ERROR_IO_ERROR,
    STATUS_ERROR_CORRUPTED,
    STATUS_ERROR_FAILED,
    STATUS_ERROR_MAX,
  };

  struct CONTENT_EXPORT RegistrationData {
    // These values are immutable for the life of a registration.
    int64 registration_id;
    GURL scope;
    GURL script;

    // Versions are first stored once they successfully install and become
    // the waiting version. Then transition to the active version. The stored
    // version may be in the ACTIVE state or in the INSTALLED state.
    int64 version_id;
    bool is_active;
    bool has_fetch_handler;
    base::Time last_update_check;

    RegistrationData();
    ~RegistrationData();
  };

  struct ResourceRecord {
    int64 resource_id;
    GURL url;

    ResourceRecord() {}
    ResourceRecord(int64 id, GURL url) : resource_id(id), url(url) {}
  };

  // Reads next available ids from the database. Returns OK if they are
  // successfully read. Fills the arguments with an initial value and returns
  // OK if they are not found in the database. Otherwise, returns an error.
  Status GetNextAvailableIds(
      int64* next_avail_registration_id,
      int64* next_avail_version_id,
      int64* next_avail_resource_id);

  // Reads origins that have one or more than one registration from the
  // database. Returns OK if they are successfully read or not found.
  // Otherwise, returns an error.
  Status GetOriginsWithRegistrations(std::set<GURL>* origins);

  // Reads registrations for |origin| from the database. Returns OK if they are
  // successfully read or not found. Otherwise, returns an error.
  Status GetRegistrationsForOrigin(
      const GURL& origin,
      std::vector<RegistrationData>* registrations);

  // Reads all registrations from the database. Returns OK if successfully read
  // or not found. Otherwise, returns an error.
  Status GetAllRegistrations(std::vector<RegistrationData>* registrations);

  // Saving, retrieving, and updating registration data.
  // (will bump next_avail_xxxx_ids as needed)
  // (resource ids will be added/removed from the uncommitted/purgeable
  // lists as needed)

  // Reads a registration for |registration_id| and resource records associated
  // with it from the database. Returns OK if they are successfully read.
  // Otherwise, returns an error.
  Status ReadRegistration(
      int64 registration_id,
      const GURL& origin,
      RegistrationData* registration,
      std::vector<ResourceRecord>* resources);

  // Writes |registration| and |resources| into the database and does following
  // things:
  //   - Deletes an old version of the registration if exists.
  //   - Bumps the next registration id and the next version id if needed.
  //   - Removes |resources| from the uncommitted list if exist.
  // Returns OK they are successfully written. Otherwise, returns an error.
  Status WriteRegistration(
      const RegistrationData& registration,
      const std::vector<ResourceRecord>& resources,
      std::vector<int64>* newly_purgeable_resources);

  // Updates a registration for |registration_id| to an active state. Returns OK
  // if it's successfully updated. Otherwise, returns an error.
  Status UpdateVersionToActive(
      int64 registration_id,
      const GURL& origin);

  // Updates last check time of a registration for |registration_id| by |time|.
  // Returns OK if it's successfully updated. Otherwise, returns an error.
  Status UpdateLastCheckTime(
      int64 registration_id,
      const GURL& origin,
      const base::Time& time);

  // Deletes a registration for |registration_id| and moves resource records
  // associated with it into the purgeable list. Returns OK if it's successfully
  // deleted or not found in the database. Otherwise, returns an error.
  Status DeleteRegistration(
      int64 registration_id,
      const GURL& origin,
      std::vector<int64>* newly_purgeable_resources);

  // As new resources are put into the diskcache, they go into an uncommitted
  // list. When a registration is saved that refers to those ids, they're
  // removed from that list. When a resource no longer has any registrations or
  // caches referring to it, it's added to the purgeable list. Periodically,
  // the purgeable list can be purged from the diskcache. At system startup, all
  // uncommitted ids are moved to the purgeable list.

  // Reads uncommitted resource ids from the database. Returns OK on success.
  // Otherwise clears |ids| and returns an error.
  Status GetUncommittedResourceIds(std::set<int64>* ids);

  // Writes |ids| into the database as uncommitted resources. Returns OK on
  // success. Otherwise writes nothing and returns an error.
  Status WriteUncommittedResourceIds(const std::set<int64>& ids);

  // Deletes uncommitted resource ids specified by |ids| from the database.
  // Returns OK on success. Otherwise deletes nothing and returns an error.
  Status ClearUncommittedResourceIds(const std::set<int64>& ids);

  // Reads purgeable resource ids from the database. Returns OK on success.
  // Otherwise clears |ids| and returns an error.
  Status GetPurgeableResourceIds(std::set<int64>* ids);

  // Writes |ids| into the database as purgeable resources. Returns OK on
  // success. Otherwise writes nothing and returns an error.
  Status WritePurgeableResourceIds(const std::set<int64>& ids);

  // Deletes purgeable resource ids specified by |ids| from the database.
  // Returns OK on success. Otherwise deletes nothing and returns an error.
  Status ClearPurgeableResourceIds(const std::set<int64>& ids);

  // Moves |ids| from the uncommitted list to the purgeable list.
  // Returns OK on success. Otherwise deletes nothing and returns an error.
  Status PurgeUncommittedResourceIds(const std::set<int64>& ids);

  // Deletes all data for |origin|, namely, unique origin, registrations and
  // resource records. Resources are moved to the purgeable list. Returns OK if
  // they are successfully deleted or not found in the database. Otherwise,
  // returns an error.
  Status DeleteAllDataForOrigin(
      const GURL& origin,
      std::vector<int64>* newly_purgeable_resources);

  // Completely deletes the contents of the database.
  // Be careful using this function.
  Status DestroyDatabase();

 private:
  // Opens the database at the |path_|. This is lazily called when the first
  // database API is called. Returns OK if the database is successfully opened.
  // Returns NOT_FOUND if the database does not exist and |create_if_missing| is
  // false. Otherwise, returns an error.
  Status LazyOpen(bool create_if_missing);

  // Helper for LazyOpen(). |status| must be the return value from LazyOpen()
  // and this must be called just after LazyOpen() is called. Returns true if
  // the database is new or nonexistent, that is, it has never been used.
  bool IsNewOrNonexistentDatabase(Status status);

  // Reads the next available id for |id_key|. Returns OK if it's successfully
  // read. Fills |next_avail_id| with an initial value and returns OK if it's
  // not found in the database. Otherwise, returns an error.
  Status ReadNextAvailableId(
      const char* id_key,
      int64* next_avail_id);

  // Reads registration data for |registration_id| from the database. Returns OK
  // if successfully reads. Otherwise, returns an error.
  Status ReadRegistrationData(
      int64 registration_id,
      const GURL& origin,
      RegistrationData* registration);

  // Reads resource records for |version_id| from the database. Returns OK if
  // it's successfully read or not found in the database. Otherwise, returns an
  // error.
  Status ReadResourceRecords(
      int64 version_id,
      std::vector<ResourceRecord>* resources);

  // Deletes resource records for |version_id| from the database. Returns OK if
  // they are successfully deleted or not found in the database. Otherwise,
  // returns an error.
  Status DeleteResourceRecords(
      int64 version_id,
      std::vector<int64>* newly_purgeable_resources,
      leveldb::WriteBatch* batch);

  // Reads resource ids for |id_key_prefix| from the database. Returns OK if
  // it's successfully read or not found in the database. Otherwise, returns an
  // error.
  Status ReadResourceIds(
      const char* id_key_prefix,
      std::set<int64>* ids);

  // Write resource ids for |id_key_prefix| into the database. Returns OK on
  // success. Otherwise, returns writes nothing and returns an error.
  Status WriteResourceIds(
      const char* id_key_prefix,
      const std::set<int64>& ids);
  Status WriteResourceIdsInBatch(
      const char* id_key_prefix,
      const std::set<int64>& ids,
      leveldb::WriteBatch* batch);

  // Deletes resource ids for |id_key_prefix| from the database. Returns OK if
  // it's successfully deleted or not found in the database. Otherwise, returns
  // an error.
  Status DeleteResourceIds(
      const char* id_key_prefix,
      const std::set<int64>& ids);
  Status DeleteResourceIdsInBatch(
      const char* id_key_prefix,
      const std::set<int64>& ids,
      leveldb::WriteBatch* batch);

  // Reads the current schema version from the database. If the database hasn't
  // been written anything yet, sets |db_version| to 0 and returns OK.
  Status ReadDatabaseVersion(int64* db_version);

  // Writes a batch into the database.
  // NOTE: You must call this when you want to put something into the database
  // because this initializes the database if needed.
  Status WriteBatch(leveldb::WriteBatch* batch);

  // Bumps the next available id if |used_id| is greater than or equal to the
  // cached one.
  void BumpNextRegistrationIdIfNeeded(
      int64 used_id,
      leveldb::WriteBatch* batch);
  void BumpNextVersionIdIfNeeded(
      int64 used_id,
      leveldb::WriteBatch* batch);

  bool IsOpen();

  void Disable(
      const tracked_objects::Location& from_here,
      Status status);
  void HandleOpenResult(
      const tracked_objects::Location& from_here,
      Status status);
  void HandleReadResult(
      const tracked_objects::Location& from_here,
      Status status);
  void HandleWriteResult(
      const tracked_objects::Location& from_here,
      Status status);

  base::FilePath path_;
  scoped_ptr<leveldb::Env> env_;
  scoped_ptr<leveldb::DB> db_;

  int64 next_avail_registration_id_;
  int64 next_avail_resource_id_;
  int64 next_avail_version_id_;

  enum State {
    UNINITIALIZED,
    INITIALIZED,
    DISABLED,
  };
  State state_;

  base::SequenceChecker sequence_checker_;

  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDatabaseTest, OpenDatabase);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDatabaseTest, OpenDatabase_InMemory);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDatabaseTest, DatabaseVersion);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDatabaseTest, GetNextAvailableIds);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDatabaseTest, DestroyDatabase);

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerDatabase);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DATABASE_H_
