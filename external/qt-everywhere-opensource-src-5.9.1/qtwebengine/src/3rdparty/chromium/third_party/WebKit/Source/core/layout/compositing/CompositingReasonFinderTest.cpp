// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/compositing/CompositingReasonFinder.h"

#include "core/frame/FrameView.h"
#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/paint/PaintLayer.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/scroll/ScrollTypes.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"

namespace blink {

class CompositingReasonFinderTest : public RenderingTest {
 public:
  CompositingReasonFinderTest()
      : RenderingTest(EmptyFrameLoaderClient::create()) {}

 private:
  void SetUp() override {
    RenderingTest::SetUp();
    enableCompositing();
  }
};

TEST_F(CompositingReasonFinderTest, PromoteOpaqueFixedPosition) {
  ScopedCompositeFixedPositionForTest compositeFixedPosition(true);

  setBodyInnerHTML(
      "<div id='translucent' style='width: 20px; height: 20px; position: "
      "fixed; top: 100px; left: 100px;'></div>"
      "<div id='opaque' style='width: 20px; height: 20px; position: fixed; "
      "top: 100px; left: 200px; background: white;'></div>"
      "<div id='opaque-with-shadow' style='width: 20px; height: 20px; "
      "position: fixed; top: 100px; left: 300px; background: white; "
      "box-shadow: 10px 10px 5px #888888;'></div>"
      "<div id='spacer' style='height: 2000px'></div>");

  document().view()->updateAllLifecyclePhases();

  // The translucent fixed box should not be promoted.
  Element* element = document().getElementById("translucent");
  PaintLayer* paintLayer =
      toLayoutBoxModelObject(element->layoutObject())->layer();
  EXPECT_EQ(NotComposited, paintLayer->compositingState());

  // The opaque fixed box should be promoted and be opaque so that text will be
  // drawn with subpixel anti-aliasing.
  element = document().getElementById("opaque");
  paintLayer = toLayoutBoxModelObject(element->layoutObject())->layer();
  EXPECT_EQ(PaintsIntoOwnBacking, paintLayer->compositingState());
  EXPECT_TRUE(paintLayer->graphicsLayerBacking()->contentsOpaque());

  // The opaque fixed box with shadow should not be promoted because the layer
  // will include the shadow which is not opaque.
  element = document().getElementById("opaque-with-shadow");
  paintLayer = toLayoutBoxModelObject(element->layoutObject())->layer();
  EXPECT_EQ(NotComposited, paintLayer->compositingState());
}

// Tests that a transform on the fixed or an ancestor will prevent promotion
// TODO(flackr): Allow integer transforms as long as all of the ancestor
// transforms are also integer.
TEST_F(CompositingReasonFinderTest, OnlyNonTransformedFixedLayersPromoted) {
  ScopedCompositeFixedPositionForTest compositeFixedPosition(true);

  setBodyInnerHTML(
      "<style>"
      "#fixed { position: fixed; height: 200px; width: 200px; background: "
      "white; top: 0; }"
      "#spacer { height: 3000px; }"
      "</style>"
      "<div id=\"parent\">"
      "  <div id=\"fixed\"></div>"
      "  <div id=\"spacer\"></div>"
      "</div>");
  document().view()->updateAllLifecyclePhases();

  EXPECT_TRUE(RuntimeEnabledFeatures::compositeOpaqueScrollersEnabled());
  Element* parent = document().getElementById("parent");
  Element* fixed = document().getElementById("fixed");
  PaintLayer* paintLayer =
      toLayoutBoxModelObject(fixed->layoutObject())->layer();
  ASSERT_TRUE(paintLayer);
  EXPECT_EQ(PaintsIntoOwnBacking, paintLayer->compositingState());
  EXPECT_TRUE(paintLayer->graphicsLayerBacking()->contentsOpaque());

  // Change the parent to have a transform.
  parent->setAttribute(HTMLNames::styleAttr, "transform: translate(1px, 0);");
  document().view()->updateAllLifecyclePhases();
  paintLayer = toLayoutBoxModelObject(fixed->layoutObject())->layer();
  ASSERT_TRUE(paintLayer);
  EXPECT_EQ(NotComposited, paintLayer->compositingState());

  // Change the parent to have no transform again.
  parent->removeAttribute(HTMLNames::styleAttr);
  document().view()->updateAllLifecyclePhases();
  paintLayer = toLayoutBoxModelObject(fixed->layoutObject())->layer();
  ASSERT_TRUE(paintLayer);
  EXPECT_EQ(PaintsIntoOwnBacking, paintLayer->compositingState());
  EXPECT_TRUE(paintLayer->graphicsLayerBacking()->contentsOpaque());

  // Apply a transform to the fixed directly.
  fixed->setAttribute(HTMLNames::styleAttr, "transform: translate(1px, 0);");
  document().view()->updateAllLifecyclePhases();
  paintLayer = toLayoutBoxModelObject(fixed->layoutObject())->layer();
  ASSERT_TRUE(paintLayer);
  EXPECT_EQ(NotComposited, paintLayer->compositingState());
}

// Test that opacity applied to the fixed or an ancestor will cause the
// scrolling contents layer to not be promoted.
TEST_F(CompositingReasonFinderTest, OnlyOpaqueFixedLayersPromoted) {
  ScopedCompositeFixedPositionForTest compositeFixedPosition(true);

  setBodyInnerHTML(
      "<style>"
      "#fixed { position: fixed; height: 200px; width: 200px; background: "
      "white; top: 0}"
      "#spacer { height: 3000px; }"
      "</style>"
      "<div id=\"parent\">"
      "  <div id=\"fixed\"></div>"
      "  <div id=\"spacer\"></div>"
      "</div>");
  document().view()->updateAllLifecyclePhases();

  EXPECT_TRUE(RuntimeEnabledFeatures::compositeOpaqueScrollersEnabled());
  Element* parent = document().getElementById("parent");
  Element* fixed = document().getElementById("fixed");
  PaintLayer* paintLayer =
      toLayoutBoxModelObject(fixed->layoutObject())->layer();
  ASSERT_TRUE(paintLayer);
  EXPECT_EQ(PaintsIntoOwnBacking, paintLayer->compositingState());
  EXPECT_TRUE(paintLayer->graphicsLayerBacking()->contentsOpaque());

  // Change the parent to be partially translucent.
  parent->setAttribute(HTMLNames::styleAttr, "opacity: 0.5;");
  document().view()->updateAllLifecyclePhases();
  paintLayer = toLayoutBoxModelObject(fixed->layoutObject())->layer();
  ASSERT_TRUE(paintLayer);
  EXPECT_EQ(NotComposited, paintLayer->compositingState());

  // Change the parent to be opaque again.
  parent->setAttribute(HTMLNames::styleAttr, "opacity: 1;");
  document().view()->updateAllLifecyclePhases();
  paintLayer = toLayoutBoxModelObject(fixed->layoutObject())->layer();
  ASSERT_TRUE(paintLayer);
  EXPECT_EQ(PaintsIntoOwnBacking, paintLayer->compositingState());
  EXPECT_TRUE(paintLayer->graphicsLayerBacking()->contentsOpaque());

  // Make the fixed translucent.
  fixed->setAttribute(HTMLNames::styleAttr, "opacity: 0.5");
  document().view()->updateAllLifecyclePhases();
  paintLayer = toLayoutBoxModelObject(fixed->layoutObject())->layer();
  ASSERT_TRUE(paintLayer);
  EXPECT_EQ(NotComposited, paintLayer->compositingState());
}
}
