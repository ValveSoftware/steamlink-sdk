// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/common/compositor/reference_tracker.h"

#include <stdint.h>
#include <algorithm>
#include <unordered_set>
#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace {

class ReferenceTrackerTest : public testing::Test {
 public:
  ReferenceTrackerTest() = default;
  ~ReferenceTrackerTest() override = default;

 protected:
  ReferenceTracker tracker_;
  std::vector<uint32_t> added_;
  std::vector<uint32_t> removed_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ReferenceTrackerTest);
};

TEST_F(ReferenceTrackerTest, SingleItemCommitFlow) {
  tracker_.CommitRefCounts(&added_, &removed_);
  EXPECT_TRUE(added_.empty());
  EXPECT_TRUE(removed_.empty());

  uint32_t item = 1;
  tracker_.IncrementRefCount(item);
  tracker_.CommitRefCounts(&added_, &removed_);
  EXPECT_THAT(added_, testing::UnorderedElementsAre(item));
  EXPECT_TRUE(removed_.empty());
  added_.clear();

  tracker_.CommitRefCounts(&added_, &removed_);
  EXPECT_TRUE(added_.empty());
  EXPECT_TRUE(removed_.empty());

  tracker_.DecrementRefCount(item);
  tracker_.CommitRefCounts(&added_, &removed_);
  EXPECT_TRUE(added_.empty());
  EXPECT_THAT(removed_, testing::UnorderedElementsAre(item));
}

TEST_F(ReferenceTrackerTest, SingleItemMultipleTimesInSingleCommit) {
  uint32_t item = 1;
  tracker_.IncrementRefCount(item);
  tracker_.IncrementRefCount(item);
  tracker_.CommitRefCounts(&added_, &removed_);
  EXPECT_THAT(added_, testing::UnorderedElementsAre(item));
  EXPECT_TRUE(removed_.empty());
  added_.clear();

  tracker_.CommitRefCounts(&added_, &removed_);
  EXPECT_TRUE(added_.empty());
  EXPECT_TRUE(removed_.empty());

  tracker_.DecrementRefCount(item);
  tracker_.DecrementRefCount(item);
  tracker_.CommitRefCounts(&added_, &removed_);
  EXPECT_TRUE(added_.empty());
  EXPECT_THAT(removed_, testing::UnorderedElementsAre(item));
}

TEST_F(ReferenceTrackerTest, SingleItemMultipleTimesAcrossCommits) {
  uint32_t item = 1;
  tracker_.IncrementRefCount(item);
  tracker_.CommitRefCounts(&added_, &removed_);
  EXPECT_THAT(added_, testing::UnorderedElementsAre(item));
  EXPECT_TRUE(removed_.empty());
  added_.clear();

  tracker_.IncrementRefCount(item);
  tracker_.CommitRefCounts(&added_, &removed_);
  EXPECT_TRUE(added_.empty());
  EXPECT_TRUE(removed_.empty());

  tracker_.DecrementRefCount(item);
  tracker_.CommitRefCounts(&added_, &removed_);
  EXPECT_TRUE(added_.empty());
  EXPECT_TRUE(removed_.empty());

  tracker_.DecrementRefCount(item);
  tracker_.CommitRefCounts(&added_, &removed_);
  EXPECT_TRUE(added_.empty());
  EXPECT_THAT(removed_, testing::UnorderedElementsAre(item));
}

TEST_F(ReferenceTrackerTest, SingleItemComplexInteractions) {
  uint32_t item = 1;
  tracker_.IncrementRefCount(item);
  tracker_.DecrementRefCount(item);
  tracker_.CommitRefCounts(&added_, &removed_);
  EXPECT_TRUE(added_.empty());
  EXPECT_TRUE(removed_.empty());

  tracker_.IncrementRefCount(item);
  tracker_.DecrementRefCount(item);
  tracker_.IncrementRefCount(item);
  tracker_.CommitRefCounts(&added_, &removed_);
  EXPECT_THAT(added_, testing::UnorderedElementsAre(item));
  EXPECT_TRUE(removed_.empty());
  added_.clear();

  tracker_.DecrementRefCount(item);
  tracker_.IncrementRefCount(item);
  tracker_.DecrementRefCount(item);
  tracker_.CommitRefCounts(&added_, &removed_);
  EXPECT_TRUE(added_.empty());
  EXPECT_THAT(removed_, testing::UnorderedElementsAre(item));
}

TEST_F(ReferenceTrackerTest, MultipleItems) {
  uint32_t item1 = 1;
  uint32_t item2 = 2;
  uint32_t item3 = 3;
  tracker_.IncrementRefCount(item1);
  tracker_.IncrementRefCount(item2);
  tracker_.IncrementRefCount(item3);
  tracker_.CommitRefCounts(&added_, &removed_);
  EXPECT_THAT(added_, testing::UnorderedElementsAre(item1, item2, item3));
  EXPECT_TRUE(removed_.empty());
  added_.clear();

  tracker_.DecrementRefCount(item3);
  tracker_.CommitRefCounts(&added_, &removed_);
  EXPECT_TRUE(added_.empty());
  EXPECT_THAT(removed_, testing::UnorderedElementsAre(item3));
  removed_.clear();

  tracker_.DecrementRefCount(item2);
  tracker_.IncrementRefCount(item3);
  tracker_.CommitRefCounts(&added_, &removed_);
  EXPECT_THAT(added_, testing::UnorderedElementsAre(item3));
  EXPECT_THAT(removed_, testing::UnorderedElementsAre(item2));
  added_.clear();
  removed_.clear();

  tracker_.IncrementRefCount(item2);
  tracker_.CommitRefCounts(&added_, &removed_);
  EXPECT_THAT(added_, testing::UnorderedElementsAre(item2));
  EXPECT_TRUE(removed_.empty());
  added_.clear();

  tracker_.DecrementRefCount(item1);
  tracker_.DecrementRefCount(item2);
  tracker_.DecrementRefCount(item3);
  tracker_.CommitRefCounts(&added_, &removed_);
  EXPECT_TRUE(added_.empty());
  EXPECT_THAT(removed_, testing::UnorderedElementsAre(item1, item2, item3));
}

TEST_F(ReferenceTrackerTest, MultipleItemsWithClear) {
  uint32_t item1 = 1;
  uint32_t item2 = 2;
  tracker_.IncrementRefCount(item1);
  tracker_.IncrementRefCount(item2);
  tracker_.ClearRefCounts();
  tracker_.CommitRefCounts(&added_, &removed_);
  EXPECT_TRUE(added_.empty());
  EXPECT_TRUE(removed_.empty());

  tracker_.IncrementRefCount(item1);
  tracker_.IncrementRefCount(item2);
  tracker_.ClearRefCounts();
  tracker_.IncrementRefCount(item1);
  tracker_.CommitRefCounts(&added_, &removed_);
  EXPECT_THAT(added_, testing::UnorderedElementsAre(item1));
  EXPECT_TRUE(removed_.empty());
  added_.clear();

  tracker_.ClearRefCounts();
  tracker_.IncrementRefCount(item2);
  tracker_.CommitRefCounts(&added_, &removed_);
  EXPECT_THAT(added_, testing::UnorderedElementsAre(item2));
  EXPECT_THAT(removed_, testing::UnorderedElementsAre(item1));
  added_.clear();
  removed_.clear();

  tracker_.ClearRefCounts();
  tracker_.IncrementRefCount(item1);
  tracker_.IncrementRefCount(item2);
  tracker_.CommitRefCounts(&added_, &removed_);
  EXPECT_THAT(added_, testing::UnorderedElementsAre(item1));
  EXPECT_TRUE(removed_.empty());
  added_.clear();
}

}  // namespace
}  // namespace blimp
