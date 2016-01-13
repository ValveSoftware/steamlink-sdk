// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_QUOTA_QUOTA_DATABASE_H_
#define WEBKIT_BROWSER_QUOTA_QUOTA_DATABASE_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "url/gurl.h"
#include "webkit/browser/webkit_storage_browser_export.h"
#include "webkit/common/quota/quota_types.h"

namespace content {
class QuotaDatabaseTest;
}

namespace sql {
class Connection;
class MetaTable;
}

class GURL;

namespace quota {

class SpecialStoragePolicy;

// All the methods of this class must run on the DB thread.
class WEBKIT_STORAGE_BROWSER_EXPORT_PRIVATE QuotaDatabase {
 public:
  // Constants for {Get,Set}QuotaConfigValue keys.
  static const char kDesiredAvailableSpaceKey[];
  static const char kTemporaryQuotaOverrideKey[];

  // If 'path' is empty, an in memory database will be used.
  explicit QuotaDatabase(const base::FilePath& path);
  ~QuotaDatabase();

  void CloseConnection();

  bool GetHostQuota(const std::string& host, StorageType type, int64* quota);
  bool SetHostQuota(const std::string& host, StorageType type, int64 quota);
  bool DeleteHostQuota(const std::string& host, StorageType type);

  bool SetOriginLastAccessTime(const GURL& origin,
                               StorageType type,
                               base::Time last_access_time);

  bool SetOriginLastModifiedTime(const GURL& origin,
                                 StorageType type,
                                 base::Time last_modified_time);

  // Register initial |origins| info |type| to the database.
  // This method is assumed to be called only after the installation or
  // the database schema reset.
  bool RegisterInitialOriginInfo(
      const std::set<GURL>& origins, StorageType type);

  bool DeleteOriginInfo(const GURL& origin, StorageType type);

  bool GetQuotaConfigValue(const char* key, int64* value);
  bool SetQuotaConfigValue(const char* key, int64 value);

  // Sets |origin| to the least recently used origin of origins not included
  // in |exceptions| and not granted the special unlimited storage right.
  // It returns false when it failed in accessing the database.
  // |origin| is set to empty when there is no matching origin.
  bool GetLRUOrigin(StorageType type,
                    const std::set<GURL>& exceptions,
                    SpecialStoragePolicy* special_storage_policy,
                    GURL* origin);

  // Populates |origins| with the ones that have been modified since
  // the |modified_since|.
  bool GetOriginsModifiedSince(StorageType type,
                               std::set<GURL>* origins,
                               base::Time modified_since);

  // Returns false if SetOriginDatabaseBootstrapped has never
  // been called before, which means existing origins may not have been
  // registered.
  bool IsOriginDatabaseBootstrapped();
  bool SetOriginDatabaseBootstrapped(bool bootstrap_flag);

 private:
  struct WEBKIT_STORAGE_BROWSER_EXPORT_PRIVATE QuotaTableEntry {
    QuotaTableEntry();
    QuotaTableEntry(
        const std::string& host,
        StorageType type,
        int64 quota);
    std::string host;
    StorageType type;
    int64 quota;
  };
  friend WEBKIT_STORAGE_BROWSER_EXPORT_PRIVATE bool operator <(
      const QuotaTableEntry& lhs, const QuotaTableEntry& rhs);

  struct WEBKIT_STORAGE_BROWSER_EXPORT_PRIVATE OriginInfoTableEntry {
    OriginInfoTableEntry();
    OriginInfoTableEntry(
        const GURL& origin,
        StorageType type,
        int used_count,
        const base::Time& last_access_time,
        const base::Time& last_modified_time);
    GURL origin;
    StorageType type;
    int used_count;
    base::Time last_access_time;
    base::Time last_modified_time;
  };
  friend WEBKIT_STORAGE_BROWSER_EXPORT_PRIVATE bool operator <(
      const OriginInfoTableEntry& lhs, const OriginInfoTableEntry& rhs);

  // Structures used for CreateSchema.
  struct TableSchema {
    const char* table_name;
    const char* columns;
  };
  struct IndexSchema {
    const char* index_name;
    const char* table_name;
    const char* columns;
    bool unique;
  };

  typedef base::Callback<bool (const QuotaTableEntry&)> QuotaTableCallback;
  typedef base::Callback<bool (const OriginInfoTableEntry&)>
      OriginInfoTableCallback;

  struct QuotaTableImporter;

  // For long-running transactions support.  We always keep a transaction open
  // so that multiple transactions can be batched.  They are flushed
  // with a delay after a modification has been made.  We support neither
  // nested transactions nor rollback (as we don't need them for now).
  void Commit();
  void ScheduleCommit();

  bool FindOriginUsedCount(const GURL& origin,
                           StorageType type,
                           int* used_count);

  bool LazyOpen(bool create_if_needed);
  bool EnsureDatabaseVersion();
  bool ResetSchema();
  bool UpgradeSchema(int current_version);

  static bool CreateSchema(
      sql::Connection* database,
      sql::MetaTable* meta_table,
      int schema_version, int compatible_version,
      const TableSchema* tables, size_t tables_size,
      const IndexSchema* indexes, size_t indexes_size);

  // |callback| may return false to stop reading data.
  bool DumpQuotaTable(const QuotaTableCallback& callback);
  bool DumpOriginInfoTable(const OriginInfoTableCallback& callback);

  base::FilePath db_file_path_;

  scoped_ptr<sql::Connection> db_;
  scoped_ptr<sql::MetaTable> meta_table_;
  bool is_recreating_;
  bool is_disabled_;

  base::OneShotTimer<QuotaDatabase> timer_;

  friend class content::QuotaDatabaseTest;
  friend class QuotaManager;

  static const TableSchema kTables[];
  static const IndexSchema kIndexes[];

  DISALLOW_COPY_AND_ASSIGN(QuotaDatabase);
};

}  // namespace quota

#endif  // WEBKIT_BROWSER_QUOTA_QUOTA_DATABASE_H_
