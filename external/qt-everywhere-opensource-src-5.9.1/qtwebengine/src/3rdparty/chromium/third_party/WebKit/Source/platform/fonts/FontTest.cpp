// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/Font.h"

#include "platform/testing/FontTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/text/TextRun.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::testing::createTestFont;

namespace blink {

static inline String fontPath(String relativePath) {
  return testing::blinkRootDir() + "/Source/platform/testing/data/" +
         relativePath;
}

TEST(FontTest, TextIntercepts) {
  Font font = createTestFont("Ahem", fontPath("Ahem.woff"), 16);
  // A sequence of LATIN CAPITAL LETTER E WITH ACUTE and LATIN SMALL LETTER P
  // characters. E ACUTES are squares above the baseline in Ahem, while p's
  // are rectangles below the baseline.
  UChar ahemAboveBelowBaselineString[] = {0xc9, 0x70, 0xc9, 0x70, 0xc9,
                                          0x70, 0xc9, 0x70, 0xc9};
  TextRun ahemAboveBelowBaseline(ahemAboveBelowBaselineString, 9);
  TextRunPaintInfo textRunPaintInfo(ahemAboveBelowBaseline);
  SkPaint defaultPaint;
  float deviceScaleFactor = 1;

  std::tuple<float, float> belowBaselineBounds = std::make_tuple(2, 4);
  Vector<Font::TextIntercept> textIntercepts;
  // 4 intercept ranges for below baseline p glyphs in the test string
  font.getTextIntercepts(textRunPaintInfo, deviceScaleFactor, defaultPaint,
                         belowBaselineBounds, textIntercepts);
  EXPECT_EQ(textIntercepts.size(), 4u);
  for (auto textIntercept : textIntercepts) {
    EXPECT_GT(textIntercept.m_end, textIntercept.m_begin);
  }

  std::tuple<float, float> aboveBaselineBounds = std::make_tuple(-4, -2);
  // 5 intercept ranges for the above baseline E ACUTE glyphs
  font.getTextIntercepts(textRunPaintInfo, deviceScaleFactor, defaultPaint,
                         aboveBaselineBounds, textIntercepts);
  EXPECT_EQ(textIntercepts.size(), 5u);
  for (auto textIntercept : textIntercepts) {
    EXPECT_GT(textIntercept.m_end, textIntercept.m_begin);
  }
}

}  // namespace blink
