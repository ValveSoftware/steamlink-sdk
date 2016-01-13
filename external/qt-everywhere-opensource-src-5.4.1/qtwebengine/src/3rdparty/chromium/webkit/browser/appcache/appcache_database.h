// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_APPCACHE_APPCACHE_DATABASE_H_
#define WEBKIT_BROWSER_APPCACHE_APPCACHE_DATABASE_H_

#include <map>
#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "url/gurl.h"
#include "webkit/browser/webkit_storage_browser_export.h"
#include "webkit/common/appcache/appcache_interfaces.h"

namespace sql {
class Connection;
class MetaTable;
class Statement;
class StatementID;
}

namespace content {
FORWARD_DECLARE_TEST(AppCacheDatabaseTest, CacheRecords);
FORWARD_DECLARE_TEST(AppCacheDatabaseTest, EntryRecords);
FORWARD_DECLARE_TEST(AppCacheDatabaseTest, QuickIntegrityCheck);
FORWARD_DECLARE_TEST(AppCacheDatabaseTest, NamespaceRecords);
FORWARD_DECLARE_TEST(AppCacheDatabaseTest, GroupRecords);
FORWARD_DECLARE_TEST(AppCacheDatabaseTest, LazyOpen);
FORWARD_DECLARE_TEST(AppCacheDatabaseTest, ExperimentalFlags);
FORWARD_DECLARE_TEST(AppCacheDatabaseTest, OnlineWhiteListRecords);
FORWARD_DECLARE_TEST(AppCacheDatabaseTest, ReCreate);
FORWARD_DECLARE_TEST(AppCacheDatabaseTest, DeletableResponseIds);
FORWARD_DECLARE_TEST(AppCacheDatabaseTest, OriginUsage);
FORWARD_DECLARE_TEST(AppCacheDatabaseTest, UpgradeSchema3to5);
FORWARD_DECLARE_TEST(AppCacheDatabaseTest, UpgradeSchema4to5);
FORWARD_DECLARE_TEST(AppCacheDatabaseTest, WasCorrutionDetected);
class AppCacheDatabaseTest;
class AppCacheStorageImplTest;
}

namespace appcache {

class WEBKIT_STORAGE_BROWSER_EXPORT AppCacheDatabase {
 public:
  struct WEBKIT_STORAGE_BROWSER_EXPORT GroupRecord {
    GroupRecord();
    ~GroupRecord();

    int64 group_id;
    GURL origin;
    GURL manifest_url;
    base::Time creation_time;
    base::Time last_access_time;
  };

  struct WEBKIT_STORAGE_BROWSER_EXPORT CacheRecord {
    CacheRecord()
        : cache_id(0), group_id(0), online_wildcard(false), cache_size(0) {}

    int64 cache_id;
    int64 group_id;
    bool online_wildcard;
    base::Time update_time;
    int64 cache_size;  // the sum of all response sizes in this cache
  };

  struct EntryRecord {
    EntryRecord() : cache_id(0), flags(0), response_id(0), response_size(0) {}

    int64 cache_id;
    GURL url;
    int flags;
    int64 response_id;
    int64 response_size;
  };

  struct WEBKIT_STORAGE_BROWSER_EXPORT NamespaceRecord {
    NamespaceRecord();
    ~NamespaceRecord();

    int64 cache_id;
    GURL origin;
    Namespace namespace_;
  };

  typedef std::vector<NamespaceRecord> NamespaceRecordVector;

  struct OnlineWhiteListRecord {
    OnlineWhiteListRecord() : cache_id(0), is_pattern(false) {}

    int64 cache_id;
    GURL namespace_url;
    bool is_pattern;
  };

  explicit AppCacheDatabase(const base::FilePath& path);
  ~AppCacheDatabase();

  void Disable();
  bool is_disabled() const { return is_disabled_; }
  bool was_corruption_detected() const { return was_corruption_detected_; }

  int64 GetOriginUsage(const GURL& origin);
  bool GetAllOriginUsage(std::map<GURL, int64>* usage_map);

  bool FindOriginsWithGroups(std::set<GURL>* origins);
  bool FindLastStorageIds(
      int64* last_group_id, int64* last_cache_id, int64* last_response_id,
      int64* last_deletable_response_rowid);

  bool FindGroup(int64 group_id, GroupRecord* record);
  bool FindGroupForManifestUrl(const GURL& manifest_url, GroupRecord* record);
  bool FindGroupsForOrigin(
      const GURL& origin, std::vector<GroupRecord>* records);
  bool FindGroupForCache(int64 cache_id, GroupRecord* record);
  bool UpdateGroupLastAccessTime(
      int64 group_id, base::Time last_access_time);
  bool InsertGroup(const GroupRecord* record);
  bool DeleteGroup(int64 group_id);

  bool FindCache(int64 cache_id, CacheRecord* record);
  bool FindCacheForGroup(int64 group_id, CacheRecord* record);
  bool FindCachesForOrigin(
      const GURL& origin, std::vector<CacheRecord>* records);
  bool InsertCache(const CacheRecord* record);
  bool DeleteCache(int64 cache_id);

  bool FindEntriesForCache(
      int64 cache_id, std::vector<EntryRecord>* records);
  bool FindEntriesForUrl(
      const GURL& url, std::vector<EntryRecord>* records);
  bool FindEntry(int64 cache_id, const GURL& url, EntryRecord* record);
  bool InsertEntry(const EntryRecord* record);
  bool InsertEntryRecords(
      const std::vector<EntryRecord>& records);
  bool DeleteEntriesForCache(int64 cache_id);
  bool AddEntryFlags(const GURL& entry_url, int64 cache_id,
                     int additional_flags);
  bool FindResponseIdsForCacheAsVector(
      int64 cache_id, std::vector<int64>* response_ids) {
    return FindResponseIdsForCacheHelper(cache_id, response_ids, NULL);
  }
  bool FindResponseIdsForCacheAsSet(
      int64 cache_id, std::set<int64>* response_ids) {
    return FindResponseIdsForCacheHelper(cache_id, NULL, response_ids);
  }

  bool FindNamespacesForOrigin(
      const GURL& origin,
      NamespaceRecordVector* intercepts,
      NamespaceRecordVector* fallbacks);
  bool FindNamespacesForCache(
      int64 cache_id,
      NamespaceRecordVector* intercepts,
      std::vector<NamespaceRecord>* fallbacks);
  bool InsertNamespaceRecords(
      const NamespaceRecordVector& records);
  bool InsertNamespace(const NamespaceRecord* record);
  bool DeleteNamespacesForCache(int64 cache_id);

  bool FindOnlineWhiteListForCache(
      int64 cache_id, std::vector<OnlineWhiteListRecord>* records);
  bool InsertOnlineWhiteList(const OnlineWhiteListRecord* record);
  bool InsertOnlineWhiteListRecords(
      const std::vector<OnlineWhiteListRecord>& records);
  bool DeleteOnlineWhiteListForCache(int64 cache_id);

  bool GetDeletableResponseIds(std::vector<int64>* response_ids,
                               int64 max_rowid, int limit);
  bool InsertDeletableResponseIds(const std::vector<int64>& response_ids);
  bool DeleteDeletableResponseIds(const std::vector<int64>& response_ids);

  // So our callers can wrap operations in transactions.
  sql::Connection* db_connection() {
    LazyOpen(true);
    return db_.get();
  }

 private:
  bool RunCachedStatementWithIds(
      const sql::StatementID& statement_id, const char* sql,
      const std::vector<int64>& ids);
  bool RunUniqueStatementWithInt64Result(const char* sql, int64* result);

  bool FindResponseIdsForCacheHelper(
      int64 cache_id, std::vector<int64>* ids_vector,
      std::set<int64>* ids_set);

  // Record retrieval helpers
  void ReadGroupRecord(const sql::Statement& statement, GroupRecord* record);
  void ReadCacheRecord(const sql::Statement& statement, CacheRecord* record);
  void ReadEntryRecord(const sql::Statement& statement, EntryRecord* record);
  void ReadNamespaceRecords(
      sql::Statement* statement,
      NamespaceRecordVector* intercepts,
      NamespaceRecordVector* fallbacks);
  void ReadNamespaceRecord(
      const sql::Statement* statement, NamespaceRecord* record);
  void ReadOnlineWhiteListRecord(
      const sql::Statement& statement, OnlineWhiteListRecord* record);

  // Database creation
  bool LazyOpen(bool create_if_needed);
  bool EnsureDatabaseVersion();
  bool CreateSchema();
  bool UpgradeSchema();

  void ResetConnectionAndTables();

  // Deletes the existing database file and the entire directory containing
  // the database file including the disk cache in which response headers
  // and bodies are stored, and then creates a new database file.
  bool DeleteExistingAndCreateNewDatabase();

  void OnDatabaseError(int err, sql::Statement* stmt);

  base::FilePath db_file_path_;
  scoped_ptr<sql::Connection> db_;
  scoped_ptr<sql::MetaTable> meta_table_;
  bool is_disabled_;
  bool is_recreating_;
  bool was_corruption_detected_;

  friend class content::AppCacheDatabaseTest;
  friend class content::AppCacheStorageImplTest;

  FRIEND_TEST_ALL_PREFIXES(content::AppCacheDatabaseTest, CacheRecords);
  FRIEND_TEST_ALL_PREFIXES(content::AppCacheDatabaseTest, EntryRecords);
  FRIEND_TEST_ALL_PREFIXES(content::AppCacheDatabaseTest, QuickIntegrityCheck);
  FRIEND_TEST_ALL_PREFIXES(content::AppCacheDatabaseTest, NamespaceRecords);
  FRIEND_TEST_ALL_PREFIXES(content::AppCacheDatabaseTest, GroupRecords);
  FRIEND_TEST_ALL_PREFIXES(content::AppCacheDatabaseTest, LazyOpen);
  FRIEND_TEST_ALL_PREFIXES(content::AppCacheDatabaseTest, ExperimentalFlags);
  FRIEND_TEST_ALL_PREFIXES(content::AppCacheDatabaseTest,
                           OnlineWhiteListRecords);
  FRIEND_TEST_ALL_PREFIXES(content::AppCacheDatabaseTest, ReCreate);
  FRIEND_TEST_ALL_PREFIXES(content::AppCacheDatabaseTest, DeletableResponseIds);
  FRIEND_TEST_ALL_PREFIXES(content::AppCacheDatabaseTest, OriginUsage);
  FRIEND_TEST_ALL_PREFIXES(content::AppCacheDatabaseTest, UpgradeSchema3to5);
  FRIEND_TEST_ALL_PREFIXES(content::AppCacheDatabaseTest, UpgradeSchema4to5);
  FRIEND_TEST_ALL_PREFIXES(content::AppCacheDatabaseTest, WasCorrutionDetected);

  DISALLOW_COPY_AND_ASSIGN(AppCacheDatabase);
};

}  // namespace appcache

#endif  // WEBKIT_BROWSER_APPCACHE_APPCACHE_DATABASE_H_
