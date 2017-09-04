// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_memory_controller.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_item.h"
#include "storage/browser/blob/shareable_blob_data_item.h"
#include "storage/common/data_element.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {

using Strategy = BlobMemoryController::Strategy;
using FileCreationInfo = BlobMemoryController::FileCreationInfo;
using base::TestSimpleTaskRunner;
using ItemState = ShareableBlobDataItem::State;
using QuotaAllocationTask = BlobMemoryController::QuotaAllocationTask;

const std::string kBlobStorageDirectory = "blob_storage";
const size_t kTestBlobStorageIPCThresholdBytes = 20;
const size_t kTestBlobStorageMaxSharedMemoryBytes = 50;
const size_t kTestBlobStorageMaxBlobMemorySize = 500;
const uint64_t kTestBlobStorageMaxDiskSpace = 1000;
const uint64_t kTestBlobStorageMinFileSizeBytes = 10;
const uint64_t kTestBlobStorageMaxFileSizeBytes = 100;

class BlobMemoryControllerTest : public testing::Test {
 protected:
  BlobMemoryControllerTest() {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::ThreadRestrictions::SetIOAllowed(false);
  };

  void TearDown() override {
    files_created_.clear();
    // Make sure we clean up the files.
    base::RunLoop().RunUntilIdle();
    RunFileThreadTasks();
    base::RunLoop().RunUntilIdle();
    base::ThreadRestrictions::SetIOAllowed(true);
    ASSERT_TRUE(temp_dir_.Delete());
  }

  std::vector<scoped_refptr<ShareableBlobDataItem>> CreateSharedDataItems(
      const BlobDataBuilder& builder) {
    std::vector<scoped_refptr<ShareableBlobDataItem>> result;
    for (size_t i = 0; i < builder.items_.size(); ++i) {
      result.push_back(make_scoped_refptr(new ShareableBlobDataItem(
          builder.items_[i], ShareableBlobDataItem::QUOTA_NEEDED)));
    }
    return result;
  }

  void SetTestMemoryLimits(BlobMemoryController* controller) {
    BlobStorageLimits limits;
    limits.max_ipc_memory_size = kTestBlobStorageIPCThresholdBytes;
    limits.max_shared_memory_size = kTestBlobStorageMaxSharedMemoryBytes;
    limits.max_blob_in_memory_space = kTestBlobStorageMaxBlobMemorySize;
    limits.max_blob_disk_space = kTestBlobStorageMaxDiskSpace;
    limits.min_page_file_size = kTestBlobStorageMinFileSizeBytes;
    limits.max_file_size = kTestBlobStorageMaxFileSizeBytes;
    controller->set_limits_for_testing(limits);
  }

  void SaveFileCreationInfo(bool success, std::vector<FileCreationInfo> info) {
    file_quota_result_ = success;
    if (success) {
      files_created_.swap(info);
    }
  }

  void SaveMemoryRequest(bool success) { memory_quota_result_ = success; }

  BlobMemoryController::FileQuotaRequestCallback GetFileCreationCallback() {
    return base::Bind(&BlobMemoryControllerTest::SaveFileCreationInfo,
                      base::Unretained(this));
  }

  BlobMemoryController::MemoryQuotaRequestCallback GetMemoryRequestCallback() {
    return base::Bind(&BlobMemoryControllerTest::SaveMemoryRequest,
                      base::Unretained(this));
  }

  void RunFileThreadTasks() {
    base::ThreadRestrictions::SetIOAllowed(true);
    file_runner_->RunPendingTasks();
    base::ThreadRestrictions::SetIOAllowed(false);
  }

  bool HasMemoryAllocation(ShareableBlobDataItem* item) {
    return static_cast<bool>(item->memory_allocation_);
  }

  bool file_quota_result_ = false;
  base::ScopedTempDir temp_dir_;
  std::vector<FileCreationInfo> files_created_;
  bool memory_quota_result_ = false;

  scoped_refptr<TestSimpleTaskRunner> file_runner_ = new TestSimpleTaskRunner();

  base::MessageLoop fake_io_message_loop_;
};

TEST_F(BlobMemoryControllerTest, Strategy) {
  {
    BlobMemoryController controller(temp_dir_.GetPath(), nullptr);
    SetTestMemoryLimits(&controller);

    // No transportation needed.
    EXPECT_EQ(Strategy::NONE_NEEDED, controller.DetermineStrategy(0, 0));

    // IPC.
    EXPECT_EQ(Strategy::IPC, controller.DetermineStrategy(
                                 0, kTestBlobStorageIPCThresholdBytes));

    // Shared Memory.
    EXPECT_EQ(
        Strategy::SHARED_MEMORY,
        controller.DetermineStrategy(kTestBlobStorageIPCThresholdBytes,
                                     kTestBlobStorageMaxSharedMemoryBytes));
    EXPECT_EQ(
        Strategy::SHARED_MEMORY,
        controller.DetermineStrategy(0, kTestBlobStorageMaxBlobMemorySize));

    // Too large.
    EXPECT_EQ(
        Strategy::TOO_LARGE,
        controller.DetermineStrategy(0, kTestBlobStorageMaxBlobMemorySize + 1));
  }
  {
    // Enable disk, and check file strategies.
    BlobMemoryController controller(temp_dir_.GetPath(), file_runner_);
    SetTestMemoryLimits(&controller);

    EXPECT_EQ(
        Strategy::SHARED_MEMORY,
        controller.DetermineStrategy(0, kTestBlobStorageMaxBlobMemorySize -
                                            kTestBlobStorageMinFileSizeBytes));
    EXPECT_EQ(Strategy::FILE, controller.DetermineStrategy(
                                  0, kTestBlobStorageMaxBlobMemorySize -
                                         kTestBlobStorageMinFileSizeBytes + 1));

    EXPECT_EQ(Strategy::FILE, controller.DetermineStrategy(
                                  0, kTestBlobStorageMaxBlobMemorySize));

    // Too large for disk.
    EXPECT_EQ(Strategy::TOO_LARGE, controller.DetermineStrategy(
                                       0, kTestBlobStorageMaxDiskSpace + 1));
  }
}

TEST_F(BlobMemoryControllerTest, GrantMemory) {
  const std::string kId = "id";
  BlobMemoryController controller(temp_dir_.GetPath(), file_runner_);
  SetTestMemoryLimits(&controller);

  BlobDataBuilder builder(kId);
  builder.AppendFutureData(10);
  builder.AppendFutureData(20);
  builder.AppendFutureData(30);

  std::vector<scoped_refptr<ShareableBlobDataItem>> items =
      CreateSharedDataItems(builder);

  controller.ReserveMemoryQuota(items, GetMemoryRequestCallback());
  EXPECT_TRUE(memory_quota_result_);
  EXPECT_EQ(ItemState::QUOTA_GRANTED, items[0]->state());
  EXPECT_TRUE(HasMemoryAllocation(items[0].get()));
  EXPECT_EQ(ItemState::QUOTA_GRANTED, items[1]->state());
  EXPECT_TRUE(HasMemoryAllocation(items[0].get()));
  EXPECT_EQ(ItemState::QUOTA_GRANTED, items[2]->state());
  EXPECT_TRUE(HasMemoryAllocation(items[0].get()));
}

TEST_F(BlobMemoryControllerTest, SimpleMemoryRequest) {
  const std::string kId = "id";
  BlobMemoryController controller(temp_dir_.GetPath(), file_runner_);
  SetTestMemoryLimits(&controller);

  // Add memory item that is the memory quota.
  BlobDataBuilder builder(kId);
  builder.AppendFutureData(kTestBlobStorageMaxBlobMemorySize);

  std::vector<scoped_refptr<ShareableBlobDataItem>> items =
      CreateSharedDataItems(builder);

  base::WeakPtr<QuotaAllocationTask> task =
      controller.ReserveMemoryQuota(items, GetMemoryRequestCallback());
  EXPECT_EQ(nullptr, task);
  EXPECT_TRUE(memory_quota_result_);
  memory_quota_result_ = false;
  EXPECT_EQ(ItemState::QUOTA_GRANTED, items[0]->state());
  EXPECT_FALSE(file_runner_->HasPendingTask());
  EXPECT_EQ(kTestBlobStorageMaxBlobMemorySize, controller.memory_usage());
  EXPECT_EQ(0u, controller.disk_usage());

  items.clear();
  EXPECT_EQ(0u, controller.memory_usage());
}

TEST_F(BlobMemoryControllerTest, PageToDisk) {
  const std::string kId = "id";
  const std::string kId2 = "id2";
  BlobMemoryController controller(temp_dir_.GetPath(), file_runner_);
  SetTestMemoryLimits(&controller);

  char kData[kTestBlobStorageMaxBlobMemorySize];
  std::memset(kData, 'e', kTestBlobStorageMaxBlobMemorySize);

  // Add memory item that is the memory quota.
  BlobDataBuilder builder(kId);
  builder.AppendFutureData(kTestBlobStorageMaxBlobMemorySize);

  std::vector<scoped_refptr<ShareableBlobDataItem>> items =
      CreateSharedDataItems(builder);

  controller.ReserveMemoryQuota(items, GetMemoryRequestCallback());
  EXPECT_TRUE(memory_quota_result_);
  memory_quota_result_ = false;
  EXPECT_EQ(ItemState::QUOTA_GRANTED, items[0]->state());
  EXPECT_FALSE(file_runner_->HasPendingTask());
  EXPECT_EQ(kTestBlobStorageMaxBlobMemorySize, controller.memory_usage());
  EXPECT_EQ(0u, controller.disk_usage());

  // Create an item that is just a little too big.
  BlobDataBuilder builder2(kId2);
  builder2.AppendFutureData(kTestBlobStorageMinFileSizeBytes + 1);

  // Reserve memory, which should request successfuly but we can't fit it yet
  // (no callback).
  std::vector<scoped_refptr<ShareableBlobDataItem>> items2 =
      CreateSharedDataItems(builder2);
  base::WeakPtr<QuotaAllocationTask> task =
      controller.ReserveMemoryQuota(items2, GetMemoryRequestCallback());
  EXPECT_NE(nullptr, task);
  // We don't count the usage yet.
  EXPECT_EQ(kTestBlobStorageMaxBlobMemorySize, controller.memory_usage());
  EXPECT_EQ(0u, controller.disk_usage());

  EXPECT_FALSE(memory_quota_result_);
  EXPECT_EQ(ItemState::QUOTA_REQUESTED, items2[0]->state());
  EXPECT_FALSE(file_runner_->HasPendingTask());

  // Add our original item as populated so it's paged to disk.
  items[0]->item()->data_element_ptr()->SetToBytes(
      kData, kTestBlobStorageMaxBlobMemorySize);
  items[0]->set_state(ItemState::POPULATED_WITH_QUOTA);
  controller.NotifyMemoryItemsUsed(items);

  EXPECT_TRUE(file_runner_->HasPendingTask());
  RunFileThreadTasks();
  base::RunLoop().RunUntilIdle();
  // items2 are successfuly allocated.
  EXPECT_EQ(nullptr, task);
  EXPECT_EQ(ItemState::QUOTA_GRANTED, items2[0]->state());
  EXPECT_EQ(DataElement::TYPE_FILE, items[0]->item()->type());
  EXPECT_EQ(kTestBlobStorageMinFileSizeBytes + 1, controller.memory_usage());
  EXPECT_EQ(kTestBlobStorageMaxBlobMemorySize, controller.disk_usage());

  EXPECT_FALSE(controller.CanReserveQuota(kTestBlobStorageMaxDiskSpace));

  items2.clear();
  EXPECT_EQ(0u, controller.memory_usage());
  items.clear();
  EXPECT_TRUE(file_runner_->HasPendingTask());
  RunFileThreadTasks();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, controller.disk_usage());
}

TEST_F(BlobMemoryControllerTest, NoDiskTooLarge) {
  const std::string kId = "id";
  BlobMemoryController controller(temp_dir_.GetPath(), nullptr);
  SetTestMemoryLimits(&controller);

  EXPECT_FALSE(controller.CanReserveQuota(kTestBlobStorageMaxBlobMemorySize +
                                          kTestBlobStorageMinFileSizeBytes +
                                          1));
}

TEST_F(BlobMemoryControllerTest, TooLargeForDisk) {
  const std::string kId = "id";
  BlobMemoryController controller(temp_dir_.GetPath(), file_runner_);
  SetTestMemoryLimits(&controller);

  EXPECT_FALSE(controller.CanReserveQuota(kTestBlobStorageMaxDiskSpace + 1));
}

TEST_F(BlobMemoryControllerTest, CancelMemoryRequest) {
  const std::string kId = "id";
  const std::string kId2 = "id2";
  BlobMemoryController controller(temp_dir_.GetPath(), file_runner_);
  SetTestMemoryLimits(&controller);

  char kData[kTestBlobStorageMaxBlobMemorySize];
  std::memset(kData, 'e', kTestBlobStorageMaxBlobMemorySize);

  // Add memory item that is the memory quota.
  BlobDataBuilder builder(kId);
  builder.AppendFutureData(kTestBlobStorageMaxBlobMemorySize);

  std::vector<scoped_refptr<ShareableBlobDataItem>> items =
      CreateSharedDataItems(builder);

  controller.ReserveMemoryQuota(items, GetMemoryRequestCallback());

  // Create an item that is just a little too big.
  BlobDataBuilder builder2(kId2);
  builder2.AppendFutureData(kTestBlobStorageMinFileSizeBytes + 1);

  // Reserve memory, which should request successfuly but we can't fit it yet
  // (no callback).
  std::vector<scoped_refptr<ShareableBlobDataItem>> items2 =
      CreateSharedDataItems(builder2);
  base::WeakPtr<QuotaAllocationTask> task =
      controller.ReserveMemoryQuota(items2, GetMemoryRequestCallback());
  // We don't count the usage yet.
  EXPECT_EQ(kTestBlobStorageMaxBlobMemorySize, controller.memory_usage());
  EXPECT_EQ(0u, controller.disk_usage());

  // Add our original item as populated so we start paging to disk.
  items[0]->item()->data_element_ptr()->SetToBytes(
      kData, kTestBlobStorageMaxBlobMemorySize);
  items[0]->set_state(ItemState::POPULATED_WITH_QUOTA);
  controller.NotifyMemoryItemsUsed(items);

  EXPECT_TRUE(file_runner_->HasPendingTask());
  EXPECT_TRUE(task);

  task->Cancel();
  EXPECT_FALSE(task);
  RunFileThreadTasks();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ItemState::QUOTA_REQUESTED, items2[0]->state());
  EXPECT_EQ(DataElement::TYPE_FILE, items[0]->item()->type());
  EXPECT_EQ(0u, controller.memory_usage());
  EXPECT_EQ(kTestBlobStorageMaxBlobMemorySize, controller.disk_usage());

  items.clear();
  // Run cleanup tasks from the ShareableFileReferences.
  base::RunLoop().RunUntilIdle();
  RunFileThreadTasks();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, controller.disk_usage());
}

TEST_F(BlobMemoryControllerTest, FileRequest) {
  const std::string kId = "id";
  const size_t kBlobSize = kTestBlobStorageMaxBlobMemorySize + 1;

  BlobMemoryController controller(temp_dir_.GetPath(), file_runner_);
  SetTestMemoryLimits(&controller);

  char kData[kBlobSize];
  std::memset(kData, 'e', kBlobSize);

  // Add item that is the file quota.
  BlobDataBuilder builder(kId);
  builder.AppendFutureFile(0, kBlobSize, 0);

  std::vector<scoped_refptr<ShareableBlobDataItem>> items =
      CreateSharedDataItems(builder);

  file_quota_result_ = false;
  base::WeakPtr<QuotaAllocationTask> task =
      controller.ReserveFileQuota(items, GetFileCreationCallback());
  EXPECT_TRUE(task);
  EXPECT_FALSE(file_quota_result_);
  EXPECT_EQ(ItemState::QUOTA_REQUESTED, items[0]->state());
  EXPECT_TRUE(file_runner_->HasPendingTask());
  EXPECT_EQ(0u, controller.memory_usage());
  EXPECT_EQ(kBlobSize, controller.disk_usage());

  EXPECT_FALSE(controller.CanReserveQuota(kTestBlobStorageMaxDiskSpace));

  RunFileThreadTasks();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(file_quota_result_);
  EXPECT_FALSE(file_runner_->HasPendingTask());
  EXPECT_FALSE(task);

  // Do the work to populate the file.
  EXPECT_EQ(1u, files_created_.size());
  EXPECT_TRUE(
      builder.PopulateFutureFile(0, std::move(files_created_[0].file_reference),
                                 files_created_[0].last_modified));
  base::ThreadRestrictions::SetIOAllowed(true);
  files_created_.clear();
  base::ThreadRestrictions::SetIOAllowed(false);
  EXPECT_EQ(DataElement::TYPE_FILE, items[0]->item()->type());
  EXPECT_FALSE(
      BlobDataBuilder::IsFutureFileItem(items[0]->item()->data_element()));

  builder.Clear();
  items.clear();
  // Run cleanup tasks from the ShareableFileReferences.
  base::RunLoop().RunUntilIdle();
  RunFileThreadTasks();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, controller.disk_usage());
}

TEST_F(BlobMemoryControllerTest, CancelFileRequest) {
  const std::string kId = "id";
  const size_t kBlobSize = kTestBlobStorageMaxBlobMemorySize + 1;

  BlobMemoryController controller(temp_dir_.GetPath(), file_runner_);
  SetTestMemoryLimits(&controller);

  char kData[kBlobSize];
  std::memset(kData, 'e', kBlobSize);

  // Add memory item that is the memory quota.
  BlobDataBuilder builder(kId);
  builder.AppendFutureFile(0, kBlobSize, 0);

  std::vector<scoped_refptr<ShareableBlobDataItem>> items =
      CreateSharedDataItems(builder);

  base::WeakPtr<QuotaAllocationTask> task =
      controller.ReserveFileQuota(items, GetFileCreationCallback());
  EXPECT_TRUE(task);
  EXPECT_EQ(ItemState::QUOTA_REQUESTED, items[0]->state());
  EXPECT_TRUE(file_runner_->HasPendingTask());
  EXPECT_EQ(0u, controller.memory_usage());
  EXPECT_EQ(kBlobSize, controller.disk_usage());

  task->Cancel();
  EXPECT_FALSE(task);
  EXPECT_EQ(kBlobSize, controller.disk_usage());
  EXPECT_TRUE(file_runner_->HasPendingTask());
  RunFileThreadTasks();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, controller.disk_usage());
}

TEST_F(BlobMemoryControllerTest, MultipleFilesPaged) {
  const std::string kId1 = "id";
  const size_t kSize1 = kTestBlobStorageMaxFileSizeBytes;
  char kData1[kSize1];
  std::memset(kData1, 'e', kSize1);

  const std::string kId2 = "id2";
  const size_t kSize2 = kTestBlobStorageMaxFileSizeBytes;
  char kData2[kSize2];
  std::memset(kData2, 'f', kSize2);

  const std::string kId3 = "id3";
  const size_t kSize3 = kTestBlobStorageMaxBlobMemorySize - 1;

  // Assert we shouldn't trigger paging preemptively.
  ASSERT_LE(kSize1 + kSize2, kTestBlobStorageMaxBlobMemorySize);

  BlobMemoryController controller(temp_dir_.GetPath(), file_runner_);
  SetTestMemoryLimits(&controller);

  // We add two items that should be their own files when we page to disk, and
  // then add the last item to trigger the paging.

  BlobDataBuilder builder1(kId1);
  builder1.AppendFutureData(kSize1);
  BlobDataBuilder builder2(kId2);
  builder2.AppendFutureData(kSize2);

  std::vector<scoped_refptr<ShareableBlobDataItem>> items1 =
      CreateSharedDataItems(builder1);
  std::vector<scoped_refptr<ShareableBlobDataItem>> items2 =
      CreateSharedDataItems(builder2);

  memory_quota_result_ = false;
  controller.ReserveMemoryQuota(items1, GetMemoryRequestCallback());
  EXPECT_TRUE(memory_quota_result_);
  memory_quota_result_ = false;
  controller.ReserveMemoryQuota(items2, GetMemoryRequestCallback());
  EXPECT_TRUE(memory_quota_result_);
  EXPECT_EQ(ItemState::QUOTA_GRANTED, items1[0]->state());
  EXPECT_EQ(ItemState::QUOTA_GRANTED, items2[0]->state());
  EXPECT_FALSE(file_runner_->HasPendingTask());
  EXPECT_EQ(kSize1 + kSize2, controller.memory_usage());
  EXPECT_EQ(0u, controller.disk_usage());

  // Create an item that is too big.
  BlobDataBuilder builder3(kId3);
  builder3.AppendFutureData(kSize3);

  std::vector<scoped_refptr<ShareableBlobDataItem>> items3 =
      CreateSharedDataItems(builder3);
  memory_quota_result_ = false;
  controller.ReserveMemoryQuota(items3, GetMemoryRequestCallback());
  EXPECT_FALSE(memory_quota_result_);

  EXPECT_EQ(ItemState::QUOTA_REQUESTED, items3[0]->state());
  EXPECT_FALSE(file_runner_->HasPendingTask());

  // Add our original item as populated so it's paged to disk.
  items1[0]->item()->data_element_ptr()->SetToBytes(kData1, kSize1);
  items1[0]->set_state(ItemState::POPULATED_WITH_QUOTA);
  items2[0]->item()->data_element_ptr()->SetToBytes(kData2, kSize2);
  items2[0]->set_state(ItemState::POPULATED_WITH_QUOTA);

  std::vector<scoped_refptr<ShareableBlobDataItem>> both_items = {items1[0],
                                                                  items2[0]};
  controller.NotifyMemoryItemsUsed(both_items);
  both_items.clear();

  EXPECT_TRUE(file_runner_->HasPendingTask());
  RunFileThreadTasks();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(memory_quota_result_);
  EXPECT_EQ(ItemState::QUOTA_GRANTED, items3[0]->state());
  EXPECT_EQ(DataElement::TYPE_FILE, items1[0]->item()->type());
  EXPECT_EQ(DataElement::TYPE_FILE, items2[0]->item()->type());
  EXPECT_NE(items1[0]->item()->path(), items2[0]->item()->path());
  EXPECT_EQ(kSize3, controller.memory_usage());
  EXPECT_EQ(kSize1 + kSize2, controller.disk_usage());

  items1.clear();
  items2.clear();
  items3.clear();

  EXPECT_EQ(0u, controller.memory_usage());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(file_runner_->HasPendingTask());
  RunFileThreadTasks();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, controller.disk_usage());
}

TEST_F(BlobMemoryControllerTest, FullEviction) {
  BlobMemoryController controller(temp_dir_.GetPath(), file_runner_);
  SetTestMemoryLimits(&controller);

  char kData[1];
  kData[0] = 'e';

  // Create a bunch of small stuff.
  std::vector<scoped_refptr<ShareableBlobDataItem>> small_items;
  for (size_t i = 0;
       i < kTestBlobStorageMaxBlobMemorySize - kTestBlobStorageMinFileSizeBytes;
       i++) {
    BlobDataBuilder builder("fake");
    builder.AppendData(kData, 1);
    std::vector<scoped_refptr<ShareableBlobDataItem>> items =
        CreateSharedDataItems(builder);
    base::WeakPtr<QuotaAllocationTask> memory_task =
        controller.ReserveMemoryQuota(items, GetMemoryRequestCallback());
    EXPECT_FALSE(memory_task);
    items[0]->set_state(ItemState::POPULATED_WITH_QUOTA);
    small_items.insert(small_items.end(), items.begin(), items.end());
  }
  controller.NotifyMemoryItemsUsed(small_items);
  EXPECT_FALSE(file_runner_->HasPendingTask());
  EXPECT_EQ(
      kTestBlobStorageMaxBlobMemorySize - kTestBlobStorageMinFileSizeBytes,
      controller.memory_usage());

  // Create maximum size blob to evict ALL small stuff.
  BlobDataBuilder builder("fake");
  builder.AppendFutureData(kTestBlobStorageMaxBlobMemorySize -
                           kTestBlobStorageMinFileSizeBytes);
  std::vector<scoped_refptr<ShareableBlobDataItem>> items =
      CreateSharedDataItems(builder);

  memory_quota_result_ = false;
  base::WeakPtr<QuotaAllocationTask> memory_task =
      controller.ReserveMemoryQuota(items, GetMemoryRequestCallback());
  EXPECT_TRUE(memory_task);
  EXPECT_TRUE(file_runner_->HasPendingTask());

  RunFileThreadTasks();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(
      kTestBlobStorageMaxBlobMemorySize - kTestBlobStorageMinFileSizeBytes,
      controller.memory_usage());
  EXPECT_EQ(
      kTestBlobStorageMaxBlobMemorySize - kTestBlobStorageMinFileSizeBytes,
      controller.disk_usage());

  EXPECT_TRUE(memory_quota_result_);
}

TEST_F(BlobMemoryControllerTest, DisableDiskWithFileAndMemoryPending) {
  const std::string kFirstMemoryId = "id";
  const uint64_t kFirstMemorySize = kTestBlobStorageMaxBlobMemorySize;
  const std::string kSecondMemoryId = "id2";
  const uint64_t kSecondMemorySize = 1;
  const std::string kFileId = "id2";
  const uint64_t kFileBlobSize = kTestBlobStorageMaxBlobMemorySize;

  BlobMemoryController controller(temp_dir_.GetPath(), file_runner_);
  SetTestMemoryLimits(&controller);

  char kDataMemoryData[kFirstMemorySize];
  std::memset(kDataMemoryData, 'e', kFirstMemorySize);

  // Add first memory item to fill up some memory quota.
  BlobDataBuilder builder(kFirstMemoryId);
  builder.AppendFutureData(kTestBlobStorageMaxBlobMemorySize);

  std::vector<scoped_refptr<ShareableBlobDataItem>> items =
      CreateSharedDataItems(builder);

  controller.ReserveMemoryQuota(items, GetMemoryRequestCallback());

  // Create a second memory item that is just a little too big.
  BlobDataBuilder builder2(kSecondMemoryId);
  builder2.AppendFutureData(kSecondMemorySize);

  // Reserve memory, which should request successfuly but we can't fit it yet.
  std::vector<scoped_refptr<ShareableBlobDataItem>> items2 =
      CreateSharedDataItems(builder2);
  base::WeakPtr<QuotaAllocationTask> memory_task =
      controller.ReserveMemoryQuota(items2, GetMemoryRequestCallback());
  // We don't count the usage yet.
  EXPECT_EQ(kTestBlobStorageMaxBlobMemorySize, controller.memory_usage());
  EXPECT_EQ(0u, controller.disk_usage());

  // Add our original item as populated so we start paging it to disk.
  items[0]->item()->data_element_ptr()->SetToBytes(kDataMemoryData,
                                                   kFirstMemorySize);
  items[0]->set_state(ItemState::POPULATED_WITH_QUOTA);
  controller.NotifyMemoryItemsUsed(items);

  EXPECT_TRUE(file_runner_->HasPendingTask());
  EXPECT_TRUE(memory_task);
  EXPECT_EQ(kFirstMemorySize, controller.disk_usage());

  // Add our file item now.
  BlobDataBuilder file_builder(kFileId);
  file_builder.AppendFutureFile(0, kFileBlobSize, 0);

  std::vector<scoped_refptr<ShareableBlobDataItem>> file_items =
      CreateSharedDataItems(file_builder);

  base::WeakPtr<QuotaAllocationTask> file_task =
      controller.ReserveFileQuota(file_items, GetFileCreationCallback());
  EXPECT_TRUE(file_task);
  EXPECT_TRUE(file_runner_->HasPendingTask());

  // We should have both memory paging tasks and file paging tasks.
  EXPECT_EQ(kFirstMemorySize, controller.memory_usage());
  EXPECT_EQ(kFirstMemorySize + kFileBlobSize, controller.disk_usage());
  file_quota_result_ = true;
  memory_quota_result_ = true;

  files_created_.clear();

  // Disable paging! This should cancel all file-related tasks and leave us with
  // only the first memory item.
  controller.DisableFilePaging(base::File::FILE_ERROR_FAILED);
  EXPECT_FALSE(file_quota_result_);
  EXPECT_FALSE(memory_quota_result_);
  EXPECT_FALSE(file_task);
  EXPECT_FALSE(memory_task);

  file_items.clear();
  items.clear();

  RunFileThreadTasks();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0ull, controller.disk_usage());
  EXPECT_EQ(0ull, controller.memory_usage());
}
}  // namespace storage
