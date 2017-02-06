// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/precache/core/precache_url_table.h"

#include <map>
#include <memory>

#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "sql/connection.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace precache {

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
    precache_url_table_->Init(db_.get());
  }

  std::unique_ptr<PrecacheURLTable> precache_url_table_;
  std::unique_ptr<sql::Connection> db_;
};

TEST_F(PrecacheURLTableTest, AddURLWithNoExistingRow) {
  const base::Time kTime = base::Time::FromInternalValue(100);
  precache_url_table_->AddURL(GURL("http://url.com"), kTime);

  std::map<GURL, base::Time> expected_map;
  expected_map[GURL("http://url.com")] = kTime;

  std::map<GURL, base::Time> actual_map;
  precache_url_table_->GetAllDataForTesting(&actual_map);
  EXPECT_EQ(expected_map, actual_map);
}

TEST_F(PrecacheURLTableTest, AddURLWithExistingRow) {
  const base::Time kOldTime = base::Time::FromInternalValue(50);
  const base::Time kNewTime = base::Time::FromInternalValue(100);
  precache_url_table_->AddURL(GURL("http://url.com"), kOldTime);
  precache_url_table_->AddURL(GURL("http://url.com"), kNewTime);

  std::map<GURL, base::Time> expected_map;
  expected_map[GURL("http://url.com")] = kNewTime;

  std::map<GURL, base::Time> actual_map;
  precache_url_table_->GetAllDataForTesting(&actual_map);
  EXPECT_EQ(expected_map, actual_map);
}

TEST_F(PrecacheURLTableTest, DeleteURL) {
  const base::Time kStaysTime = base::Time::FromInternalValue(50);
  const base::Time kDeletedTime = base::Time::FromInternalValue(100);

  precache_url_table_->AddURL(GURL("http://stays.com"), kStaysTime);
  precache_url_table_->AddURL(GURL("http://deleted.com"), kDeletedTime);

  precache_url_table_->DeleteURL(GURL("http://deleted.com"));

  std::map<GURL, base::Time> expected_map;
  expected_map[GURL("http://stays.com")] = kStaysTime;

  std::map<GURL, base::Time> actual_map;
  precache_url_table_->GetAllDataForTesting(&actual_map);
  EXPECT_EQ(expected_map, actual_map);
}

TEST_F(PrecacheURLTableTest, HasURL) {
  EXPECT_FALSE(precache_url_table_->HasURL(GURL("http://url.com")));

  precache_url_table_->AddURL(GURL("http://url.com"),
                              base::Time::FromInternalValue(100));

  EXPECT_TRUE(precache_url_table_->HasURL(GURL("http://url.com")));

  precache_url_table_->DeleteURL(GURL("http://url.com"));

  EXPECT_FALSE(precache_url_table_->HasURL(GURL("http://url.com")));
}

TEST_F(PrecacheURLTableTest, DeleteAllPrecachedBefore) {
  const base::Time kOldTime = base::Time::FromInternalValue(10);
  const base::Time kBeforeTime = base::Time::FromInternalValue(20);
  const base::Time kEndTime = base::Time::FromInternalValue(30);
  const base::Time kAfterTime = base::Time::FromInternalValue(40);

  precache_url_table_->AddURL(GURL("http://old.com"), kOldTime);
  precache_url_table_->AddURL(GURL("http://before.com"), kBeforeTime);
  precache_url_table_->AddURL(GURL("http://end.com"), kEndTime);
  precache_url_table_->AddURL(GURL("http://after.com"), kAfterTime);

  precache_url_table_->DeleteAllPrecachedBefore(kEndTime);

  std::map<GURL, base::Time> expected_map;
  expected_map[GURL("http://end.com")] = kEndTime;
  expected_map[GURL("http://after.com")] = kAfterTime;

  std::map<GURL, base::Time> actual_map;
  precache_url_table_->GetAllDataForTesting(&actual_map);
  EXPECT_EQ(expected_map, actual_map);
}

}  // namespace

}  // namespace precache
