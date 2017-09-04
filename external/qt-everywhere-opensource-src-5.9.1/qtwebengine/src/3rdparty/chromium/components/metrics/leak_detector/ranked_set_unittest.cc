// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/leak_detector/ranked_set.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "base/macros.h"
#include "components/metrics/leak_detector/custom_allocator.h"
#include "components/metrics/leak_detector/leak_detector_value_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {
namespace leak_detector {

namespace {

// Makes it easier to instantiate LeakDetectorValueTypes.
LeakDetectorValueType Value(uint32_t value) {
  return LeakDetectorValueType(value);
}

}  // namespace

class RankedSetTest : public ::testing::Test {
 public:
  RankedSetTest() {}

  void SetUp() override { CustomAllocator::Initialize(); }
  void TearDown() override { EXPECT_TRUE(CustomAllocator::Shutdown()); }

 private:
  DISALLOW_COPY_AND_ASSIGN(RankedSetTest);
};

TEST_F(RankedSetTest, Iterators) {
  RankedSet set(10);
  EXPECT_TRUE(set.begin() == set.end());

  set.Add(Value(0x1234), 100);
  EXPECT_FALSE(set.begin() == set.end());
}

TEST_F(RankedSetTest, SingleInsertion) {
  RankedSet set(10);
  EXPECT_EQ(0U, set.size());

  set.Add(Value(0x1234), 100);
  EXPECT_EQ(1U, set.size());

  auto iter = set.begin();
  EXPECT_EQ(0x1234U, iter->value.size());
  EXPECT_EQ(100, iter->count);
}

TEST_F(RankedSetTest, InOrderInsertion) {
  RankedSet set(10);
  EXPECT_EQ(0U, set.size());

  set.Add(Value(0x1234), 100);
  EXPECT_EQ(1U, set.size());
  set.Add(Value(0x2345), 95);
  EXPECT_EQ(2U, set.size());
  set.Add(Value(0x3456), 90);
  EXPECT_EQ(3U, set.size());
  set.Add(Value(0x4567), 85);
  EXPECT_EQ(4U, set.size());
  set.Add(Value(0x5678), 80);
  EXPECT_EQ(5U, set.size());

  // Iterate through the contents to make sure they match what went in.
  const RankedSet::Entry kExpectedValues[] = {
      {Value(0x1234), 100}, {Value(0x2345), 95}, {Value(0x3456), 90},
      {Value(0x4567), 85},  {Value(0x5678), 80},
  };

  size_t index = 0;
  for (const auto& entry : set) {
    EXPECT_LT(index, arraysize(kExpectedValues));
    EXPECT_EQ(kExpectedValues[index].value.size(), entry.value.size());
    EXPECT_EQ(kExpectedValues[index].count, entry.count);
    ++index;
  }
}

TEST_F(RankedSetTest, ReverseOrderInsertion) {
  RankedSet set(10);
  EXPECT_EQ(0U, set.size());

  set.Add(Value(0x1234), 0);
  EXPECT_EQ(1U, set.size());
  set.Add(Value(0x2345), 5);
  EXPECT_EQ(2U, set.size());
  set.Add(Value(0x3456), 10);
  EXPECT_EQ(3U, set.size());
  set.Add(Value(0x4567), 15);
  EXPECT_EQ(4U, set.size());
  set.Add(Value(0x5678), 20);
  EXPECT_EQ(5U, set.size());

  // Iterate through the contents to make sure they match what went in.
  const RankedSet::Entry kExpectedValues[] = {
      {Value(0x5678), 20}, {Value(0x4567), 15}, {Value(0x3456), 10},
      {Value(0x2345), 5},  {Value(0x1234), 0},
  };

  size_t index = 0;
  for (const auto& entry : set) {
    EXPECT_LT(index, arraysize(kExpectedValues));
    EXPECT_EQ(kExpectedValues[index].value.size(), entry.value.size());
    EXPECT_EQ(kExpectedValues[index].count, entry.count);
    ++index;
  }
}

TEST_F(RankedSetTest, UnorderedInsertion) {
  RankedSet set(10);
  EXPECT_EQ(0U, set.size());

  set.Add(Value(0x1234), 15);
  set.Add(Value(0x2345), 20);
  set.Add(Value(0x3456), 10);
  set.Add(Value(0x4567), 30);
  set.Add(Value(0x5678), 25);
  EXPECT_EQ(5U, set.size());

  // Iterate through the contents to make sure they match what went in.
  const RankedSet::Entry kExpectedValues1[] = {
      {Value(0x4567), 30}, {Value(0x5678), 25}, {Value(0x2345), 20},
      {Value(0x1234), 15}, {Value(0x3456), 10},
  };

  size_t index = 0;
  for (const auto& entry : set) {
    EXPECT_LT(index, arraysize(kExpectedValues1));
    EXPECT_EQ(kExpectedValues1[index].value.size(), entry.value.size());
    EXPECT_EQ(kExpectedValues1[index].count, entry.count);
    ++index;
  }

  // Add more items.
  set.Add(Value(0x6789), 35);
  set.Add(Value(0x789a), 40);
  set.Add(Value(0x89ab), 50);
  set.Add(Value(0x9abc), 5);
  set.Add(Value(0xabcd), 0);
  EXPECT_EQ(10U, set.size());

  // Iterate through the contents to make sure they match what went in.
  const RankedSet::Entry kExpectedValues2[] = {
      {Value(0x89ab), 50}, {Value(0x789a), 40}, {Value(0x6789), 35},
      {Value(0x4567), 30}, {Value(0x5678), 25}, {Value(0x2345), 20},
      {Value(0x1234), 15}, {Value(0x3456), 10}, {Value(0x9abc), 5},
      {Value(0xabcd), 0},
  };

  index = 0;
  for (const auto& entry : set) {
    EXPECT_LT(index, arraysize(kExpectedValues2));
    EXPECT_EQ(kExpectedValues2[index].value.size(), entry.value.size());
    EXPECT_EQ(kExpectedValues2[index].count, entry.count);
    ++index;
  }
}

TEST_F(RankedSetTest, UnorderedInsertionWithSameCounts) {
  RankedSet set(10);
  EXPECT_EQ(0U, set.size());

  set.Add(Value(0x1234), 20);
  set.Add(Value(0x2345), 20);
  set.Add(Value(0x3456), 30);
  set.Add(Value(0x4567), 30);
  set.Add(Value(0x5678), 20);
  EXPECT_EQ(5U, set.size());

  // Iterate through the contents to make sure they match what went in. Entries
  // with the same count should be ordered from lowest value to highest value.
  const RankedSet::Entry kExpectedValues1[] = {
      {Value(0x3456), 30}, {Value(0x4567), 30}, {Value(0x1234), 20},
      {Value(0x2345), 20}, {Value(0x5678), 20},
  };

  size_t index = 0;
  for (const auto& entry : set) {
    EXPECT_LT(index, arraysize(kExpectedValues1));
    EXPECT_EQ(kExpectedValues1[index].value.size(), entry.value.size());
    EXPECT_EQ(kExpectedValues1[index].count, entry.count);
    ++index;
  }
}

TEST_F(RankedSetTest, RepeatedEntries) {
  RankedSet set(10);
  EXPECT_EQ(0U, set.size());

  set.Add(Value(0x1234), 20);
  set.Add(Value(0x3456), 30);
  set.Add(Value(0x1234), 20);
  set.Add(Value(0x3456), 30);
  set.Add(Value(0x4567), 30);
  set.Add(Value(0x4567), 30);
  EXPECT_EQ(3U, set.size());

  // Iterate through the contents to make sure they match what went in.
  const RankedSet::Entry kExpectedValues1[] = {
      {Value(0x3456), 30}, {Value(0x4567), 30}, {Value(0x1234), 20},
  };

  size_t index = 0;
  for (const auto& entry : set) {
    EXPECT_LT(index, arraysize(kExpectedValues1));
    EXPECT_EQ(kExpectedValues1[index].value.size(), entry.value.size());
    EXPECT_EQ(kExpectedValues1[index].count, entry.count);
    ++index;
  }
}

TEST_F(RankedSetTest, InsertionWithOverflow) {
  RankedSet set(5);
  EXPECT_EQ(0U, set.size());

  set.Add(Value(0x1234), 15);
  set.Add(Value(0x2345), 20);
  set.Add(Value(0x3456), 10);
  set.Add(Value(0x4567), 30);
  set.Add(Value(0x5678), 25);
  EXPECT_EQ(5U, set.size());

  // These values will not make it into the set, which is now full.
  set.Add(Value(0x6789), 0);
  EXPECT_EQ(5U, set.size());
  set.Add(Value(0x789a), 5);
  EXPECT_EQ(5U, set.size());

  // Iterate through the contents to make sure they match what went in.
  const RankedSet::Entry kExpectedValues1[] = {
      {Value(0x4567), 30}, {Value(0x5678), 25}, {Value(0x2345), 20},
      {Value(0x1234), 15}, {Value(0x3456), 10},
  };

  size_t index = 0;
  for (const auto& entry : set) {
    EXPECT_LT(index, arraysize(kExpectedValues1));
    EXPECT_EQ(kExpectedValues1[index].value.size(), entry.value.size());
    EXPECT_EQ(kExpectedValues1[index].count, entry.count);
    ++index;
  }

  // Insert some more values that go in the middle of the set.
  set.Add(Value(0x89ab), 27);
  EXPECT_EQ(5U, set.size());
  set.Add(Value(0x9abc), 22);
  EXPECT_EQ(5U, set.size());

  // Iterate through the contents to make sure they match what went in.
  const RankedSet::Entry kExpectedValues2[] = {
      {Value(0x4567), 30}, {Value(0x89ab), 27}, {Value(0x5678), 25},
      {Value(0x9abc), 22}, {Value(0x2345), 20},
  };

  index = 0;
  for (const auto& entry : set) {
    EXPECT_LT(index, arraysize(kExpectedValues2));
    EXPECT_EQ(kExpectedValues2[index].value.size(), entry.value.size());
    EXPECT_EQ(kExpectedValues2[index].count, entry.count);
    ++index;
  }

  // Insert some more values at the front of the set.
  set.Add(Value(0xabcd), 40);
  EXPECT_EQ(5U, set.size());
  set.Add(Value(0xbcde), 35);
  EXPECT_EQ(5U, set.size());

  // Iterate through the contents to make sure they match what went in.
  const RankedSet::Entry kExpectedValues3[] = {
      {Value(0xabcd), 40}, {Value(0xbcde), 35}, {Value(0x4567), 30},
      {Value(0x89ab), 27}, {Value(0x5678), 25},
  };

  index = 0;
  for (const auto& entry : set) {
    EXPECT_LT(index, arraysize(kExpectedValues3));
    EXPECT_EQ(kExpectedValues3[index].value.size(), entry.value.size());
    EXPECT_EQ(kExpectedValues3[index].count, entry.count);
    ++index;
  }
}

TEST_F(RankedSetTest, MoveOperation) {
  const RankedSet::Entry kExpectedValues[] = {
      {Value(0x89ab), 50}, {Value(0x789a), 40}, {Value(0x6789), 35},
      {Value(0x4567), 30}, {Value(0x5678), 25}, {Value(0x2345), 20},
      {Value(0x1234), 15}, {Value(0x3456), 10}, {Value(0x9abc), 5},
      {Value(0xabcd), 0},
  };

  RankedSet source(10);
  for (const RankedSet::Entry& entry : kExpectedValues) {
    source.Add(entry.value, entry.count);
  }
  EXPECT_EQ(10U, source.size());

  RankedSet dest(25);  // This should be changed by the move.
  dest = std::move(source);
  EXPECT_EQ(10U, dest.size());
  EXPECT_EQ(10U, dest.max_size());

  size_t index = 0;
  for (const auto& entry : dest) {
    EXPECT_LT(index, arraysize(kExpectedValues));
    EXPECT_EQ(kExpectedValues[index].value.size(), entry.value.size());
    EXPECT_EQ(kExpectedValues[index].count, entry.count);
    ++index;
  }
}

TEST_F(RankedSetTest, Find) {
  RankedSet set(10);
  EXPECT_EQ(0U, set.size());

  set.AddSize(0x1234, 15);
  set.AddSize(0x2345, 20);
  set.AddSize(0x3456, 10);
  set.AddSize(0x4567, 30);
  set.AddSize(0x5678, 25);
  EXPECT_EQ(5U, set.size());

  auto iter = set.FindSize(0x1234);
  EXPECT_TRUE(iter != set.end());
  EXPECT_EQ(0x1234U, iter->value.size());
  EXPECT_EQ(15, iter->count);

  iter = set.FindSize(0x2345);
  EXPECT_TRUE(iter != set.end());
  EXPECT_EQ(0x2345U, iter->value.size());
  EXPECT_EQ(20, iter->count);

  iter = set.FindSize(0x3456);
  EXPECT_TRUE(iter != set.end());
  EXPECT_EQ(0x3456U, iter->value.size());
  EXPECT_EQ(10, iter->count);

  iter = set.FindSize(0x4567);
  EXPECT_TRUE(iter != set.end());
  EXPECT_EQ(0x4567U, iter->value.size());
  EXPECT_EQ(30, iter->count);

  iter = set.FindSize(0x5678);
  EXPECT_TRUE(iter != set.end());
  EXPECT_EQ(0x5678U, iter->value.size());
  EXPECT_EQ(25, iter->count);
}

}  // namespace leak_detector
}  // namespace metrics
