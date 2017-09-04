// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/iterators/TextSearcherICU.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/StringView.h"
#include "wtf/text/WTFString.h"

namespace blink {

namespace {

String makeUTF16(const char* str) {
  String utf16String = String::fromUTF8(str);
  utf16String.ensure16Bit();
  return utf16String;
}

}  // namespace

TEST(TextSearcherICUTest, FindSubstring) {
  TextSearcherICU searcher;
  const String& pattern = makeUTF16("substring");
  searcher.setPattern(pattern, true);

  const String& text = makeUTF16("Long text with substring content.");
  searcher.setText(text.characters16(), text.length());

  MatchResult result;

  EXPECT_TRUE(searcher.nextMatchResult(result));
  EXPECT_NE(0u, result.start);
  EXPECT_NE(0u, result.length);
  ASSERT_LT(result.length, text.length());
  EXPECT_EQ(pattern, text.substring(result.start, result.length));

  EXPECT_FALSE(searcher.nextMatchResult(result));
  EXPECT_EQ(0u, result.start);
  EXPECT_EQ(0u, result.length);
}

TEST(TextSearcherICUTest, FindIgnoreCaseSubstring) {
  TextSearcherICU searcher;
  const String& pattern = makeUTF16("substring");
  searcher.setPattern(pattern, false);

  const String& text = makeUTF16("Long text with SubStrinG content.");
  searcher.setText(text.characters16(), text.length());

  MatchResult result;
  EXPECT_TRUE(searcher.nextMatchResult(result));
  EXPECT_NE(0u, result.start);
  EXPECT_NE(0u, result.length);
  ASSERT_LT(result.length, text.length());
  EXPECT_EQ(pattern, text.substring(result.start, result.length).lower());

  searcher.setPattern(pattern, true);
  searcher.setOffset(0u);
  EXPECT_FALSE(searcher.nextMatchResult(result));
  EXPECT_EQ(0u, result.start);
  EXPECT_EQ(0u, result.length);
}

TEST(TextSearcherICUTest, FindSubstringWithOffset) {
  TextSearcherICU searcher;
  const String& pattern = makeUTF16("substring");
  searcher.setPattern(pattern, true);

  const String& text =
      makeUTF16("Long text with substring content. Second substring");
  searcher.setText(text.characters16(), text.length());

  MatchResult firstResult;

  EXPECT_TRUE(searcher.nextMatchResult(firstResult));
  EXPECT_NE(0u, firstResult.start);
  EXPECT_NE(0u, firstResult.length);

  MatchResult secondResult;
  EXPECT_TRUE(searcher.nextMatchResult(secondResult));
  EXPECT_NE(0u, secondResult.start);
  EXPECT_NE(0u, secondResult.length);

  searcher.setOffset(firstResult.start + firstResult.length);

  MatchResult offsetResult;
  EXPECT_TRUE(searcher.nextMatchResult(offsetResult));
  EXPECT_EQ(offsetResult.start, secondResult.start);
  EXPECT_EQ(offsetResult.length, secondResult.length);

  searcher.setOffset(firstResult.start);

  EXPECT_TRUE(searcher.nextMatchResult(offsetResult));
  EXPECT_EQ(offsetResult.start, firstResult.start);
  EXPECT_EQ(offsetResult.length, firstResult.length);
}

}  // namespace blink
