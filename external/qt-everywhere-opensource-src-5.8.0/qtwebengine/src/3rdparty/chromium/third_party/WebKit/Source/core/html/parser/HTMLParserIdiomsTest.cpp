// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/parser/HTMLParserIdioms.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

TEST(HTMLParserIdiomsTest, ParseHTMLNonNegativeInteger)
{
    unsigned value = 0;

    EXPECT_TRUE(parseHTMLNonNegativeInteger("0", value));
    EXPECT_EQ(0U, value);

    EXPECT_TRUE(parseHTMLNonNegativeInteger("+0", value));
    EXPECT_EQ(0U, value);

    EXPECT_TRUE(parseHTMLNonNegativeInteger("-0", value));
    EXPECT_EQ(0U, value);

    EXPECT_TRUE(parseHTMLNonNegativeInteger("2147483647", value));
    EXPECT_EQ(2147483647U, value);
    EXPECT_TRUE(parseHTMLNonNegativeInteger("4294967295", value));
    EXPECT_EQ(4294967295U, value);

    EXPECT_TRUE(parseHTMLNonNegativeInteger("0abc", value));
    EXPECT_EQ(0U, value);
    EXPECT_TRUE(parseHTMLNonNegativeInteger(" 0", value));
    EXPECT_EQ(0U, value);

    value = 12345U;
    EXPECT_FALSE(parseHTMLNonNegativeInteger("-1", value));
    EXPECT_EQ(12345U, value);
    EXPECT_FALSE(parseHTMLNonNegativeInteger("abc", value));
    EXPECT_EQ(12345U, value);
    EXPECT_FALSE(parseHTMLNonNegativeInteger("  ", value));
    EXPECT_EQ(12345U, value);
    EXPECT_FALSE(parseHTMLNonNegativeInteger("-", value));
    EXPECT_EQ(12345U, value);
}

TEST(HTMLParserIdiomsTest, ParseHTMLListOfFloatingPointNumbers_null)
{
    Vector<double> numbers = parseHTMLListOfFloatingPointNumbers(nullAtom);
    EXPECT_EQ(0u, numbers.size());
}

} // namespace

} // namespace blink
