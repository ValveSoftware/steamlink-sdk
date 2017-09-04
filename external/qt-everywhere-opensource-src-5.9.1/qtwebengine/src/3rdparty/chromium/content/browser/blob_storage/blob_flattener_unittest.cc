// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_storage_context.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_data_item.h"
#include "storage/browser/blob/blob_entry.h"
#include "storage/browser/blob/blob_storage_registry.h"
#include "storage/browser/blob/shareable_blob_data_item.h"
#include "storage/common/data_element.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {
namespace {
using base::TestSimpleTaskRunner;

const char kType[] = "type";
const char kDisposition[] = "";

}  // namespace

class BlobFlattenerTest : public testing::Test {
 protected:
  using BlobFlattener = BlobStorageContext::BlobFlattener;

  BlobFlattenerTest()
      : fake_file_path_(base::FilePath(FILE_PATH_LITERAL("kFakePath"))) {}
  ~BlobFlattenerTest() override {}

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void TearDown() override {
    base::RunLoop().RunUntilIdle();
    file_runner_->RunPendingTasks();
    ASSERT_TRUE(temp_dir_.Delete());
  }

  scoped_refptr<BlobDataItem> CreateDataDescriptionItem(size_t size) {
    std::unique_ptr<DataElement> element(new DataElement());
    element->SetToBytesDescription(size);
    return scoped_refptr<BlobDataItem>(new BlobDataItem(std::move(element)));
  };

  scoped_refptr<BlobDataItem> CreateDataItem(const char* memory, size_t size) {
    std::unique_ptr<DataElement> element(new DataElement());
    element->SetToBytes(memory, size);
    return scoped_refptr<BlobDataItem>(new BlobDataItem(std::move(element)));
  };

  scoped_refptr<BlobDataItem> CreateFileItem(size_t offset, size_t size) {
    std::unique_ptr<DataElement> element(new DataElement());
    element->SetToFilePathRange(fake_file_path_, offset, size,
                                base::Time::Max());
    return scoped_refptr<BlobDataItem>(new BlobDataItem(std::move(element)));
  };

  std::unique_ptr<BlobDataHandle> SetupBasicBlob(const std::string& id) {
    BlobDataBuilder builder(id);
    builder.AppendData("1", 1);
    builder.set_content_type("text/plain");
    return context_.AddFinishedBlob(builder);
  }

  BlobStorageRegistry* registry() { return context_.mutable_registry(); }

  const ShareableBlobDataItem& GetItemInBlob(const std::string& uuid,
                                             size_t index) {
    BlobEntry* entry = registry()->GetEntry(uuid);
    EXPECT_TRUE(entry);
    return *entry->items()[index];
  }

  base::FilePath fake_file_path_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<TestSimpleTaskRunner> file_runner_ = new TestSimpleTaskRunner();

  base::MessageLoop fake_io_message_loop;
  BlobStorageContext context_;
};

TEST_F(BlobFlattenerTest, NoBlobItems) {
  const std::string kBlobUUID = "kId";

  BlobDataBuilder builder(kBlobUUID);
  builder.AppendData("hi", 2u);
  builder.AppendFile(fake_file_path_, 0u, 10u, base::Time::Max());
  BlobEntry output(kType, kDisposition);
  BlobFlattener flattener(builder, &output, registry());

  EXPECT_EQ(BlobStatus::PENDING_QUOTA, flattener.status);
  EXPECT_EQ(0u, flattener.dependent_blobs.size());
  EXPECT_EQ(0u, flattener.copies.size());
  EXPECT_EQ(12u, flattener.total_size);
  EXPECT_EQ(2u, flattener.memory_quota_needed);

  ASSERT_EQ(2u, output.items().size());
  EXPECT_EQ(*CreateDataItem("hi", 2u), *output.items()[0]->item());
  EXPECT_EQ(*CreateFileItem(0, 10u), *output.items()[1]->item());
}

TEST_F(BlobFlattenerTest, ErrorCases) {
  const std::string kBlobUUID = "kId";
  const std::string kBlob2UUID = "kId2";

  // Invalid blob reference.
  {
    BlobDataBuilder builder(kBlobUUID);
    builder.AppendBlob("doesnotexist");
    BlobEntry output(kType, kDisposition);
    BlobFlattener flattener(builder, &output, registry());
    EXPECT_EQ(BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS, flattener.status);
  }

  // Circular reference.
  {
    BlobDataBuilder builder(kBlobUUID);
    builder.AppendBlob(kBlobUUID);
    BlobEntry output(kType, kDisposition);
    BlobFlattener flattener(builder, &output, registry());
    EXPECT_EQ(BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS, flattener.status);
  }

  // Bad slice.
  {
    std::unique_ptr<BlobDataHandle> handle = SetupBasicBlob(kBlob2UUID);
    BlobDataBuilder builder(kBlobUUID);
    builder.AppendBlob(kBlob2UUID, 1, 2);
    BlobEntry output(kType, kDisposition);
    BlobFlattener flattener(builder, &output, registry());
    EXPECT_EQ(BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS, flattener.status);
  }
}

TEST_F(BlobFlattenerTest, BlobWithSlices) {
  const std::string kBlobUUID = "kId";
  const std::string kDataBlob = "kId2";
  const std::string kFileBlob = "kId3";

  // We have the following:
  // * data,
  // * sliced data blob,
  // * file
  // * full data blob,
  // * pending data,

  std::unique_ptr<BlobDataHandle> data_blob;
  {
    BlobDataBuilder builder(kDataBlob);
    builder.AppendData("12345", 5);
    builder.set_content_type("text/plain");
    data_blob = context_.AddFinishedBlob(builder);
  }

  std::unique_ptr<BlobDataHandle> file_blob;
  {
    BlobDataBuilder builder(kFileBlob);
    builder.AppendFile(fake_file_path_, 1u, 10u, base::Time::Max());
    file_blob = context_.AddFinishedBlob(builder);
  }

  BlobDataBuilder builder(kBlobUUID);
  builder.AppendData("hi", 2u);
  builder.AppendBlob(kDataBlob, 1u, 2u);
  builder.AppendFile(fake_file_path_, 3u, 5u, base::Time::Max());
  builder.AppendBlob(kDataBlob);
  builder.AppendBlob(kFileBlob, 1u, 3u);
  builder.AppendFutureData(12u);

  BlobEntry output(kType, kDisposition);
  BlobFlattener flattener(builder, &output, registry());
  EXPECT_EQ(BlobStatus::PENDING_QUOTA, flattener.status);

  EXPECT_EQ(2u, flattener.dependent_blobs.size());
  EXPECT_EQ(29u, flattener.total_size);
  EXPECT_EQ(16u, flattener.memory_quota_needed);

  ASSERT_EQ(6u, output.items().size());
  EXPECT_EQ(*CreateDataItem("hi", 2u), *output.items()[0]->item());
  EXPECT_EQ(*CreateDataDescriptionItem(2u), *output.items()[1]->item());
  EXPECT_EQ(*CreateFileItem(3u, 5u), *output.items()[2]->item());
  EXPECT_EQ(GetItemInBlob(kDataBlob, 0), *output.items()[3]);
  EXPECT_EQ(*CreateFileItem(2u, 3u), *output.items()[4]->item());
  EXPECT_EQ(*CreateDataDescriptionItem(12u), *output.items()[5]->item());

  // We're copying item at index 1
  ASSERT_EQ(1u, flattener.copies.size());
  EXPECT_EQ(*flattener.copies[0].dest_item, *output.items()[1]);
  EXPECT_EQ(GetItemInBlob(kDataBlob, 0), *flattener.copies[0].source_item);
  EXPECT_EQ(1u, flattener.copies[0].source_item_offset);
}

}  // namespace storage
