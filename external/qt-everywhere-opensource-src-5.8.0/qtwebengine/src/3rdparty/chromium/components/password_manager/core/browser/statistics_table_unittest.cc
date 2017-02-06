// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/statistics_table.h"

#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "sql/connection.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

const char kTestDomain[] = "http://google.com";
const char kTestDomain2[] = "http://example.com";
const char kUsername1[] = "user1";
const char kUsername2[] = "user2";

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pointee;
using ::testing::UnorderedElementsAre;

class StatisticsTableTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ReloadDatabase();

    test_data_.origin_domain = GURL(kTestDomain);
    test_data_.username_value = base::ASCIIToUTF16(kUsername1);
    test_data_.dismissal_count = 10;
    test_data_.update_time = base::Time::FromTimeT(1);
  }

  void ReloadDatabase() {
    base::FilePath file = temp_dir_.path().AppendASCII("TestDatabase");
    db_.reset(new StatisticsTable);
    connection_.reset(new sql::Connection);
    connection_->set_exclusive_locking();
    ASSERT_TRUE(connection_->Open(file));
    db_->Init(connection_.get());
    ASSERT_TRUE(db_->CreateTableIfNecessary());
  }

  InteractionsStats& test_data() { return test_data_; }
  StatisticsTable* db() { return db_.get(); }

 private:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<sql::Connection> connection_;
  std::unique_ptr<StatisticsTable> db_;
  InteractionsStats test_data_;
};

TEST_F(StatisticsTableTest, Sanity) {
  EXPECT_TRUE(db()->AddRow(test_data()));
  EXPECT_THAT(db()->GetRows(test_data().origin_domain),
              ElementsAre(Pointee(test_data())));
  EXPECT_TRUE(db()->RemoveRow(test_data().origin_domain));
  EXPECT_THAT(db()->GetRows(test_data().origin_domain), IsEmpty());
}

TEST_F(StatisticsTableTest, Reload) {
  EXPECT_TRUE(db()->AddRow(test_data()));

  ReloadDatabase();

  EXPECT_THAT(db()->GetRows(test_data().origin_domain),
              ElementsAre(Pointee(test_data())));
}

TEST_F(StatisticsTableTest, DoubleOperation) {
  EXPECT_TRUE(db()->AddRow(test_data()));
  test_data().dismissal_count++;
  EXPECT_TRUE(db()->AddRow(test_data()));

  EXPECT_THAT(db()->GetRows(test_data().origin_domain),
              ElementsAre(Pointee(test_data())));

  EXPECT_TRUE(db()->RemoveRow(test_data().origin_domain));
  EXPECT_THAT(db()->GetRows(test_data().origin_domain), IsEmpty());
  EXPECT_TRUE(db()->RemoveRow(test_data().origin_domain));
}

TEST_F(StatisticsTableTest, DifferentUsernames) {
  InteractionsStats stats1 = test_data();
  InteractionsStats stats2 = test_data();
  stats2.username_value = base::ASCIIToUTF16(kUsername2);

  EXPECT_TRUE(db()->AddRow(stats1));
  EXPECT_TRUE(db()->AddRow(stats2));
  EXPECT_THAT(db()->GetRows(test_data().origin_domain),
              UnorderedElementsAre(Pointee(stats1), Pointee(stats2)));
  EXPECT_TRUE(db()->RemoveRow(test_data().origin_domain));
  EXPECT_THAT(db()->GetRows(test_data().origin_domain), IsEmpty());
}

TEST_F(StatisticsTableTest, RemoveBetween) {
  InteractionsStats stats1 = test_data();
  stats1.update_time = base::Time::FromTimeT(1);
  InteractionsStats stats2 = test_data();
  stats2.update_time = base::Time::FromTimeT(2);
  stats2.origin_domain = GURL(kTestDomain2);

  EXPECT_TRUE(db()->AddRow(stats1));
  EXPECT_TRUE(db()->AddRow(stats2));
  EXPECT_THAT(db()->GetRows(stats1.origin_domain),
              ElementsAre(Pointee(stats1)));
  EXPECT_THAT(db()->GetRows(stats2.origin_domain),
              ElementsAre(Pointee(stats2)));

  // Remove the first one only.
  EXPECT_TRUE(db()->RemoveStatsBetween(base::Time(), base::Time::FromTimeT(2)));
  EXPECT_THAT(db()->GetRows(stats1.origin_domain), IsEmpty());
  EXPECT_THAT(db()->GetRows(stats2.origin_domain),
              ElementsAre(Pointee(stats2)));

  // Remove the second one only.
  EXPECT_TRUE(db()->RemoveStatsBetween(base::Time::FromTimeT(2), base::Time()));
  EXPECT_THAT(db()->GetRows(stats1.origin_domain), IsEmpty());
  EXPECT_THAT(db()->GetRows(stats2.origin_domain), IsEmpty());
}

TEST_F(StatisticsTableTest, BadURL) {
  test_data().origin_domain = GURL("trash");
  EXPECT_FALSE(db()->AddRow(test_data()));
  EXPECT_THAT(db()->GetRows(test_data().origin_domain), IsEmpty());
  EXPECT_FALSE(db()->RemoveRow(test_data().origin_domain));
}

TEST_F(StatisticsTableTest, EmptyURL) {
  test_data().origin_domain = GURL();
  EXPECT_FALSE(db()->AddRow(test_data()));
  EXPECT_THAT(db()->GetRows(test_data().origin_domain), IsEmpty());
  EXPECT_FALSE(db()->RemoveRow(test_data().origin_domain));
}

}  // namespace
}  // namespace password_manager
