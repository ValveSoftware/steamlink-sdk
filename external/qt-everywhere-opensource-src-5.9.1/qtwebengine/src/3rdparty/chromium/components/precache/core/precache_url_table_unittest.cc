// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/precache/core/precache_url_table.h"

#include <map>
#include <memory>
#include <set>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace precache {

void PrintTo(const PrecacheURLInfo& url_info, ::std::ostream* os) {
  *os << "{" << url_info.was_precached << ", " << url_info.is_precached << ", "
      << url_info.was_used << ", " << url_info.is_download_reported << "}";
}

namespace {

class PrecacheURLTableTest : public testing::Test {
 public:
  PrecacheURLTableTest() {}
  ~PrecacheURLTableTest() override {}

 protected:
  void SetUp() override {
    precache_url_table_.reset(new PrecacheURLTable());
    db_.reset(new sql::Connection());
    ASSERT_TRUE(db_->OpenInMemory());
    ASSERT_TRUE(precache_url_table_->Init(db_.get()));
  }

  std::unique_ptr<PrecacheURLTable> precache_url_table_;
  std::unique_ptr<sql::Connection> db_;
};

TEST_F(PrecacheURLTableTest, AddURLWithNoExistingRow) {
  const base::Time kTime = base::Time::FromInternalValue(100);
  precache_url_table_->AddURL(GURL("http://url.com"), 1, true, kTime, false);

  std::map<GURL, base::Time> expected_map;
  expected_map[GURL("http://url.com")] = kTime;

  std::map<GURL, base::Time> actual_map;
  precache_url_table_->GetAllDataForTesting(&actual_map);
  EXPECT_EQ(expected_map, actual_map);
}

TEST_F(PrecacheURLTableTest, AddURLWithExistingRow) {
  const base::Time kOldTime = base::Time::FromInternalValue(50);
  const base::Time kNewTime = base::Time::FromInternalValue(100);
  precache_url_table_->AddURL(GURL("http://url.com"), 1, true, kOldTime, false);
  precache_url_table_->AddURL(GURL("http://url.com"), 1, true, kNewTime, false);

  std::map<GURL, base::Time> expected_map;
  expected_map[GURL("http://url.com")] = kNewTime;

  std::map<GURL, base::Time> actual_map;
  precache_url_table_->GetAllDataForTesting(&actual_map);
  EXPECT_EQ(expected_map, actual_map);
}

TEST_F(PrecacheURLTableTest, SetURLAsNotPrecached) {
  const base::Time kStaysTime = base::Time::FromInternalValue(50);
  const base::Time kDeletedTime = base::Time::FromInternalValue(100);

  precache_url_table_->AddURL(GURL("http://stays.com"), 1, true, kStaysTime,
                              false);
  precache_url_table_->AddURL(GURL("http://deleted.com"), 1, true, kDeletedTime,
                              false);

  precache_url_table_->SetURLAsNotPrecached(GURL("http://deleted.com"));

  std::map<GURL, base::Time> expected_map;
  expected_map[GURL("http://stays.com")] = kStaysTime;

  std::map<GURL, base::Time> actual_map;
  precache_url_table_->GetAllDataForTesting(&actual_map);
  EXPECT_EQ(expected_map, actual_map);
}

TEST_F(PrecacheURLTableTest, SetDownloadReported) {
  const GURL url("http://stays.com");
  precache_url_table_->AddURL(url, 1, true, base::Time::FromInternalValue(50),
                              false);

  precache_url_table_->SetDownloadReported(1);

  EXPECT_EQ((PrecacheURLInfo{true, true, false, true}),
            precache_url_table_->GetURLInfo(url));
}

TEST_F(PrecacheURLTableTest, GetURLInfo) {
  const GURL url("http://url.com");

  EXPECT_EQ((PrecacheURLInfo{false, false, false, false}),
            precache_url_table_->GetURLInfo(url));

  precache_url_table_->AddURL(url, 1, true, base::Time::FromInternalValue(100),
                              true);

  EXPECT_EQ((PrecacheURLInfo{true, true, false, true}),
            precache_url_table_->GetURLInfo(url));

  precache_url_table_->SetPrecachedURLAsUsed(url);

  EXPECT_EQ((PrecacheURLInfo{true, false, true, true}),
            precache_url_table_->GetURLInfo(url));

  precache_url_table_->AddURL(url, 1, true, base::Time::FromInternalValue(100),
                              false);

  EXPECT_EQ((PrecacheURLInfo{true, true, false, false}),
            precache_url_table_->GetURLInfo(url));

  precache_url_table_->SetURLAsNotPrecached(url);

  EXPECT_EQ((PrecacheURLInfo{true, false, false, false}),
            precache_url_table_->GetURLInfo(url));

  precache_url_table_->SetDownloadReported(1);

  EXPECT_EQ((PrecacheURLInfo{true, false, false, true}),
            precache_url_table_->GetURLInfo(url));
}

TEST_F(PrecacheURLTableTest, DeleteAllPrecachedBefore) {
  const base::Time kOldTime = base::Time::FromInternalValue(10);
  const base::Time kBeforeTime = base::Time::FromInternalValue(20);
  const base::Time kEndTime = base::Time::FromInternalValue(30);
  const base::Time kAfterTime = base::Time::FromInternalValue(40);

  precache_url_table_->AddURL(GURL("http://old.com"), 1, true, kOldTime, false);
  precache_url_table_->AddURL(GURL("http://before.com"), 1, true, kBeforeTime,
                              false);
  precache_url_table_->AddURL(GURL("http://end.com"), 1, true, kEndTime, false);
  precache_url_table_->AddURL(GURL("http://after.com"), 1, true, kAfterTime,
                              false);

  precache_url_table_->DeleteAllPrecachedBefore(kEndTime);

  std::map<GURL, base::Time> expected_map;
  expected_map[GURL("http://end.com")] = kEndTime;
  expected_map[GURL("http://after.com")] = kAfterTime;

  std::map<GURL, base::Time> actual_map;
  precache_url_table_->GetAllDataForTesting(&actual_map);
  EXPECT_EQ(expected_map, actual_map);
}

TEST_F(PrecacheURLTableTest, TableMigration) {
  // Create the previous version of the URL table.
  precache_url_table_.reset(new PrecacheURLTable());
  db_.reset(new sql::Connection());
  ASSERT_TRUE(db_->OpenInMemory());
  ASSERT_TRUE(db_->Execute(
      "CREATE TABLE IF NOT EXISTS precache_urls (url TEXT PRIMARY KEY, time "
      "INTEGER)"));

  // Populate data for the previous version.
  const std::string old_urls[] = {"http://foo.com", "http://bar.com",
                                  "http://foobar.com"};
  for (const auto& url : old_urls) {
    sql::Statement statement(db_->GetCachedStatement(
        SQL_FROM_HERE, "INSERT INTO precache_urls (url, time) VALUES(?,100)"));
    statement.BindString(0, url);
    statement.Run();
  }

  // Verify the migration.
  ASSERT_TRUE(precache_url_table_->Init(db_.get()));
  EXPECT_TRUE(db_->DoesColumnExist("precache_urls", "was_used"));
  EXPECT_TRUE(db_->DoesColumnExist("precache_urls", "is_precached"));
  EXPECT_TRUE(db_->DoesColumnExist("precache_urls", "referrer_host_id"));

  std::set<std::string> actual_urls;
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE,
                              "select url, referrer_host_id, was_used, "
                              "is_precached from precache_urls"));
  while (statement.Step()) {
    actual_urls.insert(statement.ColumnString(0));
    EXPECT_EQ(0, statement.ColumnInt(1));
    EXPECT_EQ(0, statement.ColumnInt(2));
    EXPECT_EQ(1, statement.ColumnInt(3));
  }
  EXPECT_THAT(std::set<std::string>(begin(old_urls), end(old_urls)),
              ::testing::ContainerEq(actual_urls));
}

}  // namespace

}  // namespace precache
