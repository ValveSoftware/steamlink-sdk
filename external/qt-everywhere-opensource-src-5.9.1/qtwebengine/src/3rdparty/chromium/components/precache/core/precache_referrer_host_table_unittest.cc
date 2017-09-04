// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/precache/core/precache_referrer_host_table.h"

#include <map>
#include <memory>

#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "sql/connection.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace precache {

namespace {

const char* kReffererHostFoo = "foo.com";
const char* kReffererHostBar = "bar.com";
const int64_t kManifestIdFoo = 1001;
const int64_t kManifestIdBar = 1002;

class PrecacheReferrerHostTableTest : public testing::Test {
 public:
  PrecacheReferrerHostTableTest() {}
  ~PrecacheReferrerHostTableTest() override {}

 protected:
  void SetUp() override {
    precache_referrer_host_table_.reset(new PrecacheReferrerHostTable());
    db_.reset(new sql::Connection());
    ASSERT_TRUE(db_->OpenInMemory());
    precache_referrer_host_table_->Init(db_.get());
  }

  std::unique_ptr<PrecacheReferrerHostTable> precache_referrer_host_table_;
  std::unique_ptr<sql::Connection> db_;
};

TEST_F(PrecacheReferrerHostTableTest, GetReferrerHost) {
  const base::Time kTimeFoo = base::Time::FromInternalValue(100);
  const base::Time kTimeBar = base::Time::FromInternalValue(200);
  std::map<std::string, PrecacheReferrerHostEntry> expected_entries;

  // Add new referrer hosts.
  int64_t foo_id = precache_referrer_host_table_->UpdateReferrerHost(
      kReffererHostFoo, kManifestIdFoo, kTimeFoo);
  int64_t bar_id = precache_referrer_host_table_->UpdateReferrerHost(
      kReffererHostBar, kManifestIdBar, kTimeBar);

  EXPECT_NE(-1, foo_id);
  EXPECT_NE(-1, bar_id);

  EXPECT_EQ(PrecacheReferrerHostEntry(foo_id, kReffererHostFoo, kManifestIdFoo,
                                      kTimeFoo),
            precache_referrer_host_table_->GetReferrerHost(kReffererHostFoo));
  EXPECT_EQ(PrecacheReferrerHostEntry(bar_id, kReffererHostBar, kManifestIdBar,
                                      kTimeBar),
            precache_referrer_host_table_->GetReferrerHost(kReffererHostBar));

  expected_entries[kReffererHostFoo] = PrecacheReferrerHostEntry(
      foo_id, kReffererHostFoo, kManifestIdFoo, kTimeFoo);
  expected_entries[kReffererHostBar] = PrecacheReferrerHostEntry(
      bar_id, kReffererHostBar, kManifestIdBar, kTimeBar);
  EXPECT_THAT(expected_entries,
              ::testing::ContainerEq(
                  precache_referrer_host_table_->GetAllDataForTesting()));
}

TEST_F(PrecacheReferrerHostTableTest, UpdateReferrerHost) {
  const base::Time kTimeFoo = base::Time::FromInternalValue(100);
  const base::Time kTimeBar = base::Time::FromInternalValue(200);
  std::map<std::string, PrecacheReferrerHostEntry> expected_entries;

  // Add new referrer hosts.
  int64_t foo_id = precache_referrer_host_table_->UpdateReferrerHost(
      kReffererHostFoo, kManifestIdFoo, kTimeFoo);
  int64_t bar_id = precache_referrer_host_table_->UpdateReferrerHost(
      kReffererHostBar, kManifestIdBar, kTimeBar);

  EXPECT_NE(-1, foo_id);
  EXPECT_NE(-1, bar_id);

  expected_entries[kReffererHostFoo] = PrecacheReferrerHostEntry(
      foo_id, kReffererHostFoo, kManifestIdFoo, kTimeFoo);
  expected_entries[kReffererHostBar] = PrecacheReferrerHostEntry(
      bar_id, kReffererHostBar, kManifestIdBar, kTimeBar);
  EXPECT_THAT(expected_entries,
              ::testing::ContainerEq(
                  precache_referrer_host_table_->GetAllDataForTesting()));

  // Updating referrer hosts should return the same ID.
  EXPECT_EQ(foo_id, precache_referrer_host_table_->UpdateReferrerHost(
                        kReffererHostFoo, kManifestIdFoo, kTimeFoo));
  EXPECT_EQ(bar_id, precache_referrer_host_table_->UpdateReferrerHost(
                        kReffererHostBar, kManifestIdBar, kTimeBar));
  EXPECT_THAT(expected_entries,
              ::testing::ContainerEq(
                  precache_referrer_host_table_->GetAllDataForTesting()));
}

TEST_F(PrecacheReferrerHostTableTest, DeleteAll) {
  const base::Time kTimeFoo = base::Time::FromInternalValue(100);
  const base::Time kTimeBar = base::Time::FromInternalValue(200);
  std::map<std::string, PrecacheReferrerHostEntry> expected_entries;

  // Add new referrer hosts.
  int64_t foo_id = precache_referrer_host_table_->UpdateReferrerHost(
      kReffererHostFoo, kManifestIdFoo, kTimeFoo);
  int64_t bar_id = precache_referrer_host_table_->UpdateReferrerHost(
      kReffererHostBar, kManifestIdBar, kTimeBar);

  EXPECT_NE(-1, foo_id);
  EXPECT_NE(-1, bar_id);

  expected_entries[kReffererHostFoo] = PrecacheReferrerHostEntry(
      foo_id, kReffererHostFoo, kManifestIdFoo, kTimeFoo);
  expected_entries[kReffererHostBar] = PrecacheReferrerHostEntry(
      bar_id, kReffererHostBar, kManifestIdBar, kTimeBar);
  EXPECT_THAT(expected_entries,
              ::testing::ContainerEq(
                  precache_referrer_host_table_->GetAllDataForTesting()));

  precache_referrer_host_table_->DeleteAll();

  EXPECT_EQ(0UL, precache_referrer_host_table_->GetAllDataForTesting().size());
}

TEST_F(PrecacheReferrerHostTableTest, DeleteAllEntriesBefore) {
  const base::Time kOldTime = base::Time::FromInternalValue(10);
  const base::Time kBeforeTime = base::Time::FromInternalValue(20);
  const base::Time kEndTime = base::Time::FromInternalValue(30);
  const base::Time kAfterTime = base::Time::FromInternalValue(40);

  precache_referrer_host_table_->UpdateReferrerHost("old.com", 1, kOldTime);
  precache_referrer_host_table_->UpdateReferrerHost("before.com", 2,
                                                    kBeforeTime);
  precache_referrer_host_table_->UpdateReferrerHost("end.com", 3, kEndTime);
  precache_referrer_host_table_->UpdateReferrerHost("after.com", 4, kAfterTime);

  precache_referrer_host_table_->DeleteAllEntriesBefore(kEndTime);

  const auto actual_entries =
      precache_referrer_host_table_->GetAllDataForTesting();
  EXPECT_EQ(2UL, actual_entries.size());
  EXPECT_NE(actual_entries.end(), actual_entries.find("end.com"));
  EXPECT_NE(actual_entries.end(), actual_entries.find("after.com"));
}

}  // namespace

}  // namespace precache
