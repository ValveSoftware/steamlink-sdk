// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_http_utils.h"

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(SpdyHttpUtilsTest, ConvertRequestPriorityToSpdy2Priority) {
  EXPECT_EQ(0, ConvertRequestPriorityToSpdyPriority(HIGHEST, SPDY2));
  EXPECT_EQ(1, ConvertRequestPriorityToSpdyPriority(MEDIUM, SPDY2));
  EXPECT_EQ(2, ConvertRequestPriorityToSpdyPriority(LOW, SPDY2));
  EXPECT_EQ(2, ConvertRequestPriorityToSpdyPriority(LOWEST, SPDY2));
  EXPECT_EQ(3, ConvertRequestPriorityToSpdyPriority(IDLE, SPDY2));
}

TEST(SpdyHttpUtilsTest, ConvertRequestPriorityToSpdy3Priority) {
  EXPECT_EQ(0, ConvertRequestPriorityToSpdyPriority(HIGHEST, SPDY3));
  EXPECT_EQ(1, ConvertRequestPriorityToSpdyPriority(MEDIUM, SPDY3));
  EXPECT_EQ(2, ConvertRequestPriorityToSpdyPriority(LOW, SPDY3));
  EXPECT_EQ(3, ConvertRequestPriorityToSpdyPriority(LOWEST, SPDY3));
  EXPECT_EQ(4, ConvertRequestPriorityToSpdyPriority(IDLE, SPDY3));
}

TEST(SpdyHttpUtilsTest, ConvertSpdy2PriorityToRequestPriority) {
  EXPECT_EQ(HIGHEST, ConvertSpdyPriorityToRequestPriority(0, SPDY2));
  EXPECT_EQ(MEDIUM, ConvertSpdyPriorityToRequestPriority(1, SPDY2));
  EXPECT_EQ(LOW, ConvertSpdyPriorityToRequestPriority(2, SPDY2));
  EXPECT_EQ(IDLE, ConvertSpdyPriorityToRequestPriority(3, SPDY2));
  // These are invalid values, but we should still handle them
  // gracefully.
  for (int i = 4; i < kuint8max; ++i) {
    EXPECT_EQ(IDLE, ConvertSpdyPriorityToRequestPriority(i, SPDY2));
  }
}

TEST(SpdyHttpUtilsTest, ConvertSpdy3PriorityToRequestPriority) {
  EXPECT_EQ(HIGHEST, ConvertSpdyPriorityToRequestPriority(0, SPDY3));
  EXPECT_EQ(MEDIUM, ConvertSpdyPriorityToRequestPriority(1, SPDY3));
  EXPECT_EQ(LOW, ConvertSpdyPriorityToRequestPriority(2, SPDY3));
  EXPECT_EQ(LOWEST, ConvertSpdyPriorityToRequestPriority(3, SPDY3));
  EXPECT_EQ(IDLE, ConvertSpdyPriorityToRequestPriority(4, SPDY3));
  // These are invalid values, but we should still handle them
  // gracefully.
  for (int i = 5; i < kuint8max; ++i) {
    EXPECT_EQ(IDLE, ConvertSpdyPriorityToRequestPriority(i, SPDY3));
  }
}

}  // namespace

}  // namespace net
