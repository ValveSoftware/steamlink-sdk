// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/parser/BisonCSSParser.h"

#include "core/css/CSSTimingFunctionValue.h"
#include "platform/animation/TimingFunction.h"

#include <gtest/gtest.h>

namespace WebCore {

TEST(BisonCSSParserTest, ParseAnimationTimingFunctionValue)
{
    RefPtrWillBeRawPtr<CSSValue> timingFunctionValue;
    timingFunctionValue = BisonCSSParser::parseAnimationTimingFunctionValue("ease");
    EXPECT_EQ(CSSValueEase, toCSSPrimitiveValue(timingFunctionValue.get())->getValueID());

    timingFunctionValue = BisonCSSParser::parseAnimationTimingFunctionValue("linear");
    EXPECT_EQ(CSSValueLinear, toCSSPrimitiveValue(timingFunctionValue.get())->getValueID());

    timingFunctionValue = BisonCSSParser::parseAnimationTimingFunctionValue("ease-in");
    EXPECT_EQ(CSSValueEaseIn, toCSSPrimitiveValue(timingFunctionValue.get())->getValueID());

    timingFunctionValue = BisonCSSParser::parseAnimationTimingFunctionValue("ease-out");
    EXPECT_EQ(CSSValueEaseOut, toCSSPrimitiveValue(timingFunctionValue.get())->getValueID());

    timingFunctionValue = BisonCSSParser::parseAnimationTimingFunctionValue("ease-in-out");
    EXPECT_EQ(CSSValueEaseInOut, toCSSPrimitiveValue(timingFunctionValue.get())->getValueID());

    timingFunctionValue = BisonCSSParser::parseAnimationTimingFunctionValue("step-start");
    EXPECT_EQ(CSSValueStepStart, toCSSPrimitiveValue(timingFunctionValue.get())->getValueID());

    timingFunctionValue = BisonCSSParser::parseAnimationTimingFunctionValue("step-middle");
    EXPECT_EQ(CSSValueStepMiddle, toCSSPrimitiveValue(timingFunctionValue.get())->getValueID());

    timingFunctionValue = BisonCSSParser::parseAnimationTimingFunctionValue("step-end");
    EXPECT_EQ(CSSValueStepEnd, toCSSPrimitiveValue(timingFunctionValue.get())->getValueID());

    timingFunctionValue = BisonCSSParser::parseAnimationTimingFunctionValue("steps(3, start)");
    EXPECT_TRUE(CSSStepsTimingFunctionValue::create(3, StepsTimingFunction::StepAtStart)->equals(toCSSStepsTimingFunctionValue(*timingFunctionValue.get())));

    timingFunctionValue = BisonCSSParser::parseAnimationTimingFunctionValue("steps(3, middle)");
    EXPECT_TRUE(CSSStepsTimingFunctionValue::create(3, StepsTimingFunction::StepAtMiddle)->equals(toCSSStepsTimingFunctionValue(*timingFunctionValue.get())));

    timingFunctionValue = BisonCSSParser::parseAnimationTimingFunctionValue("steps(3, end)");
    EXPECT_TRUE(CSSStepsTimingFunctionValue::create(3, StepsTimingFunction::StepAtEnd)->equals(toCSSStepsTimingFunctionValue(*timingFunctionValue.get())));

    timingFunctionValue = BisonCSSParser::parseAnimationTimingFunctionValue("steps(3, nowhere)");
    EXPECT_EQ(0, timingFunctionValue.get());

    timingFunctionValue = BisonCSSParser::parseAnimationTimingFunctionValue("steps(-3, end)");
    EXPECT_EQ(0, timingFunctionValue.get());

    timingFunctionValue = BisonCSSParser::parseAnimationTimingFunctionValue("steps(3)");
    EXPECT_TRUE(CSSStepsTimingFunctionValue::create(3, StepsTimingFunction::StepAtEnd)->equals(toCSSStepsTimingFunctionValue(*timingFunctionValue.get())));

    timingFunctionValue = BisonCSSParser::parseAnimationTimingFunctionValue("cubic-bezier(0.1, 5, 0.23, 0)");
    EXPECT_TRUE(CSSCubicBezierTimingFunctionValue::create(0.1, 5, 0.23, 0)->equals(toCSSCubicBezierTimingFunctionValue(*timingFunctionValue.get())));

    timingFunctionValue = BisonCSSParser::parseAnimationTimingFunctionValue("cubic-bezier(0.1, 0, 4, 0.4)");
    EXPECT_EQ(0, timingFunctionValue.get());
}

} // namespace WebCore
