// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "media/blink/buffered_data_source_host_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class BufferedDataSourceHostImplTest : public testing::Test {
 public:
  BufferedDataSourceHostImplTest() {}

  void Add() {
    host_.AddBufferedTimeRanges(&ranges_, base::TimeDelta::FromSeconds(10));
  }

 protected:
  BufferedDataSourceHostImpl host_;
  Ranges<base::TimeDelta> ranges_;

  DISALLOW_COPY_AND_ASSIGN(BufferedDataSourceHostImplTest);
};

TEST_F(BufferedDataSourceHostImplTest, Empty) {
  EXPECT_FALSE(host_.DidLoadingProgress());
  Add();
  EXPECT_EQ(0u, ranges_.size());
}

TEST_F(BufferedDataSourceHostImplTest, AddBufferedTimeRanges) {
  host_.AddBufferedByteRange(10, 20);
  host_.SetTotalBytes(100);
  Add();
  EXPECT_EQ(1u, ranges_.size());
  EXPECT_EQ(base::TimeDelta::FromSeconds(1), ranges_.start(0));
  EXPECT_EQ(base::TimeDelta::FromSeconds(2), ranges_.end(0));
}

TEST_F(BufferedDataSourceHostImplTest, AddBufferedTimeRanges_Merges) {
  ranges_.Add(base::TimeDelta::FromSeconds(0), base::TimeDelta::FromSeconds(1));
  host_.AddBufferedByteRange(10, 20);
  host_.SetTotalBytes(100);
  Add();
  EXPECT_EQ(1u, ranges_.size());
  EXPECT_EQ(base::TimeDelta::FromSeconds(0), ranges_.start(0));
  EXPECT_EQ(base::TimeDelta::FromSeconds(2), ranges_.end(0));
}

TEST_F(BufferedDataSourceHostImplTest, AddBufferedTimeRanges_Snaps) {
  host_.AddBufferedByteRange(5, 995);
  host_.SetTotalBytes(1000);
  Add();
  EXPECT_EQ(1u, ranges_.size());
  EXPECT_EQ(base::TimeDelta::FromSeconds(0), ranges_.start(0));
  EXPECT_EQ(base::TimeDelta::FromSeconds(10), ranges_.end(0));
}

TEST_F(BufferedDataSourceHostImplTest, SetTotalBytes) {
  host_.AddBufferedByteRange(10, 20);
  Add();
  EXPECT_EQ(0u, ranges_.size());

  host_.SetTotalBytes(100);
  Add();
  EXPECT_EQ(1u, ranges_.size());
}

TEST_F(BufferedDataSourceHostImplTest, DidLoadingProgress) {
  host_.AddBufferedByteRange(10, 20);
  EXPECT_TRUE(host_.DidLoadingProgress());
  EXPECT_FALSE(host_.DidLoadingProgress());
}

}  // namespace media
