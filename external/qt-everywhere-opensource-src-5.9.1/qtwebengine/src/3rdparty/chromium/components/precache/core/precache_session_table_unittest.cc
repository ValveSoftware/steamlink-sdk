// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "components/precache/core/precache_session_table.h"
#include "components/precache/core/proto/quota.pb.h"
#include "components/precache/core/proto/unfinished_work.pb.h"
#include "sql/connection.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace precache {

namespace {

class PrecacheSessionTableTest : public testing::Test {
 public:
  PrecacheSessionTableTest() {}
  ~PrecacheSessionTableTest() override {}

 protected:
  void SetUp() override {
    precache_session_table_.reset(new PrecacheSessionTable());
    db_.reset(new sql::Connection());
    ASSERT_TRUE(db_->OpenInMemory());
    precache_session_table_->Init(db_.get());
  }

  std::unique_ptr<PrecacheSessionTable> precache_session_table_;
  std::unique_ptr<sql::Connection> db_;
};

TEST_F(PrecacheSessionTableTest, LastPrecacheTimestamp) {
  const base::Time sometime = base::Time::FromDoubleT(42);

  precache_session_table_->SetLastPrecacheTimestamp(sometime);

  EXPECT_EQ(sometime, precache_session_table_->GetLastPrecacheTimestamp());

  precache_session_table_->DeleteLastPrecacheTimestamp();

  EXPECT_EQ(base::Time(), precache_session_table_->GetLastPrecacheTimestamp());
}

TEST_F(PrecacheSessionTableTest, SaveAndGetUnfinishedWork) {
  std::unique_ptr<PrecacheUnfinishedWork> unfinished_work(
      new PrecacheUnfinishedWork());
  unfinished_work->add_top_host()->set_hostname("foo.com");
  unfinished_work->add_top_host()->set_hostname("bar.com");
  auto* s = unfinished_work->mutable_config_settings();
  s->set_top_sites_count(11);
  s->add_forced_site("baz.com");
  s->set_top_resources_count(12);
  s->set_max_bytes_per_resource(501);
  s->set_max_bytes_total(1001);
  unfinished_work->add_resource()->set_url("http://x.com/");
  unfinished_work->add_resource()->set_url("http://y.com/");
  unfinished_work->add_resource()->set_url("http://z.com/");
  unfinished_work->set_total_bytes(13);
  unfinished_work->set_network_bytes(14);
  unfinished_work->set_num_manifest_urls(15);
  base::Time sometime = base::Time::UnixEpoch();
  unfinished_work->set_start_time(sometime.ToInternalValue());

  precache_session_table_->SaveUnfinishedWork(std::move(unfinished_work));
  std::unique_ptr<PrecacheUnfinishedWork> unfinished_work2 =
      precache_session_table_->GetUnfinishedWork();

  EXPECT_EQ(2, unfinished_work2->top_host_size());
  EXPECT_EQ("foo.com", unfinished_work2->top_host(0).hostname());
  EXPECT_EQ("bar.com", unfinished_work2->top_host(1).hostname());
  EXPECT_EQ(11, unfinished_work2->config_settings().top_sites_count());
  EXPECT_EQ(1, unfinished_work2->config_settings().forced_site_size());
  EXPECT_EQ("baz.com", unfinished_work2->config_settings().forced_site(0));
  EXPECT_EQ(12, unfinished_work2->config_settings().top_resources_count());
  EXPECT_EQ(501ul,
            unfinished_work2->config_settings().max_bytes_per_resource());
  EXPECT_EQ(1001ul, unfinished_work2->config_settings().max_bytes_total());
  EXPECT_EQ(3, unfinished_work2->resource_size());
  EXPECT_EQ("http://x.com/", unfinished_work2->resource(0).url());
  EXPECT_EQ("http://y.com/", unfinished_work2->resource(1).url());
  EXPECT_EQ("http://z.com/", unfinished_work2->resource(2).url());
  EXPECT_EQ(13ul, unfinished_work2->total_bytes());
  EXPECT_EQ(14ul, unfinished_work2->network_bytes());
  EXPECT_EQ(15ul, unfinished_work2->num_manifest_urls());
  EXPECT_EQ(base::Time::UnixEpoch(),
            base::Time::FromInternalValue(unfinished_work2->start_time()));
}

// Test that storing overwrites previous unfinished work.
TEST_F(PrecacheSessionTableTest, SaveAgainAndGet) {
  std::unique_ptr<PrecacheUnfinishedWork> unfinished_work(
      new PrecacheUnfinishedWork());
  unfinished_work->add_top_host()->set_hostname("a.com");
  precache_session_table_->SaveUnfinishedWork(std::move(unfinished_work));

  std::unique_ptr<PrecacheUnfinishedWork> unfinished_work2(
      new PrecacheUnfinishedWork());
  unfinished_work2->add_top_host()->set_hostname("b.com");
  precache_session_table_->SaveUnfinishedWork(std::move(unfinished_work2));

  std::unique_ptr<PrecacheUnfinishedWork> unfinished_work3 =
      precache_session_table_->GetUnfinishedWork();
  EXPECT_EQ("b.com", unfinished_work3->top_host(0).hostname());
}

// Test that reading does not remove unfinished work from storage.
TEST_F(PrecacheSessionTableTest, SaveAndGetAgain) {
  std::unique_ptr<PrecacheUnfinishedWork> unfinished_work(
      new PrecacheUnfinishedWork());
  unfinished_work->add_top_host()->set_hostname("a.com");
  precache_session_table_->SaveUnfinishedWork(std::move(unfinished_work));

  std::unique_ptr<PrecacheUnfinishedWork> unfinished_work2 =
      precache_session_table_->GetUnfinishedWork();

  std::unique_ptr<PrecacheUnfinishedWork> unfinished_work3 =
      precache_session_table_->GetUnfinishedWork();

  EXPECT_EQ("a.com", unfinished_work3->top_host(0).hostname());
}

// Test that storing a large proto works.
TEST_F(PrecacheSessionTableTest, SaveManyURLs) {
  std::unique_ptr<PrecacheUnfinishedWork> unfinished_work(
      new PrecacheUnfinishedWork());
  for (int i = 0; i < 1000; ++i)
    unfinished_work->add_top_host()->set_hostname("a.com");
  precache_session_table_->SaveUnfinishedWork(std::move(unfinished_work));

  std::unique_ptr<PrecacheUnfinishedWork> unfinished_work2 =
      precache_session_table_->GetUnfinishedWork();

  EXPECT_EQ(1000, unfinished_work2->top_host_size());
  for (int i = 0; i < 1000; ++i)
    EXPECT_EQ("a.com", unfinished_work2->top_host(i).hostname());
}

// Test that reading after deletion returns no unfinished work.
TEST_F(PrecacheSessionTableTest, SaveDeleteGet) {
  std::unique_ptr<PrecacheUnfinishedWork> unfinished_work(
      new PrecacheUnfinishedWork());
  unfinished_work->add_top_host()->set_hostname("a.com");
  precache_session_table_->SaveUnfinishedWork(std::move(unfinished_work));
  precache_session_table_->DeleteUnfinishedWork();

  std::unique_ptr<PrecacheUnfinishedWork> unfinished_work2 =
      precache_session_table_->GetUnfinishedWork();

  EXPECT_EQ(0, unfinished_work2->top_host_size());
}

TEST_F(PrecacheSessionTableTest, SaveAndGetQuota) {
  // Initial quota, should have expired.
  EXPECT_LT(base::Time::FromInternalValue(
                precache_session_table_->GetQuota().start_time()),
            base::Time::Now());

  PrecacheQuota quota;
  quota.set_start_time(base::Time::Now().ToInternalValue());
  quota.set_remaining(1000U);

  PrecacheQuota expected_quota = quota;
  precache_session_table_->SaveQuota(quota);
  EXPECT_EQ(expected_quota.start_time(),
            precache_session_table_->GetQuota().start_time());
  EXPECT_EQ(expected_quota.remaining(),
            precache_session_table_->GetQuota().remaining());
}

}  // namespace

}  // namespace precache
