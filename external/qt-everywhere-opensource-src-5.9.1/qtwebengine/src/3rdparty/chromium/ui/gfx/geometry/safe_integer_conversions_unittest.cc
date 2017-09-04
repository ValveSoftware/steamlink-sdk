// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/safe_integer_conversions.h"

#include <limits>

#include "testing/gtest/include/gtest/gtest.h"

namespace gfx {

TEST(SafeIntegerConversions, ToFlooredInt) {
  float max = std::numeric_limits<int>::max();
  float min = std::numeric_limits<int>::min();
  float infinity = std::numeric_limits<float>::infinity();

  int int_max = std::numeric_limits<int>::max();
  int int_min = std::numeric_limits<int>::min();

  EXPECT_EQ(int_max, ToFlooredInt(infinity));
  EXPECT_EQ(int_max, ToFlooredInt(max));
  EXPECT_EQ(int_max, ToFlooredInt(max + 100));

  EXPECT_EQ(-101, ToFlooredInt(-100.5f));
  EXPECT_EQ(0, ToFlooredInt(0.f));
  EXPECT_EQ(100, ToFlooredInt(100.5f));

  EXPECT_EQ(int_min, ToFlooredInt(-infinity));
  EXPECT_EQ(int_min, ToFlooredInt(min));
  EXPECT_EQ(int_min, ToFlooredInt(min - 100));
}

TEST(SafeIntegerConversions, ToCeiledInt) {
  float max = std::numeric_limits<int>::max();
  float min = std::numeric_limits<int>::min();
  float infinity = std::numeric_limits<float>::infinity();

  int int_max = std::numeric_limits<int>::max();
  int int_min = std::numeric_limits<int>::min();

  EXPECT_EQ(int_max, ToCeiledInt(infinity));
  EXPECT_EQ(int_max, ToCeiledInt(max));
  EXPECT_EQ(int_max, ToCeiledInt(max + 100));

  EXPECT_EQ(-100, ToCeiledInt(-100.5f));
  EXPECT_EQ(0, ToCeiledInt(0.f));
  EXPECT_EQ(101, ToCeiledInt(100.5f));

  EXPECT_EQ(int_min, ToCeiledInt(-infinity));
  EXPECT_EQ(int_min, ToCeiledInt(min));
  EXPECT_EQ(int_min, ToCeiledInt(min - 100));
}

TEST(SafeIntegerConversions, ToRoundedInt) {
  float max = std::numeric_limits<int>::max();
  float min = std::numeric_limits<int>::min();
  float infinity = std::numeric_limits<float>::infinity();

  int int_max = std::numeric_limits<int>::max();
  int int_min = std::numeric_limits<int>::min();

  EXPECT_EQ(int_max, ToRoundedInt(infinity));
  EXPECT_EQ(int_max, ToRoundedInt(max));
  EXPECT_EQ(int_max, ToRoundedInt(max + 100));

  EXPECT_EQ(-100, ToRoundedInt(-100.1f));
  EXPECT_EQ(-101, ToRoundedInt(-100.5f));
  EXPECT_EQ(-101, ToRoundedInt(-100.9f));
  EXPECT_EQ(0, ToRoundedInt(0.f));
  EXPECT_EQ(100, ToRoundedInt(100.1f));
  EXPECT_EQ(101, ToRoundedInt(100.5f));
  EXPECT_EQ(101, ToRoundedInt(100.9f));

  EXPECT_EQ(int_min, ToRoundedInt(-infinity));
  EXPECT_EQ(int_min, ToRoundedInt(min));
  EXPECT_EQ(int_min, ToRoundedInt(min - 100));
}

TEST(SafeIntegerConversions, IntegerOverflow) {
  int int_max = std::numeric_limits<int>::max();
  int int_min = std::numeric_limits<int>::min();

  EXPECT_TRUE(AddWouldOverflow(int_max, 1));
  EXPECT_TRUE(AddWouldOverflow(int_max, int_max));
  EXPECT_TRUE(AddWouldOverflow(int_max - 1000, 1001));
  EXPECT_FALSE(AddWouldOverflow(int_max, 0));
  EXPECT_FALSE(AddWouldOverflow(1, 2));
  EXPECT_FALSE(AddWouldOverflow(1, -1));
  EXPECT_FALSE(AddWouldOverflow(int_min, int_max));
  EXPECT_FALSE(AddWouldOverflow(int_max - 1000, 1000));

  EXPECT_TRUE(AddWouldUnderflow(int_min, -1));
  EXPECT_TRUE(AddWouldUnderflow(int_min, int_min));
  EXPECT_TRUE(AddWouldUnderflow(int_min + 1000, -1001));
  EXPECT_FALSE(AddWouldUnderflow(int_min, 0));
  EXPECT_FALSE(AddWouldUnderflow(1, 2));
  EXPECT_FALSE(AddWouldUnderflow(1, -1));
  EXPECT_FALSE(AddWouldUnderflow(int_min, int_max));
  EXPECT_FALSE(AddWouldUnderflow(int_min + 1000, -1000));

  EXPECT_TRUE(SubtractWouldOverflow(int_max, -1));
  EXPECT_TRUE(SubtractWouldOverflow(int_max, int_min));
  EXPECT_TRUE(SubtractWouldOverflow(int_max - 1000, -1001));
  EXPECT_TRUE(SubtractWouldOverflow(0, int_min));
  EXPECT_FALSE(SubtractWouldOverflow(int_max, 0));
  EXPECT_FALSE(SubtractWouldOverflow(1, 2));
  EXPECT_FALSE(SubtractWouldOverflow(-1, 1));
  EXPECT_FALSE(SubtractWouldOverflow(int_min, int_min));
  EXPECT_FALSE(SubtractWouldOverflow(int_max - 1000, -1000));
  EXPECT_FALSE(SubtractWouldOverflow(-1, int_min));

  EXPECT_TRUE(SubtractWouldUnderflow(int_min, 1));
  EXPECT_TRUE(SubtractWouldUnderflow(int_min, int_max));
  EXPECT_TRUE(SubtractWouldUnderflow(int_min + 1000, 1001));
  EXPECT_FALSE(SubtractWouldUnderflow(int_min, 0));
  EXPECT_FALSE(SubtractWouldUnderflow(1, 2));
  EXPECT_FALSE(SubtractWouldUnderflow(-1, -1));
  EXPECT_FALSE(SubtractWouldUnderflow(int_max, int_max));
  EXPECT_FALSE(SubtractWouldUnderflow(int_min + 1000, -1000));

  EXPECT_EQ(SafeAdd(0, 0), 0);
  EXPECT_EQ(SafeAdd(1, 2), 3);
  EXPECT_EQ(SafeAdd(int_max, 0), int_max);
  EXPECT_EQ(SafeAdd(int_max, 1), int_max);
  EXPECT_EQ(SafeAdd(int_max, int_max), int_max);
  EXPECT_EQ(SafeAdd(int_max, int_min), -1);
  EXPECT_EQ(SafeAdd(int_min, 1), int_min + 1);
  EXPECT_EQ(SafeAdd(int_min, -1), int_min);
  EXPECT_EQ(SafeAdd(int_min, 0), int_min);
  EXPECT_EQ(SafeAdd(int_min, int_min), int_min);

  EXPECT_EQ(SafeSubtract(0, 0), 0);
  EXPECT_EQ(SafeSubtract(3, 2), 1);
  EXPECT_EQ(SafeSubtract(int_max, 0), int_max);
  EXPECT_EQ(SafeSubtract(int_max, 1), int_max - 1);
  EXPECT_EQ(SafeSubtract(int_max, -1), int_max);
  EXPECT_EQ(SafeSubtract(int_max, int_min), int_max);
  EXPECT_EQ(SafeSubtract(int_min, 0), int_min);
  EXPECT_EQ(SafeSubtract(int_min, -1), int_min + 1);
  EXPECT_EQ(SafeSubtract(int_min, 1), int_min);
  EXPECT_EQ(SafeSubtract(int_min, int_min), 0);
  EXPECT_EQ(SafeSubtract(int_max, int_max), 0);
  EXPECT_EQ(SafeSubtract(0, int_min), int_max);
  EXPECT_EQ(SafeSubtract(-1, int_min), int_max);
}

}  // namespace gfx
