// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/quota/quota_database.h"

#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/file_util.h"
#include "base/time/time.h"
#include "sql/connection.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "url/gurl.h"
#include "webkit/browser/quota/special_storage_policy.h"

namespace quota {
namespace {

// Definitions for database schema.

const int kCurrentVersion = 4;
const int kCompatibleVersion = 2;

const char kHostQuotaTable[] = "HostQuotaTable";
const char kOriginInfoTable[] = "OriginInfoTable";
const char kIsOriginTableBootstrapped[] = "IsOriginTableBootstrapped";

bool VerifyValidQuotaConfig(const char* key) {
  return (key != NULL &&
          (!strcmp(key, QuotaDatabase::kDesiredAvailableSpaceKey) ||
           !strcmp(key, QuotaDatabase::kTemporaryQuotaOverrideKey)));
}

const int kCommitIntervalMs = 30000;

}  // anonymous namespace

// static
const char QuotaDatabase::kDesiredAvailableSpaceKey[] = "DesiredAvailableSpace";
const char QuotaDatabase::kTemporaryQuotaOverrideKey[] =
    "TemporaryQuotaOverride";

const QuotaDatabase::TableSchema QuotaDatabase::kTables[] = {
  { kHostQuotaTable,
    "(host TEXT NOT NULL,"
    " type INTEGER NOT NULL,"
    " quota INTEGER DEFAULT 0,"
    " UNIQUE(host, type))" },
  { kOriginInfoTable,
    "(origin TEXT NOT NULL,"
    " type INTEGER NOT NULL,"
    " used_count INTEGER DEFAULT 0,"
    " last_access_time INTEGER DEFAULT 0,"
    " last_modified_time INTEGER DEFAULT 0,"
    " UNIQUE(origin, type))" },
};

// static
const QuotaDatabase::IndexSchema QuotaDatabase::kIndexes[] = {
  { "HostIndex",
    kHostQuotaTable,
    "(host)",
    false },
  { "OriginInfoIndex",
    kOriginInfoTable,
    "(origin)",
    false },
  { "OriginLastAccessTimeIndex",
    kOriginInfoTable,
    "(last_access_time)",
    false },
  { "OriginLastModifiedTimeIndex",
    kOriginInfoTable,
    "(last_modified_time)",
    false },
};

struct QuotaDatabase::QuotaTableImporter {
  bool Append(const QuotaTableEntry& entry) {
    entries.push_back(entry);
    return true;
  }
  std::vector<QuotaTableEntry> entries;
};

// Clang requires explicit out-of-line constructors for them.
QuotaDatabase::QuotaTableEntry::QuotaTableEntry()
    : type(kStorageTypeUnknown),
      quota(0) {
}

QuotaDatabase::QuotaTableEntry::QuotaTableEntry(
    const std::string& host,
    StorageType type,
    int64 quota)
    : host(host),
      type(type),
      quota(quota) {
}

QuotaDatabase::OriginInfoTableEntry::OriginInfoTableEntry()
    : type(kStorageTypeUnknown),
      used_count(0) {
}

QuotaDatabase::OriginInfoTableEntry::OriginInfoTableEntry(
    const GURL& origin,
    StorageType type,
    int used_count,
    const base::Time& last_access_time,
    const base::Time& last_modified_time)
    : origin(origin),
      type(type),
      used_count(used_count),
      last_access_time(last_access_time),
      last_modified_time(last_modified_time) {
}

// QuotaDatabase ------------------------------------------------------------
QuotaDatabase::QuotaDatabase(const base::FilePath& path)
    : db_file_path_(path),
      is_recreating_(false),
      is_disabled_(false) {
}

QuotaDatabase::~QuotaDatabase() {
  if (db_) {
    db_->CommitTransaction();
  }
}

void QuotaDatabase::CloseConnection() {
  meta_table_.reset();
  db_.reset();
}

bool QuotaDatabase::GetHostQuota(
    const std::string& host, StorageType type, int64* quota) {
  DCHECK(quota);
  if (!LazyOpen(false))
    return false;

  const char* kSql =
      "SELECT quota"
      " FROM HostQuotaTable"
      " WHERE host = ? AND type = ?";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, host);
  statement.BindInt(1, static_cast<int>(type));

  if (!statement.Step())
    return false;

  *quota = statement.ColumnInt64(0);
  return true;
}

bool QuotaDatabase::SetHostQuota(
    const std::string& host, StorageType type, int64 quota) {
  DCHECK_GE(quota, 0);
  if (!LazyOpen(true))
    return false;

  const char* kSql =
      "INSERT OR REPLACE INTO HostQuotaTable"
      " (quota, host, type)"
      " VALUES (?, ?, ?)";
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, quota);
  statement.BindString(1, host);
  statement.BindInt(2, static_cast<int>(type));

  if (!statement.Run())
    return false;

  ScheduleCommit();
  return true;
}

bool QuotaDatabase::SetOriginLastAccessTime(
    const GURL& origin, StorageType type, base::Time last_access_time) {
  if (!LazyOpen(true))
    return false;

  sql::Statement statement;

  int used_count = 1;
  if (FindOriginUsedCount(origin, type, &used_count)) {
    ++used_count;
    const char* kSql =
        "UPDATE OriginInfoTable"
        " SET used_count = ?, last_access_time = ?"
        " WHERE origin = ? AND type = ?";
    statement.Assign(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  } else  {
    const char* kSql =
        "INSERT INTO OriginInfoTable"
        " (used_count, last_access_time, origin, type)"
        " VALUES (?, ?, ?, ?)";
    statement.Assign(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  }
  statement.BindInt(0, used_count);
  statement.BindInt64(1, last_access_time.ToInternalValue());
  statement.BindString(2, origin.spec());
  statement.BindInt(3, static_cast<int>(type));

  if (!statement.Run())
    return false;

  ScheduleCommit();
  return true;
}

bool QuotaDatabase::SetOriginLastModifiedTime(
    const GURL& origin, StorageType type, base::Time last_modified_time) {
  if (!LazyOpen(true))
    return false;

  sql::Statement statement;

  int dummy;
  if (FindOriginUsedCount(origin, type, &dummy)) {
    const char* kSql =
        "UPDATE OriginInfoTable"
        " SET last_modified_time = ?"
        " WHERE origin = ? AND type = ?";
    statement.Assign(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  } else {
    const char* kSql =
        "INSERT INTO OriginInfoTable"
        " (last_modified_time, origin, type)  VALUES (?, ?, ?)";
    statement.Assign(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  }
  statement.BindInt64(0, last_modified_time.ToInternalValue());
  statement.BindString(1, origin.spec());
  statement.BindInt(2, static_cast<int>(type));

  if (!statement.Run())
    return false;

  ScheduleCommit();
  return true;
}

bool QuotaDatabase::RegisterInitialOriginInfo(
    const std::set<GURL>& origins, StorageType type) {
  if (!LazyOpen(true))
    return false;

  typedef std::set<GURL>::const_iterator itr_type;
  for (itr_type itr = origins.begin(), end = origins.end();
       itr != end; ++itr) {
    const char* kSql =
        "INSERT OR IGNORE INTO OriginInfoTable"
        " (origin, type) VALUES (?, ?)";
    sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
    statement.BindString(0, itr->spec());
    statement.BindInt(1, static_cast<int>(type));

    if (!statement.Run())
      return false;
  }

  ScheduleCommit();
  return true;
}

bool QuotaDatabase::DeleteHostQuota(
    const std::string& host, StorageType type) {
  if (!LazyOpen(false))
    return false;

  const char* kSql =
      "DELETE FROM HostQuotaTable"
      " WHERE host = ? AND type = ?";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, host);
  statement.BindInt(1, static_cast<int>(type));

  if (!statement.Run())
    return false;

  ScheduleCommit();
  return true;
}

bool QuotaDatabase::DeleteOriginInfo(
    const GURL& origin, StorageType type) {
  if (!LazyOpen(false))
    return false;

  const char* kSql =
      "DELETE FROM OriginInfoTable"
      " WHERE origin = ? AND type = ?";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, origin.spec());
  statement.BindInt(1, static_cast<int>(type));

  if (!statement.Run())
    return false;

  ScheduleCommit();
  return true;
}

bool QuotaDatabase::GetQuotaConfigValue(const char* key, int64* value) {
  if (!LazyOpen(false))
    return false;
  DCHECK(VerifyValidQuotaConfig(key));
  return meta_table_->GetValue(key, value);
}

bool QuotaDatabase::SetQuotaConfigValue(const char* key, int64 value) {
  if (!LazyOpen(true))
    return false;
  DCHECK(VerifyValidQuotaConfig(key));
  return meta_table_->SetValue(key, value);
}

bool QuotaDatabase::GetLRUOrigin(
    StorageType type,
    const std::set<GURL>& exceptions,
    SpecialStoragePolicy* special_storage_policy,
    GURL* origin) {
  DCHECK(origin);
  if (!LazyOpen(false))
    return false;

  const char* kSql = "SELECT origin FROM OriginInfoTable"
                     " WHERE type = ?"
                     " ORDER BY last_access_time ASC";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(type));

  while (statement.Step()) {
    GURL url(statement.ColumnString(0));
    if (exceptions.find(url) != exceptions.end())
      continue;
    if (special_storage_policy &&
        special_storage_policy->IsStorageUnlimited(url))
      continue;
    *origin = url;
    return true;
  }

  *origin = GURL();
  return statement.Succeeded();
}

bool QuotaDatabase::GetOriginsModifiedSince(
    StorageType type, std::set<GURL>* origins, base::Time modified_since) {
  DCHECK(origins);
  if (!LazyOpen(false))
    return false;

  const char* kSql = "SELECT origin FROM OriginInfoTable"
                     " WHERE type = ? AND last_modified_time >= ?";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(type));
  statement.BindInt64(1, modified_since.ToInternalValue());

  origins->clear();
  while (statement.Step())
    origins->insert(GURL(statement.ColumnString(0)));

  return statement.Succeeded();
}

bool QuotaDatabase::IsOriginDatabaseBootstrapped() {
  if (!LazyOpen(true))
    return false;

  int flag = 0;
  return meta_table_->GetValue(kIsOriginTableBootstrapped, &flag) && flag;
}

bool QuotaDatabase::SetOriginDatabaseBootstrapped(bool bootstrap_flag) {
  if (!LazyOpen(true))
    return false;

  return meta_table_->SetValue(kIsOriginTableBootstrapped, bootstrap_flag);
}

void QuotaDatabase::Commit() {
  if (!db_)
    return;

  if (timer_.IsRunning())
    timer_.Stop();

  db_->CommitTransaction();
  db_->BeginTransaction();
}

void QuotaDatabase::ScheduleCommit() {
  if (timer_.IsRunning())
    return;
  timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(kCommitIntervalMs),
               this, &QuotaDatabase::Commit);
}

bool QuotaDatabase::FindOriginUsedCount(
    const GURL& origin, StorageType type, int* used_count) {
  DCHECK(used_count);
  if (!LazyOpen(false))
    return false;

  const char* kSql =
      "SELECT used_count FROM OriginInfoTable"
      " WHERE origin = ? AND type = ?";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, origin.spec());
  statement.BindInt(1, static_cast<int>(type));

  if (!statement.Step())
    return false;

  *used_count = statement.ColumnInt(0);
  return true;
}

bool QuotaDatabase::LazyOpen(bool create_if_needed) {
  if (db_)
    return true;

  // If we tried and failed once, don't try again in the same session
  // to avoid creating an incoherent mess on disk.
  if (is_disabled_)
    return false;

  bool in_memory_only = db_file_path_.empty();
  if (!create_if_needed &&
      (in_memory_only || !base::PathExists(db_file_path_))) {
    return false;
  }

  db_.reset(new sql::Connection);
  meta_table_.reset(new sql::MetaTable);

  db_->set_histogram_tag("Quota");

  bool opened = false;
  if (in_memory_only) {
    opened = db_->OpenInMemory();
  } else if (!base::CreateDirectory(db_file_path_.DirName())) {
      LOG(ERROR) << "Failed to create quota database directory.";
  } else {
    opened = db_->Open(db_file_path_);
    if (opened)
      db_->Preload();
  }

  if (!opened || !EnsureDatabaseVersion()) {
    LOG(ERROR) << "Failed to open the quota database.";
    is_disabled_ = true;
    db_.reset();
    meta_table_.reset();
    return false;
  }

  // Start a long-running transaction.
  db_->BeginTransaction();

  return true;
}

bool QuotaDatabase::EnsureDatabaseVersion() {
  static const size_t kTableCount = ARRAYSIZE_UNSAFE(kTables);
  static const size_t kIndexCount = ARRAYSIZE_UNSAFE(kIndexes);
  if (!sql::MetaTable::DoesTableExist(db_.get()))
    return CreateSchema(db_.get(), meta_table_.get(),
                        kCurrentVersion, kCompatibleVersion,
                        kTables, kTableCount,
                        kIndexes, kIndexCount);

  if (!meta_table_->Init(db_.get(), kCurrentVersion, kCompatibleVersion))
    return false;

  if (meta_table_->GetCompatibleVersionNumber() > kCurrentVersion) {
    LOG(WARNING) << "Quota database is too new.";
    return false;
  }

  if (meta_table_->GetVersionNumber() < kCurrentVersion) {
    if (!UpgradeSchema(meta_table_->GetVersionNumber()))
      return ResetSchema();
  }

#ifndef NDEBUG
  DCHECK(sql::MetaTable::DoesTableExist(db_.get()));
  for (size_t i = 0; i < kTableCount; ++i) {
    DCHECK(db_->DoesTableExist(kTables[i].table_name));
  }
#endif

  return true;
}

// static
bool QuotaDatabase::CreateSchema(
    sql::Connection* database,
    sql::MetaTable* meta_table,
    int schema_version, int compatible_version,
    const TableSchema* tables, size_t tables_size,
    const IndexSchema* indexes, size_t indexes_size) {
  // TODO(kinuko): Factor out the common code to create databases.
  sql::Transaction transaction(database);
  if (!transaction.Begin())
    return false;

  if (!meta_table->Init(database, schema_version, compatible_version))
    return false;

  for (size_t i = 0; i < tables_size; ++i) {
    std::string sql("CREATE TABLE ");
    sql += tables[i].table_name;
    sql += tables[i].columns;
    if (!database->Execute(sql.c_str())) {
      VLOG(1) << "Failed to execute " << sql;
      return false;
    }
  }

  for (size_t i = 0; i < indexes_size; ++i) {
    std::string sql;
    if (indexes[i].unique)
      sql += "CREATE UNIQUE INDEX ";
    else
      sql += "CREATE INDEX ";
    sql += indexes[i].index_name;
    sql += " ON ";
    sql += indexes[i].table_name;
    sql += indexes[i].columns;
    if (!database->Execute(sql.c_str())) {
      VLOG(1) << "Failed to execute " << sql;
      return false;
    }
  }

  return transaction.Commit();
}

bool QuotaDatabase::ResetSchema() {
  DCHECK(!db_file_path_.empty());
  DCHECK(base::PathExists(db_file_path_));
  VLOG(1) << "Deleting existing quota data and starting over.";

  db_.reset();
  meta_table_.reset();

  if (!sql::Connection::Delete(db_file_path_))
    return false;

  // So we can't go recursive.
  if (is_recreating_)
    return false;

  base::AutoReset<bool> auto_reset(&is_recreating_, true);
  return LazyOpen(true);
}

bool QuotaDatabase::UpgradeSchema(int current_version) {
  if (current_version == 2) {
    QuotaTableImporter importer;
    typedef std::vector<QuotaTableEntry> QuotaTableEntries;
    if (!DumpQuotaTable(base::Bind(&QuotaTableImporter::Append,
                                   base::Unretained(&importer)))) {
      return false;
    }
    ResetSchema();
    for (QuotaTableEntries::const_iterator iter = importer.entries.begin();
         iter != importer.entries.end(); ++iter) {
      if (!SetHostQuota(iter->host, iter->type, iter->quota))
        return false;
    }
    Commit();
    return true;
  }
  return false;
}

bool QuotaDatabase::DumpQuotaTable(const QuotaTableCallback& callback) {
  if (!LazyOpen(true))
    return false;

  const char* kSql = "SELECT * FROM HostQuotaTable";
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));

  while (statement.Step()) {
    QuotaTableEntry entry = QuotaTableEntry(
      statement.ColumnString(0),
      static_cast<StorageType>(statement.ColumnInt(1)),
      statement.ColumnInt64(2));

    if (!callback.Run(entry))
      return true;
  }

  return statement.Succeeded();
}

bool QuotaDatabase::DumpOriginInfoTable(
    const OriginInfoTableCallback& callback) {

  if (!LazyOpen(true))
    return false;

  const char* kSql = "SELECT * FROM OriginInfoTable";
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));

  while (statement.Step()) {
    OriginInfoTableEntry entry(
      GURL(statement.ColumnString(0)),
      static_cast<StorageType>(statement.ColumnInt(1)),
      statement.ColumnInt(2),
      base::Time::FromInternalValue(statement.ColumnInt64(3)),
      base::Time::FromInternalValue(statement.ColumnInt64(4)));

    if (!callback.Run(entry))
      return true;
  }

  return statement.Succeeded();
}

bool operator<(const QuotaDatabase::QuotaTableEntry& lhs,
               const QuotaDatabase::QuotaTableEntry& rhs) {
  if (lhs.host < rhs.host) return true;
  if (rhs.host < lhs.host) return false;
  if (lhs.type < rhs.type) return true;
  if (rhs.type < lhs.type) return false;
  return lhs.quota < rhs.quota;
}

bool operator<(const QuotaDatabase::OriginInfoTableEntry& lhs,
               const QuotaDatabase::OriginInfoTableEntry& rhs) {
  if (lhs.origin < rhs.origin) return true;
  if (rhs.origin < lhs.origin) return false;
  if (lhs.type < rhs.type) return true;
  if (rhs.type < lhs.type) return false;
  if (lhs.used_count < rhs.used_count) return true;
  if (rhs.used_count < lhs.used_count) return false;
  return lhs.last_access_time < rhs.last_access_time;
}

}  // namespace quota
