// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/blob/BlobData.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

TEST(BlobDataTest, Consolidation) {
  const size_t kMaxConsolidatedItemSizeInBytes = 15 * 1024;
  BlobData data(BlobData::FileCompositionStatus::NO_UNKNOWN_SIZE_FILES);
  const char* text1 = "abc";
  const char* text2 = "def";
  data.appendBytes(text1, 3u);
  data.appendBytes(text2, 3u);
  data.appendText("ps1", false);
  data.appendText("ps2", false);

  EXPECT_EQ(1u, data.m_items.size());
  EXPECT_EQ(12u, data.m_items[0].data->length());
  EXPECT_EQ(0, memcmp(data.m_items[0].data->data(), "abcdefps1ps2", 12));

  std::unique_ptr<char[]> large_data =
      wrapArrayUnique(new char[kMaxConsolidatedItemSizeInBytes]);
  data.appendBytes(large_data.get(), kMaxConsolidatedItemSizeInBytes);

  EXPECT_EQ(2u, data.m_items.size());
  EXPECT_EQ(12u, data.m_items[0].data->length());
  EXPECT_EQ(kMaxConsolidatedItemSizeInBytes, data.m_items[1].data->length());
}

}  // namespace blink
