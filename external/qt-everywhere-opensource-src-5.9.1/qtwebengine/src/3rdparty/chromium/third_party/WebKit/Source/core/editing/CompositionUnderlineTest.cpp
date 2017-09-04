// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/CompositionUnderline.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

CompositionUnderline createCompositionUnderline(unsigned startOffset,
                                                unsigned endOffset) {
  return CompositionUnderline(startOffset, endOffset, Color::transparent, false,
                              Color::transparent);
}

TEST(CompositionUnderlineTest, OneChar) {
  CompositionUnderline underline = createCompositionUnderline(0, 1);
  EXPECT_EQ(0u, underline.startOffset());
  EXPECT_EQ(1u, underline.endOffset());
}

TEST(CompositionUnderlineTest, MultiChar) {
  CompositionUnderline underline = createCompositionUnderline(0, 5);
  EXPECT_EQ(0u, underline.startOffset());
  EXPECT_EQ(5u, underline.endOffset());
}

TEST(CompositionUnderlineTest, ZeroLength) {
  CompositionUnderline underline = createCompositionUnderline(0, 0);
  EXPECT_EQ(0u, underline.startOffset());
  EXPECT_EQ(1u, underline.endOffset());
}

TEST(CompositionUnderlineTest, ZeroLengthNonZeroStart) {
  CompositionUnderline underline = createCompositionUnderline(3, 3);
  EXPECT_EQ(3u, underline.startOffset());
  EXPECT_EQ(4u, underline.endOffset());
}

TEST(CompositionUnderlineTest, EndBeforeStart) {
  CompositionUnderline underline = createCompositionUnderline(1, 0);
  EXPECT_EQ(1u, underline.startOffset());
  EXPECT_EQ(2u, underline.endOffset());
}

TEST(CompositionUnderlineTest, LastChar) {
  CompositionUnderline underline =
      createCompositionUnderline(std::numeric_limits<unsigned>::max() - 1,
                                 std::numeric_limits<unsigned>::max());
  EXPECT_EQ(std::numeric_limits<unsigned>::max() - 1, underline.startOffset());
  EXPECT_EQ(std::numeric_limits<unsigned>::max(), underline.endOffset());
}

TEST(CompositionUnderlineTest, LastCharEndBeforeStart) {
  CompositionUnderline underline =
      createCompositionUnderline(std::numeric_limits<unsigned>::max(),
                                 std::numeric_limits<unsigned>::max() - 1);
  EXPECT_EQ(std::numeric_limits<unsigned>::max() - 1, underline.startOffset());
  EXPECT_EQ(std::numeric_limits<unsigned>::max(), underline.endOffset());
}

TEST(CompositionUnderlineTest, LastCharEndBeforeStartZeroEnd) {
  CompositionUnderline underline =
      createCompositionUnderline(std::numeric_limits<unsigned>::max(), 0);
  EXPECT_EQ(std::numeric_limits<unsigned>::max() - 1, underline.startOffset());
  EXPECT_EQ(std::numeric_limits<unsigned>::max(), underline.endOffset());
}

}  // namespace
}  // namespace blink
