// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wtf/text/TextEncoding.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace WTF {

namespace {

TEST(TextEncoding, NonByteBased) {
  EXPECT_FALSE(TextEncoding("utf-8").isNonByteBasedEncoding());
  EXPECT_TRUE(TextEncoding("utf-16").isNonByteBasedEncoding());
  EXPECT_TRUE(TextEncoding("utf-16le").isNonByteBasedEncoding());
  EXPECT_TRUE(TextEncoding("utf-16be").isNonByteBasedEncoding());
  EXPECT_TRUE(TextEncoding("utf-32").isNonByteBasedEncoding());
  EXPECT_TRUE(TextEncoding("utf-32le").isNonByteBasedEncoding());
  EXPECT_TRUE(TextEncoding("utf-32be").isNonByteBasedEncoding());
  EXPECT_FALSE(TextEncoding("windows-1252").isNonByteBasedEncoding());
  EXPECT_FALSE(TextEncoding("gbk").isNonByteBasedEncoding());
  EXPECT_FALSE(TextEncoding("utf-7").isNonByteBasedEncoding());
}

TEST(TextEncoding, ClosestByteBased) {
  EXPECT_STREQ("UTF-8",
               TextEncoding("utf-8").closestByteBasedEquivalent().name());
  EXPECT_STREQ("UTF-8",
               TextEncoding("utf-16").closestByteBasedEquivalent().name());
  EXPECT_STREQ("UTF-8",
               TextEncoding("utf-16le").closestByteBasedEquivalent().name());
  EXPECT_STREQ("UTF-8",
               TextEncoding("utf-16be").closestByteBasedEquivalent().name());
  EXPECT_STREQ("UTF-8",
               TextEncoding("utf-32").closestByteBasedEquivalent().name());
  EXPECT_STREQ("UTF-8",
               TextEncoding("utf-32le").closestByteBasedEquivalent().name());
  EXPECT_STREQ("UTF-8",
               TextEncoding("utf-32be").closestByteBasedEquivalent().name());
  EXPECT_STREQ(
      "windows-1252",
      TextEncoding("windows-1252").closestByteBasedEquivalent().name());
  EXPECT_STREQ("GBK", TextEncoding("gbk").closestByteBasedEquivalent().name());
  EXPECT_EQ(nullptr, TextEncoding("utf-7").closestByteBasedEquivalent().name());
}

TEST(TextEncoding, EncodingForFormSubmission) {
  EXPECT_STREQ("UTF-8",
               TextEncoding("utf-8").encodingForFormSubmission().name());
  EXPECT_STREQ("UTF-8",
               TextEncoding("utf-16").encodingForFormSubmission().name());
  EXPECT_STREQ("UTF-8",
               TextEncoding("utf-16le").encodingForFormSubmission().name());
  EXPECT_STREQ("UTF-8",
               TextEncoding("utf-16be").encodingForFormSubmission().name());
  EXPECT_STREQ("UTF-8",
               TextEncoding("utf-32").encodingForFormSubmission().name());
  EXPECT_STREQ("UTF-8",
               TextEncoding("utf-32le").encodingForFormSubmission().name());
  EXPECT_STREQ("UTF-8",
               TextEncoding("utf-32be").encodingForFormSubmission().name());
  EXPECT_STREQ("windows-1252",
               TextEncoding("windows-1252").encodingForFormSubmission().name());
  EXPECT_STREQ("GBK", TextEncoding("gbk").encodingForFormSubmission().name());
  EXPECT_STREQ("UTF-8",
               TextEncoding("utf-7").encodingForFormSubmission().name());
}
}
}
