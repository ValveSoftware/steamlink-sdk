// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/blob_storage/blob_consolidation.h"

#include <stddef.h>

#include "base/files/file_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ReadStatus = content::BlobConsolidation::ReadStatus;
using storage::DataElement;

namespace content {
namespace {

static blink::WebThreadSafeData CreateData(const std::string& str) {
  return blink::WebThreadSafeData(str.c_str(), str.size());
}

TEST(BlobConsolidationTest, TestSegmentation) {
  scoped_refptr<BlobConsolidation> consolidation(new BlobConsolidation());
  consolidation->AddDataItem(CreateData("12345"));
  EXPECT_EQ(5lu, consolidation->total_memory());

  const auto& items = consolidation->consolidated_items();
  EXPECT_EQ(1u, items.size());
  EXPECT_EQ(5lu, items[0].length);
  EXPECT_EQ(DataElement::TYPE_BYTES, items[0].type);
  EXPECT_EQ(0lu, items[0].offset);

  char memory[] = {'E'};
  EXPECT_EQ(ReadStatus::OK, consolidation->ReadMemory(0, 0, 1, memory));
  EXPECT_EQ('1', memory[0]);
  EXPECT_EQ(ReadStatus::OK, consolidation->ReadMemory(0, 1, 1, memory));
  EXPECT_EQ('2', memory[0]);
  EXPECT_EQ(ReadStatus::OK, consolidation->ReadMemory(0, 2, 1, memory));
  EXPECT_EQ('3', memory[0]);
  EXPECT_EQ(ReadStatus::OK, consolidation->ReadMemory(0, 3, 1, memory));
  EXPECT_EQ('4', memory[0]);
  EXPECT_EQ(ReadStatus::OK, consolidation->ReadMemory(0, 4, 1, memory));
  EXPECT_EQ('5', memory[0]);
}

TEST(BlobConsolidationTest, TestConsolidation) {
  scoped_refptr<BlobConsolidation> consolidation(new BlobConsolidation());
  consolidation->AddDataItem(CreateData("1"));
  consolidation->AddDataItem(CreateData("23"));
  consolidation->AddDataItem(CreateData("4"));
  EXPECT_EQ(4u, consolidation->total_memory());

  const auto& items = consolidation->consolidated_items();
  EXPECT_EQ(1u, items.size());
  EXPECT_EQ(4lu, items[0].length);
  EXPECT_EQ(DataElement::TYPE_BYTES, items[0].type);
  EXPECT_EQ(0lu, items[0].offset);

  char memory[4];
  EXPECT_EQ(ReadStatus::ERROR_OUT_OF_BOUNDS,
            consolidation->ReadMemory(0, 0, 5, memory));
  EXPECT_EQ(ReadStatus::ERROR_OUT_OF_BOUNDS,
            consolidation->ReadMemory(1, 0, 4, memory));
  EXPECT_EQ(ReadStatus::ERROR_OUT_OF_BOUNDS,
            consolidation->ReadMemory(0, 1, 4, memory));
  EXPECT_EQ(ReadStatus::OK, consolidation->ReadMemory(0, 0, 4, memory));

  char expected_memory[] = {'1', '2', '3', '4'};
  EXPECT_THAT(memory, testing::ElementsAreArray(expected_memory));
}

TEST(BlobConsolidationTest, TestMassiveConsolidation) {
  scoped_refptr<BlobConsolidation> consolidation(new BlobConsolidation());
  const size_t kNumParts = 300;
  const size_t kPartSize = 5;
  const size_t kTotalMemory = kNumParts * kPartSize;
  const size_t kReadSegmentSize = 6;  // Must be factor of kTotalMemory
  const size_t kNumReadSegments = kTotalMemory / kReadSegmentSize;

  char current_value = 0;
  for (size_t i = 0; i < kNumParts; i++) {
    char data[kPartSize];
    for (size_t j = 0; j < kPartSize; j++) {
      data[j] = current_value;
      ++current_value;
    }
    consolidation->AddDataItem(blink::WebThreadSafeData(data, kPartSize));
  }
  EXPECT_EQ(kTotalMemory, consolidation->total_memory());

  const auto& items = consolidation->consolidated_items();
  EXPECT_EQ(1u, items.size());
  EXPECT_EQ(kTotalMemory, items[0].length);

  char expected_value = 0;
  char read_buffer[kReadSegmentSize];
  for (size_t i = 0; i < kNumReadSegments; i++) {
    EXPECT_EQ(ReadStatus::OK,
              consolidation->ReadMemory(0, i * kReadSegmentSize,
                                        kReadSegmentSize, read_buffer));
    for (size_t j = 0; j < kReadSegmentSize; j++) {
      EXPECT_EQ(expected_value, read_buffer[j]);
      ++expected_value;
    }
  }
}

TEST(BlobConsolidationTest, TestPartialRead) {
  scoped_refptr<BlobConsolidation> consolidation(new BlobConsolidation());
  consolidation->AddDataItem(CreateData("1"));
  consolidation->AddDataItem(CreateData("23"));
  consolidation->AddDataItem(CreateData("45"));
  EXPECT_EQ(5u, consolidation->total_memory());

  const auto& items = consolidation->consolidated_items();
  EXPECT_EQ(1u, items.size());
  EXPECT_EQ(5lu, items[0].length);
  EXPECT_EQ(0lu, items[0].offset);

  char memory_part1[] = {'X', 'X'};
  EXPECT_EQ(ReadStatus::OK, consolidation->ReadMemory(0, 0, 2, memory_part1));
  char expected_memory_part1[] = {'1', '2'};
  EXPECT_THAT(memory_part1, testing::ElementsAreArray(expected_memory_part1));

  char memory_part2[] = {'X', 'X'};
  EXPECT_EQ(ReadStatus::OK, consolidation->ReadMemory(0, 2, 2, memory_part2));
  char expected_memory_part2[] = {'3', '4'};
  EXPECT_THAT(memory_part2, testing::ElementsAreArray(expected_memory_part2));

  char memory_part3[] = {'X'};
  EXPECT_EQ(ReadStatus::OK, consolidation->ReadMemory(0, 4, 1, memory_part3));
  char expected_memory_part3[] = {'5'};
  EXPECT_THAT(memory_part3, testing::ElementsAreArray(expected_memory_part3));
}

TEST(BlobConsolidationTest, TestBoundaries) {
  scoped_refptr<BlobConsolidation> consolidation(new BlobConsolidation());
  consolidation->AddDataItem(CreateData("1"));
  consolidation->AddFileItem(base::FilePath(FILE_PATH_LITERAL("testPath")), 1,
                             10, 5.0);
  consolidation->AddDataItem(CreateData("2"));
  consolidation->AddDataItem(CreateData("3"));
  consolidation->AddBlobItem("testUUID", 1, 2);
  consolidation->AddDataItem(CreateData("45"));
  EXPECT_EQ(5u, consolidation->total_memory());

  const auto& items = consolidation->consolidated_items();
  EXPECT_EQ(5u, items.size());

  EXPECT_EQ(1lu, items[0].length);
  EXPECT_EQ(DataElement::TYPE_BYTES, items[0].type);

  EXPECT_EQ(10lu, items[1].length);
  EXPECT_EQ(DataElement::TYPE_FILE, items[1].type);

  EXPECT_EQ(2lu, items[2].length);
  EXPECT_EQ(DataElement::TYPE_BYTES, items[2].type);

  EXPECT_EQ(2lu, items[3].length);
  EXPECT_EQ(DataElement::TYPE_BLOB, items[3].type);

  EXPECT_EQ(2lu, items[4].length);
  EXPECT_EQ(DataElement::TYPE_BYTES, items[4].type);

  char test_memory[5];
  EXPECT_EQ(ReadStatus::ERROR_WRONG_TYPE,
            consolidation->ReadMemory(1, 0, 1, test_memory));
  EXPECT_EQ(ReadStatus::ERROR_WRONG_TYPE,
            consolidation->ReadMemory(3, 0, 1, test_memory));

  char memory_part1[1];
  EXPECT_EQ(ReadStatus::OK, consolidation->ReadMemory(0, 0, 1, memory_part1));
  char expected_memory_part1[] = {'1'};
  EXPECT_THAT(memory_part1, testing::ElementsAreArray(expected_memory_part1));

  char memory_part2[2];
  EXPECT_EQ(ReadStatus::OK, consolidation->ReadMemory(2, 0, 2, memory_part2));
  char expected_memory_part2[] = {'2', '3'};
  EXPECT_THAT(memory_part2, testing::ElementsAreArray(expected_memory_part2));

  char memory_part3[2];
  EXPECT_EQ(ReadStatus::OK, consolidation->ReadMemory(4, 0, 2, memory_part3));
  char expected_memory_part3[] = {'4', '5'};
  EXPECT_THAT(memory_part3, testing::ElementsAreArray(expected_memory_part3));
}

}  // namespace
}  // namespace content
