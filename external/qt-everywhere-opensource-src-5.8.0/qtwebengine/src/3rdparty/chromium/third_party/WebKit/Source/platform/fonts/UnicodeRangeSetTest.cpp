// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/UnicodeRangeSet.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

static const UChar hiraganaA[2] = { 0x3042, 0 };

TEST(UnicodeRangeSet, Empty)
{
    Vector<UnicodeRange> ranges;
    RefPtr<UnicodeRangeSet> set = adoptRef(new UnicodeRangeSet(ranges));
    EXPECT_TRUE(set->isEntireRange());
    EXPECT_EQ(0u, set->size());
    EXPECT_FALSE(set->intersectsWith(String()));
    EXPECT_TRUE(set->intersectsWith(String("a")));
    EXPECT_TRUE(set->intersectsWith(String(hiraganaA)));
}

TEST(UnicodeRangeSet, SingleCharacter)
{
    Vector<UnicodeRange> ranges;
    ranges.append(UnicodeRange('b', 'b'));
    RefPtr<UnicodeRangeSet> set = adoptRef(new UnicodeRangeSet(ranges));
    EXPECT_FALSE(set->isEntireRange());
    EXPECT_FALSE(set->intersectsWith(String()));
    EXPECT_FALSE(set->intersectsWith(String("a")));
    EXPECT_TRUE(set->intersectsWith(String("b")));
    EXPECT_FALSE(set->intersectsWith(String("c")));
    EXPECT_TRUE(set->intersectsWith(String("abc")));
    EXPECT_FALSE(set->intersectsWith(String(hiraganaA)));
    ASSERT_EQ(1u, set->size());
    EXPECT_EQ('b', set->rangeAt(0).from());
    EXPECT_EQ('b', set->rangeAt(0).to());
}

TEST(UnicodeRangeSet, TwoRanges)
{
    Vector<UnicodeRange> ranges;
    ranges.append(UnicodeRange('6', '7'));
    ranges.append(UnicodeRange('2', '4'));
    RefPtr<UnicodeRangeSet> set = adoptRef(new UnicodeRangeSet(ranges));
    EXPECT_FALSE(set->isEntireRange());
    EXPECT_FALSE(set->intersectsWith(String()));
    EXPECT_FALSE(set->intersectsWith(String("1")));
    EXPECT_TRUE(set->intersectsWith(String("2")));
    EXPECT_TRUE(set->intersectsWith(String("3")));
    EXPECT_TRUE(set->intersectsWith(String("4")));
    EXPECT_FALSE(set->intersectsWith(String("5")));
    EXPECT_TRUE(set->intersectsWith(String("6")));
    EXPECT_TRUE(set->intersectsWith(String("7")));
    EXPECT_FALSE(set->intersectsWith(String("8")));
    ASSERT_EQ(2u, set->size());
    EXPECT_EQ('2', set->rangeAt(0).from());
    EXPECT_EQ('4', set->rangeAt(0).to());
    EXPECT_EQ('6', set->rangeAt(1).from());
    EXPECT_EQ('7', set->rangeAt(1).to());
}

TEST(UnicodeRangeSet, Overlap)
{
    Vector<UnicodeRange> ranges;
    ranges.append(UnicodeRange('0', '2'));
    ranges.append(UnicodeRange('1', '1'));
    ranges.append(UnicodeRange('3', '5'));
    ranges.append(UnicodeRange('4', '6'));
    RefPtr<UnicodeRangeSet> set = adoptRef(new UnicodeRangeSet(ranges));
    ASSERT_EQ(1u, set->size());
    EXPECT_EQ('0', set->rangeAt(0).from());
    EXPECT_EQ('6', set->rangeAt(0).to());
}

TEST(UnicodeRangeSet, Non8Bit)
{
    Vector<UnicodeRange> ranges;
    ranges.append(UnicodeRange(0x3042, 0x3042));
    RefPtr<UnicodeRangeSet> set = adoptRef(new UnicodeRangeSet(ranges));
    ASSERT_EQ(1u, set->size());
    EXPECT_EQ(0x3042, set->rangeAt(0).from());
    EXPECT_EQ(0x3042, set->rangeAt(0).to());
    EXPECT_FALSE(set->intersectsWith(String("a")));
    EXPECT_TRUE(set->intersectsWith(String(hiraganaA)));
}

} // namespace blink
