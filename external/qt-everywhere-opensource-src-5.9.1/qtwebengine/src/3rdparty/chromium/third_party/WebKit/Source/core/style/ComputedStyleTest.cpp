// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/style/ComputedStyle.h"

#include "core/style/ClipPathOperation.h"
#include "core/style/ShapeValue.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(ComputedStyleTest, ShapeOutsideBoxEqual) {
  ShapeValue* shape1 = ShapeValue::createBoxShapeValue(ContentBox);
  ShapeValue* shape2 = ShapeValue::createBoxShapeValue(ContentBox);
  RefPtr<ComputedStyle> style1 = ComputedStyle::create();
  RefPtr<ComputedStyle> style2 = ComputedStyle::create();
  style1->setShapeOutside(shape1);
  style2->setShapeOutside(shape2);
  ASSERT_EQ(*style1, *style2);
}

TEST(ComputedStyleTest, ShapeOutsideCircleEqual) {
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

TEST(ComputedStyleTest, ClipPathEqual) {
  RefPtr<BasicShapeCircle> shape = BasicShapeCircle::create();
  RefPtr<ShapeClipPathOperation> path1 = ShapeClipPathOperation::create(shape);
  RefPtr<ShapeClipPathOperation> path2 = ShapeClipPathOperation::create(shape);
  RefPtr<ComputedStyle> style1 = ComputedStyle::create();
  RefPtr<ComputedStyle> style2 = ComputedStyle::create();
  style1->setClipPath(path1);
  style2->setClipPath(path2);
  ASSERT_EQ(*style1, *style2);
}

TEST(ComputedStyleTest, FocusRingWidth) {
  RefPtr<ComputedStyle> style = ComputedStyle::create();
  style->setEffectiveZoom(3.5);
#if OS(MACOSX)
  style->setOutlineStyle(BorderStyleSolid);
  ASSERT_EQ(3, style->getOutlineStrokeWidthForFocusRing());
#else
  ASSERT_EQ(3.5, style->getOutlineStrokeWidthForFocusRing());
  style->setEffectiveZoom(0.5);
  ASSERT_EQ(1, style->getOutlineStrokeWidthForFocusRing());
#endif
}

TEST(ComputedStyleTest, FocusRingOutset) {
  RefPtr<ComputedStyle> style = ComputedStyle::create();
  style->setOutlineStyle(BorderStyleSolid);
  style->setOutlineStyleIsAuto(OutlineIsAutoOn);
  style->setEffectiveZoom(4.75);
#if OS(MACOSX)
  ASSERT_EQ(4, style->outlineOutsetExtent());
#else
  ASSERT_EQ(3, style->outlineOutsetExtent());
#endif
}

}  // namespace blink
