// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutTreeAsText.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/paint/ObjectPaintProperties.h"
#include "core/paint/PaintPropertyTreePrinter.h"
#include "platform/graphics/paint/GeometryMapper.h"
#include "platform/graphics/paint/ScrollPaintPropertyNode.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/text/TextStream.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/HashMap.h"
#include "wtf/Vector.h"

namespace blink {

typedef bool TestParamRootLayerScrolling;
class PrePaintTreeWalkTest
    : public ::testing::WithParamInterface<TestParamRootLayerScrolling>,
      private ScopedSlimmingPaintV2ForTest,
      private ScopedRootLayerScrollingForTest,
      public RenderingTest {
 public:
  PrePaintTreeWalkTest()
      : ScopedSlimmingPaintV2ForTest(true),
        ScopedRootLayerScrollingForTest(GetParam()),
        RenderingTest(EmptyFrameLoaderClient::create()) {}

  const TransformPaintPropertyNode* framePreTranslation() {
    FrameView* frameView = document().view();
    if (RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
      return frameView->layoutView()
          ->paintProperties()
          ->paintOffsetTranslation();
    }
    return frameView->preTranslation();
  }

  const TransformPaintPropertyNode* frameScrollTranslation() {
    FrameView* frameView = document().view();
    if (RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
      return frameView->layoutView()->paintProperties()->scrollTranslation();
    }
    return frameView->scrollTranslation();
  }

 private:
  void SetUp() override {
    Settings::setMockScrollbarsEnabled(true);

    RenderingTest::SetUp();
    enableCompositing();
  }

  void TearDown() override {
    RenderingTest::TearDown();

    Settings::setMockScrollbarsEnabled(false);
  }
};

INSTANTIATE_TEST_CASE_P(All, PrePaintTreeWalkTest, ::testing::Bool());

TEST_P(PrePaintTreeWalkTest, PropertyTreesRebuiltWithBorderInvalidation) {
  setBodyInnerHTML(
      "<style>"
      "  body { margin: 0; }"
      "  #transformed { transform: translate(100px, 100px); }"
      "  .border { border: 10px solid black; }"
      "</style>"
      "<div id='transformed'></div>");

  auto* transformedElement = document().getElementById("transformed");
  const auto* transformedProperties =
      transformedElement->layoutObject()->paintProperties();
  EXPECT_EQ(TransformationMatrix().translate(100, 100),
            transformedProperties->transform()->matrix());

  // Artifically change the transform node.
  const_cast<ObjectPaintProperties*>(transformedProperties)->clearTransform();
  EXPECT_EQ(nullptr, transformedProperties->transform());

  // Cause a paint invalidation.
  transformedElement->setAttribute(HTMLNames::classAttr, "border");
  document().view()->updateAllLifecyclePhases();

  // Should have changed back.
  EXPECT_EQ(TransformationMatrix().translate(100, 100),
            transformedProperties->transform()->matrix());
}

TEST_P(PrePaintTreeWalkTest, PropertyTreesRebuiltWithFrameScroll) {
  setBodyInnerHTML("<style> body { height: 10000px; } </style>");
  EXPECT_EQ(TransformationMatrix().translate(0, 0),
            frameScrollTranslation()->matrix());

  // Cause a scroll invalidation and ensure the translation is updated.
  document().domWindow()->scrollTo(0, 100);
  document().view()->updateAllLifecyclePhases();

  EXPECT_EQ(TransformationMatrix().translate(0, -100),
            frameScrollTranslation()->matrix());
}

TEST_P(PrePaintTreeWalkTest, PropertyTreesRebuiltWithCSSTransformInvalidation) {
  setBodyInnerHTML(
      "<style>"
      "  .transformA { transform: translate(100px, 100px); }"
      "  .transformB { transform: translate(200px, 200px); }"
      "  #transformed { will-change: transform; }"
      "</style>"
      "<div id='transformed' class='transformA'></div>");

  auto* transformedElement = document().getElementById("transformed");
  const auto* transformedProperties =
      transformedElement->layoutObject()->paintProperties();
  EXPECT_EQ(TransformationMatrix().translate(100, 100),
            transformedProperties->transform()->matrix());

  // Invalidate the CSS transform property.
  transformedElement->setAttribute(HTMLNames::classAttr, "transformB");
  document().view()->updateAllLifecyclePhases();

  // The transform should have changed.
  EXPECT_EQ(TransformationMatrix().translate(200, 200),
            transformedProperties->transform()->matrix());
}

TEST_P(PrePaintTreeWalkTest, PropertyTreesRebuiltWithOpacityInvalidation) {
  setBodyInnerHTML(
      "<style>"
      "  .opacityA { opacity: 0.9; }"
      "  .opacityB { opacity: 0.4; }"
      "</style>"
      "<div id='transparent' class='opacityA'></div>");

  auto* transparentElement = document().getElementById("transparent");
  const auto* transparentProperties =
      transparentElement->layoutObject()->paintProperties();
  EXPECT_EQ(0.9f, transparentProperties->effect()->opacity());

  // Invalidate the opacity property.
  transparentElement->setAttribute(HTMLNames::classAttr, "opacityB");
  document().view()->updateAllLifecyclePhases();

  // The opacity should have changed.
  EXPECT_EQ(0.4f, transparentProperties->effect()->opacity());
}

}  // namespace blink
