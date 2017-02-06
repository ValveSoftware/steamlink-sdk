/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/animation/animatable/AnimatableValueTestHelper.h"

#include "core/style/BasicShapes.h"
#include "core/style/ClipPathOperation.h"
#include "platform/transforms/ScaleTransformOperation.h"
#include "platform/transforms/TranslateTransformOperation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <sstream>
#include <string>


namespace blink {

class AnimationAnimatableValueTestHelperTest : public ::testing::Test {
protected:
    ::std::string PrintToString(PassRefPtr<AnimatableValue> animValue)
    {
        return ::testing::PrintToString(*animValue.get());
    }
};

TEST_F(AnimationAnimatableValueTestHelperTest, PrintTo)
{
    EXPECT_THAT(
        PrintToString(AnimatableClipPathOperation::create(ShapeClipPathOperation::create(BasicShapeCircle::create()).get())),
        testing::StartsWith("AnimatableClipPathOperation")
        );

    EXPECT_EQ(
        ::std::string("AnimatableColor(rgba(0, 0, 0, 0), #ff0000)"),
        PrintToString(AnimatableColor::create(Color(0x000000FF), Color(0xFFFF0000))));

    EXPECT_THAT(
        PrintToString(AnimatableValue::neutralValue().get()),
        testing::StartsWith("AnimatableNeutral@"));

    EXPECT_THAT(
        PrintToString(AnimatableShapeValue::create(ShapeValue::createShapeValue(BasicShapeCircle::create(), ContentBox))),
        testing::StartsWith("AnimatableShapeValue@"));

    RefPtr<SVGDashArray> l2 = SVGDashArray::create();
    l2->append(Length(1, blink::Fixed));
    l2->append(Length(2, blink::Percent));
    EXPECT_EQ(
        ::std::string("AnimatableStrokeDasharrayList(1+0%, 0+2%)"),
        PrintToString(AnimatableStrokeDasharrayList::create(l2, 1)));

    EXPECT_EQ(
        ::std::string("AnimatableUnknown(none)"),
        PrintToString(AnimatableUnknown::create(CSSPrimitiveValue::createIdentifier(CSSValueNone))));

    EXPECT_EQ(
        ::std::string("AnimatableVisibility(VISIBLE)"),
        PrintToString(AnimatableVisibility::create(VISIBLE)));
}

} // namespace blink
