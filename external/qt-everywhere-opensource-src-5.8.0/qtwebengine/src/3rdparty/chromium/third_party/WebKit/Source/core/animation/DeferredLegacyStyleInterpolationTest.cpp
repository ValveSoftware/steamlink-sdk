// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/DeferredLegacyStyleInterpolation.h"

#include "core/css/CSSInheritedValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/StylePropertySet.h"
#include "core/css/parser/CSSParser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class AnimationDeferredLegacyStyleInterpolationTest : public ::testing::Test {
protected:
    static bool test(CSSPropertyID propertyID, const String& string)
    {
        CSSParserMode parserMode = HTMLStandardMode;
        if (propertyID == CSSPropertyFloodColor)
            parserMode = SVGAttributeMode;
        CSSValue* value = CSSParser::parseSingleValue(propertyID, string, CSSParserContext(parserMode, nullptr));
        ASSERT(value);
        return DeferredLegacyStyleInterpolation::interpolationRequiresStyleResolve(*value);
    }
};

TEST_F(AnimationDeferredLegacyStyleInterpolationTest, Inherit)
{
    EXPECT_TRUE(test(CSSPropertyCaptionSide, "inherit"));
}

TEST_F(AnimationDeferredLegacyStyleInterpolationTest, Color)
{
    EXPECT_FALSE(test(CSSPropertyColor, "rgb(10, 20, 30)"));
    EXPECT_FALSE(test(CSSPropertyColor, "aqua"));
    EXPECT_FALSE(test(CSSPropertyColor, "yellow"));
    EXPECT_FALSE(test(CSSPropertyColor, "transparent"));
    EXPECT_FALSE(test(CSSPropertyFloodColor, "aliceblue"));
    EXPECT_FALSE(test(CSSPropertyFloodColor, "yellowgreen"));
    EXPECT_FALSE(test(CSSPropertyFloodColor, "grey"));
    EXPECT_TRUE(test(CSSPropertyColor, "currentcolor"));
}

TEST_F(AnimationDeferredLegacyStyleInterpolationTest, Relative)
{
    EXPECT_TRUE(test(CSSPropertyFontWeight, "bolder"));
    EXPECT_TRUE(test(CSSPropertyFontWeight, "lighter"));
    EXPECT_TRUE(test(CSSPropertyFontSize, "smaller"));
    EXPECT_TRUE(test(CSSPropertyFontSize, "larger"));
}

TEST_F(AnimationDeferredLegacyStyleInterpolationTest, Length)
{
    EXPECT_FALSE(test(CSSPropertyWidth, "10px"));
    EXPECT_TRUE(test(CSSPropertyWidth, "10em"));
    EXPECT_TRUE(test(CSSPropertyWidth, "10vh"));
}

TEST_F(AnimationDeferredLegacyStyleInterpolationTest, Number)
{
    EXPECT_FALSE(test(CSSPropertyOpacity, "0.5"));
}

TEST_F(AnimationDeferredLegacyStyleInterpolationTest, Transform)
{
    EXPECT_TRUE(test(CSSPropertyTransform, "translateX(1em)"));
    EXPECT_FALSE(test(CSSPropertyTransform, "translateY(20px)"));
    EXPECT_FALSE(test(CSSPropertyTransform, "skewX(10rad) perspective(400px)"));
    EXPECT_TRUE(test(CSSPropertyTransform, "skewX(20rad) perspective(50em)"));
}

TEST_F(AnimationDeferredLegacyStyleInterpolationTest, Filter)
{
    EXPECT_FALSE(test(CSSPropertyFilter, "hue-rotate(180deg) blur(6px)"));
    EXPECT_FALSE(test(CSSPropertyFilter, "grayscale(0) blur(0px)"));
    EXPECT_FALSE(test(CSSPropertyFilter, "none"));
    EXPECT_FALSE(test(CSSPropertyFilter, "brightness(0) contrast(0)"));
    EXPECT_FALSE(test(CSSPropertyFilter, "drop-shadow(20px 10px green)"));
    EXPECT_TRUE(test(CSSPropertyFilter, "drop-shadow(20px 10vw green)"));
    EXPECT_TRUE(test(CSSPropertyFilter, "drop-shadow(0px 0px 0px currentcolor)"));
    EXPECT_FALSE(test(CSSPropertyFilter, "opacity(1)"));
    EXPECT_FALSE(test(CSSPropertyFilter, "saturate(0)"));
    EXPECT_FALSE(test(CSSPropertyFilter, "grayscale(1)"));
    EXPECT_FALSE(test(CSSPropertyFilter, "invert(1)"));
    EXPECT_FALSE(test(CSSPropertyFilter, "sepia(1)"));
    EXPECT_TRUE(test(CSSPropertyFilter, "url(#svgfilter)"));
}

TEST_F(AnimationDeferredLegacyStyleInterpolationTest, BackdropFilter)
{
    EXPECT_FALSE(test(CSSPropertyBackdropFilter, "hue-rotate(180deg) blur(6px)"));
    EXPECT_FALSE(test(CSSPropertyBackdropFilter, "grayscale(0) blur(0px)"));
    EXPECT_FALSE(test(CSSPropertyBackdropFilter, "none"));
    EXPECT_FALSE(test(CSSPropertyBackdropFilter, "brightness(0) contrast(0)"));
    EXPECT_FALSE(test(CSSPropertyBackdropFilter, "drop-shadow(20px 10px green)"));
    EXPECT_TRUE(test(CSSPropertyBackdropFilter, "drop-shadow(20px 10vw green)"));
    EXPECT_TRUE(test(CSSPropertyBackdropFilter, "drop-shadow(0px 0px 0px currentcolor)"));
    EXPECT_FALSE(test(CSSPropertyBackdropFilter, "opacity(1)"));
    EXPECT_FALSE(test(CSSPropertyBackdropFilter, "saturate(0)"));
    EXPECT_FALSE(test(CSSPropertyBackdropFilter, "grayscale(1)"));
    EXPECT_FALSE(test(CSSPropertyBackdropFilter, "invert(1)"));
    EXPECT_FALSE(test(CSSPropertyBackdropFilter, "sepia(1)"));
    EXPECT_TRUE(test(CSSPropertyBackdropFilter, "url(#svgfilter)"));
}
} // namespace blink
