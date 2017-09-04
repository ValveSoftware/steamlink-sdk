// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "base/at_exit.h"
#include "base/macros.h"
#include "blimp/net/blimp_stats.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {

class BlimpStatsTest : public testing::Test {
 public:
  BlimpStatsTest() {}
  ~BlimpStatsTest() override {}

 private:
  base::ShadowingAtExitManager at_exit_manager_;

  DISALLOW_COPY_AND_ASSIGN(BlimpStatsTest);
};

TEST_F(BlimpStatsTest, AddStatsAndVerify) {
  EXPECT_EQ(0, BlimpStats::GetInstance()->Get(BlimpStats::BYTES_SENT));
  EXPECT_EQ(0, BlimpStats::GetInstance()->Get(BlimpStats::BYTES_RECEIVED));
  EXPECT_EQ(0, BlimpStats::GetInstance()->Get(BlimpStats::COMMIT));

  BlimpStats::GetInstance()->Add(BlimpStats::BYTES_SENT, 10);
  BlimpStats::GetInstance()->Add(BlimpStats::BYTES_SENT, 20);
  BlimpStats::GetInstance()->Add(BlimpStats::BYTES_RECEIVED, 5);
  BlimpStats::GetInstance()->Add(BlimpStats::COMMIT, 1);

  EXPECT_EQ(30, BlimpStats::GetInstance()->Get(BlimpStats::BYTES_SENT));
  EXPECT_EQ(5, BlimpStats::GetInstance()->Get(BlimpStats::BYTES_RECEIVED));
  EXPECT_EQ(1, BlimpStats::GetInstance()->Get(BlimpStats::COMMIT));
}

}  // namespace blimp
