// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_data_builder.h"

#include <string>

#include "base/logging.h"
#include "storage/common/data_element.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {

TEST(BlobDataBuilderTest, TestFutureFiles) {
  const std::string kId = "id";

  DataElement element;
  element.SetToFilePath(BlobDataBuilder::GetFutureFileItemPath(0));
  EXPECT_TRUE(BlobDataBuilder::IsFutureFileItem(element));
  EXPECT_EQ(0ull, BlobDataBuilder::GetFutureFileID(element));

  BlobDataBuilder builder(kId);
  builder.AppendFutureFile(0, 10, 0);
  EXPECT_TRUE(
      BlobDataBuilder::IsFutureFileItem(builder.items_[0]->data_element()));
  EXPECT_EQ(0ull, BlobDataBuilder::GetFutureFileID(
                      builder.items_[0]->data_element()));
}

}  // namespace storage
