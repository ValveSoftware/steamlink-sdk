// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/style/ComputedStyle.h"

#include "core/style/ClipPathOperation.h"
#include "core/style/ShapeValue.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(ComputedStyleTest, ShapeOutsideBoxEqual)
{
    ShapeValue* shape1 = ShapeValue::createBoxShapeValue(ContentBox);
    ShapeValue* shape2 = ShapeValue::createBoxShapeValue(ContentBox);
    RefPtr<ComputedStyle> style1 = ComputedStyle::create();
    RefPtr<ComputedStyle> style2 = ComputedStyle::create();
    style1->setShapeOutside(shape1);
    style2->setShapeOutside(shape2);
    ASSERT_EQ(*style1, *style2);
}

TEST(ComputedStyleTest, ShapeOutsideCircleEqual)
{
    RefPtr<BasicShapeCircle> circle1 = BasicShapeCircle::create();
    RefPtr<BasicShapeCircle> circle2 = BasicShapeCircle::create();
    ShapeValue* shape1 = ShapeValue::createShapeValue(circle1, ContentBox);
    ShapeValue* shape2 = ShapeValue::createShapeValue(circle2, ContentBox);
    RefPtr<ComputedStyle> style1 = ComputedStyle::create();
    RefPtr<ComputedStyle> style2 = ComputedStyle::create();
    style1->setShapeOutside(shape1);
    style2->setShapeOutside(shape2);
    ASSERT_EQ(*style1, *style2);
}

TEST(ComputedStyleTest, ClipPathEqual)
{
    RefPtr<BasicShapeCircle> shape = BasicShapeCircle::create();
    RefPtr<ShapeClipPathOperation> path1 = ShapeClipPathOperation::create(shape);
    RefPtr<ShapeClipPathOperation> path2 = ShapeClipPathOperation::create(shape);
    RefPtr<ComputedStyle> style1 = ComputedStyle::create();
    RefPtr<ComputedStyle> style2 = ComputedStyle::create();
    style1->setClipPath(path1);
    style2->setClipPath(path2);
    ASSERT_EQ(*style1, *style2);
}

} // namespace blink
