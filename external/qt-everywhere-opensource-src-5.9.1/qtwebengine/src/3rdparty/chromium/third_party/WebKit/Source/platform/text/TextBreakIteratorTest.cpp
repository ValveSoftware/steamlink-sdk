// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/text/TextBreakIterator.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/WTFString.h"

namespace blink {

class TextBreakIteratorTest : public testing::Test {
 protected:
  void SetTestString(const char* testString) {
    m_testString = String::fromUTF8(testString);
  }

  // The expected break positions must be specified UTF-16 character boundaries.
  void MatchLineBreaks(LineBreakType lineBreakType,
                       const Vector<int> expectedBreakPositions) {
    if (m_testString.is8Bit()) {
      m_testString = String::make16BitFrom8BitSource(m_testString.characters8(),
                                                     m_testString.length());
    }
    LazyLineBreakIterator lazyBreakIterator(m_testString);
    int nextBreakable = 0;
    for (auto breakPosition : expectedBreakPositions) {
      int triggerPos = std::min(static_cast<unsigned>(nextBreakable + 1),
                                m_testString.length());
      bool isBreakable = lazyBreakIterator.isBreakable(
          triggerPos, nextBreakable, lineBreakType);
      if (isBreakable) {
        ASSERT_EQ(triggerPos, breakPosition);
      }
      ASSERT_EQ(breakPosition, nextBreakable);
    }
  }

 private:
  String m_testString;
};

// Initializing Vector from an initializer list still not possible, C++ feature
// banned in Blink.
#define DECLARE_BREAKSVECTOR(...)                   \
  static const int32_t breaksArray[] = __VA_ARGS__; \
  Vector<int> breaks;                               \
  breaks.append(breaksArray, sizeof(breaksArray) / sizeof(*breaksArray));

#define MATCH_LINE_BREAKS(LINEBREAKTYPE, ...) \
  {                                           \
    DECLARE_BREAKSVECTOR(__VA_ARGS__);        \
    MatchLineBreaks(LINEBREAKTYPE, breaks);   \
  }

TEST_F(TextBreakIteratorTest, Basic) {
  SetTestString("a b c");
  MATCH_LINE_BREAKS(LineBreakType::Normal, {1, 3, 5});
}

TEST_F(TextBreakIteratorTest, Chinese) {
  SetTestString("標準萬國碼");
  MATCH_LINE_BREAKS(LineBreakType::Normal, {1, 2, 3, 4, 5});
  MATCH_LINE_BREAKS(LineBreakType::BreakAll, {1, 2, 3, 4, 5});
  MATCH_LINE_BREAKS(LineBreakType::KeepAll, {5});
}

TEST_F(TextBreakIteratorTest, KeepEmojiZWJFamilyIsolate) {
  SetTestString(u8"\U0001F468\u200D\U0001F469\u200D\U0001F467\u200D\U0001F466");
  MATCH_LINE_BREAKS(LineBreakType::Normal, {11});
  MATCH_LINE_BREAKS(LineBreakType::BreakAll, {11});
  MATCH_LINE_BREAKS(LineBreakType::KeepAll, {11});
}

TEST_F(TextBreakIteratorTest, KeepEmojiModifierSequenceIsolate) {
  SetTestString(u8"\u261D\U0001F3FB");
  MATCH_LINE_BREAKS(LineBreakType::Normal, {3});
  MATCH_LINE_BREAKS(LineBreakType::BreakAll, {3});
  MATCH_LINE_BREAKS(LineBreakType::KeepAll, {3});
}

TEST_F(TextBreakIteratorTest, KeepEmojiZWJSequence) {
  SetTestString(
      u8"abc \U0001F469\u200D\U0001F469\u200D\U0001F467\u200D\U0001F467 def");
  MATCH_LINE_BREAKS(LineBreakType::Normal, {3, 15, 19});
  MATCH_LINE_BREAKS(LineBreakType::BreakAll, {1, 2, 3, 15, 17, 18, 19});
  MATCH_LINE_BREAKS(LineBreakType::KeepAll, {3, 15, 19});
}

TEST_F(TextBreakIteratorTest, KeepEmojiModifierSequence) {
  SetTestString(u8"abc \u261D\U0001F3FB def");
  MATCH_LINE_BREAKS(LineBreakType::Normal, {3, 7, 11});
  MATCH_LINE_BREAKS(LineBreakType::BreakAll, {1, 2, 3, 7, 9, 10, 11});
  MATCH_LINE_BREAKS(LineBreakType::KeepAll, {3, 7, 11});
}

}  // namespace blink
