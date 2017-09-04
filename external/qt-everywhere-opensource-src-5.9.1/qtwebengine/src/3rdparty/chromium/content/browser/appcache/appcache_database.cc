// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_database.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/appcache/appcache_entry.h"
#include "content/browser/appcache/appcache_histograms.h"
#include "sql/connection.h"
#include "sql/error_delegate_util.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace content {

// Schema -------------------------------------------------------------------
namespace {

const int kCurrentVersion = 7;
const int kCompatibleVersion = 7;
const bool kCreateIfNeeded = true;
const bool kDontCreate = false;

// A mechanism to run experiments that may affect in data being persisted
// in different ways such that when the experiment is toggled on/off via
// cmd line flags, the database gets reset. The active flags are stored at
// the time of database creation and compared when reopening. If different
// the database is reset.
const char kExperimentFlagsKey[] = "ExperimentFlags";

const char kGroupsTable[] = "Groups";
const char kCachesTable[] = "Caches";
const char kEntriesTable[] = "Entries";
const char kNamespacesTable[] = "Namespaces";
const char kOnlineWhiteListsTable[] = "OnlineWhiteLists";
const char kDeletableResponseIdsTable[] = "DeletableResponseIds";

struct TableInfo {
  const char* table_name;
  const char* columns;
};

struct IndexInfo {
  const char* index_name;
  const char* table_name;
  const char* columns;
  bool unique;
};

const TableInfo kTables[] = {
  { kGroupsTable,
    "(group_id INTEGER PRIMARY KEY,"
    " origin TEXT,"
    " manifest_url TEXT,"
    " creation_time INTEGER,"
    " last_access_time INTEGER,"
    " last_full_update_check_time INTEGER,"
    " first_evictable_error_time INTEGER)" },

  { kCachesTable,
    "(cache_id INTEGER PRIMARY KEY,"
    " group_id INTEGER,"
    " online_wildcard INTEGER CHECK(online_wildcard IN (0, 1)),"
    " update_time INTEGER,"
    " cache_size INTEGER)" },  // intentionally not normalized

  { kEntriesTable,
    "(cache_id INTEGER,"
    " url TEXT,"
    " flags INTEGER,"
    " response_id INTEGER,"
    " response_size INTEGER)" },

  { kNamespacesTable,
    "(cache_id INTEGER,"
    " origin TEXT,"  // intentionally not normalized
    " type INTEGER,"
    " namespace_url TEXT,"
    " target_url TEXT,"
    " is_pattern INTEGER CHECK(is_pattern IN (0, 1)))" },

  { kOnlineWhiteListsTable,
    "(cache_id INTEGER,"
    " namespace_url TEXT,"
    " is_pattern INTEGER CHECK(is_pattern IN (0, 1)))" },

  { kDeletableResponseIdsTable,
    "(response_id INTEGER NOT NULL)" },
};

const IndexInfo kIndexes[] = {
  { "GroupsOriginIndex",
    kGroupsTable,
    "(origin)",
    false },

  { "GroupsManifestIndex",
    kGroupsTable,
    "(manifest_url)",
    true },

  { "CachesGroupIndex",
    kCachesTable,
    "(group_id)",
    false },

  { "EntriesCacheIndex",
    kEntriesTable,
    "(cache_id)",
    false },

  { "EntriesCacheAndUrlIndex",
    kEntriesTable,
    "(cache_id, url)",
    true },

  { "EntriesResponseIdIndex",
    kEntriesTable,
    "(response_id)",
    true },

  { "NamespacesCacheIndex",
    kNamespacesTable,
    "(cache_id)",
    false },

  { "NamespacesOriginIndex",
    kNamespacesTable,
    "(origin)",
    false },

  { "NamespacesCacheAndUrlIndex",
    kNamespacesTable,
    "(cache_id, namespace_url)",
    true },

  { "OnlineWhiteListCacheIndex",
    kOnlineWhiteListsTable,
    "(cache_id)",
    false },

  { "DeletableResponsesIdIndex",
    kDeletableResponseIdsTable,
    "(response_id)",
    true },
};

const int kTableCount = arraysize(kTables);
const int kIndexCount = arraysize(kIndexes);

bool CreateTable(sql::Connection* db, const TableInfo& info) {
  std::string sql("CREATE TABLE ");
  sql += info.table_name;
  sql += info.columns;
  return db->Execute(sql.c_str());
}

bool CreateIndex(sql::Connection* db, const IndexInfo& info) {
  std::string sql;
  if (info.unique)
    sql += "CREATE UNIQUE INDEX ";
  else
    sql += "CREATE INDEX ";
  sql += info.index_name;
  sql += " ON ";
  sql += info.table_name;
  sql += info.columns;
  return db->Execute(sql.c_str());
}

std::string GetActiveExperimentFlags() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kEnableExecutableHandlers))
    return std::string("executableHandlersEnabled");
  return std::string();
}

}  // anon namespace

// AppCacheDatabase ----------------------------------------------------------

AppCacheDatabase::GroupRecord::GroupRecord()
    : group_id(0) {
}

AppCacheDatabase::GroupRecord::GroupRecord(const GroupRecord& other) = default;

AppCacheDatabase::GroupRecord::~GroupRecord() {
}

AppCacheDatabase::NamespaceRecord::NamespaceRecord()
    : cache_id(0) {
}

AppCacheDatabase::NamespaceRecord::~NamespaceRecord() {
}


AppCacheDatabase::AppCacheDatabase(const base::FilePath& path)
    : db_file_path_(path),
      is_disabled_(false),
      is_recreating_(false),
      was_corruption_detected_(false) {
}

AppCacheDatabase::~AppCacheDatabase() {
  CommitLazyLastAccessTimes();
}

void AppCacheDatabase::Disable() {
  VLOG(1) << "Disabling appcache database.";
  is_disabled_ = true;
  ResetConnectionAndTables();
}

int64_t AppCacheDatabase::GetOriginUsage(const GURL& origin) {
  std::vector<CacheRecord> records;
  if (!FindCachesForOrigin(origin, &records))
    return 0;

  int64_t origin_usage = 0;
  std::vector<CacheRecord>::const_iterator iter = records.begin();
  while (iter != records.end()) {
    origin_usage += iter->cache_size;
    ++iter;
  }
  return origin_usage;
}

bool AppCacheDatabase::GetAllOriginUsage(std::map<GURL, int64_t>* usage_map) {
  std::set<GURL> origins;
  if (!FindOriginsWithGroups(&origins))
    return false;
  for (std::set<GURL>::const_iterator origin = origins.begin();
       origin != origins.end(); ++origin) {
    (*usage_map)[*origin] = GetOriginUsage(*origin);
  }
  return true;
}

bool AppCacheDatabase::FindOriginsWithGroups(std::set<GURL>* origins) {
  DCHECK(origins && origins->empty());
  if (!LazyOpen(kDontCreate))
    return false;

  const char kSql[] =
      "SELECT DISTINCT(origin) FROM Groups";

  sql::Statement statement(db_->GetUniqueStatement(kSql));

  while (statement.Step())
    origins->insert(GURL(statement.ColumnString(0)));

  return statement.Succeeded();
}

bool AppCacheDatabase::FindLastStorageIds(
    int64_t* last_group_id,
    int64_t* last_cache_id,
    int64_t* last_response_id,
    int64_t* last_deletable_response_rowid) {
  DCHECK(last_group_id && last_cache_id && last_response_id &&
         last_deletable_response_rowid);

  *last_group_id = 0;
  *last_cache_id = 0;
  *last_response_id = 0;
  *last_deletable_response_rowid = 0;

  if (!LazyOpen(kDontCreate))
    return false;

  const char kMaxGroupIdSql[] = "SELECT MAX(group_id) FROM Groups";
  const char kMaxCacheIdSql[] = "SELECT MAX(cache_id) FROM Caches";
  const char kMaxResponseIdFromEntriesSql[] =
      "SELECT MAX(response_id) FROM Entries";
  const char kMaxResponseIdFromDeletablesSql[] =
      "SELECT MAX(response_id) FROM DeletableResponseIds";
  const char kMaxDeletableResponseRowIdSql[] =
      "SELECT MAX(rowid) FROM DeletableResponseIds";
  int64_t max_group_id;
  int64_t max_cache_id;
  int64_t max_response_id_from_entries;
  int64_t max_response_id_from_deletables;
  int64_t max_deletable_response_rowid;
  if (!RunUniqueStatementWithInt64Result(kMaxGroupIdSql, &max_group_id) ||
      !RunUniqueStatementWithInt64Result(kMaxCacheIdSql, &max_cache_id) ||
      !RunUniqueStatementWithInt64Result(kMaxResponseIdFromEntriesSql,
                                         &max_response_id_from_entries) ||
      !RunUniqueStatementWithInt64Result(kMaxResponseIdFromDeletablesSql,
                                         &max_response_id_from_deletables) ||
      !RunUniqueStatementWithInt64Result(kMaxDeletableResponseRowIdSql,
                                         &max_deletable_response_rowid)) {
    return false;
  }

  *last_group_id = max_group_id;
  *last_cache_id = max_cache_id;
  *last_response_id = std::max(max_response_id_from_entries,
                               max_response_id_from_deletables);
  *last_deletable_response_rowid = max_deletable_response_rowid;
  return true;
}

bool AppCacheDatabase::FindGroup(int64_t group_id, GroupRecord* record) {
  DCHECK(record);
  if (!LazyOpen(kDontCreate))
    return false;

  const char kSql[] =
      "SELECT group_id, origin, manifest_url,"
      "       creation_time, last_access_time,"
      "       last_full_update_check_time,"
      "       first_evictable_error_time"
      "  FROM Groups WHERE group_id = ?";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));

  statement.BindInt64(0, group_id);
  if (!statement.Step())
    return false;

  ReadGroupRecord(statement, record);
  DCHECK(record->group_id == group_id);
  return true;
}

bool AppCacheDatabase::FindGroupForManifestUrl(
    const GURL& manifest_url, GroupRecord* record) {
  DCHECK(record);
  if (!LazyOpen(kDontCreate))
    return false;

  const char kSql[] =
      "SELECT group_id, origin, manifest_url,"
      "       creation_time, last_access_time,"
      "       last_full_update_check_time,"
      "       first_evictable_error_time"
      "  FROM Groups WHERE manifest_url = ?";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, manifest_url.spec());

  if (!statement.Step())
    return false;

  ReadGroupRecord(statement, record);
  DCHECK(record->manifest_url == manifest_url);
  return true;
}

bool AppCacheDatabase::FindGroupsForOrigin(
    const GURL& origin, std::vector<GroupRecord>* records) {
  DCHECK(records && records->empty());
  if (!LazyOpen(kDontCreate))
    return false;

  const char kSql[] =
      "SELECT group_id, origin, manifest_url,"
      "       creation_time, last_access_time,"
      "       last_full_update_check_time,"
      "       first_evictable_error_time"
      "   FROM Groups WHERE origin = ?";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, origin.spec());

  while (statement.Step()) {
    records->push_back(GroupRecord());
    ReadGroupRecord(statement, &records->back());
    DCHECK(records->back().origin == origin);
  }

  return statement.Succeeded();
}

bool AppCacheDatabase::FindGroupForCache(int64_t cache_id,
                                         GroupRecord* record) {
  DCHECK(record);
  if (!LazyOpen(kDontCreate))
    return false;

  const char kSql[] =
      "SELECT g.group_id, g.origin, g.manifest_url,"
      "       g.creation_time, g.last_access_time,"
      "       g.last_full_update_check_time,"
      "       g.first_evictable_error_time"
      "  FROM Groups g, Caches c"
      "  WHERE c.cache_id = ? AND c.group_id = g.group_id";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, cache_id);

  if (!statement.Step())
    return false;

  ReadGroupRecord(statement, record);
  return true;
}

bool AppCacheDatabase::InsertGroup(const GroupRecord* record) {
  if (!LazyOpen(kCreateIfNeeded))
    return false;

  const char kSql[] =
      "INSERT INTO Groups"
      "  (group_id, origin, manifest_url, creation_time, last_access_time,"
      "   last_full_update_check_time, first_evictable_error_time)"
      "  VALUES(?, ?, ?, ?, ?, ?, ?)";
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, record->group_id);
  statement.BindString(1, record->origin.spec());
  statement.BindString(2, record->manifest_url.spec());
  statement.BindInt64(3, record->creation_time.ToInternalValue());
  statement.BindInt64(4, record->last_access_time.ToInternalValue());
  statement.BindInt64(5, record->last_full_update_check_time.ToInternalValue());
  statement.BindInt64(6, record->first_evictable_error_time.ToInternalValue());
  return statement.Run();
}

bool AppCacheDatabase::DeleteGroup(int64_t group_id) {
  if (!LazyOpen(kDontCreate))
    return false;

  const char kSql[] =
      "DELETE FROM Groups WHERE group_id = ?";
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, group_id);
  return statement.Run();
}

bool AppCacheDatabase::UpdateLastAccessTime(int64_t group_id, base::Time time) {
  if (!LazyUpdateLastAccessTime(group_id, time))
    return false;
  return CommitLazyLastAccessTimes();
}

bool AppCacheDatabase::LazyUpdateLastAccessTime(int64_t group_id,
                                                base::Time time) {
  if (!LazyOpen(kCreateIfNeeded))
    return false;
  lazy_last_access_times_[group_id] = time;
  return true;
}

bool AppCacheDatabase::CommitLazyLastAccessTimes() {
  if (lazy_last_access_times_.empty())
    return true;
  if (!LazyOpen(kDontCreate))
    return false;

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;
  for (const auto& pair : lazy_last_access_times_) {
    const char kSql[] =
        "UPDATE Groups SET last_access_time = ? WHERE group_id = ?";
    sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
    statement.BindInt64(0, pair.second.ToInternalValue());  // time
    statement.BindInt64(1, pair.first);  // group_id
    statement.Run();
  }
  lazy_last_access_times_.clear();
  return transaction.Commit();
}

bool AppCacheDatabase::UpdateEvictionTimes(
    int64_t group_id,
    base::Time last_full_update_check_time,
    base::Time first_evictable_error_time) {
  if (!LazyOpen(kCreateIfNeeded))
    return false;

  const char kSql[] =
      "UPDATE Groups"
      " SET last_full_update_check_time = ?, first_evictable_error_time = ?"
      " WHERE group_id = ?";
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, last_full_update_check_time.ToInternalValue());
  statement.BindInt64(1, first_evictable_error_time.ToInternalValue());
  statement.BindInt64(2, group_id);
  return statement.Run();  // Will succeed even if group_id is invalid.
}

bool AppCacheDatabase::FindCache(int64_t cache_id, CacheRecord* record) {
  DCHECK(record);
  if (!LazyOpen(kDontCreate))
    return false;

  const char kSql[] =
      "SELECT cache_id, group_id, online_wildcard, update_time, cache_size"
      " FROM Caches WHERE cache_id = ?";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, cache_id);

  if (!statement.Step())
    return false;

  ReadCacheRecord(statement, record);
  return true;
}

bool AppCacheDatabase::FindCacheForGroup(int64_t group_id,
                                         CacheRecord* record) {
  DCHECK(record);
  if (!LazyOpen(kDontCreate))
    return false;

  const char kSql[] =
      "SELECT cache_id, group_id, online_wildcard, update_time, cache_size"
      "  FROM Caches WHERE group_id = ?";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, group_id);

  if (!statement.Step())
    return false;

  ReadCacheRecord(statement, record);
  return true;
}

bool AppCacheDatabase::FindCachesForOrigin(
    const GURL& origin, std::vector<CacheRecord>* records) {
  DCHECK(records);
  std::vector<GroupRecord> group_records;
  if (!FindGroupsForOrigin(origin, &group_records))
    return false;

  CacheRecord cache_record;
  std::vector<GroupRecord>::const_iterator iter = group_records.begin();
  while (iter != group_records.end()) {
    if (FindCacheForGroup(iter->group_id, &cache_record))
      records->push_back(cache_record);
    ++iter;
  }
  return true;
}

bool AppCacheDatabase::InsertCache(const CacheRecord* record) {
  if (!LazyOpen(kCreateIfNeeded))
    return false;

  const char kSql[] =
      "INSERT INTO Caches (cache_id, group_id, online_wildcard,"
      "                    update_time, cache_size)"
      "  VALUES(?, ?, ?, ?, ?)";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, record->cache_id);
  statement.BindInt64(1, record->group_id);
  statement.BindBool(2, record->online_wildcard);
  statement.BindInt64(3, record->update_time.ToInternalValue());
  statement.BindInt64(4, record->cache_size);

  return statement.Run();
}

bool AppCacheDatabase::DeleteCache(int64_t cache_id) {
  if (!LazyOpen(kDontCreate))
    return false;

  const char kSql[] =
      "DELETE FROM Caches WHERE cache_id = ?";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, cache_id);

  return statement.Run();
}

bool AppCacheDatabase::FindEntriesForCache(int64_t cache_id,
                                           std::vector<EntryRecord>* records) {
  DCHECK(records && records->empty());
  if (!LazyOpen(kDontCreate))
    return false;

  const char kSql[] =
      "SELECT cache_id, url, flags, response_id, response_size FROM Entries"
      "  WHERE cache_id = ?";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, cache_id);

  while (statement.Step()) {
    records->push_back(EntryRecord());
    ReadEntryRecord(statement, &records->back());
    DCHECK(records->back().cache_id == cache_id);
  }

  return statement.Succeeded();
}

bool AppCacheDatabase::FindEntriesForUrl(
    const GURL& url, std::vector<EntryRecord>* records) {
  DCHECK(records && records->empty());
  if (!LazyOpen(kDontCreate))
    return false;

  const char kSql[] =
      "SELECT cache_id, url, flags, response_id, response_size FROM Entries"
      "  WHERE url = ?";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, url.spec());

  while (statement.Step()) {
    records->push_back(EntryRecord());
    ReadEntryRecord(statement, &records->back());
    DCHECK(records->back().url == url);
  }

  return statement.Succeeded();
}

bool AppCacheDatabase::FindEntry(int64_t cache_id,
                                 const GURL& url,
                                 EntryRecord* record) {
  DCHECK(record);
  if (!LazyOpen(kDontCreate))
    return false;

  const char kSql[] =
      "SELECT cache_id, url, flags, response_id, response_size FROM Entries"
      "  WHERE cache_id = ? AND url = ?";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, cache_id);
  statement.BindString(1, url.spec());

  if (!statement.Step())
    return false;

  ReadEntryRecord(statement, record);
  DCHECK(record->cache_id == cache_id);
  DCHECK(record->url == url);
  return true;
}

bool AppCacheDatabase::InsertEntry(const EntryRecord* record) {
  if (!LazyOpen(kCreateIfNeeded))
    return false;

  const char kSql[] =
      "INSERT INTO Entries (cache_id, url, flags, response_id, response_size)"
      "  VALUES(?, ?, ?, ?, ?)";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, record->cache_id);
  statement.BindString(1, record->url.spec());
  statement.BindInt(2, record->flags);
  statement.BindInt64(3, record->response_id);
  statement.BindInt64(4, record->response_size);

  return statement.Run();
}

bool AppCacheDatabase::InsertEntryRecords(
    const std::vector<EntryRecord>& records) {
  if (records.empty())
    return true;
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;
  std::vector<EntryRecord>::const_iterator iter = records.begin();
  while (iter != records.end()) {
    if (!InsertEntry(&(*iter)))
      return false;
    ++iter;
  }
  return transaction.Commit();
}

bool AppCacheDatabase::DeleteEntriesForCache(int64_t cache_id) {
  if (!LazyOpen(kDontCreate))
    return false;

  const char kSql[] =
      "DELETE FROM Entries WHERE cache_id = ?";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, cache_id);

  return statement.Run();
}

bool AppCacheDatabase::AddEntryFlags(const GURL& entry_url,
                                     int64_t cache_id,
                                     int additional_flags) {
  if (!LazyOpen(kDontCreate))
    return false;

  const char kSql[] =
      "UPDATE Entries SET flags = flags | ? WHERE cache_id = ? AND url = ?";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, additional_flags);
  statement.BindInt64(1, cache_id);
  statement.BindString(2, entry_url.spec());

  return statement.Run() && db_->GetLastChangeCount();
}

bool AppCacheDatabase::FindNamespacesForOrigin(
    const GURL& origin,
    std::vector<NamespaceRecord>* intercepts,
    std::vector<NamespaceRecord>* fallbacks) {
  DCHECK(intercepts && intercepts->empty());
  DCHECK(fallbacks && fallbacks->empty());
  if (!LazyOpen(kDontCreate))
    return false;

  const char kSql[] =
      "SELECT cache_id, origin, type, namespace_url, target_url, is_pattern"
      "  FROM Namespaces WHERE origin = ?";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, origin.spec());

  ReadNamespaceRecords(&statement, intercepts, fallbacks);

  return statement.Succeeded();
}

bool AppCacheDatabase::FindNamespacesForCache(
    int64_t cache_id,
    std::vector<NamespaceRecord>* intercepts,
    std::vector<NamespaceRecord>* fallbacks) {
  DCHECK(intercepts && intercepts->empty());
  DCHECK(fallbacks && fallbacks->empty());
  if (!LazyOpen(kDontCreate))
    return false;

  const char kSql[] =
      "SELECT cache_id, origin, type, namespace_url, target_url, is_pattern"
      "  FROM Namespaces WHERE cache_id = ?";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, cache_id);

  ReadNamespaceRecords(&statement, intercepts, fallbacks);

  return statement.Succeeded();
}

bool AppCacheDatabase::InsertNamespace(
    const NamespaceRecord* record) {
  if (!LazyOpen(kCreateIfNeeded))
    return false;

  const char kSql[] =
      "INSERT INTO Namespaces"
      "  (cache_id, origin, type, namespace_url, target_url, is_pattern)"
      "  VALUES (?, ?, ?, ?, ?, ?)";

  // Note: quick and dirty storage for the 'executable' bit w/o changing
  // schemas, we use the high bit of 'type' field.
  int type_with_executable_bit = record->namespace_.type;
  if (record->namespace_.is_executable) {
    type_with_executable_bit |= 0x8000000;
    DCHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
        kEnableExecutableHandlers));
  }

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, record->cache_id);
  statement.BindString(1, record->origin.spec());
  statement.BindInt(2, type_with_executable_bit);
  statement.BindString(3, record->namespace_.namespace_url.spec());
  statement.BindString(4, record->namespace_.target_url.spec());
  statement.BindBool(5, record->namespace_.is_pattern);
  return statement.Run();
}

bool AppCacheDatabase::InsertNamespaceRecords(
    const std::vector<NamespaceRecord>& records) {
  if (records.empty())
    return true;
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;
  std::vector<NamespaceRecord>::const_iterator iter = records.begin();
  while (iter != records.end()) {
    if (!InsertNamespace(&(*iter)))
      return false;
    ++iter;
  }
  return transaction.Commit();
}

bool AppCacheDatabase::DeleteNamespacesForCache(int64_t cache_id) {
  if (!LazyOpen(kDontCreate))
    return false;

  const char kSql[] =
      "DELETE FROM Namespaces WHERE cache_id = ?";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, cache_id);

  return statement.Run();
}

bool AppCacheDatabase::FindOnlineWhiteListForCache(
    int64_t cache_id,
    std::vector<OnlineWhiteListRecord>* records) {
  DCHECK(records && records->empty());
  if (!LazyOpen(kDontCreate))
    return false;

  const char kSql[] =
      "SELECT cache_id, namespace_url, is_pattern FROM OnlineWhiteLists"
      "  WHERE cache_id = ?";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, cache_id);

  while (statement.Step()) {
    records->push_back(OnlineWhiteListRecord());
    this->ReadOnlineWhiteListRecord(statement, &records->back());
    DCHECK(records->back().cache_id == cache_id);
  }
  return statement.Succeeded();
}

bool AppCacheDatabase::InsertOnlineWhiteList(
    const OnlineWhiteListRecord* record) {
  if (!LazyOpen(kCreateIfNeeded))
    return false;

  const char kSql[] =
      "INSERT INTO OnlineWhiteLists (cache_id, namespace_url, is_pattern)"
      "  VALUES (?, ?, ?)";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, record->cache_id);
  statement.BindString(1, record->namespace_url.spec());
  statement.BindBool(2, record->is_pattern);

  return statement.Run();
}

bool AppCacheDatabase::InsertOnlineWhiteListRecords(
    const std::vector<OnlineWhiteListRecord>& records) {
  if (records.empty())
    return true;
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;
  std::vector<OnlineWhiteListRecord>::const_iterator iter = records.begin();
  while (iter != records.end()) {
    if (!InsertOnlineWhiteList(&(*iter)))
      return false;
    ++iter;
  }
  return transaction.Commit();
}

bool AppCacheDatabase::DeleteOnlineWhiteListForCache(int64_t cache_id) {
  if (!LazyOpen(kDontCreate))
    return false;

  const char kSql[] =
      "DELETE FROM OnlineWhiteLists WHERE cache_id = ?";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, cache_id);

  return statement.Run();
}

bool AppCacheDatabase::GetDeletableResponseIds(
    std::vector<int64_t>* response_ids,
    int64_t max_rowid,
    int limit) {
  if (!LazyOpen(kDontCreate))
    return false;

  const char kSql[] =
      "SELECT response_id FROM DeletableResponseIds "
      "  WHERE rowid <= ?"
      "  LIMIT ?";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, max_rowid);
  statement.BindInt64(1, limit);

  while (statement.Step())
    response_ids->push_back(statement.ColumnInt64(0));
  return statement.Succeeded();
}

bool AppCacheDatabase::InsertDeletableResponseIds(
    const std::vector<int64_t>& response_ids) {
  const char kSql[] =
      "INSERT INTO DeletableResponseIds (response_id) VALUES (?)";
  return RunCachedStatementWithIds(SQL_FROM_HERE, kSql, response_ids);
}

bool AppCacheDatabase::DeleteDeletableResponseIds(
    const std::vector<int64_t>& response_ids) {
  const char kSql[] =
      "DELETE FROM DeletableResponseIds WHERE response_id = ?";
  return RunCachedStatementWithIds(SQL_FROM_HERE, kSql, response_ids);
}

bool AppCacheDatabase::RunCachedStatementWithIds(
    const sql::StatementID& statement_id,
    const char* sql,
    const std::vector<int64_t>& ids) {
  DCHECK(sql);
  if (!LazyOpen(kCreateIfNeeded))
    return false;

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  sql::Statement statement(db_->GetCachedStatement(statement_id, sql));

  std::vector<int64_t>::const_iterator iter = ids.begin();
  while (iter != ids.end()) {
    statement.BindInt64(0, *iter);
    if (!statement.Run())
      return false;
    statement.Reset(true);
    ++iter;
  }

  return transaction.Commit();
}

bool AppCacheDatabase::RunUniqueStatementWithInt64Result(const char* sql,
                                                         int64_t* result) {
  DCHECK(sql);
  sql::Statement statement(db_->GetUniqueStatement(sql));
  if (!statement.Step()) {
    return false;
  }
  *result = statement.ColumnInt64(0);
  return true;
}

bool AppCacheDatabase::FindResponseIdsForCacheHelper(
    int64_t cache_id,
    std::vector<int64_t>* ids_vector,
    std::set<int64_t>* ids_set) {
  DCHECK(ids_vector || ids_set);
  DCHECK(!(ids_vector && ids_set));
  if (!LazyOpen(kDontCreate))
    return false;

  const char kSql[] =
      "SELECT response_id FROM Entries WHERE cache_id = ?";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));

  statement.BindInt64(0, cache_id);
  while (statement.Step()) {
    int64_t id = statement.ColumnInt64(0);
    if (ids_set)
      ids_set->insert(id);
    else
      ids_vector->push_back(id);
  }

  return statement.Succeeded();
}

void AppCacheDatabase::ReadGroupRecord(
    const sql::Statement& statement, GroupRecord* record) {
  record->group_id = statement.ColumnInt64(0);
  record->origin = GURL(statement.ColumnString(1));
  record->manifest_url = GURL(statement.ColumnString(2));
  record->creation_time =
      base::Time::FromInternalValue(statement.ColumnInt64(3));

  const auto found = lazy_last_access_times_.find(record->group_id);
  if (found != lazy_last_access_times_.end()) {
    record->last_access_time = found->second;
  } else {
    record->last_access_time =
        base::Time::FromInternalValue(statement.ColumnInt64(4));
  }

  record->last_full_update_check_time =
      base::Time::FromInternalValue(statement.ColumnInt64(5));
  record->first_evictable_error_time =
      base::Time::FromInternalValue(statement.ColumnInt64(6));
}

void AppCacheDatabase::ReadCacheRecord(
    const sql::Statement& statement, CacheRecord* record) {
  record->cache_id = statement.ColumnInt64(0);
  record->group_id = statement.ColumnInt64(1);
  record->online_wildcard = statement.ColumnBool(2);
  record->update_time =
      base::Time::FromInternalValue(statement.ColumnInt64(3));
  record->cache_size = statement.ColumnInt64(4);
}

void AppCacheDatabase::ReadEntryRecord(
    const sql::Statement& statement, EntryRecord* record) {
  record->cache_id = statement.ColumnInt64(0);
  record->url = GURL(statement.ColumnString(1));
  record->flags = statement.ColumnInt(2);
  record->response_id = statement.ColumnInt64(3);
  record->response_size = statement.ColumnInt64(4);
}

void AppCacheDatabase::ReadNamespaceRecords(
      sql::Statement* statement,
      NamespaceRecordVector* intercepts,
      NamespaceRecordVector* fallbacks) {
  while (statement->Step()) {
    AppCacheNamespaceType type = static_cast<AppCacheNamespaceType>(
        statement->ColumnInt(2));
    NamespaceRecordVector* records =
        (type == APPCACHE_FALLBACK_NAMESPACE) ? fallbacks : intercepts;
    records->push_back(NamespaceRecord());
    ReadNamespaceRecord(statement, &records->back());
  }
}

void AppCacheDatabase::ReadNamespaceRecord(
    const sql::Statement* statement, NamespaceRecord* record) {
  record->cache_id = statement->ColumnInt64(0);
  record->origin = GURL(statement->ColumnString(1));
  int type_with_executable_bit = statement->ColumnInt(2);
  record->namespace_.namespace_url = GURL(statement->ColumnString(3));
  record->namespace_.target_url = GURL(statement->ColumnString(4));
  record->namespace_.is_pattern = statement->ColumnBool(5);

  // Note: quick and dirty storage for the 'executable' bit w/o changing
  // schemas, we use the high bit of 'type' field.
  record->namespace_.type = static_cast<AppCacheNamespaceType>
      (type_with_executable_bit & 0x7ffffff);
  record->namespace_.is_executable =
      (type_with_executable_bit & 0x80000000) != 0;
  DCHECK(!record->namespace_.is_executable ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          kEnableExecutableHandlers));
}

void AppCacheDatabase::ReadOnlineWhiteListRecord(
    const sql::Statement& statement, OnlineWhiteListRecord* record) {
  record->cache_id = statement.ColumnInt64(0);
  record->namespace_url = GURL(statement.ColumnString(1));
  record->is_pattern = statement.ColumnBool(2);
}

bool AppCacheDatabase::LazyOpen(bool create_if_needed) {
  if (db_)
    return true;

  // If we tried and failed once, don't try again in the same session
  // to avoid creating an incoherent mess on disk.
  if (is_disabled_)
    return false;

  // Avoid creating a database at all if we can.
  bool use_in_memory_db = db_file_path_.empty();
  if (!create_if_needed &&
      (use_in_memory_db || !base::PathExists(db_file_path_))) {
    return false;
  }

  db_.reset(new sql::Connection);
  meta_table_.reset(new sql::MetaTable);

  db_->set_histogram_tag("AppCache");

  bool opened = false;
  if (use_in_memory_db) {
    opened = db_->OpenInMemory();
  } else if (!base::CreateDirectory(db_file_path_.DirName())) {
    LOG(ERROR) << "Failed to create appcache directory.";
  } else {
    opened = db_->Open(db_file_path_);
    if (opened)
      db_->Preload();
  }

  if (!opened || !db_->QuickIntegrityCheck() || !EnsureDatabaseVersion()) {
    LOG(ERROR) << "Failed to open the appcache database.";
    AppCacheHistograms::CountInitResult(
        AppCacheHistograms::SQL_DATABASE_ERROR);

    // We're unable to open the database. This is a fatal error
    // which we can't recover from. We try to handle it by deleting
    // the existing appcache data and starting with a clean slate in
    // this browser session.
    if (!use_in_memory_db && DeleteExistingAndCreateNewDatabase())
      return true;

    Disable();
    return false;
  }

  AppCacheHistograms::CountInitResult(AppCacheHistograms::INIT_OK);
  was_corruption_detected_ = false;
  db_->set_error_callback(
      base::Bind(&AppCacheDatabase::OnDatabaseError, base::Unretained(this)));
  return true;
}

bool AppCacheDatabase::EnsureDatabaseVersion() {
  if (!sql::MetaTable::DoesTableExist(db_.get()))
    return CreateSchema();

  if (!meta_table_->Init(db_.get(), kCurrentVersion, kCompatibleVersion))
    return false;

  if (meta_table_->GetCompatibleVersionNumber() > kCurrentVersion) {
    LOG(WARNING) << "AppCache database is too new.";
    return false;
  }

  std::string stored_flags;
  meta_table_->GetValue(kExperimentFlagsKey, &stored_flags);
  if (stored_flags != GetActiveExperimentFlags())
    return false;

  if (meta_table_->GetVersionNumber() < kCurrentVersion)
    return UpgradeSchema();

#ifndef NDEBUG
  DCHECK(sql::MetaTable::DoesTableExist(db_.get()));
  for (int i = 0; i < kTableCount; ++i) {
    DCHECK(db_->DoesTableExist(kTables[i].table_name));
  }
  for (int i = 0; i < kIndexCount; ++i) {
    DCHECK(db_->DoesIndexExist(kIndexes[i].index_name));
  }
#endif

  return true;
}

bool AppCacheDatabase::CreateSchema() {
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  if (!meta_table_->Init(db_.get(), kCurrentVersion, kCompatibleVersion))
    return false;

  if (!meta_table_->SetValue(kExperimentFlagsKey,
                             GetActiveExperimentFlags())) {
    return false;
  }

  for (int i = 0; i < kTableCount; ++i) {
    if (!CreateTable(db_.get(), kTables[i]))
      return false;
  }

  for (int i = 0; i < kIndexCount; ++i) {
    if (!CreateIndex(db_.get(), kIndexes[i]))
      return false;
  }

  return transaction.Commit();
}

bool AppCacheDatabase::UpgradeSchema() {
#if defined(APPCACHE_USE_SIMPLE_CACHE)
  if (meta_table_->GetVersionNumber() < 6)
    return DeleteExistingAndCreateNewDatabase();
#endif
  if (meta_table_->GetVersionNumber() == 3) {
    // version 3 was pre 12/17/2011
    DCHECK_EQ(strcmp(kNamespacesTable, kTables[3].table_name), 0);
    DCHECK_EQ(strcmp(kNamespacesTable, kIndexes[6].table_name), 0);
    DCHECK_EQ(strcmp(kNamespacesTable, kIndexes[7].table_name), 0);
    DCHECK_EQ(strcmp(kNamespacesTable, kIndexes[8].table_name), 0);

    const TableInfo kNamespaceTable_v4 = {
        kNamespacesTable,
        "(cache_id INTEGER,"
        " origin TEXT,"  // intentionally not normalized
        " type INTEGER,"
        " namespace_url TEXT,"
        " target_url TEXT)"
    };

    // Migrate from the old FallbackNameSpaces to the newer Namespaces table,
    // but without the is_pattern column added in v5.
    sql::Transaction transaction(db_.get());
    if (!transaction.Begin() ||
        !CreateTable(db_.get(), kNamespaceTable_v4)) {
      return false;
    }

    // Move data from the old table to the new table, setting the
    // 'type' for all current records to the value for
    // APPCACHE_FALLBACK_NAMESPACE.
    DCHECK_EQ(0, static_cast<int>(APPCACHE_FALLBACK_NAMESPACE));
    if (!db_->Execute(
            "INSERT INTO Namespaces"
            "  SELECT cache_id, origin, 0, namespace_url, fallback_entry_url"
            "  FROM FallbackNameSpaces")) {
      return false;
    }

    // Drop the old table, indexes on that table are also removed by this.
    if (!db_->Execute("DROP TABLE FallbackNameSpaces"))
      return false;

    // Create new indexes.
    if (!CreateIndex(db_.get(), kIndexes[6]) ||
        !CreateIndex(db_.get(), kIndexes[7]) ||
        !CreateIndex(db_.get(), kIndexes[8])) {
      return false;
    }

    meta_table_->SetVersionNumber(4);
    meta_table_->SetCompatibleVersionNumber(4);
    if (!transaction.Commit())
      return false;
  }

  if (meta_table_->GetVersionNumber() == 4) {
    // version 4 pre 3/30/2013
    // Add the is_pattern column to the Namespaces and OnlineWhitelists tables.
    DCHECK_EQ(strcmp(kNamespacesTable, "Namespaces"), 0);
    sql::Transaction transaction(db_.get());
    if (!transaction.Begin())
      return false;
    if (!db_->Execute(
            "ALTER TABLE Namespaces ADD COLUMN"
            "  is_pattern INTEGER CHECK(is_pattern IN (0, 1))")) {
      return false;
    }
    if (!db_->Execute(
            "ALTER TABLE OnlineWhitelists ADD COLUMN"
            "  is_pattern INTEGER CHECK(is_pattern IN (0, 1))")) {
      return false;
    }
    meta_table_->SetVersionNumber(5);
    meta_table_->SetCompatibleVersionNumber(5);
    if (!transaction.Commit())
      return false;
  }

#if defined(APPCACHE_USE_SIMPLE_CACHE)
  // The schema version number was increased to 6 when we switched to the
  // SimpleCache for Android, but the SQL part of the schema is identical
  // to v5 on desktop chrome.
  if (meta_table_->GetVersionNumber() == 6) {
#else
  if (meta_table_->GetVersionNumber() == 5) {
#endif
    // Versions 5 and 6 were pre-July 2015.
    // Version 7 adds support for expiring caches that are failing to update.
    sql::Transaction transaction(db_.get());
    if (!transaction.Begin() ||
        !db_->Execute(
            "ALTER TABLE Groups ADD COLUMN"
            " last_full_update_check_time INTEGER") ||
        !db_->Execute(
            "ALTER TABLE Groups ADD COLUMN"
            " first_evictable_error_time INTEGER") ||
        !db_->Execute(
            "UPDATE Groups"
            " SET last_full_update_check_time ="
            "   (SELECT update_time FROM Caches"
            "    WHERE Caches.group_id = Groups.group_id)")) {
      return false;
    }
    meta_table_->SetVersionNumber(7);
    meta_table_->SetCompatibleVersionNumber(7);
    return transaction.Commit();
  }

  // If there is no upgrade path for the version on disk to the current
  // version, nuke everything and start over.
  return DeleteExistingAndCreateNewDatabase();
}

void AppCacheDatabase::ResetConnectionAndTables() {
  meta_table_.reset();
  db_.reset();
}

bool AppCacheDatabase::DeleteExistingAndCreateNewDatabase() {
  DCHECK(!db_file_path_.empty());
  DCHECK(base::PathExists(db_file_path_));
  VLOG(1) << "Deleting existing appcache data and starting over.";

  ResetConnectionAndTables();

  // This also deletes the disk cache data.
  base::FilePath directory = db_file_path_.DirName();
  if (!base::DeleteFile(directory, true))
    return false;

  // Make sure the steps above actually deleted things.
  if (base::PathExists(directory))
    return false;

  if (!base::CreateDirectory(directory))
    return false;

  // So we can't go recursive.
  if (is_recreating_)
    return false;

  base::AutoReset<bool> auto_reset(&is_recreating_, true);
  return LazyOpen(kCreateIfNeeded);
}

void AppCacheDatabase::OnDatabaseError(int err, sql::Statement* stmt) {
  was_corruption_detected_ |= sql::IsErrorCatastrophic(err);
  if (!db_->IsExpectedSqliteError(err))
    DLOG(ERROR) << db_->GetErrorMessage();
  // TODO: Maybe use non-catostrophic errors to trigger a full integrity check?
}

}  // namespace content
