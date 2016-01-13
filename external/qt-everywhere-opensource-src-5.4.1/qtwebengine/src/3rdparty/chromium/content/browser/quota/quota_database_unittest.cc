// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <iterator>
#include <set>

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "content/public/test/mock_special_storage_policy.h"
#include "sql/connection.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "webkit/browser/quota/quota_database.h"

using quota::kStorageTypePersistent;
using quota::kStorageTypeTemporary;
using quota::QuotaDatabase;

namespace content {
namespace {

const base::Time kZeroTime;

const char kDBFileName[] = "quota_manager.db";

}  // namespace

class QuotaDatabaseTest : public testing::Test {
 protected:
  typedef QuotaDatabase::QuotaTableEntry QuotaTableEntry;
  typedef QuotaDatabase::QuotaTableCallback QuotaTableCallback;
  typedef QuotaDatabase::OriginInfoTableCallback
      OriginInfoTableCallback;

  void LazyOpen(const base::FilePath& kDbFile) {
    QuotaDatabase db(kDbFile);
    EXPECT_FALSE(db.LazyOpen(false));
    ASSERT_TRUE(db.LazyOpen(true));
    EXPECT_TRUE(db.db_.get());
    EXPECT_TRUE(kDbFile.empty() || base::PathExists(kDbFile));
  }

  void UpgradeSchemaV2toV3(const base::FilePath& kDbFile) {
    const QuotaTableEntry entries[] = {
      QuotaTableEntry("a", kStorageTypeTemporary,  1),
      QuotaTableEntry("b", kStorageTypeTemporary,  2),
      QuotaTableEntry("c", kStorageTypePersistent, 3),
    };

    CreateV2Database(kDbFile, entries, ARRAYSIZE_UNSAFE(entries));

    QuotaDatabase db(kDbFile);
    EXPECT_TRUE(db.LazyOpen(true));
    EXPECT_TRUE(db.db_.get());

    typedef EntryVerifier<QuotaTableEntry> Verifier;
    Verifier verifier(entries, entries + ARRAYSIZE_UNSAFE(entries));
    EXPECT_TRUE(db.DumpQuotaTable(
        base::Bind(&Verifier::Run, base::Unretained(&verifier))));
    EXPECT_TRUE(verifier.table.empty());
  }

  void HostQuota(const base::FilePath& kDbFile) {
    QuotaDatabase db(kDbFile);
    ASSERT_TRUE(db.LazyOpen(true));

    const char* kHost = "foo.com";
    const int kQuota1 = 13579;
    const int kQuota2 = kQuota1 + 1024;

    int64 quota = -1;
    EXPECT_FALSE(db.GetHostQuota(kHost, kStorageTypeTemporary, &quota));
    EXPECT_FALSE(db.GetHostQuota(kHost, kStorageTypePersistent, &quota));

    // Insert quota for temporary.
    EXPECT_TRUE(db.SetHostQuota(kHost, kStorageTypeTemporary, kQuota1));
    EXPECT_TRUE(db.GetHostQuota(kHost, kStorageTypeTemporary, &quota));
    EXPECT_EQ(kQuota1, quota);

    // Update quota for temporary.
    EXPECT_TRUE(db.SetHostQuota(kHost, kStorageTypeTemporary, kQuota2));
    EXPECT_TRUE(db.GetHostQuota(kHost, kStorageTypeTemporary, &quota));
    EXPECT_EQ(kQuota2, quota);

    // Quota for persistent must not be updated.
    EXPECT_FALSE(db.GetHostQuota(kHost, kStorageTypePersistent, &quota));

    // Delete temporary storage quota.
    EXPECT_TRUE(db.DeleteHostQuota(kHost, kStorageTypeTemporary));
    EXPECT_FALSE(db.GetHostQuota(kHost, kStorageTypeTemporary, &quota));
  }

  void GlobalQuota(const base::FilePath& kDbFile) {
    QuotaDatabase db(kDbFile);
    ASSERT_TRUE(db.LazyOpen(true));

    const char* kTempQuotaKey = QuotaDatabase::kTemporaryQuotaOverrideKey;
    const char* kAvailSpaceKey = QuotaDatabase::kDesiredAvailableSpaceKey;

    int64 value = 0;
    const int64 kValue1 = 456;
    const int64 kValue2 = 123000;
    EXPECT_FALSE(db.GetQuotaConfigValue(kTempQuotaKey, &value));
    EXPECT_FALSE(db.GetQuotaConfigValue(kAvailSpaceKey, &value));

    EXPECT_TRUE(db.SetQuotaConfigValue(kTempQuotaKey, kValue1));
    EXPECT_TRUE(db.GetQuotaConfigValue(kTempQuotaKey, &value));
    EXPECT_EQ(kValue1, value);

    EXPECT_TRUE(db.SetQuotaConfigValue(kTempQuotaKey, kValue2));
    EXPECT_TRUE(db.GetQuotaConfigValue(kTempQuotaKey, &value));
    EXPECT_EQ(kValue2, value);

    EXPECT_TRUE(db.SetQuotaConfigValue(kAvailSpaceKey, kValue1));
    EXPECT_TRUE(db.GetQuotaConfigValue(kAvailSpaceKey, &value));
    EXPECT_EQ(kValue1, value);

    EXPECT_TRUE(db.SetQuotaConfigValue(kAvailSpaceKey, kValue2));
    EXPECT_TRUE(db.GetQuotaConfigValue(kAvailSpaceKey, &value));
    EXPECT_EQ(kValue2, value);
  }

  void OriginLastAccessTimeLRU(const base::FilePath& kDbFile) {
    QuotaDatabase db(kDbFile);
    ASSERT_TRUE(db.LazyOpen(true));

    std::set<GURL> exceptions;
    GURL origin;
    EXPECT_TRUE(db.GetLRUOrigin(kStorageTypeTemporary, exceptions,
                                NULL, &origin));
    EXPECT_TRUE(origin.is_empty());

    const GURL kOrigin1("http://a/");
    const GURL kOrigin2("http://b/");
    const GURL kOrigin3("http://c/");
    const GURL kOrigin4("http://p/");

    // Adding three temporary storages, and
    EXPECT_TRUE(db.SetOriginLastAccessTime(
        kOrigin1, kStorageTypeTemporary, base::Time::FromInternalValue(10)));
    EXPECT_TRUE(db.SetOriginLastAccessTime(
        kOrigin2, kStorageTypeTemporary, base::Time::FromInternalValue(20)));
    EXPECT_TRUE(db.SetOriginLastAccessTime(
        kOrigin3, kStorageTypeTemporary, base::Time::FromInternalValue(30)));

    // one persistent.
    EXPECT_TRUE(db.SetOriginLastAccessTime(
        kOrigin4, kStorageTypePersistent, base::Time::FromInternalValue(40)));

    EXPECT_TRUE(db.GetLRUOrigin(kStorageTypeTemporary, exceptions,
                                NULL, &origin));
    EXPECT_EQ(kOrigin1.spec(), origin.spec());

    // Test that unlimited origins are exluded from eviction, but
    // protected origins are not excluded.
    scoped_refptr<MockSpecialStoragePolicy> policy(
        new MockSpecialStoragePolicy);
    policy->AddUnlimited(kOrigin1);
    policy->AddProtected(kOrigin2);
    EXPECT_TRUE(db.GetLRUOrigin(
        kStorageTypeTemporary, exceptions, policy.get(), &origin));
    EXPECT_EQ(kOrigin2.spec(), origin.spec());

    exceptions.insert(kOrigin1);
    EXPECT_TRUE(db.GetLRUOrigin(kStorageTypeTemporary, exceptions,
                                NULL, &origin));
    EXPECT_EQ(kOrigin2.spec(), origin.spec());

    exceptions.insert(kOrigin2);
    EXPECT_TRUE(db.GetLRUOrigin(kStorageTypeTemporary, exceptions,
                                NULL, &origin));
    EXPECT_EQ(kOrigin3.spec(), origin.spec());

    exceptions.insert(kOrigin3);
    EXPECT_TRUE(db.GetLRUOrigin(kStorageTypeTemporary, exceptions,
                                NULL, &origin));
    EXPECT_TRUE(origin.is_empty());

    EXPECT_TRUE(db.SetOriginLastAccessTime(
        kOrigin1, kStorageTypeTemporary, base::Time::Now()));

    // Delete origin/type last access time information.
    EXPECT_TRUE(db.DeleteOriginInfo(kOrigin3, kStorageTypeTemporary));

    // Querying again to see if the deletion has worked.
    exceptions.clear();
    EXPECT_TRUE(db.GetLRUOrigin(kStorageTypeTemporary, exceptions,
                                NULL, &origin));
    EXPECT_EQ(kOrigin2.spec(), origin.spec());

    exceptions.insert(kOrigin1);
    exceptions.insert(kOrigin2);
    EXPECT_TRUE(db.GetLRUOrigin(kStorageTypeTemporary, exceptions,
                                NULL, &origin));
    EXPECT_TRUE(origin.is_empty());
  }

  void OriginLastModifiedSince(const base::FilePath& kDbFile) {
    QuotaDatabase db(kDbFile);
    ASSERT_TRUE(db.LazyOpen(true));

    std::set<GURL> origins;
    EXPECT_TRUE(db.GetOriginsModifiedSince(
        kStorageTypeTemporary, &origins, base::Time()));
    EXPECT_TRUE(origins.empty());

    const GURL kOrigin1("http://a/");
    const GURL kOrigin2("http://b/");
    const GURL kOrigin3("http://c/");

    // Report last mod time for the test origins.
    EXPECT_TRUE(db.SetOriginLastModifiedTime(
        kOrigin1, kStorageTypeTemporary, base::Time::FromInternalValue(0)));
    EXPECT_TRUE(db.SetOriginLastModifiedTime(
        kOrigin2, kStorageTypeTemporary, base::Time::FromInternalValue(10)));
    EXPECT_TRUE(db.SetOriginLastModifiedTime(
        kOrigin3, kStorageTypeTemporary, base::Time::FromInternalValue(20)));

    EXPECT_TRUE(db.GetOriginsModifiedSince(
        kStorageTypeTemporary, &origins, base::Time()));
    EXPECT_EQ(3U, origins.size());
    EXPECT_EQ(1U, origins.count(kOrigin1));
    EXPECT_EQ(1U, origins.count(kOrigin2));
    EXPECT_EQ(1U, origins.count(kOrigin3));

    EXPECT_TRUE(db.GetOriginsModifiedSince(
        kStorageTypeTemporary, &origins, base::Time::FromInternalValue(5)));
    EXPECT_EQ(2U, origins.size());
    EXPECT_EQ(0U, origins.count(kOrigin1));
    EXPECT_EQ(1U, origins.count(kOrigin2));
    EXPECT_EQ(1U, origins.count(kOrigin3));

    EXPECT_TRUE(db.GetOriginsModifiedSince(
        kStorageTypeTemporary, &origins, base::Time::FromInternalValue(15)));
    EXPECT_EQ(1U, origins.size());
    EXPECT_EQ(0U, origins.count(kOrigin1));
    EXPECT_EQ(0U, origins.count(kOrigin2));
    EXPECT_EQ(1U, origins.count(kOrigin3));

    EXPECT_TRUE(db.GetOriginsModifiedSince(
        kStorageTypeTemporary, &origins, base::Time::FromInternalValue(25)));
    EXPECT_TRUE(origins.empty());

    // Update origin1's mod time but for persistent storage.
    EXPECT_TRUE(db.SetOriginLastModifiedTime(
        kOrigin1, kStorageTypePersistent, base::Time::FromInternalValue(30)));

    // Must have no effects on temporary origins info.
    EXPECT_TRUE(db.GetOriginsModifiedSince(
        kStorageTypeTemporary, &origins, base::Time::FromInternalValue(5)));
    EXPECT_EQ(2U, origins.size());
    EXPECT_EQ(0U, origins.count(kOrigin1));
    EXPECT_EQ(1U, origins.count(kOrigin2));
    EXPECT_EQ(1U, origins.count(kOrigin3));

    // One more update for persistent origin2.
    EXPECT_TRUE(db.SetOriginLastModifiedTime(
        kOrigin2, kStorageTypePersistent, base::Time::FromInternalValue(40)));

    EXPECT_TRUE(db.GetOriginsModifiedSince(
        kStorageTypePersistent, &origins, base::Time::FromInternalValue(25)));
    EXPECT_EQ(2U, origins.size());
    EXPECT_EQ(1U, origins.count(kOrigin1));
    EXPECT_EQ(1U, origins.count(kOrigin2));
    EXPECT_EQ(0U, origins.count(kOrigin3));

    EXPECT_TRUE(db.GetOriginsModifiedSince(
        kStorageTypePersistent, &origins, base::Time::FromInternalValue(35)));
    EXPECT_EQ(1U, origins.size());
    EXPECT_EQ(0U, origins.count(kOrigin1));
    EXPECT_EQ(1U, origins.count(kOrigin2));
    EXPECT_EQ(0U, origins.count(kOrigin3));
  }

  void RegisterInitialOriginInfo(const base::FilePath& kDbFile) {
    QuotaDatabase db(kDbFile);

    const GURL kOrigins[] = {
      GURL("http://a/"),
      GURL("http://b/"),
      GURL("http://c/") };
    std::set<GURL> origins(kOrigins, kOrigins + ARRAYSIZE_UNSAFE(kOrigins));

    EXPECT_TRUE(db.RegisterInitialOriginInfo(origins, kStorageTypeTemporary));

    int used_count = -1;
    EXPECT_TRUE(db.FindOriginUsedCount(GURL("http://a/"),
                                       kStorageTypeTemporary,
                                       &used_count));
    EXPECT_EQ(0, used_count);

    EXPECT_TRUE(db.SetOriginLastAccessTime(
        GURL("http://a/"), kStorageTypeTemporary,
        base::Time::FromDoubleT(1.0)));
    used_count = -1;
    EXPECT_TRUE(db.FindOriginUsedCount(GURL("http://a/"),
                                       kStorageTypeTemporary,
                                       &used_count));
    EXPECT_EQ(1, used_count);

    EXPECT_TRUE(db.RegisterInitialOriginInfo(origins, kStorageTypeTemporary));

    used_count = -1;
    EXPECT_TRUE(db.FindOriginUsedCount(GURL("http://a/"),
                                       kStorageTypeTemporary,
                                       &used_count));
    EXPECT_EQ(1, used_count);
  }

  template <typename EntryType>
  struct EntryVerifier {
    std::set<EntryType> table;

    template <typename Iterator>
    EntryVerifier(Iterator itr, Iterator end)
        : table(itr, end) {}

    bool Run(const EntryType& entry) {
      EXPECT_EQ(1u, table.erase(entry));
      return true;
    }
  };

  void DumpQuotaTable(const base::FilePath& kDbFile) {
    QuotaTableEntry kTableEntries[] = {
      QuotaTableEntry("http://go/", kStorageTypeTemporary, 1),
      QuotaTableEntry("http://oo/", kStorageTypeTemporary, 2),
      QuotaTableEntry("http://gle/", kStorageTypePersistent, 3)
    };
    QuotaTableEntry* begin = kTableEntries;
    QuotaTableEntry* end = kTableEntries + ARRAYSIZE_UNSAFE(kTableEntries);

    QuotaDatabase db(kDbFile);
    EXPECT_TRUE(db.LazyOpen(true));
    AssignQuotaTable(db.db_.get(), begin, end);
    db.Commit();

    typedef EntryVerifier<QuotaTableEntry> Verifier;
    Verifier verifier(begin, end);
    EXPECT_TRUE(db.DumpQuotaTable(
        base::Bind(&Verifier::Run, base::Unretained(&verifier))));
    EXPECT_TRUE(verifier.table.empty());
  }

  void DumpOriginInfoTable(const base::FilePath& kDbFile) {
    base::Time now(base::Time::Now());
    typedef QuotaDatabase::OriginInfoTableEntry Entry;
    Entry kTableEntries[] = {
      Entry(GURL("http://go/"), kStorageTypeTemporary, 2147483647, now, now),
      Entry(GURL("http://oo/"), kStorageTypeTemporary, 0, now, now),
      Entry(GURL("http://gle/"), kStorageTypeTemporary, 1, now, now),
    };
    Entry* begin = kTableEntries;
    Entry* end = kTableEntries + ARRAYSIZE_UNSAFE(kTableEntries);

    QuotaDatabase db(kDbFile);
    EXPECT_TRUE(db.LazyOpen(true));
    AssignOriginInfoTable(db.db_.get(), begin, end);
    db.Commit();

    typedef EntryVerifier<Entry> Verifier;
    Verifier verifier(begin, end);
    EXPECT_TRUE(db.DumpOriginInfoTable(
        base::Bind(&Verifier::Run, base::Unretained(&verifier))));
    EXPECT_TRUE(verifier.table.empty());
  }

 private:
  template <typename Iterator>
  void AssignQuotaTable(sql::Connection* db, Iterator itr, Iterator end) {
    ASSERT_NE(db, (sql::Connection*)NULL);
    for (; itr != end; ++itr) {
      const char* kSql =
          "INSERT INTO HostQuotaTable"
          " (host, type, quota)"
          " VALUES (?, ?, ?)";
      sql::Statement statement;
      statement.Assign(db->GetCachedStatement(SQL_FROM_HERE, kSql));
      ASSERT_TRUE(statement.is_valid());

      statement.BindString(0, itr->host);
      statement.BindInt(1, static_cast<int>(itr->type));
      statement.BindInt64(2, itr->quota);
      EXPECT_TRUE(statement.Run());
    }
  }

  template <typename Iterator>
  void AssignOriginInfoTable(sql::Connection* db, Iterator itr, Iterator end) {
    ASSERT_NE(db, (sql::Connection*)NULL);
    for (; itr != end; ++itr) {
      const char* kSql =
          "INSERT INTO OriginInfoTable"
          " (origin, type, used_count, last_access_time, last_modified_time)"
          " VALUES (?, ?, ?, ?, ?)";
      sql::Statement statement;
      statement.Assign(db->GetCachedStatement(SQL_FROM_HERE, kSql));
      ASSERT_TRUE(statement.is_valid());

      statement.BindString(0, itr->origin.spec());
      statement.BindInt(1, static_cast<int>(itr->type));
      statement.BindInt(2, itr->used_count);
      statement.BindInt64(3, itr->last_access_time.ToInternalValue());
      statement.BindInt64(4, itr->last_modified_time.ToInternalValue());
      EXPECT_TRUE(statement.Run());
    }
  }

  bool OpenDatabase(sql::Connection* db, const base::FilePath& kDbFile) {
    if (kDbFile.empty()) {
      return db->OpenInMemory();
    }
    if (!base::CreateDirectory(kDbFile.DirName()))
      return false;
    if (!db->Open(kDbFile))
      return false;
    db->Preload();
    return true;
  }

  // Create V2 database and populate some data.
  void CreateV2Database(
      const base::FilePath& kDbFile,
      const QuotaTableEntry* entries,
      size_t entries_size) {
    scoped_ptr<sql::Connection> db(new sql::Connection);
    scoped_ptr<sql::MetaTable> meta_table(new sql::MetaTable);

    // V2 schema definitions.
    static const int kCurrentVersion = 2;
    static const int kCompatibleVersion = 2;
    static const char kHostQuotaTable[] = "HostQuotaTable";
    static const char kOriginLastAccessTable[] = "OriginLastAccessTable";
    static const QuotaDatabase::TableSchema kTables[] = {
      { kHostQuotaTable,
        "(host TEXT NOT NULL,"
        " type INTEGER NOT NULL,"
        " quota INTEGER,"
        " UNIQUE(host, type))" },
      { kOriginLastAccessTable,
        "(origin TEXT NOT NULL,"
        " type INTEGER NOT NULL,"
        " used_count INTEGER,"
        " last_access_time INTEGER,"
        " UNIQUE(origin, type))" },
    };
    static const QuotaDatabase::IndexSchema kIndexes[] = {
      { "HostIndex",
        kHostQuotaTable,
        "(host)",
        false },
      { "OriginLastAccessIndex",
        kOriginLastAccessTable,
        "(origin, last_access_time)",
        false },
    };

    ASSERT_TRUE(OpenDatabase(db.get(), kDbFile));
    EXPECT_TRUE(QuotaDatabase::CreateSchema(
            db.get(), meta_table.get(),
            kCurrentVersion, kCompatibleVersion,
            kTables, ARRAYSIZE_UNSAFE(kTables),
            kIndexes, ARRAYSIZE_UNSAFE(kIndexes)));

    // V2 and V3 QuotaTable are compatible, so we can simply use
    // AssignQuotaTable to poplulate v2 database here.
    db->BeginTransaction();
    AssignQuotaTable(db.get(), entries, entries + entries_size);
    db->CommitTransaction();
  }

  base::MessageLoop message_loop_;
};

TEST_F(QuotaDatabaseTest, LazyOpen) {
  base::ScopedTempDir data_dir;
  ASSERT_TRUE(data_dir.CreateUniqueTempDir());
  const base::FilePath kDbFile = data_dir.path().AppendASCII(kDBFileName);
  LazyOpen(kDbFile);
  LazyOpen(base::FilePath());
}

TEST_F(QuotaDatabaseTest, UpgradeSchema) {
  base::ScopedTempDir data_dir;
  ASSERT_TRUE(data_dir.CreateUniqueTempDir());
  const base::FilePath kDbFile = data_dir.path().AppendASCII(kDBFileName);
  UpgradeSchemaV2toV3(kDbFile);
}

TEST_F(QuotaDatabaseTest, HostQuota) {
  base::ScopedTempDir data_dir;
  ASSERT_TRUE(data_dir.CreateUniqueTempDir());
  const base::FilePath kDbFile = data_dir.path().AppendASCII(kDBFileName);
  HostQuota(kDbFile);
  HostQuota(base::FilePath());
}

TEST_F(QuotaDatabaseTest, GlobalQuota) {
  base::ScopedTempDir data_dir;
  ASSERT_TRUE(data_dir.CreateUniqueTempDir());
  const base::FilePath kDbFile = data_dir.path().AppendASCII(kDBFileName);
  GlobalQuota(kDbFile);
  GlobalQuota(base::FilePath());
}

TEST_F(QuotaDatabaseTest, OriginLastAccessTimeLRU) {
  base::ScopedTempDir data_dir;
  ASSERT_TRUE(data_dir.CreateUniqueTempDir());
  const base::FilePath kDbFile = data_dir.path().AppendASCII(kDBFileName);
  OriginLastAccessTimeLRU(kDbFile);
  OriginLastAccessTimeLRU(base::FilePath());
}

TEST_F(QuotaDatabaseTest, OriginLastModifiedSince) {
  base::ScopedTempDir data_dir;
  ASSERT_TRUE(data_dir.CreateUniqueTempDir());
  const base::FilePath kDbFile = data_dir.path().AppendASCII(kDBFileName);
  OriginLastModifiedSince(kDbFile);
  OriginLastModifiedSince(base::FilePath());
}

TEST_F(QuotaDatabaseTest, BootstrapFlag) {
  base::ScopedTempDir data_dir;
  ASSERT_TRUE(data_dir.CreateUniqueTempDir());

  const base::FilePath kDbFile = data_dir.path().AppendASCII(kDBFileName);
  QuotaDatabase db(kDbFile);

  EXPECT_FALSE(db.IsOriginDatabaseBootstrapped());
  EXPECT_TRUE(db.SetOriginDatabaseBootstrapped(true));
  EXPECT_TRUE(db.IsOriginDatabaseBootstrapped());
  EXPECT_TRUE(db.SetOriginDatabaseBootstrapped(false));
  EXPECT_FALSE(db.IsOriginDatabaseBootstrapped());
}

TEST_F(QuotaDatabaseTest, RegisterInitialOriginInfo) {
  base::ScopedTempDir data_dir;
  ASSERT_TRUE(data_dir.CreateUniqueTempDir());
  const base::FilePath kDbFile = data_dir.path().AppendASCII(kDBFileName);
  RegisterInitialOriginInfo(kDbFile);
  RegisterInitialOriginInfo(base::FilePath());
}

TEST_F(QuotaDatabaseTest, DumpQuotaTable) {
  base::ScopedTempDir data_dir;
  ASSERT_TRUE(data_dir.CreateUniqueTempDir());
  const base::FilePath kDbFile = data_dir.path().AppendASCII(kDBFileName);
  DumpQuotaTable(kDbFile);
  DumpQuotaTable(base::FilePath());
}

TEST_F(QuotaDatabaseTest, DumpOriginInfoTable) {
  base::ScopedTempDir data_dir;
  ASSERT_TRUE(data_dir.CreateUniqueTempDir());
  const base::FilePath kDbFile = data_dir.path().AppendASCII(kDBFileName);
  DumpOriginInfoTable(kDbFile);
  DumpOriginInfoTable(base::FilePath());
}
}  // namespace content
