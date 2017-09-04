// Copyright 2015 The Chromium Authors. All rights reserved.
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
class PaintPropertyTreeBuilderTest
    : public ::testing::WithParamInterface<TestParamRootLayerScrolling>,
      private ScopedSlimmingPaintV2ForTest,
      private ScopedRootLayerScrollingForTest,
      public RenderingTest {
 public:
  PaintPropertyTreeBuilderTest()
      : ScopedSlimmingPaintV2ForTest(true),
        ScopedRootLayerScrollingForTest(GetParam()),
        RenderingTest(SingleChildFrameLoaderClient::create()) {}

  void loadTestData(const char* fileName) {
    String fullPath = testing::blinkRootDir();
    fullPath.append("/Source/core/paint/test_data/");
    fullPath.append(fileName);
    RefPtr<SharedBuffer> inputBuffer = testing::readFromFile(fullPath);
    setBodyInnerHTML(String(inputBuffer->data(), inputBuffer->size()));
  }

  const TransformPaintPropertyNode* framePreTranslation() {
    FrameView* frameView = document().view();
    if (RuntimeEnabledFeatures::rootLayerScrollingEnabled())
      return frameView->layoutView()
          ->paintProperties()
          ->paintOffsetTranslation();
    return frameView->preTranslation();
  }

  const TransformPaintPropertyNode* frameScrollTranslation() {
    FrameView* frameView = document().view();
    if (RuntimeEnabledFeatures::rootLayerScrollingEnabled())
      return frameView->layoutView()->paintProperties()->scrollTranslation();
    return frameView->scrollTranslation();
  }

  const ClipPaintPropertyNode* frameContentClip() {
    FrameView* frameView = document().view();
    if (RuntimeEnabledFeatures::rootLayerScrollingEnabled())
      return frameView->layoutView()->paintProperties()->overflowClip();
    return frameView->contentClip();
  }

  const ScrollPaintPropertyNode* frameScroll() {
    FrameView* frameView = document().view();
    if (RuntimeEnabledFeatures::rootLayerScrollingEnabled())
      return frameView->layoutView()->paintProperties()->scroll();
    return frameView->scroll();
  }

  LayoutPoint paintOffset(const LayoutObject* object) {
    return object->paintProperties()->localBorderBoxProperties()->paintOffset;
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

#define CHECK_VISUAL_RECT(expected, sourceLayoutObject, ancestorLayoutObject,  \
                          slopFactor)                                          \
  do {                                                                         \
    GeometryMapper geometryMapper;                                             \
    LayoutRect source((sourceLayoutObject)->localVisualRect());                \
    source.moveBy((sourceLayoutObject)                                         \
                      ->paintProperties()                                      \
                      ->localBorderBoxProperties()                             \
                      ->paintOffset);                                          \
    bool success = false;                                                      \
    auto contentsProperties =                                                  \
        (ancestorLayoutObject)->paintProperties()->contentsProperties();       \
    LayoutRect actual =                                                        \
        LayoutRect(geometryMapper.mapToVisualRectInDestinationSpace(           \
            FloatRect(source), (sourceLayoutObject)                            \
                                   ->paintProperties()                         \
                                   ->localBorderBoxProperties()                \
                                   ->propertyTreeState,                        \
            contentsProperties.propertyTreeState, success));                   \
    ASSERT_TRUE(success);                                                      \
    actual.moveBy(-contentsProperties.paintOffset);                            \
    EXPECT_EQ(expected, actual)                                                \
        << "GeometryMapper: expected: " << expected.toString()                 \
        << ", actual: " << actual.toString();                                  \
                                                                               \
    if (slopFactor == LayoutUnit::max())                                       \
      break;                                                                   \
    LayoutRect slowPathRect = (sourceLayoutObject)->localVisualRect();         \
    (sourceLayoutObject)                                                       \
        ->mapToVisualRectInAncestorSpace(ancestorLayoutObject, slowPathRect);  \
    if (slopFactor) {                                                          \
      LayoutRect inflatedActual = LayoutRect(actual);                          \
      inflatedActual.inflate(slopFactor);                                      \
      SCOPED_TRACE(                                                            \
          String::format("Old path rect: %s, Actual: %s, Inflated actual: %s", \
                         slowPathRect.toString().ascii().data(),               \
                         actual.toString().ascii().data(),                     \
                         inflatedActual.toString().ascii().data()));           \
      EXPECT_TRUE(slowPathRect.contains(LayoutRect(actual)));                  \
      EXPECT_TRUE(inflatedActual.contains(slowPathRect));                      \
    } else {                                                                   \
      EXPECT_EQ(expected, slowPathRect)                                        \
          << "Slow path: expected: " << slowPathRect.toString()                \
          << ", actual: " << actual.toString().ascii().data();                 \
    }                                                                          \
  } while (0)

#define CHECK_EXACT_VISUAL_RECT(expected, sourceLayoutObject, \
                                ancestorLayoutObject)         \
  CHECK_VISUAL_RECT(expected, sourceLayoutObject, ancestorLayoutObject, 0)

INSTANTIATE_TEST_CASE_P(All, PaintPropertyTreeBuilderTest, ::testing::Bool());

TEST_P(PaintPropertyTreeBuilderTest, FixedPosition) {
  loadTestData("fixed-position.html");

  Element* positionedScroll = document().getElementById("positionedScroll");
  positionedScroll->setScrollTop(3);
  Element* transformedScroll = document().getElementById("transformedScroll");
  transformedScroll->setScrollTop(5);

  FrameView* frameView = document().view();
  frameView->updateAllLifecyclePhases();

  // target1 is a fixed-position element inside an absolute-position scrolling
  // element.  It should be attached under the viewport to skip scrolling and
  // offset of the parent.
  Element* target1 = document().getElementById("target1");
  const ObjectPaintProperties* target1Properties =
      target1->layoutObject()->paintProperties();
  EXPECT_EQ(TransformationMatrix().translate(200, 150),
            target1Properties->paintOffsetTranslation()->matrix());
  EXPECT_EQ(framePreTranslation(),
            target1Properties->paintOffsetTranslation()->parent());
  EXPECT_EQ(target1Properties->paintOffsetTranslation(),
            target1Properties->overflowClip()->localTransformSpace());
  EXPECT_EQ(FloatRoundedRect(0, 0, 100, 100),
            target1Properties->overflowClip()->clipRect());
  // Likewise, it inherits clip from the viewport, skipping overflow clip of the
  // scroller.
  EXPECT_EQ(frameContentClip(), target1Properties->overflowClip()->parent());
  // target1 should not have it's own scroll node and instead should inherit
  // positionedScroll's.
  const ObjectPaintProperties* positionedScrollProperties =
      positionedScroll->layoutObject()->paintProperties();
  EXPECT_TRUE(positionedScrollProperties->scroll()->parent()->isRoot());
  EXPECT_EQ(TransformationMatrix().translate(0, -3),
            positionedScrollProperties->scroll()
                ->scrollOffsetTranslation()
                ->matrix());
  EXPECT_EQ(nullptr, target1Properties->scroll());

  CHECK_EXACT_VISUAL_RECT(LayoutRect(200, 150, 100, 100),
                          target1->layoutObject(), frameView->layoutView());

  // target2 is a fixed-position element inside a transformed scrolling element.
  // It should be attached under the scrolled box of the transformed element.
  Element* target2 = document().getElementById("target2");
  const ObjectPaintProperties* target2Properties =
      target2->layoutObject()->paintProperties();
  Element* scroller = document().getElementById("transformedScroll");
  const ObjectPaintProperties* scrollerProperties =
      scroller->layoutObject()->paintProperties();
  EXPECT_EQ(TransformationMatrix().translate(200, 150),
            target2Properties->paintOffsetTranslation()->matrix());
  EXPECT_EQ(scrollerProperties->scrollTranslation(),
            target2Properties->paintOffsetTranslation()->parent());
  EXPECT_EQ(target2Properties->paintOffsetTranslation(),
            target2Properties->overflowClip()->localTransformSpace());
  EXPECT_EQ(FloatRoundedRect(0, 0, 100, 100),
            target2Properties->overflowClip()->clipRect());
  EXPECT_EQ(scrollerProperties->overflowClip(),
            target2Properties->overflowClip()->parent());
  // target2 should not have it's own scroll node and instead should inherit
  // transformedScroll's.
  const ObjectPaintProperties* transformedScrollProperties =
      transformedScroll->layoutObject()->paintProperties();
  EXPECT_TRUE(transformedScrollProperties->scroll()->parent()->isRoot());
  EXPECT_EQ(TransformationMatrix().translate(0, -5),
            transformedScrollProperties->scroll()
                ->scrollOffsetTranslation()
                ->matrix());
  EXPECT_EQ(nullptr, target2Properties->scroll());

  CHECK_EXACT_VISUAL_RECT(LayoutRect(208, 153, 200, 100),
                          target2->layoutObject(), frameView->layoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, PositionAndScroll) {
  loadTestData("position-and-scroll.html");

  Element* scroller = document().getElementById("scroller");
  scroller->scrollTo(0, 100);
  FrameView* frameView = document().view();
  frameView->updateAllLifecyclePhases();
  const ObjectPaintProperties* scrollerProperties =
      scroller->layoutObject()->paintProperties();
  EXPECT_EQ(TransformationMatrix().translate(0, -100),
            scrollerProperties->scrollTranslation()->matrix());
  EXPECT_EQ(frameScrollTranslation(),
            scrollerProperties->scrollTranslation()->parent());
  EXPECT_EQ(frameScrollTranslation(),
            scrollerProperties->overflowClip()->localTransformSpace());
  EXPECT_EQ(frameScroll(), scrollerProperties->scroll()->parent());
  EXPECT_EQ(FloatSize(413, 317), scrollerProperties->scroll()->clip());
  EXPECT_EQ(FloatSize(660, 10200), scrollerProperties->scroll()->bounds());
  EXPECT_FALSE(scrollerProperties->scroll()->userScrollableHorizontal());
  EXPECT_TRUE(scrollerProperties->scroll()->userScrollableVertical());
  EXPECT_EQ(FloatRoundedRect(120, 340, 413, 317),
            scrollerProperties->overflowClip()->clipRect());
  EXPECT_EQ(frameContentClip(), scrollerProperties->overflowClip()->parent());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(120, 340, 413, 317),
                          scroller->layoutObject(), frameView->layoutView());

  // The relative-positioned element should have accumulated box offset (exclude
  // scrolling), and should be affected by ancestor scroll transforms.
  Element* relPos = document().getElementById("rel-pos");
  const ObjectPaintProperties* relPosProperties =
      relPos->layoutObject()->paintProperties();
  EXPECT_EQ(TransformationMatrix().translate(680, 1120),
            relPosProperties->paintOffsetTranslation()->matrix());
  EXPECT_EQ(scrollerProperties->scrollTranslation(),
            relPosProperties->paintOffsetTranslation()->parent());
  EXPECT_EQ(relPosProperties->transform(),
            relPosProperties->overflowClip()->localTransformSpace());
  EXPECT_EQ(FloatRoundedRect(0, 0, 100, 200),
            relPosProperties->overflowClip()->clipRect());
  EXPECT_EQ(scrollerProperties->overflowClip(),
            relPosProperties->overflowClip()->parent());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(), relPos->layoutObject(),
                          frameView->layoutView());

  // The absolute-positioned element should not be affected by non-positioned
  // scroller at all.
  Element* absPos = document().getElementById("abs-pos");
  const ObjectPaintProperties* absPosProperties =
      absPos->layoutObject()->paintProperties();
  EXPECT_EQ(TransformationMatrix().translate(123, 456),
            absPosProperties->paintOffsetTranslation()->matrix());
  EXPECT_EQ(frameScrollTranslation(),
            absPosProperties->paintOffsetTranslation()->parent());
  EXPECT_EQ(absPosProperties->transform(),
            absPosProperties->overflowClip()->localTransformSpace());
  EXPECT_EQ(FloatRoundedRect(0, 0, 300, 400),
            absPosProperties->overflowClip()->clipRect());
  EXPECT_EQ(frameContentClip(), absPosProperties->overflowClip()->parent());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(123, 456, 300, 400),
                          absPos->layoutObject(), frameView->layoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, FrameScrollingTraditional) {
  setBodyInnerHTML("<style> body { height: 10000px; } </style>");

  document().domWindow()->scrollTo(0, 100);

  FrameView* frameView = document().view();
  frameView->updateAllLifecyclePhases();
  EXPECT_EQ(TransformationMatrix(), framePreTranslation()->matrix());
  EXPECT_TRUE(framePreTranslation()->parent()->isRoot());

  EXPECT_EQ(TransformationMatrix().translate(0, -100),
            frameScrollTranslation()->matrix());
  EXPECT_EQ(framePreTranslation(), frameScrollTranslation()->parent());
  EXPECT_EQ(framePreTranslation(), frameContentClip()->localTransformSpace());
  EXPECT_EQ(FloatRoundedRect(0, 0, 800, 600), frameContentClip()->clipRect());
  EXPECT_TRUE(frameContentClip()->parent()->isRoot());

  if (!RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
    const auto* viewProperties = frameView->layoutView()->paintProperties();
    EXPECT_EQ(nullptr, viewProperties->scrollTranslation());
  }
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 784, 10000),
                          document().body()->layoutObject(),
                          frameView->layoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, Perspective) {
  loadTestData("perspective.html");

  Element* perspective = document().getElementById("perspective");
  const ObjectPaintProperties* perspectiveProperties =
      perspective->layoutObject()->paintProperties();
  EXPECT_EQ(TransformationMatrix().applyPerspective(100),
            perspectiveProperties->perspective()->matrix());
  // The perspective origin is the center of the border box plus accumulated
  // paint offset.
  EXPECT_EQ(FloatPoint3D(250, 250, 0),
            perspectiveProperties->perspective()->origin());
  EXPECT_EQ(framePreTranslation(),
            perspectiveProperties->perspective()->parent());

  // Adding perspective doesn't clear paint offset. The paint offset will be
  // passed down to children.
  Element* inner = document().getElementById("inner");
  const ObjectPaintProperties* innerProperties =
      inner->layoutObject()->paintProperties();
  EXPECT_EQ(TransformationMatrix().translate(50, 100),
            innerProperties->paintOffsetTranslation()->matrix());
  EXPECT_EQ(perspectiveProperties->perspective(),
            innerProperties->paintOffsetTranslation()->parent());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(50, 100, 100, 200), inner->layoutObject(),
                          document().view()->layoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, Transform) {
  loadTestData("transform.html");

  Element* transform = document().getElementById("transform");
  const ObjectPaintProperties* transformProperties =
      transform->layoutObject()->paintProperties();
  EXPECT_EQ(TransformationMatrix().translate3d(123, 456, 789),
            transformProperties->transform()->matrix());
  EXPECT_EQ(FloatPoint3D(200, 150, 0),
            transformProperties->transform()->origin());
  EXPECT_EQ(transformProperties->paintOffsetTranslation(),
            transformProperties->transform()->parent());
  EXPECT_EQ(TransformationMatrix().translate(50, 100),
            transformProperties->paintOffsetTranslation()->matrix());
  EXPECT_EQ(frameScrollTranslation(),
            transformProperties->paintOffsetTranslation()->parent());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(173, 556, 400, 300),
                          transform->layoutObject(),
                          document().view()->layoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, RelativePositionInline) {
  loadTestData("relative-position-inline.html");

  Element* inlineBlock = document().getElementById("inline-block");
  const ObjectPaintProperties* inlineBlockProperties =
      inlineBlock->layoutObject()->paintProperties();
  EXPECT_EQ(TransformationMatrix().translate(135, 490),
            inlineBlockProperties->paintOffsetTranslation()->matrix());
  EXPECT_EQ(framePreTranslation(),
            inlineBlockProperties->paintOffsetTranslation()->parent());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(135, 490, 10, 20),
                          inlineBlock->layoutObject(),
                          document().view()->layoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, NestedOpacityEffect) {
  setBodyInnerHTML(
      "<div id='nodeWithoutOpacity' style='width: 100px; height: 200px'>"
      "  <div id='childWithOpacity' style='opacity: 0.5; width: 50px; height: "
      "60px;'>"
      "    <div id='grandChildWithoutOpacity' style='width: 20px; height: "
      "30px'>"
      "      <div id='greatGrandChildWithOpacity' style='opacity: 0.2; width: "
      "10px; height: 15px'/>"
      "    </div>"
      "  </div>"
      "</div>");

  LayoutObject* nodeWithoutOpacity =
      document().getElementById("nodeWithoutOpacity")->layoutObject();
  const ObjectPaintProperties* nodeWithoutOpacityProperties =
      nodeWithoutOpacity->paintProperties();
  EXPECT_NE(nullptr, nodeWithoutOpacityProperties);
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 100, 200), nodeWithoutOpacity,
                          document().view()->layoutView());

  LayoutObject* childWithOpacity =
      document().getElementById("childWithOpacity")->layoutObject();
  const ObjectPaintProperties* childWithOpacityProperties =
      childWithOpacity->paintProperties();
  EXPECT_EQ(0.5f, childWithOpacityProperties->effect()->opacity());
  // childWithOpacity is the root effect node.
  EXPECT_NE(nullptr, childWithOpacityProperties->effect()->parent());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 50, 60), childWithOpacity,
                          document().view()->layoutView());

  LayoutObject* grandChildWithoutOpacity =
      document().getElementById("grandChildWithoutOpacity")->layoutObject();
  EXPECT_NE(nullptr, grandChildWithoutOpacity->paintProperties());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 20, 30), grandChildWithoutOpacity,
                          document().view()->layoutView());

  LayoutObject* greatGrandChildWithOpacity =
      document().getElementById("greatGrandChildWithOpacity")->layoutObject();
  const ObjectPaintProperties* greatGrandChildWithOpacityProperties =
      greatGrandChildWithOpacity->paintProperties();
  EXPECT_EQ(0.2f, greatGrandChildWithOpacityProperties->effect()->opacity());
  EXPECT_EQ(childWithOpacityProperties->effect(),
            greatGrandChildWithOpacityProperties->effect()->parent());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 10, 15), greatGrandChildWithOpacity,
                          document().view()->layoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, TransformNodeDoesNotAffectEffectNodes) {
  setBodyInnerHTML(
      "<div id='nodeWithOpacity' style='opacity: 0.6' style='width: 100px; "
      "height: 200px'>"
      "  <div id='childWithTransform' style='transform: translate3d(10px, "
      "10px, 0px); width: 50px; height: 60px;'>"
      "    <div id='grandChildWithOpacity' style='opacity: 0.4; width: 20px; "
      "height: 30px'/>"
      "  </div>"
      "</div>");

  LayoutObject* nodeWithOpacity =
      document().getElementById("nodeWithOpacity")->layoutObject();
  const ObjectPaintProperties* nodeWithOpacityProperties =
      nodeWithOpacity->paintProperties();
  EXPECT_EQ(0.6f, nodeWithOpacityProperties->effect()->opacity());
  EXPECT_NE(nullptr, nodeWithOpacityProperties->effect()->parent());
  EXPECT_EQ(nullptr, nodeWithOpacityProperties->transform());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 784, 60), nodeWithOpacity,
                          document().view()->layoutView());

  LayoutObject* childWithTransform =
      document().getElementById("childWithTransform")->layoutObject();
  const ObjectPaintProperties* childWithTransformProperties =
      childWithTransform->paintProperties();
  EXPECT_EQ(nullptr, childWithTransformProperties->effect());
  EXPECT_EQ(TransformationMatrix().translate(10, 10),
            childWithTransformProperties->transform()->matrix());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(18, 18, 50, 60), childWithTransform,
                          document().view()->layoutView());

  LayoutObject* grandChildWithOpacity =
      document().getElementById("grandChildWithOpacity")->layoutObject();
  const ObjectPaintProperties* grandChildWithOpacityProperties =
      grandChildWithOpacity->paintProperties();
  EXPECT_EQ(0.4f, grandChildWithOpacityProperties->effect()->opacity());
  EXPECT_EQ(nodeWithOpacityProperties->effect(),
            grandChildWithOpacityProperties->effect()->parent());
  EXPECT_EQ(nullptr, grandChildWithOpacityProperties->transform());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(18, 18, 20, 30), grandChildWithOpacity,
                          document().view()->layoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, EffectNodesAcrossStackingContext) {
  setBodyInnerHTML(
      "<div id='nodeWithOpacity' style='opacity: 0.6; width: 100px; height: "
      "200px'>"
      "  <div id='childWithStackingContext' style='position:absolute; width: "
      "50px; height: 60px;'>"
      "    <div id='grandChildWithOpacity' style='opacity: 0.4; width: 20px; "
      "height: 30px'/>"
      "  </div>"
      "</div>");

  LayoutObject* nodeWithOpacity =
      document().getElementById("nodeWithOpacity")->layoutObject();
  const ObjectPaintProperties* nodeWithOpacityProperties =
      nodeWithOpacity->paintProperties();
  EXPECT_EQ(0.6f, nodeWithOpacityProperties->effect()->opacity());
  EXPECT_NE(nullptr, nodeWithOpacityProperties->effect()->parent());
  EXPECT_EQ(nullptr, nodeWithOpacityProperties->transform());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 100, 200), nodeWithOpacity,
                          document().view()->layoutView());

  LayoutObject* childWithStackingContext =
      document().getElementById("childWithStackingContext")->layoutObject();
  const ObjectPaintProperties* childWithStackingContextProperties =
      childWithStackingContext->paintProperties();
  EXPECT_EQ(nullptr, childWithStackingContextProperties->effect());
  EXPECT_EQ(nullptr, childWithStackingContextProperties->transform());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 50, 60), childWithStackingContext,
                          document().view()->layoutView());

  LayoutObject* grandChildWithOpacity =
      document().getElementById("grandChildWithOpacity")->layoutObject();
  const ObjectPaintProperties* grandChildWithOpacityProperties =
      grandChildWithOpacity->paintProperties();
  EXPECT_EQ(0.4f, grandChildWithOpacityProperties->effect()->opacity());
  EXPECT_EQ(nodeWithOpacityProperties->effect(),
            grandChildWithOpacityProperties->effect()->parent());
  EXPECT_EQ(nullptr, grandChildWithOpacityProperties->transform());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 20, 30), grandChildWithOpacity,
                          document().view()->layoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, EffectNodesInSVG) {
  setBodyInnerHTML(
      "<svg id='svgRoot'>"
      "  <g id='groupWithOpacity' opacity='0.6'>"
      "    <rect id='rectWithoutOpacity' />"
      "    <rect id='rectWithOpacity' opacity='0.4' />"
      "    <text id='textWithOpacity' opacity='0.2'>"
      "      <tspan id='tspanWithOpacity' opacity='0.1' />"
      "    </text>"
      "  </g>"
      "</svg>");

  LayoutObject* groupWithOpacity =
      document().getElementById("groupWithOpacity")->layoutObject();
  const ObjectPaintProperties* groupWithOpacityProperties =
      groupWithOpacity->paintProperties();
  EXPECT_EQ(0.6f, groupWithOpacityProperties->effect()->opacity());
  EXPECT_NE(nullptr, groupWithOpacityProperties->effect()->parent());

  LayoutObject& rectWithoutOpacity =
      *document().getElementById("rectWithoutOpacity")->layoutObject();
  const ObjectPaintProperties* rectWithoutOpacityProperties =
      rectWithoutOpacity.paintProperties();
  EXPECT_EQ(nullptr, rectWithoutOpacityProperties);

  LayoutObject& rectWithOpacity =
      *document().getElementById("rectWithOpacity")->layoutObject();
  const ObjectPaintProperties* rectWithOpacityProperties =
      rectWithOpacity.paintProperties();
  EXPECT_EQ(0.4f, rectWithOpacityProperties->effect()->opacity());
  EXPECT_EQ(groupWithOpacityProperties->effect(),
            rectWithOpacityProperties->effect()->parent());

  // Ensure that opacity nodes are created for LayoutSVGText which inherits from
  // LayoutSVGBlock instead of LayoutSVGModelObject.
  LayoutObject& textWithOpacity =
      *document().getElementById("textWithOpacity")->layoutObject();
  const ObjectPaintProperties* textWithOpacityProperties =
      textWithOpacity.paintProperties();
  EXPECT_EQ(0.2f, textWithOpacityProperties->effect()->opacity());
  EXPECT_EQ(groupWithOpacityProperties->effect(),
            textWithOpacityProperties->effect()->parent());

  // Ensure that opacity nodes are created for LayoutSVGTSpan which inherits
  // from LayoutSVGInline instead of LayoutSVGModelObject.
  LayoutObject& tspanWithOpacity =
      *document().getElementById("tspanWithOpacity")->layoutObject();
  const ObjectPaintProperties* tspanWithOpacityProperties =
      tspanWithOpacity.paintProperties();
  EXPECT_EQ(0.1f, tspanWithOpacityProperties->effect()->opacity());
  EXPECT_EQ(textWithOpacityProperties->effect(),
            tspanWithOpacityProperties->effect()->parent());
}

TEST_P(PaintPropertyTreeBuilderTest, EffectNodesAcrossHTMLSVGBoundary) {
  setBodyInnerHTML(
      "<div id='divWithOpacity' style='opacity: 0.2;'>"
      "  <svg id='svgRootWithOpacity' style='opacity: 0.3;'>"
      "    <rect id='rectWithOpacity' opacity='0.4' />"
      "  </svg>"
      "</div>");

  LayoutObject& divWithOpacity =
      *document().getElementById("divWithOpacity")->layoutObject();
  const ObjectPaintProperties* divWithOpacityProperties =
      divWithOpacity.paintProperties();
  EXPECT_EQ(0.2f, divWithOpacityProperties->effect()->opacity());
  EXPECT_NE(nullptr, divWithOpacityProperties->effect()->parent());

  LayoutObject& svgRootWithOpacity =
      *document().getElementById("svgRootWithOpacity")->layoutObject();
  const ObjectPaintProperties* svgRootWithOpacityProperties =
      svgRootWithOpacity.paintProperties();
  EXPECT_EQ(0.3f, svgRootWithOpacityProperties->effect()->opacity());
  EXPECT_EQ(divWithOpacityProperties->effect(),
            svgRootWithOpacityProperties->effect()->parent());

  LayoutObject& rectWithOpacity =
      *document().getElementById("rectWithOpacity")->layoutObject();
  const ObjectPaintProperties* rectWithOpacityProperties =
      rectWithOpacity.paintProperties();
  EXPECT_EQ(0.4f, rectWithOpacityProperties->effect()->opacity());
  EXPECT_EQ(svgRootWithOpacityProperties->effect(),
            rectWithOpacityProperties->effect()->parent());
}

TEST_P(PaintPropertyTreeBuilderTest, EffectNodesAcrossSVGHTMLBoundary) {
  setBodyInnerHTML(
      "<svg id='svgRootWithOpacity' style='opacity: 0.3;'>"
      "  <foreignObject id='foreignObjectWithOpacity' opacity='0.4'>"
      "    <body>"
      "      <span id='spanWithOpacity' style='opacity: 0.5'/>"
      "    </body>"
      "  </foreignObject>"
      "</svg>");

  LayoutObject& svgRootWithOpacity =
      *document().getElementById("svgRootWithOpacity")->layoutObject();
  const ObjectPaintProperties* svgRootWithOpacityProperties =
      svgRootWithOpacity.paintProperties();
  EXPECT_EQ(0.3f, svgRootWithOpacityProperties->effect()->opacity());
  EXPECT_NE(nullptr, svgRootWithOpacityProperties->effect()->parent());

  LayoutObject& foreignObjectWithOpacity =
      *document().getElementById("foreignObjectWithOpacity")->layoutObject();
  const ObjectPaintProperties* foreignObjectWithOpacityProperties =
      foreignObjectWithOpacity.paintProperties();
  EXPECT_EQ(0.4f, foreignObjectWithOpacityProperties->effect()->opacity());
  EXPECT_EQ(svgRootWithOpacityProperties->effect(),
            foreignObjectWithOpacityProperties->effect()->parent());

  LayoutObject& spanWithOpacity =
      *document().getElementById("spanWithOpacity")->layoutObject();
  const ObjectPaintProperties* spanWithOpacityProperties =
      spanWithOpacity.paintProperties();
  EXPECT_EQ(0.5f, spanWithOpacityProperties->effect()->opacity());
  EXPECT_EQ(foreignObjectWithOpacityProperties->effect(),
            spanWithOpacityProperties->effect()->parent());
}

TEST_P(PaintPropertyTreeBuilderTest, TransformNodesInSVG) {
  setBodyInnerHTML(
      "<style>"
      "  body {"
      "    margin: 0px;"
      "  }"
      "  svg {"
      "    margin-left: 50px;"
      "    transform: translate3d(1px, 2px, 3px);"
      "    position: absolute;"
      "    left: 20px;"
      "    top: 25px;"
      "  }"
      "  rect {"
      "    transform: translate(100px, 100px) rotate(45deg);"
      "    transform-origin: 50px 25px;"
      "  }"
      "</style>"
      "<svg id='svgRootWith3dTransform' width='100px' height='100px'>"
      "  <rect id='rectWith2dTransform' width='100px' height='100px' />"
      "</svg>");

  LayoutObject& svgRootWith3dTransform =
      *document().getElementById("svgRootWith3dTransform")->layoutObject();
  const ObjectPaintProperties* svgRootWith3dTransformProperties =
      svgRootWith3dTransform.paintProperties();
  EXPECT_EQ(TransformationMatrix().translate3d(1, 2, 3),
            svgRootWith3dTransformProperties->transform()->matrix());
  EXPECT_EQ(FloatPoint3D(50, 50, 0),
            svgRootWith3dTransformProperties->transform()->origin());
  EXPECT_EQ(svgRootWith3dTransformProperties->paintOffsetTranslation(),
            svgRootWith3dTransformProperties->transform()->parent());
  EXPECT_EQ(
      TransformationMatrix().translate(70, 25),
      svgRootWith3dTransformProperties->paintOffsetTranslation()->matrix());
  EXPECT_EQ(
      framePreTranslation(),
      svgRootWith3dTransformProperties->paintOffsetTranslation()->parent());

  LayoutObject& rectWith2dTransform =
      *document().getElementById("rectWith2dTransform")->layoutObject();
  const ObjectPaintProperties* rectWith2dTransformProperties =
      rectWith2dTransform.paintProperties();
  TransformationMatrix matrix;
  matrix.translate(100, 100);
  matrix.rotate(45);
  // SVG's transform origin is baked into the transform.
  matrix.applyTransformOrigin(50, 25, 0);
  EXPECT_EQ(matrix, rectWith2dTransformProperties->transform()->matrix());
  EXPECT_EQ(FloatPoint3D(0, 0, 0),
            rectWith2dTransformProperties->transform()->origin());
  // SVG does not use paint offset.
  EXPECT_EQ(nullptr, rectWith2dTransformProperties->paintOffsetTranslation());
}

TEST_P(PaintPropertyTreeBuilderTest, SVGViewBoxTransform) {
  setBodyInnerHTML(
      "<style>"
      "  body {"
      "    margin: 0px;"
      "  }"
      "  svg {"
      "    transform: translate3d(1px, 2px, 3px);"
      "    position: absolute;"
      "  }"
      "  rect {"
      "    transform: translate(100px, 100px);"
      "  }"
      "</style>"
      "<svg id='svgWithViewBox' width='100px' height='100px' viewBox='50 50 "
      "100 100'>"
      "  <rect id='rect' width='100px' height='100px' />"
      "</svg>");

  LayoutObject& svgWithViewBox =
      *document().getElementById("svgWithViewBox")->layoutObject();
  const ObjectPaintProperties* svgWithViewBoxProperties =
      svgWithViewBox.paintProperties();
  EXPECT_EQ(TransformationMatrix().translate3d(1, 2, 3),
            svgWithViewBoxProperties->transform()->matrix());
  EXPECT_EQ(TransformationMatrix().translate(-50, -50),
            svgWithViewBoxProperties->svgLocalToBorderBoxTransform()->matrix());
  EXPECT_EQ(svgWithViewBoxProperties->svgLocalToBorderBoxTransform()->parent(),
            svgWithViewBoxProperties->transform());

  LayoutObject& rect = *document().getElementById("rect")->layoutObject();
  const ObjectPaintProperties* rectProperties = rect.paintProperties();
  EXPECT_EQ(TransformationMatrix().translate(100, 100),
            rectProperties->transform()->matrix());
  EXPECT_EQ(svgWithViewBoxProperties->svgLocalToBorderBoxTransform(),
            rectProperties->transform()->parent());
}

TEST_P(PaintPropertyTreeBuilderTest, SVGRootPaintOffsetTransformNode) {
  setBodyInnerHTML(
      "<style>body { margin: 0px; } </style>"
      "<svg id='svg' style='margin-left: 50px; margin-top: 25px; width: 100px; "
      "height: 100px;' />");

  LayoutObject& svg = *document().getElementById("svg")->layoutObject();
  const ObjectPaintProperties* svgProperties = svg.paintProperties();
  // Ensure that a paint offset transform is not unnecessarily emitted.
  EXPECT_EQ(nullptr, svgProperties->paintOffsetTranslation());
  EXPECT_EQ(TransformationMatrix().translate(50, 25),
            svgProperties->svgLocalToBorderBoxTransform()->matrix());
  EXPECT_EQ(framePreTranslation(),
            svgProperties->svgLocalToBorderBoxTransform()->parent());
}

TEST_P(PaintPropertyTreeBuilderTest, SVGRootLocalToBorderBoxTransformNode) {
  setBodyInnerHTML(
      "<style>"
      "  body { margin: 0px; }"
      "  svg { margin-left: 2px; margin-top: 3px; transform: translate(5px, "
      "7px); border: 11px solid green; }"
      "</style>"
      "<svg id='svg' width='100px' height='100px' viewBox='0 0 13 13'>"
      "  <rect id='rect' transform='translate(17 19)' />"
      "</svg>");

  LayoutObject& svg = *document().getElementById("svg")->layoutObject();
  const ObjectPaintProperties* svgProperties = svg.paintProperties();
  EXPECT_EQ(TransformationMatrix().translate(2, 3),
            svgProperties->paintOffsetTranslation()->matrix());
  EXPECT_EQ(TransformationMatrix().translate(5, 7),
            svgProperties->transform()->matrix());
  EXPECT_EQ(TransformationMatrix().translate(11, 11).scale(100.0 / 13.0),
            svgProperties->svgLocalToBorderBoxTransform()->matrix());
  EXPECT_EQ(svgProperties->paintOffsetTranslation(),
            svgProperties->transform()->parent());
  EXPECT_EQ(svgProperties->transform(),
            svgProperties->svgLocalToBorderBoxTransform()->parent());

  // Ensure the rect's transform is a child of the local to border box
  // transform.
  LayoutObject& rect = *document().getElementById("rect")->layoutObject();
  const ObjectPaintProperties* rectProperties = rect.paintProperties();
  EXPECT_EQ(TransformationMatrix().translate(17, 19),
            rectProperties->transform()->matrix());
  EXPECT_EQ(svgProperties->svgLocalToBorderBoxTransform(),
            rectProperties->transform()->parent());
}

TEST_P(PaintPropertyTreeBuilderTest, SVGNestedViewboxTransforms) {
  setBodyInnerHTML(
      "<style>body { margin: 0px; } </style>"
      "<svg id='svg' width='100px' height='100px' viewBox='0 0 50 50' "
      "style='transform: translate(11px, 11px);'>"
      "  <svg id='nestedSvg' width='50px' height='50px' viewBox='0 0 5 5'>"
      "    <rect id='rect' transform='translate(13 13)' />"
      "  </svg>"
      "</svg>");

  LayoutObject& svg = *document().getElementById("svg")->layoutObject();
  const ObjectPaintProperties* svgProperties = svg.paintProperties();
  EXPECT_EQ(TransformationMatrix().translate(11, 11),
            svgProperties->transform()->matrix());
  EXPECT_EQ(TransformationMatrix().scale(2),
            svgProperties->svgLocalToBorderBoxTransform()->matrix());

  LayoutObject& nestedSvg =
      *document().getElementById("nestedSvg")->layoutObject();
  const ObjectPaintProperties* nestedSvgProperties =
      nestedSvg.paintProperties();
  EXPECT_EQ(TransformationMatrix().scale(10),
            nestedSvgProperties->transform()->matrix());
  EXPECT_EQ(nullptr, nestedSvgProperties->svgLocalToBorderBoxTransform());
  EXPECT_EQ(svgProperties->svgLocalToBorderBoxTransform(),
            nestedSvgProperties->transform()->parent());

  LayoutObject& rect = *document().getElementById("rect")->layoutObject();
  const ObjectPaintProperties* rectProperties = rect.paintProperties();
  EXPECT_EQ(TransformationMatrix().translate(13, 13),
            rectProperties->transform()->matrix());
  EXPECT_EQ(nestedSvgProperties->transform(),
            rectProperties->transform()->parent());
}

TEST_P(PaintPropertyTreeBuilderTest, TransformNodesAcrossSVGHTMLBoundary) {
  setBodyInnerHTML(
      "<style> body { margin: 0px; } </style>"
      "<svg id='svgWithTransform' style='transform: translate3d(1px, 2px, "
      "3px);'>"
      "  <foreignObject>"
      "    <body>"
      "      <div id='divWithTransform' style='transform: translate3d(3px, "
      "4px, 5px);'></div>"
      "    </body>"
      "  </foreignObject>"
      "</svg>");

  LayoutObject& svgWithTransform =
      *document().getElementById("svgWithTransform")->layoutObject();
  const ObjectPaintProperties* svgWithTransformProperties =
      svgWithTransform.paintProperties();
  EXPECT_EQ(TransformationMatrix().translate3d(1, 2, 3),
            svgWithTransformProperties->transform()->matrix());

  LayoutObject& divWithTransform =
      *document().getElementById("divWithTransform")->layoutObject();
  const ObjectPaintProperties* divWithTransformProperties =
      divWithTransform.paintProperties();
  EXPECT_EQ(TransformationMatrix().translate3d(3, 4, 5),
            divWithTransformProperties->transform()->matrix());
  // Ensure the div's transform node is a child of the svg's transform node.
  EXPECT_EQ(svgWithTransformProperties->transform(),
            divWithTransformProperties->transform()->parent());
}

TEST_P(PaintPropertyTreeBuilderTest,
       FixedTransformAncestorAcrossSVGHTMLBoundary) {
  setBodyInnerHTML(
      "<style> body { margin: 0px; } </style>"
      "<svg id='svg' style='transform: translate3d(1px, 2px, 3px);'>"
      "  <g id='container' transform='translate(20 30)'>"
      "    <foreignObject>"
      "      <body>"
      "        <div id='fixed' style='position: fixed; left: 200px; top: "
      "150px;'></div>"
      "      </body>"
      "    </foreignObject>"
      "  </g>"
      "</svg>");

  LayoutObject& svg = *document().getElementById("svg")->layoutObject();
  const ObjectPaintProperties* svgProperties = svg.paintProperties();
  EXPECT_EQ(TransformationMatrix().translate3d(1, 2, 3),
            svgProperties->transform()->matrix());

  LayoutObject& container =
      *document().getElementById("container")->layoutObject();
  const ObjectPaintProperties* containerProperties =
      container.paintProperties();
  EXPECT_EQ(TransformationMatrix().translate(20, 30),
            containerProperties->transform()->matrix());
  EXPECT_EQ(svgProperties->transform(),
            containerProperties->transform()->parent());

  Element* fixed = document().getElementById("fixed");
  const ObjectPaintProperties* fixedProperties =
      fixed->layoutObject()->paintProperties();
  EXPECT_EQ(TransformationMatrix().translate(200, 150),
            fixedProperties->paintOffsetTranslation()->matrix());
  // Ensure the fixed position element is rooted at the nearest transform
  // container.
  EXPECT_EQ(containerProperties->transform(),
            fixedProperties->paintOffsetTranslation()->parent());
}

TEST_P(PaintPropertyTreeBuilderTest, ControlClip) {
  setBodyInnerHTML(
      "<style>"
      "  body {"
      "    margin: 0;"
      "  }"
      "  input {"
      "    border-width: 5px;"
      "    padding: 0;"
      "  }"
      "</style>"
      "<input id='button' type='button' style='width:345px; height:123px' "
      "value='some text'/>");

  LayoutObject& button = *document().getElementById("button")->layoutObject();
  const ObjectPaintProperties* buttonProperties = button.paintProperties();
  // No scroll translation because the document does not scroll (not enough
  // content).
  EXPECT_TRUE(!frameScrollTranslation());
  EXPECT_EQ(framePreTranslation(),
            buttonProperties->overflowClip()->localTransformSpace());
  EXPECT_EQ(FloatRoundedRect(5, 5, 335, 113),
            buttonProperties->overflowClip()->clipRect());
  EXPECT_EQ(frameContentClip(), buttonProperties->overflowClip()->parent());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(0, 0, 345, 123), &button,
                          document().view()->layoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, BorderRadiusClip) {
  setBodyInnerHTML(
      "<style>"
      " body {"
      "   margin: 0px;"
      " }"
      " #div {"
      "   border-radius: 12px 34px 56px 78px;"
      "   border-top: 45px solid;"
      "   border-right: 50px solid;"
      "   border-bottom: 55px solid;"
      "   border-left: 60px solid;"
      "   width: 500px;"
      "   height: 400px;"
      "   overflow: scroll;"
      " }"
      "</style>"
      "<div id='div'></div>");

  LayoutObject& div = *document().getElementById("div")->layoutObject();
  const ObjectPaintProperties* divProperties = div.paintProperties();
  // No scroll translation because the document does not scroll (not enough
  // content).
  EXPECT_TRUE(!frameScrollTranslation());
  EXPECT_EQ(framePreTranslation(),
            divProperties->overflowClip()->localTransformSpace());
  // The overflow clip rect includes only the padding box.
  // padding box = border box(500+60+50, 400+45+55) - border outset(60+50,
  // 45+55) - scrollbars(15, 15)
  EXPECT_EQ(FloatRoundedRect(60, 45, 500, 400),
            divProperties->overflowClip()->clipRect());
  const ClipPaintPropertyNode* borderRadiusClip =
      divProperties->overflowClip()->parent();
  EXPECT_EQ(framePreTranslation(), borderRadiusClip->localTransformSpace());
  // The border radius clip is the area enclosed by inner border edge, including
  // the scrollbars.  As the border-radius is specified in outer radius, the
  // inner radius is calculated by:
  // inner radius = max(outer radius - border width, 0)
  // In the case that two adjacent borders have different width, the inner
  // radius of the corner may transition from one value to the other. i.e. being
  // an ellipse.
  EXPECT_EQ(
      FloatRoundedRect(
          FloatRect(60, 45, 500,
                    400),  // = border box(610, 500) - border outset(110, 100)
          FloatSize(0,
                    0),  //   (top left)     = max((12, 12) - (60, 45), (0, 0))
          FloatSize(0,
                    0),  //   (top right)    = max((34, 34) - (50, 45), (0, 0))
          FloatSize(18,
                    23),  // (bottom left)  = max((78, 78) - (60, 55), (0, 0))
          FloatSize(6,
                    1)),  //  (bottom right) = max((56, 56) - (50, 55), (0, 0))
      borderRadiusClip->clipRect());
  EXPECT_EQ(frameContentClip(), borderRadiusClip->parent());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(0, 0, 610, 500), &div,
                          document().view()->layoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, TransformNodesAcrossSubframes) {
  setBodyInnerHTML(
      "<style>body { margin: 0; }</style>"
      "<div id='divWithTransform' style='transform: translate3d(1px, 2px, "
      "3px);'>"
      "  <iframe style='border: 7px solid black'></iframe>"
      "</div>");
  setChildFrameHTML(
      "<style>body { margin: 0; }</style><div id='transform' style='transform: "
      "translate3d(4px, 5px, 6px); width: 100px; height: 200px'></div>");

  FrameView* frameView = document().view();
  frameView->updateAllLifecyclePhases();

  LayoutObject* divWithTransform =
      document().getElementById("divWithTransform")->layoutObject();
  const ObjectPaintProperties* divWithTransformProperties =
      divWithTransform->paintProperties();
  EXPECT_EQ(TransformationMatrix().translate3d(1, 2, 3),
            divWithTransformProperties->transform()->matrix());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(1, 2, 800, 164), divWithTransform,
                          frameView->layoutView());

  LayoutObject* innerDivWithTransform =
      childDocument().getElementById("transform")->layoutObject();
  const ObjectPaintProperties* innerDivWithTransformProperties =
      innerDivWithTransform->paintProperties();
  auto* innerDivTransform = innerDivWithTransformProperties->transform();
  EXPECT_EQ(TransformationMatrix().translate3d(4, 5, 6),
            innerDivTransform->matrix());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(12, 14, 100, 145), innerDivWithTransform,
                          frameView->layoutView());

  // Ensure that the inner div's transform is correctly rooted in the root
  // frame's transform tree.
  // This asserts that we have the following tree structure:
  // ...
  //   Transform transform=translation=1.000000,2.000000,3.000000
  //     PreTranslation transform=translation=7.000000,7.000000,0.000000
  //       ScrollTranslation transform=translation=0.000000,0.000000,0.000000
  //         Transform transform=translation=4.000000,5.000000,6.000000
  auto* innerDocumentScrollTranslation = innerDivTransform->parent();
  EXPECT_EQ(TransformationMatrix().translate3d(0, 0, 0),
            innerDocumentScrollTranslation->matrix());
  auto* iframePreTranslation = innerDocumentScrollTranslation->parent();
  EXPECT_EQ(TransformationMatrix().translate3d(7, 7, 0),
            iframePreTranslation->matrix());
  EXPECT_EQ(divWithTransformProperties->transform(),
            iframePreTranslation->parent());
}

TEST_P(PaintPropertyTreeBuilderTest, TransformNodesInTransformedSubframes) {
  setBodyInnerHTML(
      "<style>body { margin: 0; }</style>"
      "<div id='divWithTransform' style='transform: translate3d(1px, 2px, "
      "3px);'>"
      "  <iframe style='transform: translate3d(4px, 5px, 6px); "
      "border: 42px solid; margin: 7px;'></iframe>"
      "</div>");
  setChildFrameHTML(
      "<style>body { margin: 31px; }</style><div "
      "id='transform' style='transform: translate3d(7px, 8px, "
      "9px); width: 100px; height: 200px'></div>");
  FrameView* frameView = document().view();
  frameView->updateAllLifecyclePhases();

  // Assert that we have the following tree structure:
  // ...
  //   Transform transform=translation=1.000000,2.000000,3.000000
  //     PaintOffsetTranslation transform=translation=7.000000,7.000000,0.000000
  //       Transform transform=translation=4.000000,5.000000,6.000000
  //         PreTranslation transform=translation=42.000000,42.000000,0.000000
  //           ScrollTranslation transform=translation=0.000000,0.000000,0.00000
  //             PaintOffsetTranslation transform=translation=31.00,31.00,0.00
  //               Transform transform=translation=7.000000,8.000000,9.000000

  LayoutObject* innerDivWithTransform =
      childDocument().getElementById("transform")->layoutObject();
  auto* innerDivTransform =
      innerDivWithTransform->paintProperties()->transform();
  EXPECT_EQ(TransformationMatrix().translate3d(7, 8, 9),
            innerDivTransform->matrix());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(92, 95, 100, 111), innerDivWithTransform,
                          frameView->layoutView());

  auto* innerDocumentPaintOffsetTranslation = innerDivTransform->parent();
  EXPECT_EQ(TransformationMatrix().translate3d(31, 31, 0),
            innerDocumentPaintOffsetTranslation->matrix());
  auto* innerDocumentScrollTranslation =
      innerDocumentPaintOffsetTranslation->parent();
  EXPECT_EQ(TransformationMatrix().translate3d(0, 0, 0),
            innerDocumentScrollTranslation->matrix());
  auto* iframePreTranslation = innerDocumentScrollTranslation->parent();
  EXPECT_EQ(TransformationMatrix().translate3d(42, 42, 0),
            iframePreTranslation->matrix());
  auto* iframeTransform = iframePreTranslation->parent();
  EXPECT_EQ(TransformationMatrix().translate3d(4, 5, 6),
            iframeTransform->matrix());
  auto* iframePaintOffsetTranslation = iframeTransform->parent();
  EXPECT_EQ(TransformationMatrix().translate3d(7, 7, 0),
            iframePaintOffsetTranslation->matrix());
  auto* divWithTransformTransform = iframePaintOffsetTranslation->parent();
  EXPECT_EQ(TransformationMatrix().translate3d(1, 2, 3),
            divWithTransformTransform->matrix());

  LayoutObject* divWithTransform =
      document().getElementById("divWithTransform")->layoutObject();
  EXPECT_EQ(divWithTransformTransform,
            divWithTransform->paintProperties()->transform());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(1, 2, 800, 248), divWithTransform,
                          frameView->layoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, TreeContextClipByNonStackingContext) {
  // This test verifies the tree builder correctly computes and records the
  // property tree context for a (pseudo) stacking context that is scrolled by a
  // containing block that is not one of the painting ancestors.
  setBodyInnerHTML(
      "<style>body { margin: 0; }</style>"
      "<div id='scroller' style='overflow:scroll; width:400px; height:300px;'>"
      "  <div id='child' style='position:relative; width:100px; height: "
      "200px;'></div>"
      "  <div style='height:10000px;'></div>"
      "</div>");
  FrameView* frameView = document().view();

  LayoutObject* scroller =
      document().getElementById("scroller")->layoutObject();
  const ObjectPaintProperties* scrollerProperties = scroller->paintProperties();
  LayoutObject* child = document().getElementById("child")->layoutObject();
  const ObjectPaintProperties* childProperties = child->paintProperties();

  EXPECT_EQ(
      scrollerProperties->overflowClip(),
      childProperties->localBorderBoxProperties()->propertyTreeState.clip());
  EXPECT_EQ(scrollerProperties->scrollTranslation(),
            childProperties->localBorderBoxProperties()
                ->propertyTreeState.transform());
  EXPECT_NE(
      nullptr,
      childProperties->localBorderBoxProperties()->propertyTreeState.effect());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(0, 0, 400, 300), scroller,
                          frameView->layoutView());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(0, 0, 100, 200), child,
                          frameView->layoutView());
}

TEST_P(PaintPropertyTreeBuilderTest,
       TreeContextUnclipFromParentStackingContext) {
  // This test verifies the tree builder correctly computes and records the
  // property tree context for a (pseudo) stacking context that has a scrolling
  // painting ancestor that is not its containing block (thus should not be
  // scrolled by it).

  setBodyInnerHTML(
      "<style>body { margin: 0; }</style>"
      "<div id='scroller' style='overflow:scroll; opacity:0.5;'>"
      "  <div id='child' style='position:absolute; left:0; top:0; width: "
      "100px; height: 200px'></div>"
      "  <div style='height:10000px;'></div>"
      "</div>");

  LayoutObject& scroller =
      *document().getElementById("scroller")->layoutObject();
  const ObjectPaintProperties* scrollerProperties = scroller.paintProperties();
  LayoutObject& child = *document().getElementById("child")->layoutObject();
  const ObjectPaintProperties* childProperties = child.paintProperties();

  EXPECT_EQ(
      frameContentClip(),
      childProperties->localBorderBoxProperties()->propertyTreeState.clip());
  EXPECT_EQ(frameScrollTranslation(),
            childProperties->localBorderBoxProperties()
                ->propertyTreeState.transform());
  EXPECT_EQ(
      scrollerProperties->effect(),
      childProperties->localBorderBoxProperties()->propertyTreeState.effect());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(0, 0, 800, 10000), &scroller,
                          document().view()->layoutView());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(0, 0, 100, 200), &child,
                          document().view()->layoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, TableCellLayoutLocation) {
  // This test verifies that the border box space of a table cell is being
  // correctly computed.  Table cells have weird location adjustment in our
  // layout/paint implementation.
  setBodyInnerHTML(
      "<style>"
      "  body {"
      "    margin: 0;"
      "  }"
      "  table {"
      "    border-spacing: 0;"
      "    margin: 20px;"
      "    padding: 40px;"
      "    border: 10px solid black;"
      "  }"
      "  td {"
      "    width: 100px;"
      "    height: 100px;"
      "    padding: 0;"
      "  }"
      "  #target {"
      "    position: relative;"
      "    width: 100px;"
      "    height: 100px;"
      "  }"
      "</style>"
      "<table>"
      "  <tr><td></td><td></td></tr>"
      "  <tr><td></td><td><div id='target'></div></td></tr>"
      "</table>");

  LayoutObject& target = *document().getElementById("target")->layoutObject();
  const ObjectPaintProperties* targetProperties = target.paintProperties();

  EXPECT_EQ(LayoutPoint(170, 170),
            targetProperties->localBorderBoxProperties()->paintOffset);
  EXPECT_EQ(framePreTranslation(), targetProperties->localBorderBoxProperties()
                                       ->propertyTreeState.transform());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(170, 170, 100, 100), &target,
                          document().view()->layoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, CSSClipFixedPositionDescendant) {
  // This test verifies that clip tree hierarchy being generated correctly for
  // the hard case such that a fixed position element getting clipped by an
  // absolute position CSS clip.
  setBodyInnerHTML(
      "<style>"
      "  #clip {"
      "    position: absolute;"
      "    left: 123px;"
      "    top: 456px;"
      "    clip: rect(10px, 80px, 70px, 40px);"
      "    width: 100px;"
      "    height: 100px;"
      "  }"
      "  #fixed {"
      "    position: fixed;"
      "    left: 654px;"
      "    top: 321px;"
      "    width: 10px;"
      "    height: 20px"
      "  }"
      "</style>"
      "<div id='clip'><div id='fixed'></div></div>");
  LayoutRect localClipRect(40, 10, 40, 60);
  LayoutRect absoluteClipRect = localClipRect;
  absoluteClipRect.move(123, 456);

  LayoutObject& clip = *document().getElementById("clip")->layoutObject();
  const ObjectPaintProperties* clipProperties = clip.paintProperties();
  EXPECT_EQ(frameContentClip(), clipProperties->cssClip()->parent());
  EXPECT_EQ(framePreTranslation(),
            clipProperties->cssClip()->localTransformSpace());
  EXPECT_EQ(FloatRoundedRect(FloatRect(absoluteClipRect)),
            clipProperties->cssClip()->clipRect());
  CHECK_VISUAL_RECT(absoluteClipRect, &clip, document().view()->layoutView(),
                    // TODO(crbug.com/599939): mapToVisualRectInAncestorSpace()
                    // doesn't apply css clip on the object itself.
                    LayoutUnit::max());

  LayoutObject* fixed = document().getElementById("fixed")->layoutObject();
  const ObjectPaintProperties* fixedProperties = fixed->paintProperties();
  EXPECT_EQ(
      clipProperties->cssClip(),
      fixedProperties->localBorderBoxProperties()->propertyTreeState.clip());
  EXPECT_EQ(framePreTranslation(), fixedProperties->localBorderBoxProperties()
                                       ->propertyTreeState.transform()
                                       ->parent());
  EXPECT_EQ(TransformationMatrix().translate(654, 321),
            fixedProperties->localBorderBoxProperties()
                ->propertyTreeState.transform()
                ->matrix());
  EXPECT_EQ(LayoutPoint(),
            fixedProperties->localBorderBoxProperties()->paintOffset);
  CHECK_VISUAL_RECT(LayoutRect(), fixed, document().view()->layoutView(),
                    // TODO(crbug.com/599939): CSS clip of fixed-position
                    // descendants is broken in
                    // mapToVisualRectInAncestorSpace().
                    LayoutUnit::max());
}

TEST_P(PaintPropertyTreeBuilderTest, CSSClipAbsPositionDescendant) {
  // This test verifies that clip tree hierarchy being generated correctly for
  // the hard case such that a fixed position element getting clipped by an
  // absolute position CSS clip.
  setBodyInnerHTML(
      "<style>"
      "  #clip {"
      "    position: absolute;"
      "    left: 123px;"
      "    top: 456px;"
      "    clip: rect(10px, 80px, 70px, 40px);"
      "    width: 100px;"
      "    height: 100px;"
      "  }"
      "  #abs {"
      "    position: absolute;"
      "    left: 654px;"
      "    top: 321px;"
      "    width: 10px;"
      "    heght: 20px"
      "  }"
      "</style>"
      "<div id='clip'><div id='absolute'></div></div>");

  LayoutRect localClipRect(40, 10, 40, 60);
  LayoutRect absoluteClipRect = localClipRect;
  absoluteClipRect.move(123, 456);

  LayoutObject* clip = document().getElementById("clip")->layoutObject();
  const ObjectPaintProperties* clipProperties = clip->paintProperties();
  EXPECT_EQ(frameContentClip(), clipProperties->cssClip()->parent());
  // No scroll translation because the document does not scroll (not enough
  // content).
  EXPECT_TRUE(!frameScrollTranslation());
  EXPECT_EQ(framePreTranslation(),
            clipProperties->cssClip()->localTransformSpace());
  EXPECT_EQ(FloatRoundedRect(FloatRect(absoluteClipRect)),
            clipProperties->cssClip()->clipRect());
  CHECK_VISUAL_RECT(absoluteClipRect, clip, document().view()->layoutView(),
                    // TODO(crbug.com/599939): mapToVisualRectInAncestorSpace()
                    // doesn't apply css clip on the object itself.
                    LayoutUnit::max());

  LayoutObject* absolute =
      document().getElementById("absolute")->layoutObject();
  const ObjectPaintProperties* absPosProperties = absolute->paintProperties();
  EXPECT_EQ(
      clipProperties->cssClip(),
      absPosProperties->localBorderBoxProperties()->propertyTreeState.clip());
  EXPECT_EQ(framePreTranslation(), absPosProperties->localBorderBoxProperties()
                                       ->propertyTreeState.transform());
  EXPECT_EQ(LayoutPoint(123, 456),
            absPosProperties->localBorderBoxProperties()->paintOffset);
  CHECK_VISUAL_RECT(LayoutRect(), absolute, document().view()->layoutView(),
                    // TODO(crbug.com/599939): CSS clip of fixed-position
                    // descendants is broken in
                    // mapToVisualRectInAncestorSpace().
                    LayoutUnit::max());
}

TEST_P(PaintPropertyTreeBuilderTest, CSSClipFixedPositionDescendantNonShared) {
  // This test is similar to CSSClipFixedPositionDescendant above, except that
  // now we have a parent overflow clip that should be escaped by the fixed
  // descendant.
  setBodyInnerHTML(
      "<style>"
      "  body {"
      "    margin: 0;"
      "  }"
      "  #overflow {"
      "    position: relative;"
      "    width: 50px;"
      "    height: 50px;"
      "    overflow: scroll;"
      "  }"
      "  #clip {"
      "    position: absolute;"
      "    left: 123px;"
      "    top: 456px;"
      "    clip: rect(10px, 80px, 70px, 40px);"
      "    width: 100px;"
      "    height: 100px;"
      "  }"
      "  #fixed {"
      "    position: fixed;"
      "    left: 654px;"
      "    top: 321px;"
      "  }"
      "</style>"
      "<div id='overflow'><div id='clip'><div id='fixed'></div></div></div>");
  LayoutRect localClipRect(40, 10, 40, 60);
  LayoutRect absoluteClipRect = localClipRect;
  absoluteClipRect.move(123, 456);

  LayoutObject& overflow =
      *document().getElementById("overflow")->layoutObject();
  const ObjectPaintProperties* overflowProperties = overflow.paintProperties();
  EXPECT_EQ(frameContentClip(), overflowProperties->overflowClip()->parent());
  // No scroll translation because the document does not scroll (not enough
  // content).
  EXPECT_TRUE(!frameScrollTranslation());
  EXPECT_EQ(framePreTranslation(),
            overflowProperties->scrollTranslation()->parent());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(0, 0, 50, 50), &overflow,
                          document().view()->layoutView());

  LayoutObject* clip = document().getElementById("clip")->layoutObject();
  const ObjectPaintProperties* clipProperties = clip->paintProperties();
  EXPECT_EQ(overflowProperties->overflowClip(),
            clipProperties->cssClip()->parent());
  EXPECT_EQ(overflowProperties->scrollTranslation(),
            clipProperties->cssClip()->localTransformSpace());
  EXPECT_EQ(FloatRoundedRect(FloatRect(absoluteClipRect)),
            clipProperties->cssClip()->clipRect());
  EXPECT_EQ(frameContentClip(),
            clipProperties->cssClipFixedPosition()->parent());
  EXPECT_EQ(overflowProperties->scrollTranslation(),
            clipProperties->cssClipFixedPosition()->localTransformSpace());
  EXPECT_EQ(FloatRoundedRect(FloatRect(absoluteClipRect)),
            clipProperties->cssClipFixedPosition()->clipRect());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(), clip, document().view()->layoutView());

  LayoutObject* fixed = document().getElementById("fixed")->layoutObject();
  const ObjectPaintProperties* fixedProperties = fixed->paintProperties();
  EXPECT_EQ(
      clipProperties->cssClipFixedPosition(),
      fixedProperties->localBorderBoxProperties()->propertyTreeState.clip());
  EXPECT_EQ(framePreTranslation(), fixedProperties->localBorderBoxProperties()
                                       ->propertyTreeState.transform()
                                       ->parent());
  EXPECT_EQ(TransformationMatrix().translate(654, 321),
            fixedProperties->localBorderBoxProperties()
                ->propertyTreeState.transform()
                ->matrix());
  EXPECT_EQ(LayoutPoint(),
            fixedProperties->localBorderBoxProperties()->paintOffset);
  CHECK_VISUAL_RECT(LayoutRect(), fixed, document().view()->layoutView(),
                    // TODO(crbug.com/599939): CSS clip of fixed-position
                    // descendants is broken in geometry mapping.
                    LayoutUnit::max());
}

TEST_P(PaintPropertyTreeBuilderTest, ColumnSpannerUnderRelativePositioned) {
  setBodyInnerHTML(
      "<div style='columns: 3; position: absolute; top: 44px; left: 55px;'>"
      "  <div style='position: relative; top: 100px; left: 100px'>"
      "    <div id='spanner' style='column-span: all; opacity: 0.5; width: "
      "100px; height: 100px;'></div>"
      "  </div>"
      "</div>");

  LayoutObject* spanner = getLayoutObjectByElementId("spanner");
  EXPECT_EQ(
      LayoutPoint(55, 44),
      spanner->paintProperties()->localBorderBoxProperties()->paintOffset);
  CHECK_EXACT_VISUAL_RECT(LayoutRect(55, 44, 100, 100), spanner,
                          document().view()->layoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, FractionalPaintOffset) {
  setBodyInnerHTML(
      "<style>"
      "  * { margin: 0; }"
      "  div { position: absolute; }"
      "</style>"
      "<div id='a' style='width: 70px; height: 70px; left: 0.1px; top: 0.3px;'>"
      "  <div id='b' style='width: 40px; height: 40px; left: 0.5px; top: "
      "11.1px;'></div>"
      "</div>");
  FrameView* frameView = document().view();

  LayoutObject* a = document().getElementById("a")->layoutObject();
  const ObjectPaintProperties* aProperties = a->paintProperties();
  LayoutPoint aPaintOffset = LayoutPoint(FloatPoint(0.1, 0.3));
  EXPECT_EQ(aPaintOffset, aProperties->localBorderBoxProperties()->paintOffset);
  CHECK_EXACT_VISUAL_RECT(LayoutRect(LayoutUnit(0.1), LayoutUnit(0.3),
                                     LayoutUnit(70), LayoutUnit(70)),
                          a, frameView->layoutView());

  LayoutObject* b = document().getElementById("b")->layoutObject();
  const ObjectPaintProperties* bProperties = b->paintProperties();
  LayoutPoint bPaintOffset = aPaintOffset + LayoutPoint(FloatPoint(0.5, 11.1));
  EXPECT_EQ(bPaintOffset, bProperties->localBorderBoxProperties()->paintOffset);
  CHECK_EXACT_VISUAL_RECT(LayoutRect(LayoutUnit(0.1), LayoutUnit(0.3),
                                     LayoutUnit(70), LayoutUnit(70)),
                          a, frameView->layoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, PaintOffsetWithBasicPixelSnapping) {
  setBodyInnerHTML(
      "<style>"
      "  * { margin: 0; }"
      "  div { position: relative; }"
      "</style>"
      "<div id='a' style='width: 70px; height: 70px; left: 0.3px; top: 0.3px;'>"
      "  <div id='b' style='width: 40px; height: 40px; transform: "
      "translateZ(0);'>"
      "    <div id='c' style='width: 40px; height: 40px; left: 0.1px; top: "
      "0.1px;'></div>"
      "  </div>"
      "</div>");
  FrameView* frameView = document().view();

  LayoutObject* b = document().getElementById("b")->layoutObject();
  const ObjectPaintProperties* bProperties = b->paintProperties();
  EXPECT_EQ(TransformationMatrix().translate3d(0, 0, 0),
            bProperties->transform()->matrix());
  // The paint offset transform should be snapped from (0.3,0.3) to (0,0).
  EXPECT_EQ(TransformationMatrix().translate(0, 0),
            bProperties->transform()->parent()->matrix());
  // The residual subpixel adjustment should be (0.3,0.3) - (0,0) = (0.3,0.3).
  LayoutPoint subpixelAccumulation = LayoutPoint(FloatPoint(0.3, 0.3));
  EXPECT_EQ(subpixelAccumulation,
            bProperties->localBorderBoxProperties()->paintOffset);
  CHECK_EXACT_VISUAL_RECT(LayoutRect(FloatRect(0.3, 0.3, 40, 40)), b,
                          frameView->layoutView());

  // c should be painted starting at subpixelAccumulation + (0.1,0.1) =
  // (0.4,0.4).
  LayoutObject* c = document().getElementById("c")->layoutObject();
  LayoutPoint cPaintOffset =
      subpixelAccumulation + LayoutPoint(FloatPoint(0.1, 0.1));
  const ObjectPaintProperties* cProperties = c->paintProperties();
  EXPECT_EQ(cPaintOffset, cProperties->localBorderBoxProperties()->paintOffset);
  // Visual rects via the non-paint properties system use enclosingIntRect
  // before applying transforms, because they are computed bottom-up and
  // therefore can't apply pixel snapping. Therefore apply a slop of 1px.
  CHECK_VISUAL_RECT(LayoutRect(FloatRect(0.4, 0.4, 40, 40)), c,
                    frameView->layoutView(), 1);
}

TEST_P(PaintPropertyTreeBuilderTest,
       PaintOffsetWithPixelSnappingThroughTransform) {
  setBodyInnerHTML(
      "<style>"
      "  * { margin: 0; }"
      "  div { position: relative; }"
      "</style>"
      "<div id='a' style='width: 70px; height: 70px; left: 0.7px; top: 0.7px;'>"
      "  <div id='b' style='width: 40px; height: 40px; transform: "
      "translateZ(0);'>"
      "    <div id='c' style='width: 40px; height: 40px; left: 0.7px; top: "
      "0.7px;'></div>"
      "  </div>"
      "</div>");
  FrameView* frameView = document().view();

  LayoutObject* b = document().getElementById("b")->layoutObject();
  const ObjectPaintProperties* bProperties = b->paintProperties();
  EXPECT_EQ(TransformationMatrix().translate3d(0, 0, 0),
            bProperties->transform()->matrix());
  // The paint offset transform should be snapped from (0.7,0.7) to (1,1).
  EXPECT_EQ(TransformationMatrix().translate(1, 1),
            bProperties->transform()->parent()->matrix());
  // The residual subpixel adjustment should be (0.7,0.7) - (1,1) = (-0.3,-0.3).
  LayoutPoint subpixelAccumulation =
      LayoutPoint(LayoutPoint(FloatPoint(0.7, 0.7)) - LayoutPoint(1, 1));
  EXPECT_EQ(subpixelAccumulation,
            bProperties->localBorderBoxProperties()->paintOffset);
  CHECK_EXACT_VISUAL_RECT(LayoutRect(LayoutUnit(0.7), LayoutUnit(0.7),
                                     LayoutUnit(40), LayoutUnit(40)),
                          b, frameView->layoutView());

  // c should be painted starting at subpixelAccumulation + (0.7,0.7) =
  // (0.4,0.4).
  LayoutObject* c = document().getElementById("c")->layoutObject();
  LayoutPoint cPaintOffset =
      subpixelAccumulation + LayoutPoint(FloatPoint(0.7, 0.7));
  const ObjectPaintProperties* cProperties = c->paintProperties();
  EXPECT_EQ(cPaintOffset, cProperties->localBorderBoxProperties()->paintOffset);
  // Visual rects via the non-paint properties system use enclosingIntRect
  // before applying transforms, because they are computed bottom-up and
  // therefore can't apply pixel snapping. Therefore apply a slop of 1px.
  CHECK_VISUAL_RECT(LayoutRect(LayoutUnit(0.7) + LayoutUnit(0.7),
                               LayoutUnit(0.7) + LayoutUnit(0.7),
                               LayoutUnit(40), LayoutUnit(40)),
                    c, frameView->layoutView(), 1);
}

TEST_P(PaintPropertyTreeBuilderTest,
       PaintOffsetWithPixelSnappingThroughMultipleTransforms) {
  setBodyInnerHTML(
      "<style>"
      "  * { margin: 0; }"
      "  div { position: relative; }"
      "</style>"
      "<div id='a' style='width: 70px; height: 70px; left: 0.7px; top: 0.7px;'>"
      "  <div id='b' style='width: 40px; height: 40px; transform: "
      "translate3d(5px, 7px, 0);'>"
      "    <div id='c' style='width: 40px; height: 40px; transform: "
      "translate3d(11px, 13px, 0);'>"
      "      <div id='d' style='width: 40px; height: 40px; left: 0.7px; top: "
      "0.7px;'></div>"
      "    </div>"
      "  </div>"
      "</div>");
  FrameView* frameView = document().view();

  LayoutObject* b = document().getElementById("b")->layoutObject();
  const ObjectPaintProperties* bProperties = b->paintProperties();
  EXPECT_EQ(TransformationMatrix().translate3d(5, 7, 0),
            bProperties->transform()->matrix());
  // The paint offset transform should be snapped from (0.7,0.7) to (1,1).
  EXPECT_EQ(TransformationMatrix().translate(1, 1),
            bProperties->transform()->parent()->matrix());
  // The residual subpixel adjustment should be (0.7,0.7) - (1,1) = (-0.3,-0.3).
  LayoutPoint subpixelAccumulation =
      LayoutPoint(LayoutPoint(FloatPoint(0.7, 0.7)) - LayoutPoint(1, 1));
  EXPECT_EQ(subpixelAccumulation,
            bProperties->localBorderBoxProperties()->paintOffset);
  CHECK_EXACT_VISUAL_RECT(LayoutRect(LayoutUnit(5.7), LayoutUnit(7.7),
                                     LayoutUnit(40), LayoutUnit(40)),
                          b, frameView->layoutView());

  LayoutObject* c = document().getElementById("c")->layoutObject();
  const ObjectPaintProperties* cProperties = c->paintProperties();
  EXPECT_EQ(TransformationMatrix().translate3d(11, 13, 0),
            cProperties->transform()->matrix());
  // The paint offset should be (-0.3,-0.3) but the paint offset transform
  // should still be at (0,0) because it should be snapped.
  EXPECT_EQ(TransformationMatrix().translate(0, 0),
            cProperties->transform()->parent()->matrix());
  // The residual subpixel adjustment should still be (-0.3,-0.3).
  EXPECT_EQ(subpixelAccumulation,
            cProperties->localBorderBoxProperties()->paintOffset);
  CHECK_EXACT_VISUAL_RECT(LayoutRect(LayoutUnit(16.7), LayoutUnit(20.7),
                                     LayoutUnit(40), LayoutUnit(40)),
                          c, frameView->layoutView());

  // d should be painted starting at subpixelAccumulation + (0.7,0.7) =
  // (0.4,0.4).
  LayoutObject* d = document().getElementById("d")->layoutObject();
  LayoutPoint dPaintOffset =
      subpixelAccumulation + LayoutPoint(FloatPoint(0.7, 0.7));
  const ObjectPaintProperties* dProperties = d->paintProperties();
  EXPECT_EQ(dPaintOffset, dProperties->localBorderBoxProperties()->paintOffset);
  // Visual rects via the non-paint properties system use enclosingIntRect
  // before applying transforms, because they are computed bottom-up and
  // therefore can't apply pixel snapping. Therefore apply a slop of 1px.
  CHECK_VISUAL_RECT(LayoutRect(LayoutUnit(16.7) + LayoutUnit(0.7),
                               LayoutUnit(20.7) + LayoutUnit(0.7),
                               LayoutUnit(40), LayoutUnit(40)),
                    d, frameView->layoutView(), 1);
}

TEST_P(PaintPropertyTreeBuilderTest, PaintOffsetWithPixelSnappingWithFixedPos) {
  setBodyInnerHTML(
      "<style>"
      "  * { margin: 0; }"
      "</style>"
      "<div id='a' style='width: 70px; height: 70px; left: 0.7px; position: "
      "relative;'>"
      "  <div id='b' style='width: 40px; height: 40px; transform: "
      "translateZ(0); position: relative;'>"
      "    <div id='fixed' style='width: 40px; height: 40px; position: fixed;'>"
      "      <div id='d' style='width: 40px; height: 40px; left: 0.7px; "
      "position: relative;'></div>"
      "    </div>"
      "  </div>"
      "</div>");
  FrameView* frameView = document().view();

  LayoutObject* b = document().getElementById("b")->layoutObject();
  const ObjectPaintProperties* bProperties = b->paintProperties();
  EXPECT_EQ(TransformationMatrix().translate(0, 0),
            bProperties->transform()->matrix());
  // The paint offset transform should be snapped from (0.7,0) to (1,0).
  EXPECT_EQ(TransformationMatrix().translate(1, 0),
            bProperties->transform()->parent()->matrix());
  // The residual subpixel adjustment should be (0.7,0) - (1,0) = (-0.3,0).
  LayoutPoint subpixelAccumulation =
      LayoutPoint(LayoutPoint(FloatPoint(0.7, 0)) - LayoutPoint(1, 0));
  EXPECT_EQ(subpixelAccumulation,
            bProperties->localBorderBoxProperties()->paintOffset);
  CHECK_EXACT_VISUAL_RECT(LayoutRect(LayoutUnit(0.7), LayoutUnit(0),
                                     LayoutUnit(40), LayoutUnit(40)),
                          b, frameView->layoutView());

  LayoutObject* fixed = document().getElementById("fixed")->layoutObject();
  const ObjectPaintProperties* fixedProperties = fixed->paintProperties();
  // The residual subpixel adjustment should still be (-0.3,0).
  EXPECT_EQ(subpixelAccumulation,
            fixedProperties->localBorderBoxProperties()->paintOffset);
  CHECK_EXACT_VISUAL_RECT(LayoutRect(LayoutUnit(0.7), LayoutUnit(0),
                                     LayoutUnit(40), LayoutUnit(40)),
                          fixed, frameView->layoutView());

  // d should be painted starting at subpixelAccumulation + (0.7,0) = (0.4,0).
  LayoutObject* d = document().getElementById("d")->layoutObject();
  LayoutPoint dPaintOffset =
      subpixelAccumulation + LayoutPoint(FloatPoint(0.7, 0));
  const ObjectPaintProperties* dProperties = d->paintProperties();
  EXPECT_EQ(dPaintOffset, dProperties->localBorderBoxProperties()->paintOffset);
  // Visual rects via the non-paint properties system use enclosingIntRect
  // before applying transforms, because they are computed bottom-up and
  // therefore can't apply pixel snapping. Therefore apply a slop of 1px.
  CHECK_VISUAL_RECT(LayoutRect(LayoutUnit(0.7) + LayoutUnit(0.7), LayoutUnit(),
                               LayoutUnit(40), LayoutUnit(40)),
                    d, frameView->layoutView(), 1);
}

TEST_P(PaintPropertyTreeBuilderTest, SvgPixelSnappingShouldResetPaintOffset) {
  setBodyInnerHTML(
      "<svg id='svg' style='position: relative; left: 0.1px; transform: "
      "matrix(1, 0, 0, 1, 0, 0);'>"
      "    <rect id='rect' transform='translate(1, 1)'/>"
      "</svg>");

  LayoutObject& svgWithTransform =
      *document().getElementById("svg")->layoutObject();
  const ObjectPaintProperties* svgWithTransformProperties =
      svgWithTransform.paintProperties();
  EXPECT_EQ(TransformationMatrix(),
            svgWithTransformProperties->transform()->matrix());
  EXPECT_EQ(
      LayoutPoint(FloatPoint(0.1, 0)),
      svgWithTransformProperties->localBorderBoxProperties()->paintOffset);
  EXPECT_EQ(nullptr,
            svgWithTransformProperties->svgLocalToBorderBoxTransform());

  LayoutObject& rectWithTransform =
      *document().getElementById("rect")->layoutObject();
  const ObjectPaintProperties* rectWithTransformProperties =
      rectWithTransform.paintProperties();
  EXPECT_EQ(TransformationMatrix().translate(1, 1),
            rectWithTransformProperties->transform()->matrix());

  // Ensure there is no PaintOffset transform between the rect and the svg's
  // transform.
  EXPECT_EQ(svgWithTransformProperties->transform(),
            rectWithTransformProperties->transform()->parent());
}

TEST_P(PaintPropertyTreeBuilderTest, SvgRootAndForeignObjectPixelSnapping) {
  setBodyInnerHTML(
      "<svg id=svg style='position: relative; left: 0.6px; top: 0.3px'>"
      "  <foreignObject id=foreign x='3.5' y='5.4' transform='translate(1,1)'>"
      "    <div id=div style='position: absolute; left: 5.6px; top: 7.3px'>"
      "    </div>"
      "  </foreignObject>"
      "</svg>");

  const auto* svgProperties =
      getLayoutObjectByElementId("svg")->paintProperties();
  EXPECT_EQ(nullptr, svgProperties->paintOffsetTranslation());
  EXPECT_EQ(LayoutPoint(LayoutUnit(8.6), LayoutUnit(8.3)),
            svgProperties->localBorderBoxProperties()->paintOffset);
  // Paint offset of SVGRoot be baked into svgLocalToBorderBoxTransform after
  // snapped to pixels.
  EXPECT_EQ(TransformationMatrix().translate(9, 8),
            svgProperties->svgLocalToBorderBoxTransform()->matrix());

  const auto* foreignObjectProperties =
      getLayoutObjectByElementId("foreign")->paintProperties();
  EXPECT_EQ(nullptr, foreignObjectProperties->paintOffsetTranslation());
  // Paint offset of foreignObject should be originated from SVG root and
  // snapped to pixels.
  EXPECT_EQ(LayoutPoint(4, 5),
            foreignObjectProperties->localBorderBoxProperties()->paintOffset);

  const auto* divProperties =
      getLayoutObjectByElementId("div")->paintProperties();
  // Paint offset of descendant of foreignObject accumulates on paint offset of
  // foreignObject.
  EXPECT_EQ(LayoutPoint(LayoutUnit(4 + 5.6), LayoutUnit(5 + 7.3)),
            divProperties->localBorderBoxProperties()->paintOffset);
}

TEST_P(PaintPropertyTreeBuilderTest, NoRenderingContextByDefault) {
  setBodyInnerHTML("<div style='transform: translateZ(0)'></div>");

  const ObjectPaintProperties* properties =
      document().body()->firstChild()->layoutObject()->paintProperties();
  ASSERT_TRUE(properties->transform());
  EXPECT_FALSE(properties->transform()->hasRenderingContext());
}

TEST_P(PaintPropertyTreeBuilderTest, Preserve3DCreatesSharedRenderingContext) {
  setBodyInnerHTML(
      "<div style='transform-style: preserve-3d'>"
      "  <div id='a' style='transform: translateZ(0); width: 30px; height: "
      "40px'></div>"
      "  <div id='b' style='transform: translateZ(0); width: 20px; height: "
      "10px'></div>"
      "</div>");
  FrameView* frameView = document().view();

  LayoutObject* a = document().getElementById("a")->layoutObject();
  const ObjectPaintProperties* aProperties = a->paintProperties();
  LayoutObject* b = document().getElementById("b")->layoutObject();
  const ObjectPaintProperties* bProperties = b->paintProperties();
  ASSERT_TRUE(aProperties->transform() && bProperties->transform());
  EXPECT_NE(aProperties->transform(), bProperties->transform());
  EXPECT_TRUE(aProperties->transform()->hasRenderingContext());
  EXPECT_TRUE(bProperties->transform()->hasRenderingContext());
  EXPECT_EQ(aProperties->transform()->renderingContextID(),
            bProperties->transform()->renderingContextID());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 30, 40), a, frameView->layoutView());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 48, 20, 10), b,
                          frameView->layoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, FlatTransformStyleEndsRenderingContext) {
  setBodyInnerHTML(
      "<div style='transform-style: preserve-3d'>"
      "  <div id='a' style='transform: translateZ(0); width: 30px; height: "
      "40px'>"
      "    <div id='b' style='transform: translateZ(0); width: 10px; height: "
      "20px'></div>"
      "  </div>"
      "</div>");
  FrameView* frameView = document().view();

  LayoutObject* a = document().getElementById("a")->layoutObject();
  const ObjectPaintProperties* aProperties = a->paintProperties();
  LayoutObject* b = document().getElementById("b")->layoutObject();
  const ObjectPaintProperties* bProperties = b->paintProperties();
  ASSERT_FALSE(a->styleRef().preserves3D());

  ASSERT_TRUE(aProperties->transform() && bProperties->transform());

  // #a should participate in a rendering context (due to its parent), but its
  // child #b should not.
  EXPECT_TRUE(aProperties->transform()->hasRenderingContext());
  EXPECT_FALSE(bProperties->transform()->hasRenderingContext());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 30, 40), a, frameView->layoutView());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 10, 20), b, frameView->layoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, NestedRenderingContexts) {
  setBodyInnerHTML(
      "<div style='transform-style: preserve-3d'>"
      "  <div id='a' style='transform: translateZ(0); width: 50px; height: "
      "60px'>"
      "    <div style='transform-style: preserve-3d; width: 30px; height: "
      "40px'>"
      "      <div id='b' style='transform: translateZ(0); width: 10px; height: "
      "20px'>"
      "    </div>"
      "  </div>"
      "</div>");
  FrameView* frameView = document().view();

  LayoutObject* a = document().getElementById("a")->layoutObject();
  const ObjectPaintProperties* aProperties = a->paintProperties();
  LayoutObject* b = document().getElementById("b")->layoutObject();
  const ObjectPaintProperties* bProperties = b->paintProperties();
  ASSERT_FALSE(a->styleRef().preserves3D());
  ASSERT_TRUE(aProperties->transform() && bProperties->transform());

  // #a should participate in a rendering context (due to its parent). Its
  // child does preserve 3D, but since #a does not, #a's rendering context is
  // not passed on to its children. Thus #b ends up in a separate rendering
  // context rooted at its parent.
  EXPECT_TRUE(aProperties->transform()->hasRenderingContext());
  EXPECT_TRUE(bProperties->transform()->hasRenderingContext());
  EXPECT_NE(aProperties->transform()->renderingContextID(),
            bProperties->transform()->renderingContextID());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 50, 60), a, frameView->layoutView());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 10, 20), b, frameView->layoutView());
}

// Returns true if the first node has the second as an ancestor.
static bool nodeHasAncestor(const TransformPaintPropertyNode* node,
                            const TransformPaintPropertyNode* ancestor) {
  while (node) {
    if (node == ancestor)
      return true;
    node = node->parent();
  }
  return false;
}

// Returns true if some node will flatten the transform due to |node| before it
// is inherited by |node| (including if node->flattensInheritedTransform()).
static bool someNodeFlattensTransform(
    const TransformPaintPropertyNode* node,
    const TransformPaintPropertyNode* ancestor) {
  while (node != ancestor) {
    if (node->flattensInheritedTransform())
      return true;
    node = node->parent();
  }
  return false;
}

TEST_P(PaintPropertyTreeBuilderTest, FlatTransformStylePropagatesToChildren) {
  setBodyInnerHTML(
      "<div id='a' style='transform: translateZ(0); transform-style: flat; "
      "width: 30px; height: 40px'>"
      "  <div id='b' style='transform: translateZ(0); width: 10px; height: "
      "10px'></div>"
      "</div>");
  FrameView* frameView = document().view();

  LayoutObject* a = document().getElementById("a")->layoutObject();
  LayoutObject* b = document().getElementById("b")->layoutObject();
  const auto* aTransform = a->paintProperties()->transform();
  ASSERT_TRUE(aTransform);
  const auto* bTransform = b->paintProperties()->transform();
  ASSERT_TRUE(bTransform);
  ASSERT_TRUE(nodeHasAncestor(bTransform, aTransform));

  // Some node must flatten the inherited transform from #a before it reaches
  // #b's transform.
  EXPECT_TRUE(someNodeFlattensTransform(bTransform, aTransform));
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 30, 40), a, frameView->layoutView());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 10, 10), b, frameView->layoutView());
}

TEST_P(PaintPropertyTreeBuilderTest,
       Preserve3DTransformStylePropagatesToChildren) {
  setBodyInnerHTML(
      "<div id='a' style='transform: translateZ(0); transform-style: "
      "preserve-3d; width: 30px; height: 40px'>"
      "  <div id='b' style='transform: translateZ(0); width: 10px; height: "
      "10px'></div>"
      "</div>");
  FrameView* frameView = document().view();

  LayoutObject* a = document().getElementById("a")->layoutObject();
  LayoutObject* b = document().getElementById("b")->layoutObject();
  const auto* aTransform = a->paintProperties()->transform();
  ASSERT_TRUE(aTransform);
  const auto* bTransform = b->paintProperties()->transform();
  ASSERT_TRUE(bTransform);
  ASSERT_TRUE(nodeHasAncestor(bTransform, aTransform));

  // No node may flatten the inherited transform from #a before it reaches
  // #b's transform.
  EXPECT_FALSE(someNodeFlattensTransform(bTransform, aTransform));
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 30, 40), a, frameView->layoutView());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 10, 10), b, frameView->layoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, PerspectiveIsNotFlattened) {
  // It's necessary to make nodes from the one that applies perspective to
  // ones that combine with it preserve 3D. Otherwise, the perspective doesn't
  // do anything.
  setBodyInnerHTML(
      "<div id='a' style='perspective: 800px; width: 30px; height: 40px'>"
      "  <div id='b' style='transform: translateZ(0); width: 10px; height: "
      "20px'></div>"
      "</div>");
  FrameView* frameView = document().view();

  LayoutObject* a = document().getElementById("a")->layoutObject();
  LayoutObject* b = document().getElementById("b")->layoutObject();
  const ObjectPaintProperties* aProperties = a->paintProperties();
  const ObjectPaintProperties* bProperties = b->paintProperties();
  const TransformPaintPropertyNode* aPerspective = aProperties->perspective();
  ASSERT_TRUE(aPerspective);
  const TransformPaintPropertyNode* bTransform = bProperties->transform();
  ASSERT_TRUE(bTransform);
  ASSERT_TRUE(nodeHasAncestor(bTransform, aPerspective));
  EXPECT_FALSE(someNodeFlattensTransform(bTransform, aPerspective));
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 30, 40), a, frameView->layoutView());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 10, 20), b, frameView->layoutView());
}

TEST_P(PaintPropertyTreeBuilderTest,
       PerspectiveDoesNotEstablishRenderingContext) {
  // It's necessary to make nodes from the one that applies perspective to
  // ones that combine with it preserve 3D. Otherwise, the perspective doesn't
  // do anything.
  setBodyInnerHTML(
      "<div id='a' style='perspective: 800px; width: 30px; height: 40px'>"
      "  <div id='b' style='transform: translateZ(0); width: 10px; height: "
      "20px'></div>"
      "</div>");
  FrameView* frameView = document().view();

  LayoutObject* a = document().getElementById("a")->layoutObject();
  LayoutObject* b = document().getElementById("b")->layoutObject();
  const ObjectPaintProperties* aProperties = a->paintProperties();
  const ObjectPaintProperties* bProperties = b->paintProperties();
  const TransformPaintPropertyNode* aPerspective = aProperties->perspective();
  ASSERT_TRUE(aPerspective);
  EXPECT_FALSE(aPerspective->hasRenderingContext());
  const TransformPaintPropertyNode* bTransform = bProperties->transform();
  ASSERT_TRUE(bTransform);
  EXPECT_FALSE(bTransform->hasRenderingContext());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 30, 40), a, frameView->layoutView());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 10, 20), b, frameView->layoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, CachedProperties) {
  setBodyInnerHTML(
      "<style>body { margin: 0 }</style>"
      "<div id='a' style='transform: translate(33px, 44px); width: 50px; "
      "height: 60px'>"
      "  <div id='b' style='transform: translate(55px, 66px); width: 30px; "
      "height: 40px'>"
      "    <div id='c' style='transform: translate(77px, 88px); width: 10px; "
      "height: 20px'>C<div>"
      "  </div>"
      "</div>");
  FrameView* frameView = document().view();

  Element* a = document().getElementById("a");
  const ObjectPaintProperties* aProperties =
      a->layoutObject()->paintProperties();
  const TransformPaintPropertyNode* aTransformNode = aProperties->transform();
  EXPECT_EQ(TransformationMatrix().translate(33, 44), aTransformNode->matrix());

  Element* b = document().getElementById("b");
  const ObjectPaintProperties* bProperties =
      b->layoutObject()->paintProperties();
  const TransformPaintPropertyNode* bTransformNode = bProperties->transform();
  EXPECT_EQ(TransformationMatrix().translate(55, 66), bTransformNode->matrix());

  Element* c = document().getElementById("c");
  const ObjectPaintProperties* cProperties =
      c->layoutObject()->paintProperties();
  const TransformPaintPropertyNode* cTransformNode = cProperties->transform();
  EXPECT_EQ(TransformationMatrix().translate(77, 88), cTransformNode->matrix());

  CHECK_EXACT_VISUAL_RECT(LayoutRect(33, 44, 50, 60), a->layoutObject(),
                          frameView->layoutView());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(88, 110, 30, 40), b->layoutObject(),
                          frameView->layoutView());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(165, 198, 10, 20), c->layoutObject(),
                          frameView->layoutView());

  // Change transform of b. B's transform node should be a new node with the new
  // value, and a and c's transform nodes should be unchanged (with c's parent
  // adjusted).
  b->setAttribute(HTMLNames::styleAttr, "transform: translate(111px, 222px)");
  document().view()->updateAllLifecyclePhases();

  EXPECT_EQ(aProperties, a->layoutObject()->paintProperties());
  EXPECT_EQ(aTransformNode, aProperties->transform());

  EXPECT_EQ(bProperties, b->layoutObject()->paintProperties());
  bTransformNode = bProperties->transform();
  EXPECT_EQ(TransformationMatrix().translate(111, 222),
            bTransformNode->matrix());
  EXPECT_EQ(aTransformNode, bTransformNode->parent());

  EXPECT_EQ(cProperties, c->layoutObject()->paintProperties());
  EXPECT_EQ(cTransformNode, cProperties->transform());
  EXPECT_EQ(bTransformNode, cTransformNode->parent());

  CHECK_EXACT_VISUAL_RECT(LayoutRect(33, 44, 50, 60), a->layoutObject(),
                          frameView->layoutView());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(144, 266, 50, 20), b->layoutObject(),
                          frameView->layoutView());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(221, 354, 10, 20), c->layoutObject(),
                          frameView->layoutView());

  // Remove transform from b. B's transform node should be removed from the
  // tree, and a and c's transform nodes should be unchanged (with c's parent
  // adjusted).
  b->setAttribute(HTMLNames::styleAttr, "");
  document().view()->updateAllLifecyclePhases();

  EXPECT_EQ(aProperties, a->layoutObject()->paintProperties());
  EXPECT_EQ(aTransformNode, aProperties->transform());

  EXPECT_EQ(bProperties, b->layoutObject()->paintProperties());
  EXPECT_EQ(nullptr, bProperties->transform());

  EXPECT_EQ(cProperties, c->layoutObject()->paintProperties());
  EXPECT_EQ(cTransformNode, cProperties->transform());
  EXPECT_EQ(aTransformNode, cTransformNode->parent());

  CHECK_EXACT_VISUAL_RECT(LayoutRect(33, 44, 50, 60), a->layoutObject(),
                          frameView->layoutView());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(33, 44, 50, 20), b->layoutObject(),
                          frameView->layoutView());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(110, 132, 10, 20), c->layoutObject(),
                          frameView->layoutView());

  // Re-add transform to b. B's transform node should be inserted into the tree,
  // and a and c's transform nodes should be unchanged (with c's parent
  // adjusted).
  b->setAttribute(HTMLNames::styleAttr, "transform: translate(4px, 5px)");
  document().view()->updateAllLifecyclePhases();

  EXPECT_EQ(aProperties, a->layoutObject()->paintProperties());
  EXPECT_EQ(aTransformNode, aProperties->transform());

  EXPECT_EQ(bProperties, b->layoutObject()->paintProperties());
  bTransformNode = bProperties->transform();
  EXPECT_EQ(TransformationMatrix().translate(4, 5), bTransformNode->matrix());
  EXPECT_EQ(aTransformNode, bTransformNode->parent());

  EXPECT_EQ(cProperties, c->layoutObject()->paintProperties());
  EXPECT_EQ(cTransformNode, cProperties->transform());
  EXPECT_EQ(bTransformNode, cTransformNode->parent());

  CHECK_EXACT_VISUAL_RECT(LayoutRect(33, 44, 50, 60), a->layoutObject(),
                          frameView->layoutView());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(37, 49, 50, 20), b->layoutObject(),
                          frameView->layoutView());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(114, 137, 10, 20), c->layoutObject(),
                          frameView->layoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, OverflowClipContentsTreeState) {
  // This test verifies the tree builder correctly computes and records the
  // property tree context for a (pseudo) stacking context that is scrolled by a
  // containing block that is not one of the painting ancestors.
  setBodyInnerHTML(
      "<style>body { margin: 20px 30px; }</style>"
      "<div id='clipper' style='overflow:hidden; width:400px; height:300px;'>"
      "  <div id='child' style='position:relative; width:500px; height: "
      "600px;'></div>"
      "</div>");

  LayoutBoxModelObject* clipper = toLayoutBoxModelObject(
      document().getElementById("clipper")->layoutObject());
  const ObjectPaintProperties* clipProperties = clipper->paintProperties();
  LayoutObject* child = document().getElementById("child")->layoutObject();
  const ObjectPaintProperties* childProperties = child->paintProperties();

  // No scroll translation because the document does not scroll (not enough
  // content).
  EXPECT_TRUE(!frameScrollTranslation());
  EXPECT_EQ(framePreTranslation(), clipProperties->localBorderBoxProperties()
                                       ->propertyTreeState.transform());
  EXPECT_EQ(
      frameContentClip(),
      clipProperties->localBorderBoxProperties()->propertyTreeState.clip());

  auto contentsProperties = clipProperties->contentsProperties();
  EXPECT_EQ(LayoutPoint(30, 20), contentsProperties.paintOffset);
  EXPECT_EQ(framePreTranslation(),
            contentsProperties.propertyTreeState.transform());
  EXPECT_EQ(clipProperties->overflowClip(),
            contentsProperties.propertyTreeState.clip());

  EXPECT_EQ(framePreTranslation(), childProperties->localBorderBoxProperties()
                                       ->propertyTreeState.transform());
  EXPECT_EQ(
      clipProperties->overflowClip(),
      childProperties->localBorderBoxProperties()->propertyTreeState.clip());

  EXPECT_NE(
      nullptr,
      childProperties->localBorderBoxProperties()->propertyTreeState.effect());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(0, 0, 500, 600), child, clipper);
}

TEST_P(PaintPropertyTreeBuilderTest, ContainsPaintContentsTreeState) {
  setBodyInnerHTML(
      "<style>body { margin: 20px 30px; }</style>"
      "<div id='clipper' style='contain:paint; width:300px; height:200px;'>"
      "  <div id='child' style='position:relative; width:400px; height: "
      "500px;'></div>"
      "</div>");

  LayoutBoxModelObject* clipper = toLayoutBoxModelObject(
      document().getElementById("clipper")->layoutObject());
  const ObjectPaintProperties* clipProperties = clipper->paintProperties();
  LayoutObject* child = document().getElementById("child")->layoutObject();
  const ObjectPaintProperties* childProperties = child->paintProperties();

  // No scroll translation because the document does not scroll (not enough
  // content).
  EXPECT_TRUE(!frameScrollTranslation());
  EXPECT_EQ(framePreTranslation(), clipProperties->localBorderBoxProperties()
                                       ->propertyTreeState.transform());
  EXPECT_EQ(
      frameContentClip(),
      clipProperties->localBorderBoxProperties()->propertyTreeState.clip());

  auto contentsProperties = clipProperties->contentsProperties();
  EXPECT_EQ(LayoutPoint(30, 20), contentsProperties.paintOffset);
  EXPECT_EQ(framePreTranslation(),
            contentsProperties.propertyTreeState.transform());
  EXPECT_EQ(clipProperties->overflowClip(),
            contentsProperties.propertyTreeState.clip());

  EXPECT_EQ(framePreTranslation(), childProperties->localBorderBoxProperties()
                                       ->propertyTreeState.transform());
  EXPECT_EQ(
      clipProperties->overflowClip(),
      childProperties->localBorderBoxProperties()->propertyTreeState.clip());

  EXPECT_NE(
      nullptr,
      childProperties->localBorderBoxProperties()->propertyTreeState.effect());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(0, 0, 400, 500), child, clipper);
}

TEST_P(PaintPropertyTreeBuilderTest, OverflowScrollContentsTreeState) {
  // This test verifies the tree builder correctly computes and records the
  // property tree context for a (pseudo) stacking context that is scrolled by a
  // containing block that is not one of the painting ancestors.
  setBodyInnerHTML(
      "<style>body { margin: 20px 30px; }</style>"
      "<div id='clipper' style='overflow:scroll; width:400px; height:300px;'>"
      "  <div id='child' style='position:relative; width:500px; height: "
      "600px;'></div>"
      "  <div style='width: 200px; height: 10000px'></div>"
      "</div>"
      "<div id='forceScroll' style='height: 4000px;'></div>");

  Element* clipperElement = document().getElementById("clipper");
  clipperElement->scrollTo(1, 2);

  LayoutBoxModelObject* clipper =
      toLayoutBoxModelObject(clipperElement->layoutObject());
  const ObjectPaintProperties* clipProperties = clipper->paintProperties();
  LayoutObject* child = document().getElementById("child")->layoutObject();
  const ObjectPaintProperties* childProperties = child->paintProperties();

  EXPECT_EQ(frameScrollTranslation(), clipProperties->localBorderBoxProperties()
                                          ->propertyTreeState.transform());
  EXPECT_EQ(
      frameContentClip(),
      clipProperties->localBorderBoxProperties()->propertyTreeState.clip());

  auto contentsProperties = clipProperties->contentsProperties();
  EXPECT_EQ(LayoutPoint(30, 20), contentsProperties.paintOffset);
  EXPECT_EQ(clipProperties->scrollTranslation(),
            contentsProperties.propertyTreeState.transform());
  EXPECT_EQ(clipProperties->overflowClip(),
            contentsProperties.propertyTreeState.clip());

  EXPECT_EQ(clipProperties->scrollTranslation(),
            childProperties->localBorderBoxProperties()
                ->propertyTreeState.transform());
  EXPECT_EQ(
      clipProperties->overflowClip(),
      childProperties->localBorderBoxProperties()->propertyTreeState.clip());

  CHECK_EXACT_VISUAL_RECT(LayoutRect(0, 0, 500, 600), child, clipper);
}

TEST_P(PaintPropertyTreeBuilderTest, OverflowScrollWithRoundedRect) {
  setBodyInnerHTML(
      "<style>"
      "  * { margin: 0; }"
      "  ::-webkit-scrollbar {"
      "    width: 13px;"
      "    height: 13px;"
      "  }"
      "  #roundedBox {"
      "    width: 200px;"
      "    height: 200px;"
      "    border-radius: 100px;"
      "    background-color: red;"
      "    border: 50px solid green;"
      "    overflow: scroll;"
      "  }"
      "  #roundedBoxChild {"
      "    width: 200px;"
      "    height: 200px;"
      "    background-color: orange;"
      "  }"
      "</style>"
      "<div id='roundedBox'>"
      "  <div id='roundedBoxChild'></div>"
      "</div>");

  LayoutObject& roundedBox =
      *document().getElementById("roundedBox")->layoutObject();
  const ObjectPaintProperties* roundedBoxProperties =
      roundedBox.paintProperties();
  EXPECT_EQ(
      FloatRoundedRect(FloatRect(50, 50, 200, 200), FloatSize(50, 50),
                       FloatSize(50, 50), FloatSize(50, 50), FloatSize(50, 50)),
      roundedBoxProperties->innerBorderRadiusClip()->clipRect());

  // Unlike the inner border radius clip, the overflow clip is inset by the
  // scrollbars (13px).
  EXPECT_EQ(FloatRoundedRect(50, 50, 187, 187),
            roundedBoxProperties->overflowClip()->clipRect());
  EXPECT_EQ(frameContentClip(),
            roundedBoxProperties->innerBorderRadiusClip()->parent());
  EXPECT_EQ(roundedBoxProperties->innerBorderRadiusClip(),
            roundedBoxProperties->overflowClip()->parent());
}

TEST_P(PaintPropertyTreeBuilderTest, CssClipContentsTreeState) {
  // This test verifies the tree builder correctly computes and records the
  // property tree context for a (pseudo) stacking context that is scrolled by a
  // containing block that is not one of the painting ancestors.
  setBodyInnerHTML(
      "<style>body { margin: 20px 30px; }</style>"
      "<div id='clipper' style='position: absolute; clip: rect(10px, 80px, "
      "70px, 40px); width:300px; height:200px;'>"
      "  <div id='child' style='position:relative; width:400px; height: "
      "500px;'></div>"
      "</div>");

  LayoutBoxModelObject* clipper = toLayoutBoxModelObject(
      document().getElementById("clipper")->layoutObject());
  const ObjectPaintProperties* clipProperties = clipper->paintProperties();
  LayoutObject* child = document().getElementById("child")->layoutObject();

  // No scroll translation because the document does not scroll (not enough
  // content).
  EXPECT_TRUE(!frameScrollTranslation());
  EXPECT_EQ(framePreTranslation(), clipProperties->localBorderBoxProperties()
                                       ->propertyTreeState.transform());
  // CSS clip on an element causes it to clip itself, not just descendants.
  EXPECT_EQ(
      clipProperties->cssClip(),
      clipProperties->localBorderBoxProperties()->propertyTreeState.clip());

  auto contentsProperties = clipProperties->contentsProperties();
  EXPECT_EQ(LayoutPoint(30, 20), contentsProperties.paintOffset);
  EXPECT_EQ(framePreTranslation(),
            contentsProperties.propertyTreeState.transform());
  EXPECT_EQ(clipProperties->cssClip(),
            contentsProperties.propertyTreeState.clip());

  CHECK_EXACT_VISUAL_RECT(LayoutRect(0, 0, 400, 500), child, clipper);
}

TEST_P(PaintPropertyTreeBuilderTest,
       SvgLocalToBorderBoxTransformContentsTreeState) {
  setBodyInnerHTML(
      "<style>"
      "  body {"
      "    margin: 20px 30px;"
      "  }"
      "  svg {"
      "    position: absolute;"
      "  }"
      "  rect {"
      "    transform: translate(100px, 100px);"
      "  }"
      "</style>"
      "<svg id='svgWithViewBox' width='100px' height='100px' viewBox='50 50 "
      "100 100'>"
      "  <rect id='rect' width='100px' height='100px' />"
      "</svg>");

  LayoutObject& svgWithViewBox =
      *document().getElementById("svgWithViewBox")->layoutObject();
  const ObjectPaintProperties* svgWithViewBoxProperties =
      svgWithViewBox.paintProperties();

  EXPECT_EQ(framePreTranslation(),
            svgWithViewBoxProperties->localBorderBoxProperties()
                ->propertyTreeState.transform());

  auto contentsProperties = svgWithViewBoxProperties->contentsProperties();
  EXPECT_EQ(LayoutPoint(), contentsProperties.paintOffset);
  EXPECT_EQ(svgWithViewBoxProperties->svgLocalToBorderBoxTransform(),
            contentsProperties.propertyTreeState.transform());
}

TEST_P(PaintPropertyTreeBuilderTest, OverflowHiddenScrollProperties) {
  setBodyInnerHTML(
      "<style>"
      "  body {"
      "    margin: 0px;"
      "  }"
      "  #overflowHidden {"
      "    overflow: hidden;"
      "    width: 5px;"
      "    height: 3px;"
      "  }"
      "  .forceScroll {"
      "    height: 79px;"
      "  }"
      "</style>"
      "<div id='overflowHidden'>"
      "  <div class='forceScroll'></div>"
      "</div>");

  Element* overflowHidden = document().getElementById("overflowHidden");
  overflowHidden->setScrollTop(37);

  document().view()->updateAllLifecyclePhases();

  const ObjectPaintProperties* overflowHiddenScrollProperties =
      overflowHidden->layoutObject()->paintProperties();
  // Because the frameView is does not scroll, overflowHidden's scroll should be
  // under the root.
  EXPECT_TRUE(overflowHiddenScrollProperties->scroll()->parent()->isRoot());
  EXPECT_EQ(TransformationMatrix().translate(0, -37),
            overflowHiddenScrollProperties->scroll()
                ->scrollOffsetTranslation()
                ->matrix());
  // This should match the overflow's dimensions.
  EXPECT_EQ(IntSize(5, 3), overflowHiddenScrollProperties->scroll()->clip());
  // The scrolling content's bounds should include both the overflow's
  // dimensions (5x3) and the 0x79 "forceScroll" object.
  EXPECT_EQ(IntSize(5, 79), overflowHiddenScrollProperties->scroll()->bounds());
  // Although overflow: hidden is programmatically scrollable, it is not user
  // scrollable.
  EXPECT_FALSE(
      overflowHiddenScrollProperties->scroll()->userScrollableHorizontal());
  EXPECT_FALSE(
      overflowHiddenScrollProperties->scroll()->userScrollableVertical());
}

TEST_P(PaintPropertyTreeBuilderTest, NestedScrollProperties) {
  setBodyInnerHTML(
      "<style>"
      "  * {"
      "    margin: 0px;"
      "  }"
      "  #overflowA {"
      "    overflow: scroll;"
      "    width: 5px;"
      "    height: 3px;"
      "  }"
      "  #overflowB {"
      "    overflow: scroll;"
      "    width: 9px;"
      "    height: 7px;"
      "  }"
      "  .forceScroll {"
      "    height: 100px;"
      "  }"
      "</style>"
      "<div id='overflowA'>"
      "  <div id='overflowB'>"
      "    <div class='forceScroll'></div>"
      "  </div>"
      "  <div class='forceScroll'></div>"
      "</div>");

  Element* overflowA = document().getElementById("overflowA");
  overflowA->setScrollTop(37);
  Element* overflowB = document().getElementById("overflowB");
  overflowB->setScrollTop(41);

  document().view()->updateAllLifecyclePhases();

  const ObjectPaintProperties* overflowAScrollProperties =
      overflowA->layoutObject()->paintProperties();
  // Because the frameView is does not scroll, overflowA's scroll should be
  // under the root.
  EXPECT_TRUE(overflowAScrollProperties->scroll()->parent()->isRoot());
  EXPECT_EQ(
      TransformationMatrix().translate(0, -37),
      overflowAScrollProperties->scroll()->scrollOffsetTranslation()->matrix());
  EXPECT_EQ(IntSize(5, 3), overflowAScrollProperties->scroll()->clip());
  // 107 is the forceScroll element plus the height of the overflow scroll child
  // (overflowB).
  EXPECT_EQ(IntSize(9, 107), overflowAScrollProperties->scroll()->bounds());
  EXPECT_TRUE(overflowAScrollProperties->scroll()->userScrollableHorizontal());
  EXPECT_TRUE(overflowAScrollProperties->scroll()->userScrollableVertical());

  const ObjectPaintProperties* overflowBScrollProperties =
      overflowB->layoutObject()->paintProperties();
  // The overflow child's scroll node should be a child of the parent's
  // (overflowA) scroll node.
  EXPECT_EQ(overflowAScrollProperties->scroll(),
            overflowBScrollProperties->scroll()->parent());
  EXPECT_EQ(
      TransformationMatrix().translate(0, -41),
      overflowBScrollProperties->scroll()->scrollOffsetTranslation()->matrix());
  EXPECT_EQ(IntSize(9, 7), overflowBScrollProperties->scroll()->clip());
  EXPECT_EQ(IntSize(9, 100), overflowBScrollProperties->scroll()->bounds());
  EXPECT_TRUE(overflowBScrollProperties->scroll()->userScrollableHorizontal());
  EXPECT_TRUE(overflowBScrollProperties->scroll()->userScrollableVertical());
}

TEST_P(PaintPropertyTreeBuilderTest, PositionedScrollerIsNotNested) {
  setBodyInnerHTML(
      "<style>"
      "  * {"
      "    margin: 0px;"
      "  }"
      "  #overflow {"
      "    overflow: scroll;"
      "    width: 5px;"
      "    height: 3px;"
      "  }"
      "  #absposOverflow {"
      "    position: absolute;"
      "    top: 0;"
      "    left: 0;"
      "    overflow: scroll;"
      "    width: 9px;"
      "    height: 7px;"
      "  }"
      "  #fixedOverflow {"
      "    position: fixed;"
      "    top: 0;"
      "    left: 0;"
      "    overflow: scroll;"
      "    width: 13px;"
      "    height: 11px;"
      "  }"
      "  .forceScroll {"
      "    height: 4000px;"
      "  }"
      "</style>"
      "<div id='overflow'>"
      "  <div id='absposOverflow'>"
      "    <div class='forceScroll'></div>"
      "  </div>"
      "  <div id='fixedOverflow'>"
      "    <div class='forceScroll'></div>"
      "  </div>"
      "  <div class='forceScroll'></div>"
      "</div>"
      "<div class='forceScroll'></div>");

  Element* overflow = document().getElementById("overflow");
  overflow->setScrollTop(37);
  Element* absposOverflow = document().getElementById("absposOverflow");
  absposOverflow->setScrollTop(41);
  Element* fixedOverflow = document().getElementById("fixedOverflow");
  fixedOverflow->setScrollTop(43);

  document().view()->updateAllLifecyclePhases();

  // The frame should scroll due to the "forceScroll" element.
  EXPECT_NE(nullptr, frameScroll());

  const ObjectPaintProperties* overflowScrollProperties =
      overflow->layoutObject()->paintProperties();
  EXPECT_EQ(frameScroll(), overflowScrollProperties->scroll()->parent());
  EXPECT_EQ(
      TransformationMatrix().translate(0, -37),
      overflowScrollProperties->scroll()->scrollOffsetTranslation()->matrix());
  EXPECT_EQ(IntSize(5, 3), overflowScrollProperties->scroll()->clip());
  // The height should be 4000px because the (dom-order) overflow children are
  // positioned and do not contribute to the height. Only the 4000px
  // "forceScroll" height is present.
  EXPECT_EQ(IntSize(5, 4000), overflowScrollProperties->scroll()->bounds());

  const ObjectPaintProperties* absposOverflowScrollProperties =
      absposOverflow->layoutObject()->paintProperties();
  // The absolute position overflow scroll node is parented under the frame, not
  // the dom-order parent.
  EXPECT_EQ(frameScroll(), absposOverflowScrollProperties->scroll()->parent());
  EXPECT_EQ(TransformationMatrix().translate(0, -41),
            absposOverflowScrollProperties->scroll()
                ->scrollOffsetTranslation()
                ->matrix());
  EXPECT_EQ(IntSize(9, 7), absposOverflowScrollProperties->scroll()->clip());
  EXPECT_EQ(IntSize(9, 4000),
            absposOverflowScrollProperties->scroll()->bounds());

  const ObjectPaintProperties* fixedOverflowScrollProperties =
      fixedOverflow->layoutObject()->paintProperties();
  // The fixed position overflow scroll node is parented under the root, not the
  // dom-order parent or frame's scroll.
  EXPECT_TRUE(fixedOverflowScrollProperties->scroll()->parent()->isRoot());
  EXPECT_EQ(TransformationMatrix().translate(0, -43),
            fixedOverflowScrollProperties->scroll()
                ->scrollOffsetTranslation()
                ->matrix());
  EXPECT_EQ(IntSize(13, 11), fixedOverflowScrollProperties->scroll()->clip());
  EXPECT_EQ(IntSize(13, 4000),
            fixedOverflowScrollProperties->scroll()->bounds());
}

TEST_P(PaintPropertyTreeBuilderTest, NestedPositionedScrollProperties) {
  setBodyInnerHTML(
      "<style>"
      "  * {"
      "    margin: 0px;"
      "  }"
      "  #overflowA {"
      "    position: absolute;"
      "    top: 7px;"
      "    left: 11px;"
      "    overflow: scroll;"
      "    width: 20px;"
      "    height: 20px;"
      "  }"
      "  #overflowB {"
      "    position: absolute;"
      "    top: 1px;"
      "    left: 3px;"
      "    overflow: scroll;"
      "    width: 5px;"
      "    height: 3px;"
      "  }"
      "  .forceScroll {"
      "    height: 100px;"
      "  }"
      "</style>"
      "<div id='overflowA'>"
      "  <div id='overflowB'>"
      "    <div class='forceScroll'></div>"
      "  </div>"
      "  <div class='forceScroll'></div>"
      "</div>");

  Element* overflowA = document().getElementById("overflowA");
  overflowA->setScrollTop(37);
  Element* overflowB = document().getElementById("overflowB");
  overflowB->setScrollTop(41);

  document().view()->updateAllLifecyclePhases();

  const ObjectPaintProperties* overflowAScrollProperties =
      overflowA->layoutObject()->paintProperties();
  // Because the frameView is does not scroll, overflowA's scroll should be
  // under the root.
  EXPECT_TRUE(overflowAScrollProperties->scroll()->parent()->isRoot());
  EXPECT_EQ(
      TransformationMatrix().translate(0, -37),
      overflowAScrollProperties->scroll()->scrollOffsetTranslation()->matrix());
  EXPECT_EQ(IntSize(20, 20), overflowAScrollProperties->scroll()->clip());
  // 100 is the forceScroll element's height because the overflow child does not
  // contribute to the height.
  EXPECT_EQ(IntSize(20, 100), overflowAScrollProperties->scroll()->bounds());
  EXPECT_TRUE(overflowAScrollProperties->scroll()->userScrollableHorizontal());
  EXPECT_TRUE(overflowAScrollProperties->scroll()->userScrollableVertical());

  const ObjectPaintProperties* overflowBScrollProperties =
      overflowB->layoutObject()->paintProperties();
  // The overflow child's scroll node should be a child of the parent's
  // (overflowA) scroll node.
  EXPECT_EQ(overflowAScrollProperties->scroll(),
            overflowBScrollProperties->scroll()->parent());
  EXPECT_EQ(
      TransformationMatrix().translate(0, -41),
      overflowBScrollProperties->scroll()->scrollOffsetTranslation()->matrix());
  EXPECT_EQ(IntSize(5, 3), overflowBScrollProperties->scroll()->clip());
  EXPECT_EQ(IntSize(5, 100), overflowBScrollProperties->scroll()->bounds());
  EXPECT_TRUE(overflowBScrollProperties->scroll()->userScrollableHorizontal());
  EXPECT_TRUE(overflowBScrollProperties->scroll()->userScrollableVertical());
}

TEST_P(PaintPropertyTreeBuilderTest, SVGRootClip) {
  setBodyInnerHTML(
      "<svg id='svg' xmlns='http://www.w3.org/2000/svg' width='100px' "
      "height='100px'>"
      "  <rect width='200' height='200' fill='red' />"
      "</svg>");

  const ClipPaintPropertyNode* clip =
      getLayoutObjectByElementId("svg")->paintProperties()->overflowClip();
  EXPECT_EQ(frameContentClip(), clip->parent());
  EXPECT_EQ(FloatRoundedRect(8, 8, 100, 100), clip->clipRect());
}

TEST_P(PaintPropertyTreeBuilderTest, SVGRootNoClip) {
  setBodyInnerHTML(
      "<svg id='svg' xmlns='http://www.w3.org/2000/svg' width='100px' "
      "height='100px' style='overflow: visible'>"
      "  <rect width='200' height='200' fill='red' />"
      "</svg>");

  EXPECT_FALSE(
      getLayoutObjectByElementId("svg")->paintProperties()->overflowClip());
}

TEST_P(PaintPropertyTreeBuilderTest,
       ThreadedScrollingDisabledMainThreadScrollReason) {
  setBodyInnerHTML(
      "<style>"
      "  #overflowA {"
      "    position: absolute;"
      "    overflow: scroll;"
      "    width: 20px;"
      "    height: 20px;"
      "  }"
      "  .forceScroll {"
      "    height: 4000px;"
      "  }"
      "</style>"
      "<div id='overflowA'>"
      "  <div class='forceScroll'></div>"
      "</div>"
      "<div class='forceScroll'></div>");
  Element* overflowA = document().getElementById("overflowA");
  EXPECT_FALSE(frameScroll()->hasMainThreadScrollingReasons(
      MainThreadScrollingReason::kThreadedScrollingDisabled));
  EXPECT_FALSE(overflowA->layoutObject()
                   ->paintProperties()
                   ->scroll()
                   ->hasMainThreadScrollingReasons(
                       MainThreadScrollingReason::kThreadedScrollingDisabled));

  document().settings()->setThreadedScrollingEnabled(false);
  document().view()->updateAllLifecyclePhases();

  EXPECT_TRUE(frameScroll()->hasMainThreadScrollingReasons(
      MainThreadScrollingReason::kThreadedScrollingDisabled));
  EXPECT_TRUE(overflowA->layoutObject()
                  ->paintProperties()
                  ->scroll()
                  ->hasMainThreadScrollingReasons(
                      MainThreadScrollingReason::kThreadedScrollingDisabled));
}

// Disabled due to missing main thread scrolling property invalidation support.
// See: https://crbug.com/664672
TEST_P(
    PaintPropertyTreeBuilderTest,
    DISABLED_BackgroundAttachmentFixedMainThreadScrollReasonsWithNestedScrollers) {
  setBodyInnerHTML(
      "<style>"
      "  #overflowA {"
      "    position: absolute;"
      "    overflow: scroll;"
      "    width: 20px;"
      "    height: 20px;"
      "  }"
      "  #overflowB {"
      "    position: absolute;"
      "    overflow: scroll;"
      "    width: 5px;"
      "    height: 3px;"
      "  }"
      "  .backgroundAttachmentFixed {"
      "    background-image: url('foo');"
      "    background-attachment: fixed;"
      "  }"
      "  .forceScroll {"
      "    height: 4000px;"
      "  }"
      "</style>"
      "<div id='overflowA'>"
      "  <div id='overflowB' class='backgroundAttachmentFixed'>"
      "    <div class='forceScroll'></div>"
      "  </div>"
      "  <div class='forceScroll'></div>"
      "</div>"
      "<div class='forceScroll'></div>");
  Element* overflowA = document().getElementById("overflowA");
  Element* overflowB = document().getElementById("overflowB");

  EXPECT_TRUE(frameScroll()->hasMainThreadScrollingReasons(
      MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects));
  EXPECT_TRUE(
      overflowA->layoutObject()
          ->paintProperties()
          ->scroll()
          ->hasMainThreadScrollingReasons(
              MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects));
  EXPECT_FALSE(
      overflowB->layoutObject()
          ->paintProperties()
          ->scroll()
          ->hasMainThreadScrollingReasons(
              MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects));

  // Removing a main thread scrolling reason should update the entire tree.
  overflowB->removeAttribute("class");
  document().view()->updateAllLifecyclePhases();
  EXPECT_FALSE(frameScroll()->hasMainThreadScrollingReasons(
      MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects));
  EXPECT_FALSE(
      overflowA->layoutObject()
          ->paintProperties()
          ->scroll()
          ->hasMainThreadScrollingReasons(
              MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects));
  EXPECT_FALSE(
      overflowB->layoutObject()
          ->paintProperties()
          ->scroll()
          ->hasMainThreadScrollingReasons(
              MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects));

  // Adding a main thread scrolling reason should update the entire tree.
  overflowB->setAttribute(HTMLNames::classAttr, "backgroundAttachmentFixed");
  document().view()->updateAllLifecyclePhases();
  EXPECT_TRUE(frameScroll()->hasMainThreadScrollingReasons(
      MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects));
  EXPECT_TRUE(
      overflowA->layoutObject()
          ->paintProperties()
          ->scroll()
          ->hasMainThreadScrollingReasons(
              MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects));
  EXPECT_FALSE(
      overflowB->layoutObject()
          ->paintProperties()
          ->scroll()
          ->hasMainThreadScrollingReasons(
              MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects));
}

// Disabled due to missing main thread scrolling property invalidation support.
// See: https://crbug.com/664672
TEST_P(
    PaintPropertyTreeBuilderTest,
    DISABLED_BackgroundAttachmentFixedMainThreadScrollReasonsWithFixedScroller) {
  setBodyInnerHTML(
      "<style>"
      "  #overflowA {"
      "    position: absolute;"
      "    overflow: scroll;"
      "    width: 20px;"
      "    height: 20px;"
      "  }"
      "  #overflowB {"
      "    position: fixed;"
      "    overflow: scroll;"
      "    width: 5px;"
      "    height: 3px;"
      "  }"
      "  .backgroundAttachmentFixed {"
      "    background-image: url('foo');"
      "    background-attachment: fixed;"
      "  }"
      "  .forceScroll {"
      "    height: 4000px;"
      "  }"
      "</style>"
      "<div id='overflowA'>"
      "  <div id='overflowB' class='backgroundAttachmentFixed'>"
      "    <div class='forceScroll'></div>"
      "  </div>"
      "  <div class='forceScroll'></div>"
      "</div>"
      "<div class='forceScroll'></div>");
  Element* overflowA = document().getElementById("overflowA");
  Element* overflowB = document().getElementById("overflowB");

  EXPECT_FALSE(
      overflowA->layoutObject()
          ->paintProperties()
          ->scroll()
          ->hasMainThreadScrollingReasons(
              MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects));
  EXPECT_FALSE(
      overflowB->layoutObject()
          ->paintProperties()
          ->scroll()
          ->hasMainThreadScrollingReasons(
              MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects));
  EXPECT_TRUE(
      overflowB->layoutObject()
          ->paintProperties()
          ->scroll()
          ->parent()
          ->hasMainThreadScrollingReasons(
              MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects));

  // Removing a main thread scrolling reason should update the entire tree.
  overflowB->removeAttribute("class");
  document().view()->updateAllLifecyclePhases();
  EXPECT_FALSE(
      overflowA->layoutObject()
          ->paintProperties()
          ->scroll()
          ->hasMainThreadScrollingReasons(
              MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects));
  EXPECT_FALSE(
      overflowB->layoutObject()
          ->paintProperties()
          ->scroll()
          ->hasMainThreadScrollingReasons(
              MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects));
  EXPECT_FALSE(
      overflowB->layoutObject()
          ->paintProperties()
          ->scroll()
          ->parent()
          ->hasMainThreadScrollingReasons(
              MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects));
}

TEST_P(PaintPropertyTreeBuilderTest, PaintOffsetsUnderMultiColumn) {
  setBodyInnerHTML(
      "<style>"
      "  body { margin: 0; }"
      "  .space { height: 30px; }"
      "  .abs { position: absolute; width: 20px; height: 20px; }"
      "</style>"
      "<div style='columns:2; width: 200px; column-gap: 0'>"
      "  <div style='position: relative'>"
      "    <div id=space1 class=space></div>"
      "    <div id=space2 class=space></div>"
      "    <div id=spanner style='column-span: all'>"
      "      <div id=normal style='height: 50px'></div>"
      "      <div id=top-left class=abs style='top: 0; left: 0'></div>"
      "      <div id=bottom-right class=abs style='bottom: 0; right: 0'></div>"
      "    </div>"
      "    <div id=space3 class=space></div>"
      "    <div id=space4 class=space></div>"
      "  </div>"
      "</div>");

  // Above the spanner.
  // Column 1.
  EXPECT_EQ(LayoutPoint(), paintOffset(getLayoutObjectByElementId("space1")));
  // Column 2. TODO(crbug.com/648274): This is incorrect. Should be (100, 0).
  EXPECT_EQ(LayoutPoint(0, 30),
            paintOffset(getLayoutObjectByElementId("space2")));

  // The spanner's normal flow.
  EXPECT_EQ(LayoutPoint(0, 30),
            paintOffset(getLayoutObjectByElementId("spanner")));
  EXPECT_EQ(LayoutPoint(0, 30),
            paintOffset(getLayoutObjectByElementId("normal")));

  // Below the spanner.
  // Column 1. TODO(crbug.com/648274): This is incorrect. Should be (0, 80).
  EXPECT_EQ(LayoutPoint(0, 60),
            paintOffset(getLayoutObjectByElementId("space3")));
  // Column 2. TODO(crbug.com/648274): This is incorrect. Should be (100, 80).
  EXPECT_EQ(LayoutPoint(0, 90),
            paintOffset(getLayoutObjectByElementId("space4")));

  // Out-of-flow positioned descendants of the spanner. They are laid out in
  // the relative-position container.

  // "top-left" should be aligned to the top-left corner of space1.
  EXPECT_EQ(LayoutPoint(0, 0),
            paintOffset(getLayoutObjectByElementId("top-left")));

  // "bottom-right" should be aligned to the bottom-right corner of space4.
  // TODO(crbug.com/648274): This is incorrect. Should be (180, 90).
  EXPECT_EQ(LayoutPoint(80, 100),
            paintOffset(getLayoutObjectByElementId("bottom-right")));
}

// Ensures no crash with multi-column containing relative-position inline with
// spanner with absolute-position children.
TEST_P(PaintPropertyTreeBuilderTest,
       MultiColumnInlineRelativeAndSpannerAndAbsPos) {
  setBodyInnerHTML(
      "<div style='columns:2; width: 200px; column-gap: 0'>"
      "  <span style='position: relative'>"
      "    <span id=spanner style='column-span: all'>"
      "      <div id=absolute style='position: absolute'>absolute</div>"
      "    </span>"
      "  </span>"
      "</div>");
  // The "spanner" isn't a real spanner because it's an inline.
  EXPECT_FALSE(getLayoutObjectByElementId("spanner")->isColumnSpanAll());

  setBodyInnerHTML(
      "<div style='columns:2; width: 200px; column-gap: 0'>"
      "  <span style='position: relative'>"
      "    <div id=spanner style='column-span: all'>"
      "      <div id=absolute style='position: absolute'>absolute</div>"
      "    </div>"
      "  </span>"
      "</div>");
  // There should be anonymous block created containing the inline "relative",
  // serving as the container of "absolute".
  EXPECT_TRUE(
      getLayoutObjectByElementId("absolute")->container()->isLayoutBlock());
}

TEST_P(PaintPropertyTreeBuilderTest, SimpleFilter) {
  setBodyInnerHTML(
      "<div id='filter' style='filter:opacity(0.5); height:1000px;'>"
      "</div>");
  const ObjectPaintProperties* filterProperties =
      getLayoutObjectByElementId("filter")->paintProperties();
  EXPECT_TRUE(filterProperties->effect()->parent()->isRoot());
  EXPECT_EQ(frameScrollTranslation(),
            filterProperties->effect()->localTransformSpace());
  EXPECT_EQ(frameContentClip(), filterProperties->effect()->outputClip());
}

TEST_P(PaintPropertyTreeBuilderTest, FilterReparentClips) {
  setBodyInnerHTML(
      "<div id='clip' style='overflow:hidden;'>"
      "  <div id='filter' style='filter:opacity(0.5); height:1000px;'>"
      "    <div id='child' style='position:fixed;'></div>"
      "  </div>"
      "</div>");
  const ObjectPaintProperties* clipProperties =
      getLayoutObjectByElementId("clip")->paintProperties();
  const ObjectPaintProperties* filterProperties =
      getLayoutObjectByElementId("filter")->paintProperties();
  EXPECT_TRUE(filterProperties->effect()->parent()->isRoot());
  EXPECT_EQ(frameScrollTranslation(),
            filterProperties->effect()->localTransformSpace());
  EXPECT_EQ(clipProperties->overflowClip(),
            filterProperties->effect()->outputClip());

  const ObjectPaintProperties* childProperties =
      getLayoutObjectByElementId("child")->paintProperties();
  const PropertyTreeState& childPaintState =
      childProperties->localBorderBoxProperties()->propertyTreeState;
  EXPECT_EQ(framePreTranslation(),
            childProperties->paintOffsetTranslation()->parent());
  EXPECT_EQ(childProperties->paintOffsetTranslation(),
            childPaintState.transform());
  // This will change once we added clip expansion node.
  EXPECT_EQ(filterProperties->effect()->outputClip(), childPaintState.clip());
  EXPECT_EQ(filterProperties->effect(), childPaintState.effect());
}

}  // namespace blink
