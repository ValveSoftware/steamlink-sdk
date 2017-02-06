// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/PropertyHandle.h"

#include "core/SVGNames.h"
#include "core/XLinkNames.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

using namespace SVGNames;

class PropertyHandleTest : public ::testing::Test {
};

TEST_F(PropertyHandleTest, Equality)
{
    EXPECT_TRUE(PropertyHandle(CSSPropertyOpacity) == PropertyHandle(CSSPropertyOpacity));
    EXPECT_FALSE(PropertyHandle(CSSPropertyOpacity) != PropertyHandle(CSSPropertyOpacity));
    EXPECT_FALSE(PropertyHandle(CSSPropertyOpacity) == PropertyHandle(CSSPropertyTransform));
    EXPECT_TRUE(PropertyHandle(CSSPropertyOpacity) != PropertyHandle(CSSPropertyTransform));
    EXPECT_FALSE(PropertyHandle(CSSPropertyOpacity) == PropertyHandle(amplitudeAttr));
    EXPECT_TRUE(PropertyHandle(CSSPropertyOpacity) != PropertyHandle(amplitudeAttr));

    EXPECT_FALSE(PropertyHandle(amplitudeAttr) == PropertyHandle(CSSPropertyOpacity));
    EXPECT_TRUE(PropertyHandle(amplitudeAttr) != PropertyHandle(CSSPropertyOpacity));
    EXPECT_TRUE(PropertyHandle(amplitudeAttr) == PropertyHandle(amplitudeAttr));
    EXPECT_FALSE(PropertyHandle(amplitudeAttr) != PropertyHandle(amplitudeAttr));
    EXPECT_FALSE(PropertyHandle(amplitudeAttr) == PropertyHandle(exponentAttr));
    EXPECT_TRUE(PropertyHandle(amplitudeAttr) != PropertyHandle(exponentAttr));
}

TEST_F(PropertyHandleTest, Hash)
{
    EXPECT_TRUE(PropertyHandle(CSSPropertyOpacity).hash() == PropertyHandle(CSSPropertyOpacity).hash());
    EXPECT_FALSE(PropertyHandle(CSSPropertyOpacity).hash() == PropertyHandle(CSSPropertyTransform).hash());
    EXPECT_FALSE(PropertyHandle(CSSPropertyOpacity).hash() == PropertyHandle(amplitudeAttr).hash());

    EXPECT_FALSE(PropertyHandle(amplitudeAttr).hash() == PropertyHandle(CSSPropertyOpacity).hash());
    EXPECT_TRUE(PropertyHandle(amplitudeAttr).hash() == PropertyHandle(amplitudeAttr).hash());
    EXPECT_FALSE(PropertyHandle(amplitudeAttr).hash() == PropertyHandle(exponentAttr).hash());
}

TEST_F(PropertyHandleTest, Accessors)
{
    EXPECT_TRUE(PropertyHandle(CSSPropertyOpacity).isCSSProperty());
    EXPECT_EQ(PropertyHandle(CSSPropertyOpacity).cssProperty(), CSSPropertyOpacity);
    EXPECT_FALSE(PropertyHandle(amplitudeAttr).isCSSProperty());

    EXPECT_FALSE(PropertyHandle(CSSPropertyOpacity).isSVGAttribute());
    EXPECT_TRUE(PropertyHandle(amplitudeAttr).isSVGAttribute());
    EXPECT_EQ(PropertyHandle(amplitudeAttr).svgAttribute(), amplitudeAttr);
}

} // namespace blink
