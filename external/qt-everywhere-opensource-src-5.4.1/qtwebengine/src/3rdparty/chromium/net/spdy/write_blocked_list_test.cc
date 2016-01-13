// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/write_blocked_list.h"

#include "testing/gtest/include/gtest/gtest.h"


namespace net {
namespace test {
namespace {

typedef WriteBlockedList<int> IntWriteBlockedList;

TEST(WriteBlockedListTest, GetHighestPriority) {
  IntWriteBlockedList list;
  EXPECT_FALSE(list.HasWriteBlockedStreams());
  list.PushBack(1, 1);
  EXPECT_TRUE(list.HasWriteBlockedStreams());
  EXPECT_EQ(1, list.GetHighestPriorityWriteBlockedList());
  list.PushBack(1, 0);
  EXPECT_TRUE(list.HasWriteBlockedStreams());
  EXPECT_EQ(0, list.GetHighestPriorityWriteBlockedList());
}

TEST(WriteBlockedListTest, HasWriteBlockedStreamsOfGreaterThanPriority) {
  IntWriteBlockedList list;
  list.PushBack(1, 4);
  EXPECT_TRUE(list.HasWriteBlockedStreamsGreaterThanPriority(5));
  EXPECT_FALSE(list.HasWriteBlockedStreamsGreaterThanPriority(4));
  list.PushBack(1, 2);
  EXPECT_TRUE(list.HasWriteBlockedStreamsGreaterThanPriority(3));
  EXPECT_FALSE(list.HasWriteBlockedStreamsGreaterThanPriority(2));
}

TEST(WriteBlockedListTest, RemoveStreamFromWriteBlockedList) {
  IntWriteBlockedList list;

  list.PushBack(1, 4);
  EXPECT_TRUE(list.HasWriteBlockedStreams());

  list.RemoveStreamFromWriteBlockedList(1, 5);
  EXPECT_TRUE(list.HasWriteBlockedStreams());

  list.PushBack(2, 4);
  list.PushBack(1, 4);
  list.RemoveStreamFromWriteBlockedList(1, 4);
  list.RemoveStreamFromWriteBlockedList(2, 4);
  EXPECT_FALSE(list.HasWriteBlockedStreams());

  list.PushBack(1, 7);
  EXPECT_TRUE(list.HasWriteBlockedStreams());
}

TEST(WriteBlockedListTest, PopFront) {
  IntWriteBlockedList list;

  list.PushBack(1, 4);
  EXPECT_EQ(1u, list.NumBlockedStreams());
  list.PushBack(2, 4);
  list.PushBack(1, 4);
  list.PushBack(3, 4);
  EXPECT_EQ(4u, list.NumBlockedStreams());

  EXPECT_EQ(1, list.PopFront(4));
  EXPECT_EQ(2, list.PopFront(4));
  EXPECT_EQ(1, list.PopFront(4));
  EXPECT_EQ(1u, list.NumBlockedStreams());
  EXPECT_EQ(3, list.PopFront(4));
}

}  // namespace
}  // namespace test
}  // namespace net
