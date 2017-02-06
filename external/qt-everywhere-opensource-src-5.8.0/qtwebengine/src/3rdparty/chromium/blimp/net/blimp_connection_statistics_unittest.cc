// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "blimp/net/blimp_connection_statistics.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {

class BlimpConnectionStatisticsTest : public testing::Test {
 public:
  BlimpConnectionStatisticsTest() {}

  ~BlimpConnectionStatisticsTest() override {}

 protected:
  BlimpConnectionStatistics stats_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BlimpConnectionStatisticsTest);
};

TEST_F(BlimpConnectionStatisticsTest, AddStatsAndVerify) {
  DCHECK_EQ(0, stats_.Get(BlimpConnectionStatistics::BYTES_SENT));
  DCHECK_EQ(0, stats_.Get(BlimpConnectionStatistics::BYTES_RECEIVED));
  DCHECK_EQ(0, stats_.Get(BlimpConnectionStatistics::COMMIT));

  stats_.Add(BlimpConnectionStatistics::BYTES_SENT, 10);
  stats_.Add(BlimpConnectionStatistics::BYTES_SENT, 20);
  stats_.Add(BlimpConnectionStatistics::BYTES_RECEIVED, 5);
  stats_.Add(BlimpConnectionStatistics::COMMIT, 1);

  DCHECK_EQ(30, stats_.Get(BlimpConnectionStatistics::BYTES_SENT));
  DCHECK_EQ(5, stats_.Get(BlimpConnectionStatistics::BYTES_RECEIVED));
  DCHECK_EQ(1, stats_.Get(BlimpConnectionStatistics::COMMIT));
}

}  // namespace blimp
