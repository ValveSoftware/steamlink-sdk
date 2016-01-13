// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "net/quic/congestion_control/cube_root.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {

class CubeRootTest : public ::testing::Test {
 protected:
  CubeRootTest() {
  }
};

TEST_F(CubeRootTest, LowRoot) {
  for (uint32 i = 1; i < 256; ++i) {
    uint64 cube = i * i * i;
    uint8 cube_root = CubeRoot::Root(cube);
    EXPECT_EQ(i, cube_root);
  }
}

TEST_F(CubeRootTest, HighRoot) {
  // Test the range we will opperate in, 1300 to 130 000.
  // We expect some loss in accuracy, accepting +-0.2%.
  for (uint64 i = 1300; i < 20000; i += 100) {
    uint64 cube = i * i * i;
    uint32 cube_root = CubeRoot::Root(cube);
    uint32 margin = cube_root >> 9;  // Calculate 0.2% roughly by
                                     // dividing by 512.
    EXPECT_LE(i - margin, cube_root);
    EXPECT_GE(i + margin, cube_root);
  }
  for (uint64 i = 20000; i < 130000; i *= 2) {
    uint64 cube = i * i * i;
    uint32 cube_root = CubeRoot::Root(cube);
    uint32 margin = cube_root >> 9;
    EXPECT_LE(i - margin, cube_root);
    EXPECT_GE(i + margin, cube_root);
  }
}

}  // namespace test
}  // namespace net
