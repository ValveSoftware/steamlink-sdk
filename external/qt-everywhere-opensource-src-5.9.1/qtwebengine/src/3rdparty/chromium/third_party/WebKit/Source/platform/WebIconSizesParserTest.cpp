// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebIconSizesParser.h"

#include "public/platform/WebSize.h"
#include "public/platform/WebString.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/AtomicString.h"

namespace blink {

class WebIconSizesParserTest : public testing::Test {};

TEST(WebIconSizesParserTest, parseSizes) {
  WebString sizesAttribute = "32x33";
  WebVector<WebSize> sizes;
  sizes = WebIconSizesParser::parseIconSizes(sizesAttribute);
  ASSERT_EQ(1U, sizes.size());
  EXPECT_EQ(32, sizes[0].width);
  EXPECT_EQ(33, sizes[0].height);

  sizesAttribute = "0x33";
  sizes = WebIconSizesParser::parseIconSizes(sizesAttribute);
  ASSERT_EQ(0U, sizes.size());

  UChar attribute[] = {'3', '2', 'x', '3', '3', 0};
  sizesAttribute = AtomicString(attribute);
  sizes = WebIconSizesParser::parseIconSizes(sizesAttribute);
  ASSERT_EQ(1U, sizes.size());
  EXPECT_EQ(32, sizes[0].width);
  EXPECT_EQ(33, sizes[0].height);

  sizesAttribute = "   32x33   16X17    128x129   ";
  sizes = WebIconSizesParser::parseIconSizes(sizesAttribute);
  ASSERT_EQ(3U, sizes.size());
  EXPECT_EQ(32, sizes[0].width);
  EXPECT_EQ(33, sizes[0].height);
  EXPECT_EQ(16, sizes[1].width);
  EXPECT_EQ(17, sizes[1].height);
  EXPECT_EQ(128, sizes[2].width);
  EXPECT_EQ(129, sizes[2].height);

  sizesAttribute = "  \n 32x33 \r  16X17 \t   128x129 \f  ";
  sizes = WebIconSizesParser::parseIconSizes(sizesAttribute);
  ASSERT_EQ(3U, sizes.size());

  sizesAttribute = "any";
  sizes = WebIconSizesParser::parseIconSizes(sizesAttribute);
  ASSERT_EQ(1U, sizes.size());
  EXPECT_EQ(0, sizes[0].width);
  EXPECT_EQ(0, sizes[0].height);

  sizesAttribute = "ANY";
  sizes = WebIconSizesParser::parseIconSizes(sizesAttribute);
  ASSERT_EQ(1U, sizes.size());

  sizesAttribute = "AnY";
  sizes = WebIconSizesParser::parseIconSizes(sizesAttribute);
  ASSERT_EQ(1U, sizes.size());

  sizesAttribute = "an";
  sizes = WebIconSizesParser::parseIconSizes(sizesAttribute);
  ASSERT_EQ(0U, sizes.size());

  sizesAttribute = "32x33 32";
  sizes = WebIconSizesParser::parseIconSizes(sizesAttribute);
  ASSERT_EQ(1U, sizes.size());
  EXPECT_EQ(32, sizes[0].width);
  EXPECT_EQ(33, sizes[0].height);

  sizesAttribute = "32x33 32x";
  sizes = WebIconSizesParser::parseIconSizes(sizesAttribute);
  ASSERT_EQ(1U, sizes.size());
  EXPECT_EQ(32, sizes[0].width);
  EXPECT_EQ(33, sizes[0].height);

  sizesAttribute = "32x33 x32";
  sizes = WebIconSizesParser::parseIconSizes(sizesAttribute);
  ASSERT_EQ(1U, sizes.size());
  EXPECT_EQ(32, sizes[0].width);
  EXPECT_EQ(33, sizes[0].height);

  sizesAttribute = "32x33 any";
  sizes = WebIconSizesParser::parseIconSizes(sizesAttribute);
  ASSERT_EQ(2U, sizes.size());
  EXPECT_EQ(32, sizes[0].width);
  EXPECT_EQ(33, sizes[0].height);
  EXPECT_EQ(0, sizes[1].width);
  EXPECT_EQ(0, sizes[1].height);

  sizesAttribute = "32x33, 64x64";
  sizes = WebIconSizesParser::parseIconSizes(sizesAttribute);
  ASSERT_EQ(1U, sizes.size());
  EXPECT_EQ(64, sizes[0].width);
  EXPECT_EQ(64, sizes[0].height);
}

}  // namespace blink
