// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_storage_context.h"

#include <memory>

#include "base/files/file_path.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_item.h"
#include "storage/browser/blob/blob_entry.h"
#include "storage/browser/blob/shareable_blob_data_item.h"
#include "storage/common/data_element.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {
namespace {
const char kType[] = "type";
const char kDisposition[] = "";
}  // namespace

class BlobSliceTest : public testing::Test {
 protected:
  using BlobSlice = BlobStorageContext::BlobSlice;

  BlobSliceTest() {}
  ~BlobSliceTest() override {}

  scoped_refptr<ShareableBlobDataItem> CreateDataItem(size_t size) {
    std::unique_ptr<DataElement> element(new DataElement());
    element->SetToAllocatedBytes(size);
    for (size_t i = 0; i < size; i++) {
      *(element->mutable_bytes() + i) = i;
    }
    return scoped_refptr<ShareableBlobDataItem>(
        new ShareableBlobDataItem(new BlobDataItem(std::move(element)),
                                  ShareableBlobDataItem::QUOTA_NEEDED));
  };

  scoped_refptr<ShareableBlobDataItem> CreateFileItem(size_t offset,
                                                      size_t size) {
    std::unique_ptr<DataElement> element(new DataElement());
    element->SetToFilePathRange(base::FilePath(FILE_PATH_LITERAL("kFakePath")),
                                offset, size, base::Time::Max());
    return scoped_refptr<ShareableBlobDataItem>(new ShareableBlobDataItem(
        new BlobDataItem(std::move(element)),
        ShareableBlobDataItem::POPULATED_WITHOUT_QUOTA));
  };

  void ExpectFirstSlice(const BlobSlice& slice,
                        scoped_refptr<ShareableBlobDataItem> source_item,
                        size_t first_item_slice_offset,
                        size_t size) {
    EXPECT_TRUE(slice.first_source_item);
    EXPECT_EQ(first_item_slice_offset, slice.first_item_slice_offset);

    ASSERT_LE(1u, slice.dest_items.size());
    const DataElement& dest_element =
        slice.dest_items[0]->item()->data_element();

    EXPECT_EQ(DataElement::TYPE_BYTES_DESCRIPTION, dest_element.type());
    EXPECT_EQ(static_cast<uint64_t>(size), dest_element.length());

    EXPECT_EQ(*source_item, *slice.first_source_item);
  }

  void ExpectLastSlice(const BlobSlice& slice,
                       scoped_refptr<ShareableBlobDataItem> source_item,
                       size_t size) {
    EXPECT_TRUE(slice.last_source_item);

    ASSERT_LE(2u, slice.dest_items.size());
    const DataElement& dest_element =
        slice.dest_items.back()->item()->data_element();

    EXPECT_EQ(DataElement::TYPE_BYTES_DESCRIPTION, dest_element.type());
    EXPECT_EQ(static_cast<uint64_t>(size), dest_element.length());

    EXPECT_EQ(*source_item, *slice.last_source_item);
  }
};

TEST_F(BlobSliceTest, FullItem) {
  const std::string kBlobUUID = "kId";
  const size_t kSize = 5u;

  BlobEntry data(kType, kDisposition);
  scoped_refptr<ShareableBlobDataItem> item = CreateDataItem(kSize);
  data.AppendSharedBlobItem(item);

  BlobSlice slice(data, 0, 5);
  EXPECT_EQ(0u, slice.copying_memory_size.ValueOrDie());
  EXPECT_FALSE(slice.first_source_item);
  EXPECT_FALSE(slice.last_source_item);
  EXPECT_FALSE(slice.first_source_item);
  EXPECT_FALSE(slice.last_source_item);
  ASSERT_EQ(1u, slice.dest_items.size());
  EXPECT_EQ(item, slice.dest_items[0]);
}

TEST_F(BlobSliceTest, SliceSingleItem) {
  const std::string kBlobUUID = "kId";
  const size_t kSize = 5u;

  BlobEntry data(kType, kDisposition);
  scoped_refptr<ShareableBlobDataItem> item = CreateDataItem(kSize);
  data.AppendSharedBlobItem(item);

  BlobSlice slice(data, 1, 3);
  EXPECT_EQ(3u, slice.copying_memory_size.ValueOrDie());
  EXPECT_FALSE(slice.last_source_item);
  ExpectFirstSlice(slice, item, 1, 3);
  ASSERT_EQ(1u, slice.dest_items.size());
}

TEST_F(BlobSliceTest, SliceSingleLastItem) {
  const std::string kBlobUUID = "kId";
  const size_t kSize1 = 5u;
  const size_t kSize2 = 10u;

  BlobEntry data(kType, kDisposition);
  scoped_refptr<ShareableBlobDataItem> item1 = CreateDataItem(kSize1);
  scoped_refptr<ShareableBlobDataItem> item2 = CreateDataItem(kSize2);
  data.AppendSharedBlobItem(item1);
  data.AppendSharedBlobItem(item2);

  BlobSlice slice(data, 6, 2);
  EXPECT_EQ(2u, slice.copying_memory_size.ValueOrDie());
  ExpectFirstSlice(slice, item2, 1, 2);
  ASSERT_EQ(1u, slice.dest_items.size());
}

TEST_F(BlobSliceTest, SliceAcrossTwoItems) {
  const std::string kBlobUUID = "kId";
  const size_t kSize1 = 5u;
  const size_t kSize2 = 10u;

  BlobEntry data(kType, kDisposition);
  scoped_refptr<ShareableBlobDataItem> item1 = CreateDataItem(kSize1);
  scoped_refptr<ShareableBlobDataItem> item2 = CreateDataItem(kSize2);
  data.AppendSharedBlobItem(item1);
  data.AppendSharedBlobItem(item2);

  BlobSlice slice(data, 4, 10);
  EXPECT_EQ(10u, slice.copying_memory_size.ValueOrDie());
  ExpectFirstSlice(slice, item1, 4, 1);
  ExpectLastSlice(slice, item2, 9);
  ASSERT_EQ(2u, slice.dest_items.size());
}

TEST_F(BlobSliceTest, SliceFileAndLastItem) {
  const std::string kBlobUUID = "kId";
  const size_t kSize1 = 5u;
  const size_t kSize2 = 10u;

  BlobEntry data(kType, kDisposition);
  scoped_refptr<ShareableBlobDataItem> item1 = CreateFileItem(0u, kSize1);
  scoped_refptr<ShareableBlobDataItem> item2 = CreateDataItem(kSize2);
  data.AppendSharedBlobItem(item1);
  data.AppendSharedBlobItem(item2);

  BlobSlice slice(data, 4, 2);
  EXPECT_EQ(1u, slice.copying_memory_size.ValueOrDie());
  EXPECT_FALSE(slice.first_source_item);
  ExpectLastSlice(slice, item2, 1);
  ASSERT_EQ(2u, slice.dest_items.size());

  EXPECT_EQ(*CreateFileItem(4u, 1u)->item(), *slice.dest_items[0]->item());
}

TEST_F(BlobSliceTest, SliceAcrossLargeItem) {
  const std::string kBlobUUID = "kId";
  const size_t kSize1 = 5u;
  const size_t kSize2 = 10u;
  const size_t kSize3 = 10u;

  BlobEntry data(kType, kDisposition);
  scoped_refptr<ShareableBlobDataItem> item1 = CreateDataItem(kSize1);
  scoped_refptr<ShareableBlobDataItem> item2 = CreateFileItem(0u, kSize2);
  scoped_refptr<ShareableBlobDataItem> item3 = CreateDataItem(kSize3);
  data.AppendSharedBlobItem(item1);
  data.AppendSharedBlobItem(item2);
  data.AppendSharedBlobItem(item3);

  BlobSlice slice(data, 2, 20);
  EXPECT_EQ(3u + 7u, slice.copying_memory_size.ValueOrDie());
  ExpectFirstSlice(slice, item1, 2, 3);
  ExpectLastSlice(slice, item3, 7);
  ASSERT_EQ(3u, slice.dest_items.size());

  EXPECT_EQ(*item2, *slice.dest_items[1]);
}

}  // namespace storage
