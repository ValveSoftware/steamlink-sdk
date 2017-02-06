// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/offline_page_storage_manager.h"

#include <stdint.h>
#include <map>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "components/offline_pages/archive_manager.h"
#include "components/offline_pages/client_namespace_constants.h"
#include "components/offline_pages/client_policy_controller.h"
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/offline_page_model_impl.h"
#include "components/offline_pages/offline_page_types.h"
#include "testing/gtest/include/gtest/gtest.h"

using LifetimePolicy = offline_pages::LifetimePolicy;
using ClearStorageResult =
    offline_pages::OfflinePageStorageManager::ClearStorageResult;
using StorageStats = offline_pages::ArchiveManager::StorageStats;

namespace offline_pages {

namespace {
const GURL kTestUrl("http://example.com");
const base::FilePath::CharType kFilePath[] = FILE_PATH_LITERAL("/data");
const int64_t kTestFileSize = 1 << 19;  // Make a page 512KB.
const int64_t kFreeSpaceNormal = 100 * (1 << 20);

enum TestOptions {
  DEFAULT = 1 << 0,
  EXPIRE_FAILURE = 1 << 1,
  DELETE_FAILURE = 1 << 2,
  EXPIRE_AND_DELETE_FAILURES = EXPIRE_FAILURE | DELETE_FAILURE,
};

struct PageSettings {
  std::string name_space;
  int fresh_pages_count;
  int expired_pages_count;
};
}  // namespace

class OfflinePageTestModel : public OfflinePageModelImpl {
 public:
  OfflinePageTestModel(std::vector<PageSettings> page_settings,
                       base::SimpleTestClock* clock,
                       TestOptions options = TestOptions::DEFAULT)
      : policy_controller_(new ClientPolicyController()),
        clock_(clock),
        options_(options),
        next_offline_id_(0) {
    for (const auto& setting : page_settings)
      AddPages(setting);
  }

  ~OfflinePageTestModel() override;

  void GetAllPages(const MultipleOfflinePageItemCallback& callback) override {
    MultipleOfflinePageItemResult pages;
    for (const auto& id_page_pair : pages_)
      pages.push_back(id_page_pair.second);
    callback.Run(pages);
  }

  void DeletePagesByOfflineId(const std::vector<int64_t>& offline_ids,
                              const DeletePageCallback& callback) override;

  void ExpirePages(const std::vector<int64_t>& offline_ids,
                   const base::Time& expiration_time,
                   const base::Callback<void(bool)>& callback) override;

  void AddPages(const PageSettings& setting);

  const std::vector<OfflinePageItem>& GetRemovedPages() {
    return removed_pages_;
  }

  int64_t GetTotalSize() const;

  base::SimpleTestClock* clock() { return clock_; }

 private:
  std::map<int64_t, OfflinePageItem> pages_;

  std::vector<OfflinePageItem> removed_pages_;

  std::unique_ptr<ClientPolicyController> policy_controller_;

  base::SimpleTestClock* clock_;

  TestOptions options_;

  int64_t next_offline_id_;
};

void OfflinePageTestModel::ExpirePages(
    const std::vector<int64_t>& offline_ids,
    const base::Time& expiration_time,
    const base::Callback<void(bool)>& callback) {
  for (const auto id : offline_ids)
    pages_.at(id).expiration_time = expiration_time;
  if (options_ & TestOptions::EXPIRE_FAILURE) {
    callback.Run(false);
    return;
  }
  callback.Run(true);
}

void OfflinePageTestModel::DeletePagesByOfflineId(
    const std::vector<int64_t>& offline_ids,
    const DeletePageCallback& callback) {
  if (options_ & TestOptions::DELETE_FAILURE) {
    callback.Run(DeletePageResult::STORE_FAILURE);
    return;
  }
  for (const auto id : offline_ids) {
    removed_pages_.push_back(pages_.at(id));
    pages_.erase(id);
  }
  callback.Run(DeletePageResult::SUCCESS);
}

int64_t OfflinePageTestModel::GetTotalSize() const {
  int64_t res = 0;
  for (const auto& id_page_pair : pages_) {
    if (!id_page_pair.second.IsExpired())
      res += id_page_pair.second.file_size;
  }
  return res;
}

OfflinePageTestModel::~OfflinePageTestModel() {}

void OfflinePageTestModel::AddPages(const PageSettings& setting) {
  std::string name_space = setting.name_space;
  int fresh_pages_count = setting.fresh_pages_count;
  int expired_pages_count = setting.expired_pages_count;
  base::Time now = clock()->Now();
  // Fresh pages.
  for (int i = 0; i < fresh_pages_count; i++) {
    OfflinePageItem page =
        OfflinePageItem(kTestUrl, next_offline_id_,
                        ClientId(name_space, std::to_string(next_offline_id_)),
                        base::FilePath(kFilePath), kTestFileSize);
    page.last_access_time = now;
    pages_[next_offline_id_] = page;
    next_offline_id_++;
  }
  // Expired pages.
  for (int i = 0; i < expired_pages_count; i++) {
    OfflinePageItem page =
        OfflinePageItem(kTestUrl, next_offline_id_,
                        ClientId(name_space, std::to_string(next_offline_id_)),
                        base::FilePath(kFilePath), kTestFileSize);
    page.last_access_time = now -
                            policy_controller_->GetPolicy(name_space)
                                .lifetime_policy.expiration_period;
    pages_[next_offline_id_] = page;
    next_offline_id_++;
  }
}

class TestArchiveManager : public ArchiveManager {
 public:
  explicit TestArchiveManager(StorageStats stats) : stats_(stats) {}

  void GetStorageStats(const base::Callback<
                       void(const ArchiveManager::StorageStats& storage_stats)>&
                           callback) const override {
    callback.Run(stats_);
  }

  void SetValues(ArchiveManager::StorageStats stats) { stats_ = stats; }

 private:
  StorageStats stats_;
};

class OfflinePageStorageManagerTest : public testing::Test {
 public:
  OfflinePageStorageManagerTest();
  OfflinePageStorageManager* manager() { return manager_.get(); }
  OfflinePageTestModel* model() { return model_.get(); }
  ClientPolicyController* policy_controller() {
    return policy_controller_.get();
  }
  TestArchiveManager* test_archive_manager() { return archive_manager_.get(); }
  void OnPagesCleared(size_t pages_cleared_count, ClearStorageResult result);
  void Initialize(const std::vector<PageSettings>& settings,
                  StorageStats stats = {kFreeSpaceNormal, 0},
                  TestOptions options = TestOptions::DEFAULT);
  void TryClearPages();

  // testing::Test
  void TearDown() override;

  base::SimpleTestClock* clock() { return clock_; }
  int last_cleared_page_count() const {
    return static_cast<int>(last_cleared_page_count_);
  }
  int total_cleared_times() const { return total_cleared_times_; }
  ClearStorageResult last_clear_storage_result() const {
    return last_clear_storage_result_;
  }

 private:
  std::unique_ptr<OfflinePageStorageManager> manager_;
  std::unique_ptr<OfflinePageTestModel> model_;
  std::unique_ptr<ClientPolicyController> policy_controller_;
  std::unique_ptr<TestArchiveManager> archive_manager_;

  base::SimpleTestClock* clock_;

  size_t last_cleared_page_count_;
  int total_cleared_times_;
  ClearStorageResult last_clear_storage_result_;
};

OfflinePageStorageManagerTest::OfflinePageStorageManagerTest()
    : policy_controller_(new ClientPolicyController()),
      last_cleared_page_count_(0),
      total_cleared_times_(0),
      last_clear_storage_result_(ClearStorageResult::SUCCESS) {}

void OfflinePageStorageManagerTest::Initialize(
    const std::vector<PageSettings>& page_settings,
    StorageStats stats,
    TestOptions options) {
  std::unique_ptr<base::SimpleTestClock> clock(new base::SimpleTestClock());
  clock_ = clock.get();
  clock_->SetNow(base::Time::Now());
  model_.reset(new OfflinePageTestModel(page_settings, clock_, options));

  if (stats.free_disk_space == 0)
    stats.free_disk_space = kFreeSpaceNormal;
  if (stats.total_archives_size == 0) {
    stats.total_archives_size = model_->GetTotalSize();
  }
  archive_manager_.reset(new TestArchiveManager(stats));
  manager_.reset(new OfflinePageStorageManager(
      model_.get(), policy_controller(), archive_manager_.get()));
  manager_->SetClockForTesting(std::move(clock));
}

void OfflinePageStorageManagerTest::TryClearPages() {
  manager()->ClearPagesIfNeeded(base::Bind(
      &OfflinePageStorageManagerTest::OnPagesCleared, base::Unretained(this)));
}

void OfflinePageStorageManagerTest::TearDown() {
  manager_.reset();
  model_.reset();
}

void OfflinePageStorageManagerTest::OnPagesCleared(size_t pages_cleared_count,
                                                   ClearStorageResult result) {
  last_cleared_page_count_ = pages_cleared_count;
  total_cleared_times_++;
  last_clear_storage_result_ = result;
}

TEST_F(OfflinePageStorageManagerTest, TestClearPagesLessThanLimit) {
  Initialize(std::vector<PageSettings>(
      {{kBookmarkNamespace, 1, 1}, {kLastNNamespace, 1, 1}}));
  clock()->Advance(base::TimeDelta::FromMinutes(30));
  TryClearPages();
  EXPECT_EQ(2, last_cleared_page_count());
  EXPECT_EQ(1, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::SUCCESS, last_clear_storage_result());
  EXPECT_EQ(0, static_cast<int>(model()->GetRemovedPages().size()));
}

TEST_F(OfflinePageStorageManagerTest, TestClearPagesMoreThanLimit) {
  Initialize(std::vector<PageSettings>(
      {{kBookmarkNamespace, 10, 15}, {kLastNNamespace, 5, 30}}));
  clock()->Advance(base::TimeDelta::FromMinutes(30));
  TryClearPages();
  EXPECT_EQ(45, last_cleared_page_count());
  EXPECT_EQ(1, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::SUCCESS, last_clear_storage_result());
  EXPECT_EQ(0, static_cast<int>(model()->GetRemovedPages().size()));
}

TEST_F(OfflinePageStorageManagerTest, TestClearPagesMoreFreshPages) {
  Initialize(std::vector<PageSettings>(
                 {{kBookmarkNamespace, 30, 0}, {kLastNNamespace, 100, 1}}),
             {1000 * (1 << 20), 0});
  clock()->Advance(base::TimeDelta::FromMinutes(30));
  TryClearPages();
  EXPECT_EQ(1, last_cleared_page_count());
  EXPECT_EQ(1, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::SUCCESS, last_clear_storage_result());
  EXPECT_EQ(0, static_cast<int>(model()->GetRemovedPages().size()));
}

TEST_F(OfflinePageStorageManagerTest, TestDeletionFailed) {
  Initialize(std::vector<PageSettings>(
                 {{kBookmarkNamespace, 10, 10}, {kLastNNamespace, 10, 10}}),
             {kFreeSpaceNormal, 0}, TestOptions::DELETE_FAILURE);
  TryClearPages();
  EXPECT_EQ(20, last_cleared_page_count());
  EXPECT_EQ(1, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::DELETE_FAILURE, last_clear_storage_result());
  EXPECT_EQ(0, static_cast<int>(model()->GetRemovedPages().size()));
}

TEST_F(OfflinePageStorageManagerTest, TestRemoveFromStoreFailure) {
  Initialize(std::vector<PageSettings>({{kBookmarkNamespace, 10, 10}}), {0, 0},
             TestOptions::EXPIRE_FAILURE);
  clock()->Advance(base::TimeDelta::FromMinutes(30));
  TryClearPages();
  EXPECT_EQ(10, last_cleared_page_count());
  EXPECT_EQ(1, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::EXPIRE_FAILURE, last_clear_storage_result());
  EXPECT_EQ(0, static_cast<int>(model()->GetRemovedPages().size()));
}

TEST_F(OfflinePageStorageManagerTest, TestBothFailure) {
  Initialize(std::vector<PageSettings>({{kBookmarkNamespace, 10, 10}}), {0, 0},
             TestOptions::EXPIRE_AND_DELETE_FAILURES);
  clock()->Advance(base::TimeDelta::FromMinutes(30));
  TryClearPages();
  EXPECT_EQ(ClearStorageResult::EXPIRE_AND_DELETE_FAILURES,
            last_clear_storage_result());
}

TEST_F(OfflinePageStorageManagerTest, TestStorageTimeInterval) {
  Initialize(std::vector<PageSettings>(
      {{kBookmarkNamespace, 10, 10}, {kLastNNamespace, 10, 10}}));
  clock()->Advance(base::TimeDelta::FromMinutes(30));
  TryClearPages();
  EXPECT_EQ(20, last_cleared_page_count());
  EXPECT_EQ(1, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::SUCCESS, last_clear_storage_result());
  EXPECT_EQ(0, static_cast<int>(model()->GetRemovedPages().size()));

  // Advance clock so we go over the gap, but no expired pages.
  clock()->Advance(kClearStorageInterval + base::TimeDelta::FromMinutes(1));
  TryClearPages();
  EXPECT_EQ(0, last_cleared_page_count());
  EXPECT_EQ(2, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::SUCCESS, last_clear_storage_result());
  EXPECT_EQ(0, static_cast<int>(model()->GetRemovedPages().size()));

  // Advance clock so we are still in the gap, should be unnecessary.
  clock()->Advance(kClearStorageInterval - base::TimeDelta::FromMinutes(1));
  TryClearPages();
  EXPECT_EQ(0, last_cleared_page_count());
  EXPECT_EQ(3, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::UNNECESSARY, last_clear_storage_result());
  EXPECT_EQ(0, static_cast<int>(model()->GetRemovedPages().size()));
}

TEST_F(OfflinePageStorageManagerTest, TestTwoStepExpiration) {
  Initialize(std::vector<PageSettings>({{kBookmarkNamespace, 10, 10}}));
  clock()->Advance(base::TimeDelta::FromMinutes(30));
  TryClearPages();
  EXPECT_EQ(10, last_cleared_page_count());
  EXPECT_EQ(1, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::SUCCESS, last_clear_storage_result());
  EXPECT_EQ(0, static_cast<int>(model()->GetRemovedPages().size()));

  clock()->Advance(kRemovePageItemInterval + base::TimeDelta::FromDays(1));
  TryClearPages();
  EXPECT_EQ(10, last_cleared_page_count());
  EXPECT_EQ(2, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::SUCCESS, last_clear_storage_result());
  EXPECT_EQ(10, static_cast<int>(model()->GetRemovedPages().size()));
}

TEST_F(OfflinePageStorageManagerTest, TestClearMultipleTimes) {
  Initialize(std::vector<PageSettings>(
                 {{kBookmarkNamespace, 30, 0}, {kLastNNamespace, 100, 1}}),
             {1000 * (1 << 20), 0});
  clock()->Advance(base::TimeDelta::FromMinutes(30));
  LifetimePolicy policy =
      policy_controller()->GetPolicy(kLastNNamespace).lifetime_policy;

  TryClearPages();
  EXPECT_EQ(1, last_cleared_page_count());
  EXPECT_EQ(1, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::SUCCESS, last_clear_storage_result());
  EXPECT_EQ(0, static_cast<int>(model()->GetRemovedPages().size()));

  // Advance the clock by expiration period of last_n namespace, should be
  // expiring all pages left in the namespace.
  clock()->Advance(policy.expiration_period);
  TryClearPages();
  EXPECT_EQ(100, last_cleared_page_count());
  EXPECT_EQ(2, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::SUCCESS, last_clear_storage_result());
  EXPECT_EQ(0, static_cast<int>(model()->GetRemovedPages().size()));

  // Only 1 ms passes and no changes in pages, so no need to clear page.
  clock()->Advance(base::TimeDelta::FromMilliseconds(1));
  TryClearPages();
  EXPECT_EQ(0, last_cleared_page_count());
  EXPECT_EQ(3, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::UNNECESSARY, last_clear_storage_result());
  EXPECT_EQ(0, static_cast<int>(model()->GetRemovedPages().size()));

  // Adding more fresh pages to make it go over limit.
  clock()->Advance(base::TimeDelta::FromMinutes(5));
  model()->AddPages({kBookmarkNamespace, 400, 0});
  int64_t total_size_before = model()->GetTotalSize();
  int64_t available_space = 300 * (1 << 20);  // 300 MB
  test_archive_manager()->SetValues({available_space, total_size_before});
  EXPECT_GE(total_size_before,
            kOfflinePageStorageLimit * (available_space + total_size_before));
  TryClearPages();
  EXPECT_LE(total_size_before * kOfflinePageStorageClearThreshold,
            model()->GetTotalSize());
  EXPECT_EQ(4, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::SUCCESS, last_clear_storage_result());
  EXPECT_EQ(0, static_cast<int>(model()->GetRemovedPages().size()));
  int expired_page_count = last_cleared_page_count();

  // After more days, all pages should be expired and .
  clock()->Advance(kRemovePageItemInterval + base::TimeDelta::FromDays(1));
  TryClearPages();
  EXPECT_EQ(0, model()->GetTotalSize());
  EXPECT_EQ(5, total_cleared_times());
  EXPECT_EQ(ClearStorageResult::SUCCESS, last_clear_storage_result());
  // Number of removed pages should be the ones expired above and all the pages
  // initially created for last_n namespace.
  EXPECT_EQ(expired_page_count + 101,
            static_cast<int>(model()->GetRemovedPages().size()));
}

}  // namespace offline_pages
