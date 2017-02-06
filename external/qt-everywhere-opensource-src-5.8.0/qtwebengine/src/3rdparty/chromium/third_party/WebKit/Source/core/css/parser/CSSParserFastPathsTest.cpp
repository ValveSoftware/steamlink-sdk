// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSParserFastPaths.h"

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSValueList.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(CSSParserFastPathsTest, ParseKeyword)
{
    CSSValue* value = CSSParserFastPaths::maybeParseValue(CSSPropertyFloat, "left", HTMLStandardMode);
    ASSERT_NE(nullptr, value);
    EXPECT_TRUE(value->isPrimitiveValue());
    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    EXPECT_TRUE(primitiveValue->isValueID());
    EXPECT_EQ(CSSValueLeft, primitiveValue->getValueID());
    value = CSSParserFastPaths::maybeParseValue(CSSPropertyFloat, "foo", HTMLStandardMode);
    ASSERT_EQ(nullptr, value);
}
TEST(CSSParserFastPathsTest, ParseInitialAndInheritKeyword)
{
    CSSValue* value = CSSParserFastPaths::maybeParseValue(CSSPropertyMarginTop, "inherit", HTMLStandardMode);
    ASSERT_NE(nullptr, value);
    EXPECT_TRUE(value->isInheritedValue());
    value = CSSParserFastPaths::maybeParseValue(CSSPropertyMarginRight, "InHeriT", HTMLStandardMode);
    ASSERT_NE(nullptr, value);
    EXPECT_TRUE(value->isInheritedValue());
    value = CSSParserFastPaths::maybeParseValue(CSSPropertyMarginBottom, "initial", HTMLStandardMode);
    ASSERT_NE(nullptr, value);
    EXPECT_TRUE(value->isInitialValue());
    value = CSSParserFastPaths::maybeParseValue(CSSPropertyMarginLeft, "IniTiaL", HTMLStandardMode);
    ASSERT_NE(nullptr, value);
    EXPECT_TRUE(value->isInitialValue());
    // Fast path doesn't handle short hands.
    value = CSSParserFastPaths::maybeParseValue(CSSPropertyMargin, "initial", HTMLStandardMode);
    ASSERT_EQ(nullptr, value);
}

TEST(CSSParserFastPathsTest, ParseTransform)
{
    CSSValue* value = CSSParserFastPaths::maybeParseValue(CSSPropertyTransform, "translate(5.5px, 5px)", HTMLStandardMode);
    ASSERT_NE(nullptr, value);
    ASSERT_TRUE(value->isValueList());
    ASSERT_EQ("translate(5.5px, 5px)", value->cssText());

    value = CSSParserFastPaths::maybeParseValue(CSSPropertyTransform, "translate3d(5px, 5px, 10.1px)", HTMLStandardMode);
    ASSERT_NE(nullptr, value);
    ASSERT_TRUE(value->isValueList());
    ASSERT_EQ("translate3d(5px, 5px, 10.1px)", value->cssText());
}

TEST(CSSParserFastPathsTest, ParseComplexTransform)
{
    // Random whitespace is on purpose.
    static const char* kComplexTransform =
        "translateX(5px) "
        "translateZ(20.5px)   "
        "translateY(10px) "
        "scale3d(0.5, 1, 0.7)   "
        "matrix3d(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16)   ";
    static const char* kComplexTransformNormalized =
        "translateX(5px) "
        "translateZ(20.5px) "
        "translateY(10px) "
        "scale3d(0.5, 1, 0.7) "
        "matrix3d(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16)";
    CSSValue* value = CSSParserFastPaths::maybeParseValue(CSSPropertyTransform, kComplexTransform, HTMLStandardMode);
    ASSERT_NE(nullptr, value);
    ASSERT_TRUE(value->isValueList());
    ASSERT_EQ(kComplexTransformNormalized, value->cssText());
}

TEST(CSSParserFastPathsTest, ParseTransformNotFastPath)
{
    CSSValue* value = CSSParserFastPaths::maybeParseValue(CSSPropertyTransform, "rotateX(1deg)", HTMLStandardMode);
    ASSERT_EQ(nullptr, value);
    value = CSSParserFastPaths::maybeParseValue(CSSPropertyTransform, "translateZ(1px) rotateX(1deg)", HTMLStandardMode);
    ASSERT_EQ(nullptr, value);
}

TEST(CSSParserFastPathsTest, ParseInvalidTransform)
{
    CSSValue* value = CSSParserFastPaths::maybeParseValue(CSSPropertyTransform, "rotateX(1deg", HTMLStandardMode);
    ASSERT_EQ(nullptr, value);
    value = CSSParserFastPaths::maybeParseValue(CSSPropertyTransform, "translateZ(1px) (1px, 1px) rotateX(1deg", HTMLStandardMode);
    ASSERT_EQ(nullptr, value);
}

} // namespace blink
