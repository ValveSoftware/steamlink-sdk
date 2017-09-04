// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/text/Character.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/CharacterNames.h"

namespace blink {

testing::AssertionResult isCJKIdeographOrSymbolWithMessage(UChar32 codepoint) {
  const size_t formatBufferSize = 10;
  char formattedAsHex[formatBufferSize];
  snprintf(formattedAsHex, formatBufferSize, "0x%x", codepoint);

  if (Character::isCJKIdeographOrSymbol(codepoint)) {
    return testing::AssertionSuccess() << "Codepoint " << formattedAsHex
                                       << " is a CJKIdeographOrSymbol.";
  }

  return testing::AssertionFailure() << "Codepoint " << formattedAsHex
                                     << " is not a CJKIdeographOrSymbol.";
}

TEST(CharacterTest, HammerEmojiVsCJKIdeographOrSymbol) {
  for (UChar32 testChar = 0; testChar < kMaxCodepoint; testChar++) {
    if (Character::isEmojiEmojiDefault(testChar)) {
      EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(testChar));
    }
  }
}

static void TestSpecificUChar32RangeIdeograph(UChar32 rangeStart,
                                              UChar32 rangeEnd,
                                              bool before = true) {
  if (before)
    EXPECT_FALSE(Character::isCJKIdeographOrSymbol(rangeStart - 1));
  EXPECT_TRUE(Character::isCJKIdeographOrSymbol(rangeStart));
  EXPECT_TRUE(Character::isCJKIdeographOrSymbol(
      (UChar32)((uint64_t)rangeStart + (uint64_t)rangeEnd) / 2));
  EXPECT_TRUE(Character::isCJKIdeographOrSymbol(rangeEnd));
  EXPECT_FALSE(Character::isCJKIdeographOrSymbol(rangeEnd + 1));
}

TEST(CharacterTest, TestIsCJKIdeograph) {
  // The basic CJK Unified Ideographs block.
  TestSpecificUChar32RangeIdeograph(0x4E00, 0x9FFF);
  // CJK Unified Ideographs Extension A.
  TestSpecificUChar32RangeIdeograph(0x3400, 0x4DBF, false);
  // CJK Unified Ideographs Extension A and Kangxi Radicals.
  TestSpecificUChar32RangeIdeograph(0x2E80, 0x2FDF);
  // CJK Strokes.
  TestSpecificUChar32RangeIdeograph(0x31C0, 0x31EF, false);
  // CJK Compatibility Ideographs.
  TestSpecificUChar32RangeIdeograph(0xF900, 0xFAFF);
  // CJK Unified Ideographs Extension B.
  TestSpecificUChar32RangeIdeograph(0x20000, 0x2A6DF);
  // CJK Unified Ideographs Extension C.
  // CJK Unified Ideographs Extension D.
  TestSpecificUChar32RangeIdeograph(0x2A700, 0x2B81F);
  // CJK Compatibility Ideographs Supplement.
  TestSpecificUChar32RangeIdeograph(0x2F800, 0x2FA1F);
}

static void TestSpecificUChar32RangeIdeographSymbol(UChar32 rangeStart,
                                                    UChar32 rangeEnd) {
  EXPECT_FALSE(isCJKIdeographOrSymbolWithMessage(rangeStart - 1));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(rangeStart));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(
      (UChar32)((uint64_t)rangeStart + (uint64_t)rangeEnd) / 2));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(rangeEnd));
  EXPECT_FALSE(isCJKIdeographOrSymbolWithMessage(rangeEnd + 1));
}

TEST(CharacterTest, TestIsCJKIdeographOrSymbol) {
  // CJK Compatibility Ideographs Supplement.
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x2C7));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x2CA));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x2CB));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x2D9));

  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x2020));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x2021));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x2030));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x203B));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x203C));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x2042));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x2047));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x2048));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x2049));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x2051));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x20DD));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x20DE));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x2100));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x2103));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x2105));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x2109));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x210A));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x2113));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x2116));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x2121));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x212B));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x213B));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x2150));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x2151));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x2152));

  TestSpecificUChar32RangeIdeographSymbol(0x2156, 0x215A);
  TestSpecificUChar32RangeIdeographSymbol(0x2160, 0x216B);
  TestSpecificUChar32RangeIdeographSymbol(0x2170, 0x217B);

  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x217F));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x2189));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x2307));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x2312));

  EXPECT_FALSE(isCJKIdeographOrSymbolWithMessage(0x23BD));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x23BE));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x23C4));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x23CC));
  EXPECT_FALSE(isCJKIdeographOrSymbolWithMessage(0x23CD));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x23CE));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x2423));

  TestSpecificUChar32RangeIdeographSymbol(0x2460, 0x2492);
  TestSpecificUChar32RangeIdeographSymbol(0x249C, 0x24FF);

  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x25A0));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x25A1));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x25A2));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x25AA));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x25AB));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x25B1));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x25B2));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x25B3));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x25B6));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x25B7));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x25BC));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x25BD));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x25C0));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x25C1));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x25C6));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x25C7));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x25C9));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x25CB));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x25CC));

  TestSpecificUChar32RangeIdeographSymbol(0x25CE, 0x25D3);
  TestSpecificUChar32RangeIdeographSymbol(0x25E2, 0x25E6);

  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x25EF));

  TestSpecificUChar32RangeIdeographSymbol(0x2600, 0x2603);

  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x2605));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x2606));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x260E));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x2616));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x2617));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x2640));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x2642));

  TestSpecificUChar32RangeIdeographSymbol(0x2660, 0x266F);
  TestSpecificUChar32RangeIdeographSymbol(0x2672, 0x267D);

  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x26A0));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x26BD));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x26BE));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x2713));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x271A));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x273F));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x2740));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x2756));

  TestSpecificUChar32RangeIdeographSymbol(0x2763, 0x2764);
  TestSpecificUChar32RangeIdeographSymbol(0x2776, 0x277F);

  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x2B1A));

  TestSpecificUChar32RangeIdeographSymbol(0x2FF0, 0x302D);
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x3031));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x312F));
  EXPECT_FALSE(isCJKIdeographOrSymbolWithMessage(0x3130));

  EXPECT_FALSE(isCJKIdeographOrSymbolWithMessage(0x318F));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x3190));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x319F));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x31BF));

  EXPECT_FALSE(isCJKIdeographOrSymbolWithMessage(0x31FF));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x3200));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x3300));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x33FF));

  TestSpecificUChar32RangeIdeographSymbol(0xF860, 0xF862);
  TestSpecificUChar32RangeIdeographSymbol(0xFE30, 0xFE6F);

  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0xFE10));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0xFE11));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0xFE12));
  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0xFE19));

  EXPECT_FALSE(isCJKIdeographOrSymbolWithMessage(0xFF0D));
  EXPECT_FALSE(isCJKIdeographOrSymbolWithMessage(0xFF1B));
  EXPECT_FALSE(isCJKIdeographOrSymbolWithMessage(0xFF1C));
  EXPECT_FALSE(isCJKIdeographOrSymbolWithMessage(0xFF1E));

  TestSpecificUChar32RangeIdeographSymbol(0xFF00, 0xFFEF);

  EXPECT_TRUE(isCJKIdeographOrSymbolWithMessage(0x1F100));

  TestSpecificUChar32RangeIdeographSymbol(0x1F110, 0x1F129);
  TestSpecificUChar32RangeIdeographSymbol(0x1F130, 0x1F149);
  TestSpecificUChar32RangeIdeographSymbol(0x1F150, 0x1F169);
  TestSpecificUChar32RangeIdeographSymbol(0x1F170, 0x1F189);
  TestSpecificUChar32RangeIdeographSymbol(0x1F1E6, 0x1F6FF);
}

TEST(CharacterTest, TestEmojiTextDefault) {
  // Text-default emoji, i.e.
  // Emoji=Yes and EmojiPresentation=No
  EXPECT_TRUE(Character::isEmojiTextDefault(0x0023));
  EXPECT_TRUE(Character::isEmojiTextDefault(0x2744));
  EXPECT_TRUE(Character::isEmojiTextDefault(0x1F6F3));

  // Non-emoji
  EXPECT_FALSE(Character::isEmojiTextDefault('A'));
  EXPECT_FALSE(Character::isEmojiTextDefault(0x2713));

  // Emoji=Yes and EmojiPresentation=Yes
  EXPECT_FALSE(Character::isEmojiTextDefault(0x1F9C0));
  EXPECT_FALSE(Character::isEmojiTextDefault(0x26BD));
  EXPECT_FALSE(Character::isEmojiTextDefault(0x26BE));
}

TEST(CharacterTest, TestEmojiEmojiDefault) {
  // Emoji=Yes and EmojiPresentation=Yes
  EXPECT_TRUE(Character::isEmojiEmojiDefault(0x231A));
  EXPECT_TRUE(Character::isEmojiEmojiDefault(0x1F191));
  EXPECT_TRUE(Character::isEmojiEmojiDefault(0x1F19A));
  EXPECT_TRUE(Character::isEmojiEmojiDefault(0x1F9C0));
  // Kiss
  EXPECT_TRUE(Character::isEmojiEmojiDefault(0x1F48F));
  // Couple with heart
  EXPECT_TRUE(Character::isEmojiEmojiDefault(0x1F491));
  EXPECT_TRUE(Character::isEmojiEmojiDefault(0x1F46A));

  // Non-emoji
  EXPECT_FALSE(Character::isEmojiEmojiDefault('A'));

  // Emoji=Yes and EmojiPresentation=No
  EXPECT_FALSE(Character::isEmojiEmojiDefault(0x1F202));
}

TEST(CharacterTest, TestEmojiModifierBase) {
  EXPECT_TRUE(Character::isEmojiModifierBase(0x261D));
  EXPECT_TRUE(Character::isEmojiModifierBase(0x1F470));
  EXPECT_TRUE(Character::isEmojiModifierBase(0x1F478));
  EXPECT_TRUE(Character::isEmojiModifierBase(0x1F918));
  EXPECT_FALSE(Character::isEmojiModifierBase('A'));
  EXPECT_FALSE(Character::isEmojiModifierBase(0x1F47D));
}

TEST(CharacterTest, TestEmoji40Data) {
  EXPECT_TRUE(Character::isEmojiEmojiDefault(0x1F32F));
  EXPECT_TRUE(Character::isEmojiEmojiDefault(0x1F57A));
  EXPECT_TRUE(Character::isEmojiEmojiDefault(0x1F919));
  EXPECT_TRUE(Character::isEmojiEmojiDefault(0x1F926));
  EXPECT_TRUE(Character::isEmojiModifierBase(0x1F574));
  EXPECT_TRUE(Character::isEmojiModifierBase(0x1F6CC));
  EXPECT_TRUE(Character::isEmojiModifierBase(0x1F919));
  EXPECT_TRUE(Character::isEmojiModifierBase(0x1F926));
  EXPECT_TRUE(Character::isEmojiModifierBase(0x1F933));
}

TEST(CharacterTest, LineBreakAndQuoteNotEmoji) {
  EXPECT_FALSE(Character::isEmojiTextDefault('\n'));
  EXPECT_FALSE(Character::isEmojiTextDefault('"'));
}

TEST(CharacterTest, Truncation) {
  const UChar32 base = 0x90000;
  UChar32 testChar = 0;

  testChar = base + spaceCharacter;
  EXPECT_FALSE(Character::treatAsSpace(testChar));
  testChar = base + noBreakSpaceCharacter;
  EXPECT_FALSE(Character::treatAsSpace(testChar));

  testChar = base + zeroWidthNonJoinerCharacter;
  EXPECT_FALSE(Character::treatAsZeroWidthSpace(testChar));
  testChar = base + zeroWidthJoinerCharacter;
  EXPECT_FALSE(Character::treatAsZeroWidthSpace(testChar));

  testChar = base + 0x12;
  EXPECT_FALSE(Character::treatAsZeroWidthSpaceInComplexScript(testChar));
  EXPECT_FALSE(Character::treatAsZeroWidthSpaceInComplexScript(testChar));
  testChar = base + objectReplacementCharacter;
  EXPECT_FALSE(Character::treatAsZeroWidthSpaceInComplexScript(testChar));

  testChar = base + 0xA;
  EXPECT_FALSE(Character::isNormalizedCanvasSpaceCharacter(testChar));
  testChar = base + 0x9;
  EXPECT_FALSE(Character::isNormalizedCanvasSpaceCharacter(testChar));
}

}  // namespace blink
