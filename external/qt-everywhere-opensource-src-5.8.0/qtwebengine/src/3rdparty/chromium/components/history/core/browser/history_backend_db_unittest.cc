// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// History unit tests come in two flavors:
//
// 1. The more complicated style is that the unit test creates a full history
//    service. This spawns a background thread for the history backend, and
//    all communication is asynchronous. This is useful for testing more
//    complicated things or end-to-end behavior.
//
// 2. The simpler style is to create a history backend on this thread and
//    access it directly without a HistoryService object. This is much simpler
//    because communication is synchronous. Generally, sets should go through
//    the history backend (since there is a lot of logic) but gets can come
//    directly from the HistoryDatabase. This is because the backend generally
//    has no logic in the getter except threading stuff, which we don't want
//    to run.

#include "components/history/core/browser/history_backend.h"

#include <stdint.h>

#include <string>
#include <unordered_set>

#include "base/format_macros.h"
#include "base/guid.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/history/core/browser/download_constants.h"
#include "components/history/core/browser/download_row.h"
#include "components/history/core/browser/history_constants.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/page_usage_data.h"
#include "components/history/core/test/history_backend_db_base_test.h"
#include "components/history/core/test/test_history_database.h"

namespace history {
namespace {

// This must be outside the anonymous namespace for the friend statement in
// HistoryBackend to work.
class HistoryBackendDBTest : public HistoryBackendDBBaseTest {
 public:
  HistoryBackendDBTest() {}
  ~HistoryBackendDBTest() override {}
};

TEST_F(HistoryBackendDBTest, ClearBrowsingData_Downloads) {
  CreateBackendAndDatabase();

  // Initially there should be nothing in the downloads database.
  std::vector<DownloadRow> downloads;
  db_->QueryDownloads(&downloads);
  EXPECT_EQ(0U, downloads.size());

  // Add a download, test that it was added correctly, remove it, test that it
  // was removed.
  base::Time now = base::Time();
  uint32_t id = 1;
  EXPECT_TRUE(AddDownload(id,
                          "BC5E3854-7B1D-4DE0-B619-B0D99C8B18B4",
                          DownloadState::COMPLETE,
                          base::Time()));
  db_->QueryDownloads(&downloads);
  EXPECT_EQ(1U, downloads.size());

  EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL("current-path")),
            downloads[0].current_path);
  EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL("target-path")),
            downloads[0].target_path);
  EXPECT_EQ(1UL, downloads[0].url_chain.size());
  EXPECT_EQ(GURL("foo-url"), downloads[0].url_chain[0]);
  EXPECT_EQ(std::string("http://referrer.example.com/"),
            downloads[0].referrer_url.spec());
  EXPECT_EQ(std::string("http://tab-url.example.com/"),
            downloads[0].tab_url.spec());
  EXPECT_EQ(std::string("http://tab-referrer-url.example.com/"),
            downloads[0].tab_referrer_url.spec());
  EXPECT_EQ(now, downloads[0].start_time);
  EXPECT_EQ(now, downloads[0].end_time);
  EXPECT_EQ(0, downloads[0].received_bytes);
  EXPECT_EQ(512, downloads[0].total_bytes);
  EXPECT_EQ(DownloadState::COMPLETE, downloads[0].state);
  EXPECT_EQ(DownloadDangerType::NOT_DANGEROUS, downloads[0].danger_type);
  EXPECT_EQ(kTestDownloadInterruptReasonNone, downloads[0].interrupt_reason);
  EXPECT_FALSE(downloads[0].opened);
  EXPECT_EQ("by_ext_id", downloads[0].by_ext_id);
  EXPECT_EQ("by_ext_name", downloads[0].by_ext_name);
  EXPECT_EQ("application/vnd.oasis.opendocument.text", downloads[0].mime_type);
  EXPECT_EQ("application/octet-stream", downloads[0].original_mime_type);

  db_->QueryDownloads(&downloads);
  EXPECT_EQ(1U, downloads.size());
  db_->RemoveDownload(id);
  db_->QueryDownloads(&downloads);
  EXPECT_EQ(0U, downloads.size());
}

TEST_F(HistoryBackendDBTest, MigrateDownloadsState) {
  // Create the db we want.
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(22));
  {
    // Open the db for manual manipulation.
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));

    // Manually insert corrupted rows; there's infrastructure in place now to
    // make this impossible, at least according to the test above.
    for (int state = 0; state < 5; ++state) {
      sql::Statement s(db.GetUniqueStatement(
            "INSERT INTO downloads (id, full_path, url, start_time, "
            "received_bytes, total_bytes, state, end_time, opened) VALUES "
            "(?, ?, ?, ?, ?, ?, ?, ?, ?)"));
      s.BindInt64(0, 1 + state);
      s.BindString(1, "path");
      s.BindString(2, "url");
      s.BindInt64(3, base::Time::Now().ToTimeT());
      s.BindInt64(4, 100);
      s.BindInt64(5, 100);
      s.BindInt(6, state);
      s.BindInt64(7, base::Time::Now().ToTimeT());
      s.BindInt(8, state % 2);
      ASSERT_TRUE(s.Run());
    }
  }

  // Re-open the db using the HistoryDatabase, which should migrate from version
  // 22 to the current version, fixing just the row whose state was 3.
  // Then close the db so that we can re-open it directly.
  CreateBackendAndDatabase();
  DeleteBackend();
  {
    // Re-open the db for manual manipulation.
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    {
      // The version should have been updated.
      int cur_version = HistoryDatabase::GetCurrentVersion();
      ASSERT_LT(22, cur_version);
      sql::Statement s(db.GetUniqueStatement(
          "SELECT value FROM meta WHERE key = 'version'"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(cur_version, s.ColumnInt(0));
    }
    {
      sql::Statement statement(db.GetUniqueStatement(
          "SELECT id, state, opened "
          "FROM downloads "
          "ORDER BY id"));
      int counter = 0;
      while (statement.Step()) {
        EXPECT_EQ(1 + counter, statement.ColumnInt64(0));
        // The only thing that migration should have changed was state from 3 to
        // 4.
        EXPECT_EQ(((counter == 3) ? 4 : counter), statement.ColumnInt(1));
        EXPECT_EQ(counter % 2, statement.ColumnInt(2));
        ++counter;
      }
      EXPECT_EQ(5, counter);
    }
  }
}

TEST_F(HistoryBackendDBTest, MigrateDownloadsReasonPathsAndDangerType) {
  base::Time now(base::Time::Now());

  // Create the db we want.  The schema didn't change from 22->23, so just
  // re-use the v22 file.
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(22));
  {
    // Re-open the db for manual manipulation.
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));

    // Manually insert some rows.
    sql::Statement s(db.GetUniqueStatement(
        "INSERT INTO downloads (id, full_path, url, start_time, "
        "received_bytes, total_bytes, state, end_time, opened) VALUES "
        "(?, ?, ?, ?, ?, ?, ?, ?, ?)"));

    int64_t id = 0;
    // Null path.
    s.BindInt64(0, ++id);
    s.BindString(1, std::string());
    s.BindString(2, "http://whatever.com/index.html");
    s.BindInt64(3, now.ToTimeT());
    s.BindInt64(4, 100);
    s.BindInt64(5, 100);
    s.BindInt(6, 1);
    s.BindInt64(7, now.ToTimeT());
    s.BindInt(8, 1);
    ASSERT_TRUE(s.Run());
    s.Reset(true);

    // Non-null path.
    s.BindInt64(0, ++id);
    s.BindString(1, "/path/to/some/file");
    s.BindString(2, "http://whatever.com/index1.html");
    s.BindInt64(3, now.ToTimeT());
    s.BindInt64(4, 100);
    s.BindInt64(5, 100);
    s.BindInt(6, 1);
    s.BindInt64(7, now.ToTimeT());
    s.BindInt(8, 1);
    ASSERT_TRUE(s.Run());
  }

  // Re-open the db using the HistoryDatabase, which should migrate from version
  // 23 to 24, creating the new tables and creating the new path, reason,
  // and danger columns.
  CreateBackendAndDatabase();
  DeleteBackend();
  {
    // Re-open the db for manual manipulation.
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    {
      // The version should have been updated.
      int cur_version = HistoryDatabase::GetCurrentVersion();
      ASSERT_LT(23, cur_version);
      sql::Statement s(db.GetUniqueStatement(
          "SELECT value FROM meta WHERE key = 'version'"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(cur_version, s.ColumnInt(0));
    }
    {
      base::Time nowish(base::Time::FromTimeT(now.ToTimeT()));

      // Confirm downloads table is valid.
      sql::Statement statement(db.GetUniqueStatement(
          "SELECT id, interrupt_reason, current_path, target_path, "
          "       danger_type, start_time, end_time "
          "FROM downloads ORDER BY id"));
      EXPECT_TRUE(statement.Step());
      EXPECT_EQ(1, statement.ColumnInt64(0));
      EXPECT_EQ(DownloadInterruptReasonToInt(kTestDownloadInterruptReasonNone),
                statement.ColumnInt(1));
      EXPECT_EQ("", statement.ColumnString(2));
      EXPECT_EQ("", statement.ColumnString(3));
      // Implicit dependence on value of kDangerTypeNotDangerous from
      // download_database.cc.
      EXPECT_EQ(0, statement.ColumnInt(4));
      EXPECT_EQ(nowish.ToInternalValue(), statement.ColumnInt64(5));
      EXPECT_EQ(nowish.ToInternalValue(), statement.ColumnInt64(6));

      EXPECT_TRUE(statement.Step());
      EXPECT_EQ(2, statement.ColumnInt64(0));
      EXPECT_EQ(DownloadInterruptReasonToInt(kTestDownloadInterruptReasonNone),
                statement.ColumnInt(1));
      EXPECT_EQ("/path/to/some/file", statement.ColumnString(2));
      EXPECT_EQ("/path/to/some/file", statement.ColumnString(3));
      EXPECT_EQ(0, statement.ColumnInt(4));
      EXPECT_EQ(nowish.ToInternalValue(), statement.ColumnInt64(5));
      EXPECT_EQ(nowish.ToInternalValue(), statement.ColumnInt64(6));

      EXPECT_FALSE(statement.Step());
    }
    {
      // Confirm downloads_url_chains table is valid.
      sql::Statement statement(db.GetUniqueStatement(
          "SELECT id, chain_index, url FROM downloads_url_chains "
          " ORDER BY id, chain_index"));
      EXPECT_TRUE(statement.Step());
      EXPECT_EQ(1, statement.ColumnInt64(0));
      EXPECT_EQ(0, statement.ColumnInt(1));
      EXPECT_EQ("http://whatever.com/index.html", statement.ColumnString(2));

      EXPECT_TRUE(statement.Step());
      EXPECT_EQ(2, statement.ColumnInt64(0));
      EXPECT_EQ(0, statement.ColumnInt(1));
      EXPECT_EQ("http://whatever.com/index1.html", statement.ColumnString(2));

      EXPECT_FALSE(statement.Step());
    }
  }
}

TEST_F(HistoryBackendDBTest, MigrateReferrer) {
  base::Time now(base::Time::Now());
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(22));
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    sql::Statement s(db.GetUniqueStatement(
        "INSERT INTO downloads (id, full_path, url, start_time, "
        "received_bytes, total_bytes, state, end_time, opened) VALUES "
        "(?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    int64_t db_handle = 0;
    s.BindInt64(0, ++db_handle);
    s.BindString(1, "full_path");
    s.BindString(2, "http://whatever.com/index.html");
    s.BindInt64(3, now.ToTimeT());
    s.BindInt64(4, 100);
    s.BindInt64(5, 100);
    s.BindInt(6, 1);
    s.BindInt64(7, now.ToTimeT());
    s.BindInt(8, 1);
    ASSERT_TRUE(s.Run());
  }
  // Re-open the db using the HistoryDatabase, which should migrate to version
  // 26, creating the referrer column.
  CreateBackendAndDatabase();
  DeleteBackend();
  {
    // Re-open the db for manual manipulation.
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    // The version should have been updated.
    int cur_version = HistoryDatabase::GetCurrentVersion();
    ASSERT_LE(26, cur_version);
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT value FROM meta WHERE key = 'version'"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(cur_version, s.ColumnInt(0));
    }
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT referrer from downloads"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(std::string(), s.ColumnString(0));
    }
  }
}

TEST_F(HistoryBackendDBTest, MigrateDownloadedByExtension) {
  base::Time now(base::Time::Now());
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(26));
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    {
      sql::Statement s(db.GetUniqueStatement(
          "INSERT INTO downloads (id, current_path, target_path, start_time, "
          "received_bytes, total_bytes, state, danger_type, interrupt_reason, "
          "end_time, opened, referrer) VALUES "
          "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
      s.BindInt64(0, 1);
      s.BindString(1, "current_path");
      s.BindString(2, "target_path");
      s.BindInt64(3, now.ToTimeT());
      s.BindInt64(4, 100);
      s.BindInt64(5, 100);
      s.BindInt(6, 1);
      s.BindInt(7, 0);
      s.BindInt(8, 0);
      s.BindInt64(9, now.ToTimeT());
      s.BindInt(10, 1);
      s.BindString(11, "referrer");
      ASSERT_TRUE(s.Run());
    }
    {
      sql::Statement s(db.GetUniqueStatement(
          "INSERT INTO downloads_url_chains (id, chain_index, url) VALUES "
          "(?, ?, ?)"));
      s.BindInt64(0, 4);
      s.BindInt64(1, 0);
      s.BindString(2, "url");
      ASSERT_TRUE(s.Run());
    }
  }
  // Re-open the db using the HistoryDatabase, which should migrate to version
  // 27, creating the by_ext_id and by_ext_name columns.
  CreateBackendAndDatabase();
  DeleteBackend();
  {
    // Re-open the db for manual manipulation.
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    // The version should have been updated.
    int cur_version = HistoryDatabase::GetCurrentVersion();
    ASSERT_LE(27, cur_version);
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT value FROM meta WHERE key = 'version'"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(cur_version, s.ColumnInt(0));
    }
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT by_ext_id, by_ext_name from downloads"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(std::string(), s.ColumnString(0));
      EXPECT_EQ(std::string(), s.ColumnString(1));
    }
  }
}

TEST_F(HistoryBackendDBTest, MigrateDownloadValidators) {
  base::Time now(base::Time::Now());
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(27));
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    {
      sql::Statement s(db.GetUniqueStatement(
          "INSERT INTO downloads (id, current_path, target_path, start_time, "
          "received_bytes, total_bytes, state, danger_type, interrupt_reason, "
          "end_time, opened, referrer, by_ext_id, by_ext_name) VALUES "
          "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
      s.BindInt64(0, 1);
      s.BindString(1, "current_path");
      s.BindString(2, "target_path");
      s.BindInt64(3, now.ToTimeT());
      s.BindInt64(4, 100);
      s.BindInt64(5, 100);
      s.BindInt(6, 1);
      s.BindInt(7, 0);
      s.BindInt(8, 0);
      s.BindInt64(9, now.ToTimeT());
      s.BindInt(10, 1);
      s.BindString(11, "referrer");
      s.BindString(12, "by extension ID");
      s.BindString(13, "by extension name");
      ASSERT_TRUE(s.Run());
    }
    {
      sql::Statement s(db.GetUniqueStatement(
          "INSERT INTO downloads_url_chains (id, chain_index, url) VALUES "
          "(?, ?, ?)"));
      s.BindInt64(0, 4);
      s.BindInt64(1, 0);
      s.BindString(2, "url");
      ASSERT_TRUE(s.Run());
    }
  }
  // Re-open the db using the HistoryDatabase, which should migrate to the
  // current version, creating the etag and last_modified columns.
  CreateBackendAndDatabase();
  DeleteBackend();
  {
    // Re-open the db for manual manipulation.
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    // The version should have been updated.
    int cur_version = HistoryDatabase::GetCurrentVersion();
    ASSERT_LE(28, cur_version);
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT value FROM meta WHERE key = 'version'"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(cur_version, s.ColumnInt(0));
    }
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT etag, last_modified from downloads"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(std::string(), s.ColumnString(0));
      EXPECT_EQ(std::string(), s.ColumnString(1));
    }
  }
}

TEST_F(HistoryBackendDBTest, MigrateDownloadMimeType) {
  base::Time now(base::Time::Now());
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(28));
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    {
      sql::Statement s(db.GetUniqueStatement(
          "INSERT INTO downloads (id, current_path, target_path, start_time, "
          "received_bytes, total_bytes, state, danger_type, interrupt_reason, "
          "end_time, opened, referrer, by_ext_id, by_ext_name, etag, "
          "last_modified) VALUES "
          "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
      s.BindInt64(0, 1);
      s.BindString(1, "current_path");
      s.BindString(2, "target_path");
      s.BindInt64(3, now.ToTimeT());
      s.BindInt64(4, 100);
      s.BindInt64(5, 100);
      s.BindInt(6, 1);
      s.BindInt(7, 0);
      s.BindInt(8, 0);
      s.BindInt64(9, now.ToTimeT());
      s.BindInt(10, 1);
      s.BindString(11, "referrer");
      s.BindString(12, "by extension ID");
      s.BindString(13, "by extension name");
      s.BindString(14, "etag");
      s.BindInt64(15, now.ToTimeT());
      ASSERT_TRUE(s.Run());
    }
    {
      sql::Statement s(db.GetUniqueStatement(
          "INSERT INTO downloads_url_chains (id, chain_index, url) VALUES "
          "(?, ?, ?)"));
      s.BindInt64(0, 4);
      s.BindInt64(1, 0);
      s.BindString(2, "url");
      ASSERT_TRUE(s.Run());
    }
  }
  // Re-open the db using the HistoryDatabase, which should migrate to the
  // current version, creating the mime_type abd original_mime_type columns.
  CreateBackendAndDatabase();
  DeleteBackend();
  {
    // Re-open the db for manual manipulation.
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    // The version should have been updated.
    int cur_version = HistoryDatabase::GetCurrentVersion();
    ASSERT_LE(29, cur_version);
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT value FROM meta WHERE key = 'version'"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(cur_version, s.ColumnInt(0));
    }
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT mime_type, original_mime_type from downloads"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(std::string(), s.ColumnString(0));
      EXPECT_EQ(std::string(), s.ColumnString(1));
    }
  }
}

bool IsValidRFC4122Ver4GUID(const std::string& guid) {
  // base::IsValidGUID() doesn't restrict its validation to version (or subtype)
  // 4 GUIDs as described in RFC 4122. So we check if base::IsValidGUID() thinks
  // it's a valid GUID first, and then check the additional constraints.
  //
  // * Bits 4-7 of time_hi_and_version should be set to 0b0100 == 4
  //   => guid[14] == '4'
  //
  // * Bits 6-7 of clk_seq_hi_res should be set to 0b10
  //   => guid[19] in {'8','9','A','B'}
  //
  // * All other bits should be random or pseudo random.
  //   => http://dilbert.com/strip/2001-10-25
  return base::IsValidGUID(guid) && guid[14] == '4' &&
         (guid[19] == '8' || guid[19] == '9' || guid[19] == 'A' ||
          guid[19] == 'B');
}

TEST_F(HistoryBackendDBTest, MigrateHashHttpMethodAndGenerateGuids) {
  const size_t kDownloadCount = 100;
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(29));
  base::Time now(base::Time::Now());
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));

    // In testing, it appeared that constructing a query where all rows are
    // specified (i.e. looks like "INSERT INTO foo (...) VALUES (...),(...)")
    // performs much better than executing a cached query multiple times where
    // the query inserts only a single row per run (i.e. looks like "INSERT INTO
    // (...) VALUES (...)"). For 100 records, the latter took 19s on a
    // developer machine while the former inserted 100 records in ~400ms.
    std::string download_insert_query =
        "INSERT INTO downloads (id, current_path, target_path, start_time, "
        "received_bytes, total_bytes, state, danger_type, interrupt_reason, "
        "end_time, opened, referrer, by_ext_id, by_ext_name, etag, "
        "last_modified, mime_type, original_mime_type) VALUES ";
    std::string url_insert_query =
        "INSERT INTO downloads_url_chains (id, chain_index, url) VALUES ";

    for (uint32_t i = 0; i < kDownloadCount; ++i) {
      uint32_t download_id = i * 13321;
      if (i != 0)
        download_insert_query += ",";
      download_insert_query += base::StringPrintf(
          "(%" PRId64 ", 'current_path', 'target_path', %" PRId64
          ", 100, 100, 1, 0, 0, %" PRId64
          ", 1, 'referrer', 'by extension ID','by extension name', 'etag', "
          "'last modified', 'mime/type', 'original/mime-type')",
          static_cast<int64_t>(download_id),
          static_cast<int64_t>(now.ToTimeT()),
          static_cast<int64_t>(now.ToTimeT()));
      if (i != 0)
        url_insert_query += ",";
      url_insert_query += base::StringPrintf("(%" PRId64 ", 0, 'url')",
                                             static_cast<int64_t>(download_id));
    }
    ASSERT_TRUE(db.Execute(download_insert_query.c_str()));
    ASSERT_TRUE(db.Execute(url_insert_query.c_str()));
  }

  CreateBackendAndDatabase();
  DeleteBackend();

  {
    // Re-open the db for manual manipulation.
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    // The version should have been updated.
    int cur_version = HistoryDatabase::GetCurrentVersion();
    ASSERT_LE(30, cur_version);
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT value FROM meta WHERE key = 'version'"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(cur_version, s.ColumnInt(0));
    }
    {
      sql::Statement s(db.GetUniqueStatement("SELECT guid, id from downloads"));
      std::unordered_set<std::string> guids;
      while (s.Step()) {
        std::string guid = s.ColumnString(0);
        uint32_t id = static_cast<uint32_t>(s.ColumnInt64(1));
        EXPECT_TRUE(IsValidRFC4122Ver4GUID(guid));
        EXPECT_EQ(guid, base::ToUpperASCII(guid));
        // Id is used as time_low in RFC 4122 to guarantee unique GUIDs
        EXPECT_EQ(guid.substr(0, 8), base::StringPrintf("%08" PRIX32, id));
        guids.insert(guid);
      }
      EXPECT_TRUE(s.Succeeded());
      EXPECT_EQ(kDownloadCount, guids.size());
    }
  }
}

TEST_F(HistoryBackendDBTest, MigrateTabUrls) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(30));
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    {
      sql::Statement s(db.GetUniqueStatement(
          "INSERT INTO downloads ("
          "    id, guid, current_path, target_path, start_time, received_bytes,"
          "    total_bytes, state, danger_type, interrupt_reason, hash,"
          "    end_time, opened, referrer, http_method, by_ext_id, by_ext_name,"
          "    etag, last_modified, mime_type, original_mime_type)"
          "VALUES("
          "    1, '435A5C7A-F6B7-4DF2-8696-22E4FCBA3EB2', 'foo.txt', 'foo.txt',"
          "    13104873187307670, 11, 11, 1, 0, 0, X'', 13104873187521021, 0,"
          "    'http://example.com/dl/', '', '', '', '', '', 'text/plain',"
          "    'text/plain')"));
      ASSERT_TRUE(s.Run());
    }
    {
      sql::Statement s(db.GetUniqueStatement(
          "INSERT INTO downloads_url_chains (id, chain_index, url) VALUES "
          "(1, 0, 'url')"));
      ASSERT_TRUE(s.Run());
    }
  }

  // Re-open the db using the HistoryDatabase, which should migrate to the
  // current version, creating the tab_url and tab_referrer_url columns.
  CreateBackendAndDatabase();
  DeleteBackend();
  {
    // Re-open the db for manual manipulation.
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    // The version should have been updated.
    int cur_version = HistoryDatabase::GetCurrentVersion();
    ASSERT_LE(31, cur_version);
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT value FROM meta WHERE key = 'version'"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(cur_version, s.ColumnInt(0));
    }
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT tab_url, tab_referrer_url from downloads"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(std::string(), s.ColumnString(0));
      EXPECT_EQ(std::string(), s.ColumnString(1));
    }
  }
}

TEST_F(HistoryBackendDBTest, MigrateDownloadSiteInstanceUrl) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(31));
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    {
      sql::Statement s(db.GetUniqueStatement(
          "INSERT INTO downloads ("
          "    id, guid, current_path, target_path, start_time, received_bytes,"
          "    total_bytes, state, danger_type, interrupt_reason, hash,"
          "    end_time, opened, referrer, tab_url, tab_referrer_url,"
          "    http_method, by_ext_id, by_ext_name, etag, last_modified,"
          "    mime_type, original_mime_type)"
          "VALUES("
          "    1, '435A5C7A-F6B7-4DF2-8696-22E4FCBA3EB2', 'foo.txt', 'foo.txt',"
          "    13104873187307670, 11, 11, 1, 0, 0, X'', 13104873187521021, 0,"
          "    'http://example.com/dl/', '', '', '', '', '', '', '',"
          "    'text/plain', 'text/plain')"));
      ASSERT_TRUE(s.Run());
    }
    {
      sql::Statement s(db.GetUniqueStatement(
          "INSERT INTO downloads_url_chains (id, chain_index, url) VALUES "
          "(1, 0, 'url')"));
      ASSERT_TRUE(s.Run());
    }
  }

  // Re-open the db using the HistoryDatabase, which should migrate to the
  // current version, creating the site_url column.
  CreateBackendAndDatabase();
  DeleteBackend();
  {
    // Re-open the db for manual manipulation.
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    // The version should have been updated.
    int cur_version = HistoryDatabase::GetCurrentVersion();
    ASSERT_LE(31, cur_version);
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT value FROM meta WHERE key = 'version'"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(cur_version, s.ColumnInt(0));
    }
    {
      sql::Statement s(db.GetUniqueStatement("SELECT site_url from downloads"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(std::string(), s.ColumnString(0));
    }
  }
}

TEST_F(HistoryBackendDBTest, DownloadCreateAndQuery) {
  CreateBackendAndDatabase();

  ASSERT_EQ(0u, db_->CountDownloads());

  std::vector<GURL> url_chain;
  url_chain.push_back(GURL("http://example.com/a"));
  url_chain.push_back(GURL("http://example.com/b"));
  url_chain.push_back(GURL("http://example.com/c"));

  base::Time start_time(base::Time::Now());
  base::Time end_time(start_time + base::TimeDelta::FromHours(1));

  DownloadRow download_A(
      base::FilePath(FILE_PATH_LITERAL("/path/1")),
      base::FilePath(FILE_PATH_LITERAL("/path/2")), url_chain,
      GURL("http://example.com/referrer"), GURL("http://example.com"),
      GURL("http://example.com/tab-url"),
      GURL("http://example.com/tab-referrer"), "GET", "mime/type",
      "original/mime-type", start_time, end_time, "etag1", "last_modified_1",
      100, 1000, DownloadState::INTERRUPTED, DownloadDangerType::NOT_DANGEROUS,
      kTestDownloadInterruptReasonCrash, "hash-value1", 1,
      "FE672168-26EF-4275-A149-FEC25F6A75F9", false, "extension-id",
      "extension-name");
  ASSERT_TRUE(db_->CreateDownload(download_A));

  url_chain.push_back(GURL("http://example.com/d"));

  base::Time start_time2(start_time + base::TimeDelta::FromHours(10));
  base::Time end_time2(end_time + base::TimeDelta::FromHours(10));

  DownloadRow download_B(
      base::FilePath(FILE_PATH_LITERAL("/path/3")),
      base::FilePath(FILE_PATH_LITERAL("/path/4")), url_chain,
      GURL("http://example.com/referrer2"), GURL("http://2.example.com"),
      GURL("http://example.com/tab-url2"),
      GURL("http://example.com/tab-referrer2"), "POST", "mime/type2",
      "original/mime-type2", start_time2, end_time2, "etag2", "last_modified_2",
      1001, 1001, DownloadState::COMPLETE, DownloadDangerType::DANGEROUS_FILE,
      kTestDownloadInterruptReasonNone, std::string(), 2,
      "b70f3869-7d75-4878-acb4-4caf7026d12b", false, "extension-id",
      "extension-name");
  ASSERT_TRUE(db_->CreateDownload(download_B));

  EXPECT_EQ(2u, db_->CountDownloads());

  std::vector<DownloadRow> results;
  db_->QueryDownloads(&results);

  ASSERT_EQ(2u, results.size());

  const DownloadRow& retrieved_download_A =
      results[0].id == 1 ? results[0] : results[1];
  const DownloadRow& retrieved_download_B =
      results[0].id == 1 ? results[1] : results[0];

  EXPECT_EQ(download_A, retrieved_download_A);
  EXPECT_EQ(download_B, retrieved_download_B);
}

TEST_F(HistoryBackendDBTest, DownloadCreateAndUpdate_VolatileFields) {
  CreateBackendAndDatabase();

  std::vector<GURL> url_chain;
  url_chain.push_back(GURL("http://example.com/a"));
  url_chain.push_back(GURL("http://example.com/b"));
  url_chain.push_back(GURL("http://example.com/c"));

  base::Time start_time(base::Time::Now());
  base::Time end_time(start_time + base::TimeDelta::FromHours(1));

  DownloadRow download(
      base::FilePath(FILE_PATH_LITERAL("/path/1")),
      base::FilePath(FILE_PATH_LITERAL("/path/2")), url_chain,
      GURL("http://example.com/referrer"), GURL("http://example.com"),
      GURL("http://example.com/tab-url"),
      GURL("http://example.com/tab-referrer"), "GET", "mime/type",
      "original/mime-type", start_time, end_time, "etag1", "last_modified_1",
      100, 1000, DownloadState::INTERRUPTED, DownloadDangerType::NOT_DANGEROUS,
      3, "some-hash-value", 1, "FE672168-26EF-4275-A149-FEC25F6A75F9", false,
      "extension-id", "extension-name");
  db_->CreateDownload(download);

  download.current_path =
      base::FilePath(FILE_PATH_LITERAL("/new/current_path"));
  download.target_path = base::FilePath(FILE_PATH_LITERAL("/new/target_path"));
  download.mime_type = "new/mime/type";
  download.original_mime_type = "new/original/mime/type";
  download.received_bytes += 1000;
  download.state = DownloadState::CANCELLED;
  download.danger_type = DownloadDangerType::USER_VALIDATED;
  download.interrupt_reason = 4;
  download.end_time += base::TimeDelta::FromHours(1);
  download.total_bytes += 1;
  download.hash = "some-other-hash";
  download.opened = !download.opened;
  download.by_ext_id = "by-new-extension-id";
  download.by_ext_name = "by-new-extension-name";
  download.etag = "new-etag";
  download.last_modified = "new-last-modified";

  ASSERT_TRUE(db_->UpdateDownload(download));

  std::vector<DownloadRow> results;
  db_->QueryDownloads(&results);
  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(download, results[0]);
}

TEST_F(HistoryBackendDBTest, ConfirmDownloadRowCreateAndDelete) {
  // Create the DB.
  CreateBackendAndDatabase();

  base::Time now(base::Time::Now());

  // Add some downloads.
  uint32_t id1 = 1, id2 = 2, id3 = 3;
  AddDownload(id1,
              "05AF6C8E-E4E0-45D7-B5CE-BC99F7019918",
              DownloadState::COMPLETE,
              now);
  AddDownload(id2,
              "05AF6C8E-E4E0-45D7-B5CE-BC99F7019919",
              DownloadState::COMPLETE,
              now + base::TimeDelta::FromDays(2));
  AddDownload(id3,
              "05AF6C8E-E4E0-45D7-B5CE-BC99F701991A",
              DownloadState::COMPLETE,
              now - base::TimeDelta::FromDays(2));

  // Confirm that resulted in the correct number of rows in the DB.
  DeleteBackend();
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    sql::Statement statement(db.GetUniqueStatement(
        "Select Count(*) from downloads"));
    EXPECT_TRUE(statement.Step());
    EXPECT_EQ(3, statement.ColumnInt(0));

    sql::Statement statement1(db.GetUniqueStatement(
        "Select Count(*) from downloads_url_chains"));
    EXPECT_TRUE(statement1.Step());
    EXPECT_EQ(3, statement1.ColumnInt(0));
  }

  // Delete some rows and make sure the results are still correct.
  CreateBackendAndDatabase();
  db_->RemoveDownload(id2);
  db_->RemoveDownload(id3);
  DeleteBackend();
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    sql::Statement statement(db.GetUniqueStatement(
        "Select Count(*) from downloads"));
    EXPECT_TRUE(statement.Step());
    EXPECT_EQ(1, statement.ColumnInt(0));

    sql::Statement statement1(db.GetUniqueStatement(
        "Select Count(*) from downloads_url_chains"));
    EXPECT_TRUE(statement1.Step());
    EXPECT_EQ(1, statement1.ColumnInt(0));
  }
}

TEST_F(HistoryBackendDBTest, DownloadNukeRecordsMissingURLs) {
  CreateBackendAndDatabase();
  base::Time now(base::Time::Now());
  std::vector<GURL> url_chain;
  DownloadRow download(
      base::FilePath(FILE_PATH_LITERAL("foo-path")),
      base::FilePath(FILE_PATH_LITERAL("foo-path")), url_chain,
      GURL(std::string()), GURL(std::string()), GURL(std::string()),
      GURL(std::string()), std::string(), "application/octet-stream",
      "application/octet-stream", now, now, std::string(), std::string(), 0,
      512, DownloadState::COMPLETE, DownloadDangerType::NOT_DANGEROUS,
      kTestDownloadInterruptReasonNone, std::string(), 1,
      "05AF6C8E-E4E0-45D7-B5CE-BC99F7019918", 0, "by_ext_id", "by_ext_name");

  // Creating records without any urls should fail.
  EXPECT_FALSE(db_->CreateDownload(download));

  download.url_chain.push_back(GURL("foo-url"));
  EXPECT_TRUE(db_->CreateDownload(download));

  // Pretend that the URLs were dropped.
  DeleteBackend();
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    sql::Statement statement(db.GetUniqueStatement(
        "DELETE FROM downloads_url_chains WHERE id=1"));
    ASSERT_TRUE(statement.Run());
  }
  CreateBackendAndDatabase();
  std::vector<DownloadRow> downloads;
  db_->QueryDownloads(&downloads);
  EXPECT_EQ(0U, downloads.size());

  // QueryDownloads should have nuked the corrupt record.
  DeleteBackend();
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    {
      sql::Statement statement(db.GetUniqueStatement(
            "SELECT count(*) from downloads"));
      ASSERT_TRUE(statement.Step());
      EXPECT_EQ(0, statement.ColumnInt(0));
    }
  }
}

TEST_F(HistoryBackendDBTest, ConfirmDownloadInProgressCleanup) {
  // Create the DB.
  CreateBackendAndDatabase();

  base::Time now(base::Time::Now());

  // Put an IN_PROGRESS download in the DB.
  AddDownload(1,
              "05AF6C8E-E4E0-45D7-B5CE-BC99F7019918",
              DownloadState::IN_PROGRESS,
              now);

  // Confirm that they made it into the DB unchanged.
  DeleteBackend();
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    sql::Statement statement(db.GetUniqueStatement(
        "Select Count(*) from downloads"));
    EXPECT_TRUE(statement.Step());
    EXPECT_EQ(1, statement.ColumnInt(0));

    sql::Statement statement1(db.GetUniqueStatement(
        "Select state, interrupt_reason from downloads"));
    EXPECT_TRUE(statement1.Step());
    EXPECT_EQ(DownloadStateToInt(DownloadState::IN_PROGRESS),
              statement1.ColumnInt(0));
    EXPECT_EQ(DownloadInterruptReasonToInt(kTestDownloadInterruptReasonNone),
              statement1.ColumnInt(1));
    EXPECT_FALSE(statement1.Step());
  }

  // Read in the DB through query downloads, then test that the
  // right transformation was returned.
  CreateBackendAndDatabase();
  std::vector<DownloadRow> results;
  db_->QueryDownloads(&results);
  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(DownloadState::INTERRUPTED, results[0].state);
  EXPECT_EQ(kTestDownloadInterruptReasonCrash, results[0].interrupt_reason);

  // Allow the update to propagate, shut down the DB, and confirm that
  // the query updated the on disk database as well.
  base::RunLoop().RunUntilIdle();
  DeleteBackend();
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    sql::Statement statement(db.GetUniqueStatement(
        "Select Count(*) from downloads"));
    EXPECT_TRUE(statement.Step());
    EXPECT_EQ(1, statement.ColumnInt(0));

    sql::Statement statement1(db.GetUniqueStatement(
        "Select state, interrupt_reason from downloads"));
    EXPECT_TRUE(statement1.Step());
    EXPECT_EQ(DownloadStateToInt(DownloadState::INTERRUPTED),
              statement1.ColumnInt(0));
    EXPECT_EQ(DownloadInterruptReasonToInt(kTestDownloadInterruptReasonCrash),
              statement1.ColumnInt(1));
    EXPECT_FALSE(statement1.Step());
  }
}

TEST_F(HistoryBackendDBTest, MigratePresentations) {
  // Create the db we want. Use 22 since segments didn't change in that time
  // frame.
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(22));

  const SegmentID segment_id = 2;
  const URLID url_id = 3;
  const GURL url("http://www.foo.com");
  const std::string url_name(VisitSegmentDatabase::ComputeSegmentName(url));
  const base::string16 title(base::ASCIIToUTF16("Title1"));
  const base::Time segment_time(base::Time::Now());

  {
    // Re-open the db for manual manipulation.
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));

    // Add an entry to urls.
    {
      sql::Statement s(db.GetUniqueStatement(
                           "INSERT INTO urls "
                           "(id, url, title, last_visit_time) VALUES "
                           "(?, ?, ?, ?)"));
      s.BindInt64(0, url_id);
      s.BindString(1, url.spec());
      s.BindString16(2, title);
      s.BindInt64(3, segment_time.ToInternalValue());
      ASSERT_TRUE(s.Run());
    }

    // Add an entry to segments.
    {
      sql::Statement s(db.GetUniqueStatement(
                           "INSERT INTO segments "
                           "(id, name, url_id, pres_index) VALUES "
                           "(?, ?, ?, ?)"));
      s.BindInt64(0, segment_id);
      s.BindString(1, url_name);
      s.BindInt64(2, url_id);
      s.BindInt(3, 4);  // pres_index
      ASSERT_TRUE(s.Run());
    }

    // And one to segment_usage.
    {
      sql::Statement s(db.GetUniqueStatement(
                           "INSERT INTO segment_usage "
                           "(id, segment_id, time_slot, visit_count) VALUES "
                           "(?, ?, ?, ?)"));
      s.BindInt64(0, 4);  // id.
      s.BindInt64(1, segment_id);
      s.BindInt64(2, segment_time.ToInternalValue());
      s.BindInt(3, 5);  // visit count.
      ASSERT_TRUE(s.Run());
    }
  }

  // Re-open the db, triggering migration.
  CreateBackendAndDatabase();

  std::vector<std::unique_ptr<PageUsageData>> results = db_->QuerySegmentUsage(
      segment_time, 10, base::Callback<bool(const GURL&)>());
  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(url, results[0]->GetURL());
  EXPECT_EQ(segment_id, results[0]->GetID());
  EXPECT_EQ(title, results[0]->GetTitle());
}

TEST_F(HistoryBackendDBTest, CheckLastCompatibleVersion) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(28));
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    {
      // Manually set last compatible version to one higher
      // than current version.
      sql::MetaTable meta;
      meta.Init(&db, 1, 1);
      meta.SetCompatibleVersionNumber(HistoryDatabase::GetCurrentVersion() + 1);
    }
  }
  // Try to create and init backend for non compatible db.
  // Allow failure in backend creation.
  CreateBackendAndDatabaseAllowFail();
  DeleteBackend();

  // Check that error delegate was called with correct init error status.
  EXPECT_EQ(sql::INIT_TOO_NEW, last_profile_error_);
  {
    // Re-open the db to check that it was not migrated.
    // Non compatible DB must be ignored.
    // Check that DB version in file remains the same.
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    {
      sql::MetaTable meta;
      meta.Init(&db, 1, 1);
      // Current browser version must be already higher than 28.
      ASSERT_LT(28, HistoryDatabase::GetCurrentVersion());
      // Expect that version in DB remains the same.
      EXPECT_EQ(28, meta.GetVersionNumber());
    }
  }
}

bool FilterURL(const GURL& url) {
  return url.SchemeIsHTTPOrHTTPS();
}

TEST_F(HistoryBackendDBTest, QuerySegmentUsage) {
  CreateBackendAndDatabase();

  const GURL url1("file://bar");
  const GURL url2("http://www.foo.com");
  const int visit_count1 = 10;
  const int visit_count2 = 5;
  const base::Time time(base::Time::Now());

  URLID url_id1 = db_->AddURL(URLRow(url1));
  ASSERT_NE(0, url_id1);
  URLID url_id2 = db_->AddURL(URLRow(url2));
  ASSERT_NE(0, url_id2);

  SegmentID segment_id1 = db_->CreateSegment(
      url_id1, VisitSegmentDatabase::ComputeSegmentName(url1));
  ASSERT_NE(0, segment_id1);
  SegmentID segment_id2 = db_->CreateSegment(
      url_id2, VisitSegmentDatabase::ComputeSegmentName(url2));
  ASSERT_NE(0, segment_id2);

  ASSERT_TRUE(db_->IncreaseSegmentVisitCount(segment_id1, time, visit_count1));
  ASSERT_TRUE(db_->IncreaseSegmentVisitCount(segment_id2, time, visit_count2));

  // Without a filter, the "file://" URL should win.
  std::vector<std::unique_ptr<PageUsageData>> results =
      db_->QuerySegmentUsage(time, 1, base::Callback<bool(const GURL&)>());
  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(url1, results[0]->GetURL());
  EXPECT_EQ(segment_id1, results[0]->GetID());

  // With the filter, the "file://" URL should be filtered out, so the "http://"
  // URL should win instead.
  std::vector<std::unique_ptr<PageUsageData>> results2 =
      db_->QuerySegmentUsage(time, 1, base::Bind(&FilterURL));
  ASSERT_EQ(1u, results2.size());
  EXPECT_EQ(url2, results2[0]->GetURL());
  EXPECT_EQ(segment_id2, results2[0]->GetID());
}

}  // namespace
}  // namespace history
