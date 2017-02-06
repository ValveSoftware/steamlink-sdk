// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/word_trimmer.h"

#include <stddef.h>

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

TEST(WordTrimmerTest, TrimWordsEmptyText) {
  size_t start = 0;
  size_t end = 0;
  EXPECT_EQ(base::string16(), TrimWords(&start, end, base::string16(), 0));
  EXPECT_EQ(0UL, start);
}

TEST(WordTrimmerTest, TrimWordsStart) {
  size_t start = 0;
  size_t end = 3;
  EXPECT_EQ(ASCIIToUTF16("one two three"),
            TrimWords(&start, end, ASCIIToUTF16("one two three four"), 2));
  EXPECT_EQ(0UL, start);
}

TEST(WordTrimmerTest, TrimWordsEnd) {
  size_t start = 14;
  size_t end = 18;
  EXPECT_EQ(ASCIIToUTF16("two three four"),
            TrimWords(&start, end, ASCIIToUTF16("one two three four"), 2));
  EXPECT_EQ(10UL, start);
}

TEST(WordTrimmerTest, TrimWordsMiddle) {
  size_t start = 14;
  size_t end = 23;
  EXPECT_EQ(ASCIIToUTF16("two three four five six seven"), TrimWords(
      &start, end, ASCIIToUTF16("one two three four five six seven eight"), 2));
  EXPECT_EQ(10UL, start);
}

TEST(WordTrimmerTest, TrimWordsEmptyKeep) {
  size_t start = 18;
  size_t end = 18;
  EXPECT_EQ(ASCIIToUTF16("two three four five six"), TrimWords(
      &start, end, ASCIIToUTF16("one two three four five six seven eight"), 2));
  EXPECT_EQ(14UL, start);
}

TEST(WordTrimmerTest, TrimWordsOutOfBounds) {
  size_t start = 4;
  size_t end = 5;
  EXPECT_EQ(ASCIIToUTF16("one"), TrimWords(
      &start, end, ASCIIToUTF16("one"), 2));
  EXPECT_EQ(4UL, start);
}

TEST(WordTrimmerTest, TrimWordsInvalid) {
  size_t start = 23;
  size_t end = 14;
  EXPECT_EQ(ASCIIToUTF16("one two three four five six seven eight"), TrimWords(
      &start, end, ASCIIToUTF16("one two three four five six seven eight"), 2));
  EXPECT_EQ(23UL, start);
}
