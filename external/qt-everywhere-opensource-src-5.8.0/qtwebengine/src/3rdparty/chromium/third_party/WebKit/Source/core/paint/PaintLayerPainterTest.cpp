// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/compositing/CompositedLayerMapping.h"
#include "core/paint/PaintControllerPaintTest.h"
#include "platform/graphics/GraphicsContext.h"

namespace blink {

class PaintLayerPainterTest
    : public PaintControllerPaintTest
    , public testing::WithParamInterface<FrameSettingOverrideFunction> {
    USING_FAST_MALLOC(PaintLayerPainterTest);
public:
    FrameSettingOverrideFunction settingOverrider() const override { return GetParam(); }
};

INSTANTIATE_TEST_CASE_P(All, PaintLayerPainterTest, ::testing::Values(
    nullptr,
    RootLayerScrollsFrameSettingOverride));

TEST_P(PaintLayerPainterTest, CachedSubsequence)
{
    setBodyInnerHTML(
        "<div id='container1' style='position: relative; z-index: 1; width: 200px; height: 200px; background-color: blue'>"
        "  <div id='content1' style='position: absolute; width: 100px; height: 100px; background-color: red'></div>"
        "</div>"
        "<div id='container2' style='position: relative; z-index: 1; width: 200px; height: 200px; background-color: blue'>"
        "  <div id='content2' style='position: absolute; width: 100px; height: 100px; background-color: green'></div>"
        "</div>");
    document().view()->updateAllLifecyclePhases();

    PaintLayer& htmlLayer = *toLayoutBoxModelObject(document().documentElement()->layoutObject())->layer();
    LayoutObject& container1 = *document().getElementById("container1")->layoutObject();
    PaintLayer& container1Layer = *toLayoutBoxModelObject(container1).layer();
    LayoutObject& content1 = *document().getElementById("content1")->layoutObject();
    LayoutObject& container2 = *document().getElementById("container2")->layoutObject();
    PaintLayer& container2Layer = *toLayoutBoxModelObject(container2).layer();
    LayoutObject& content2 = *document().getElementById("content2")->layoutObject();

    EXPECT_DISPLAY_LIST(rootPaintController().getDisplayItemList(), 11,
        TestDisplayItem(layoutView(), documentBackgroundType),
        TestDisplayItem(htmlLayer, DisplayItem::Subsequence),
        TestDisplayItem(container1Layer, DisplayItem::Subsequence),
        TestDisplayItem(container1, backgroundType),
        TestDisplayItem(content1, backgroundType),
        TestDisplayItem(container1Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(container2Layer, DisplayItem::Subsequence),
        TestDisplayItem(container2, backgroundType),
        TestDisplayItem(content2, backgroundType),
        TestDisplayItem(container2Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(htmlLayer, DisplayItem::EndSubsequence));

    toHTMLElement(content1.node())->setAttribute(HTMLNames::styleAttr, "position: absolute; width: 100px; height: 100px; background-color: green");
    document().view()->updateAllLifecyclePhasesExceptPaint();
    bool needsCommit = paintWithoutCommit();

    EXPECT_DISPLAY_LIST(rootPaintController().newDisplayItemList(), 8,
        TestDisplayItem(layoutView(), cachedDocumentBackgroundType),
        TestDisplayItem(htmlLayer, DisplayItem::Subsequence),
        TestDisplayItem(container1Layer, DisplayItem::Subsequence),
        TestDisplayItem(container1, cachedBackgroundType),
        TestDisplayItem(content1, backgroundType),
        TestDisplayItem(container1Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(container2Layer, DisplayItem::CachedSubsequence),
        TestDisplayItem(htmlLayer, DisplayItem::EndSubsequence));

    if (needsCommit)
        commit();

    EXPECT_DISPLAY_LIST(rootPaintController().getDisplayItemList(), 11,
        TestDisplayItem(layoutView(), documentBackgroundType),
        TestDisplayItem(htmlLayer, DisplayItem::Subsequence),
        TestDisplayItem(container1Layer, DisplayItem::Subsequence),
        TestDisplayItem(container1, backgroundType),
        TestDisplayItem(content1, backgroundType),
        TestDisplayItem(container1Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(container2Layer, DisplayItem::Subsequence),
        TestDisplayItem(container2, backgroundType),
        TestDisplayItem(content2, backgroundType),
        TestDisplayItem(container2Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(htmlLayer, DisplayItem::EndSubsequence));
}

TEST_P(PaintLayerPainterTest, CachedSubsequenceOnInterestRectChange)
{
    setBodyInnerHTML(
        "<div id='container1' style='position: relative; z-index: 1; width: 200px; height: 200px; background-color: blue'>"
        "  <div id='content1' style='position: absolute; width: 100px; height: 100px; background-color: green'></div>"
        "</div>"
        "<div id='container2' style='position: relative; z-index: 1; width: 200px; height: 200px; background-color: blue'>"
        "  <div id='content2a' style='position: absolute; width: 100px; height: 100px; background-color: green'></div>"
        "  <div id='content2b' style='position: absolute; top: 200px; width: 100px; height: 100px; background-color: green'></div>"
        "</div>"
        "<div id='container3' style='position: absolute; z-index: 2; left: 300px; top: 0; width: 200px; height: 200px; background-color: blue'>"
        "  <div id='content3' style='position: absolute; width: 200px; height: 200px; background-color: green'></div>"
        "</div>");
    rootPaintController().invalidateAll();

    PaintLayer& htmlLayer = *toLayoutBoxModelObject(document().documentElement()->layoutObject())->layer();
    LayoutObject& container1 = *document().getElementById("container1")->layoutObject();
    PaintLayer& container1Layer = *toLayoutBoxModelObject(container1).layer();
    LayoutObject& content1 = *document().getElementById("content1")->layoutObject();
    LayoutObject& container2 = *document().getElementById("container2")->layoutObject();
    PaintLayer& container2Layer = *toLayoutBoxModelObject(container2).layer();
    LayoutObject& content2a = *document().getElementById("content2a")->layoutObject();
    LayoutObject& content2b = *document().getElementById("content2b")->layoutObject();
    LayoutObject& container3 = *document().getElementById("container3")->layoutObject();
    PaintLayer& container3Layer = *toLayoutBoxModelObject(container3).layer();
    LayoutObject& content3 = *document().getElementById("content3")->layoutObject();

    document().view()->updateAllLifecyclePhasesExceptPaint();
    IntRect interestRect(0, 0, 400, 300);
    paint(&interestRect);

    // Container1 is fully in the interest rect;
    // Container2 is partly (including its stacking chidren) in the interest rect;
    // Content2b is out of the interest rect and output nothing;
    // Container3 is partly in the interest rect.
    EXPECT_DISPLAY_LIST(rootPaintController().getDisplayItemList(), 15,
        TestDisplayItem(layoutView(), documentBackgroundType),
        TestDisplayItem(htmlLayer, DisplayItem::Subsequence),
        TestDisplayItem(container1Layer, DisplayItem::Subsequence),
        TestDisplayItem(container1, backgroundType),
        TestDisplayItem(content1, backgroundType),
        TestDisplayItem(container1Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(container2Layer, DisplayItem::Subsequence),
        TestDisplayItem(container2, backgroundType),
        TestDisplayItem(content2a, backgroundType),
        TestDisplayItem(container2Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(container3Layer, DisplayItem::Subsequence),
        TestDisplayItem(container3, backgroundType),
        TestDisplayItem(content3, backgroundType),
        TestDisplayItem(container3Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(htmlLayer, DisplayItem::EndSubsequence));

    document().view()->updateAllLifecyclePhasesExceptPaint();
    IntRect newInterestRect(0, 100, 300, 1000);
    bool needsCommit = paintWithoutCommit(&newInterestRect);

    // Container1 becomes partly in the interest rect, but uses cached subsequence
    // because it was fully painted before;
    // Container2's intersection with the interest rect changes;
    // Content2b is out of the interest rect and outputs nothing;
    // Container3 becomes out of the interest rect and outputs empty subsequence pair..
    EXPECT_DISPLAY_LIST(rootPaintController().newDisplayItemList(), 11,
        TestDisplayItem(layoutView(), cachedDocumentBackgroundType),
        TestDisplayItem(htmlLayer, DisplayItem::Subsequence),
        TestDisplayItem(container1Layer, DisplayItem::CachedSubsequence),
        TestDisplayItem(container2Layer, DisplayItem::Subsequence),
        TestDisplayItem(container2, cachedBackgroundType),
        TestDisplayItem(content2a, cachedBackgroundType),
        TestDisplayItem(content2b, backgroundType),
        TestDisplayItem(container2Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(container3Layer, DisplayItem::Subsequence),
        TestDisplayItem(container3Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(htmlLayer, DisplayItem::EndSubsequence));

    if (needsCommit)
        commit();

    EXPECT_DISPLAY_LIST(rootPaintController().getDisplayItemList(), 14,
        TestDisplayItem(layoutView(), documentBackgroundType),
        TestDisplayItem(htmlLayer, DisplayItem::Subsequence),
        TestDisplayItem(container1Layer, DisplayItem::Subsequence),
        TestDisplayItem(container1, backgroundType),
        TestDisplayItem(content1, backgroundType),
        TestDisplayItem(container1Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(container2Layer, DisplayItem::Subsequence),
        TestDisplayItem(container2, backgroundType),
        TestDisplayItem(content2a, backgroundType),
        TestDisplayItem(content2b, backgroundType),
        TestDisplayItem(container2Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(container3Layer, DisplayItem::Subsequence),
        TestDisplayItem(container3Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(htmlLayer, DisplayItem::EndSubsequence));
}

TEST_P(PaintLayerPainterTest, CachedSubsequenceOnStyleChangeWithInterestRectClipping)
{
    setBodyInnerHTML(
        "<div id='container1' style='position: relative; z-index: 1; width: 200px; height: 200px; background-color: blue'>"
        "  <div id='content1' style='position: absolute; width: 100px; height: 100px; background-color: red'></div>"
        "</div>"
        "<div id='container2' style='position: relative; z-index: 1; width: 200px; height: 200px; background-color: blue'>"
        "  <div id='content2' style='position: absolute; width: 100px; height: 100px; background-color: green'></div>"
        "</div>");
    document().view()->updateAllLifecyclePhasesExceptPaint();
    IntRect interestRect(0, 0, 50, 300); // PaintResult of all subsequences will be MayBeClippedByPaintDirtyRect.
    paint(&interestRect);

    PaintLayer& htmlLayer = *toLayoutBoxModelObject(document().documentElement()->layoutObject())->layer();
    LayoutObject& container1 = *document().getElementById("container1")->layoutObject();
    PaintLayer& container1Layer = *toLayoutBoxModelObject(container1).layer();
    LayoutObject& content1 = *document().getElementById("content1")->layoutObject();
    LayoutObject& container2 = *document().getElementById("container2")->layoutObject();
    PaintLayer& container2Layer = *toLayoutBoxModelObject(container2).layer();
    LayoutObject& content2 = *document().getElementById("content2")->layoutObject();

    EXPECT_DISPLAY_LIST(rootPaintController().getDisplayItemList(), 11,
        TestDisplayItem(layoutView(), documentBackgroundType),
        TestDisplayItem(htmlLayer, DisplayItem::Subsequence),
        TestDisplayItem(container1Layer, DisplayItem::Subsequence),
        TestDisplayItem(container1, backgroundType),
        TestDisplayItem(content1, backgroundType),
        TestDisplayItem(container1Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(container2Layer, DisplayItem::Subsequence),
        TestDisplayItem(container2, backgroundType),
        TestDisplayItem(content2, backgroundType),
        TestDisplayItem(container2Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(htmlLayer, DisplayItem::EndSubsequence));

    toHTMLElement(content1.node())->setAttribute(HTMLNames::styleAttr, "position: absolute; width: 100px; height: 100px; background-color: green");
    document().view()->updateAllLifecyclePhasesExceptPaint();
    bool needsCommit = paintWithoutCommit(&interestRect);

    EXPECT_DISPLAY_LIST(rootPaintController().newDisplayItemList(), 8,
        TestDisplayItem(layoutView(), cachedDocumentBackgroundType),
        TestDisplayItem(htmlLayer, DisplayItem::Subsequence),
        TestDisplayItem(container1Layer, DisplayItem::Subsequence),
        TestDisplayItem(container1, cachedBackgroundType),
        TestDisplayItem(content1, backgroundType),
        TestDisplayItem(container1Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(container2Layer, DisplayItem::CachedSubsequence),
        TestDisplayItem(htmlLayer, DisplayItem::EndSubsequence));

    if (needsCommit)
        commit();

    EXPECT_DISPLAY_LIST(rootPaintController().getDisplayItemList(), 11,
        TestDisplayItem(layoutView(), documentBackgroundType),
        TestDisplayItem(htmlLayer, DisplayItem::Subsequence),
        TestDisplayItem(container1Layer, DisplayItem::Subsequence),
        TestDisplayItem(container1, backgroundType),
        TestDisplayItem(content1, backgroundType),
        TestDisplayItem(container1Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(container2Layer, DisplayItem::Subsequence),
        TestDisplayItem(container2, backgroundType),
        TestDisplayItem(content2, backgroundType),
        TestDisplayItem(container2Layer, DisplayItem::EndSubsequence),
        TestDisplayItem(htmlLayer, DisplayItem::EndSubsequence));
}

TEST_P(PaintLayerPainterTest, PaintPhaseOutline)
{
    AtomicString styleWithoutOutline = "width: 50px; height: 50px; background-color: green";
    AtomicString styleWithOutline = "outline: 1px solid blue; " + styleWithoutOutline;
    setBodyInnerHTML(
        "<div id='self-painting-layer' style='position: absolute'>"
        "  <div id='non-self-painting-layer' style='overflow: hidden'>"
        "    <div>"
        "      <div id='outline'></div>"
        "    </div>"
        "  </div>"
        "</div>");
    LayoutObject& outlineDiv = *document().getElementById("outline")->layoutObject();
    toHTMLElement(outlineDiv.node())->setAttribute(HTMLNames::styleAttr, styleWithoutOutline);
    document().view()->updateAllLifecyclePhases();

    LayoutBlock& selfPaintingLayerObject = *toLayoutBlock(document().getElementById("self-painting-layer")->layoutObject());
    PaintLayer& selfPaintingLayer = *selfPaintingLayerObject.layer();
    ASSERT_TRUE(selfPaintingLayer.isSelfPaintingLayer());
    PaintLayer& nonSelfPaintingLayer = *toLayoutBoxModelObject(document().getElementById("non-self-painting-layer")->layoutObject())->layer();
    ASSERT_FALSE(nonSelfPaintingLayer.isSelfPaintingLayer());
    ASSERT_TRUE(&nonSelfPaintingLayer == outlineDiv.enclosingLayer());

    EXPECT_FALSE(selfPaintingLayer.needsPaintPhaseDescendantOutlines());
    EXPECT_FALSE(nonSelfPaintingLayer.needsPaintPhaseDescendantOutlines());

    // Outline on the self-painting-layer node itself doesn't affect PaintPhaseDescendantOutlines.
    toHTMLElement(selfPaintingLayerObject.node())->setAttribute(HTMLNames::styleAttr, "position: absolute; outline: 1px solid green");
    document().view()->updateAllLifecyclePhases();
    EXPECT_FALSE(selfPaintingLayer.needsPaintPhaseDescendantOutlines());
    EXPECT_FALSE(nonSelfPaintingLayer.needsPaintPhaseDescendantOutlines());
    EXPECT_TRUE(displayItemListContains(rootPaintController().getDisplayItemList(), selfPaintingLayerObject, DisplayItem::paintPhaseToDrawingType(PaintPhaseSelfOutlineOnly)));

    // needsPaintPhaseDescendantOutlines should be set when any descendant on the same layer has outline.
    toHTMLElement(outlineDiv.node())->setAttribute(HTMLNames::styleAttr, styleWithOutline);
    document().view()->updateAllLifecyclePhasesExceptPaint();
    EXPECT_TRUE(selfPaintingLayer.needsPaintPhaseDescendantOutlines());
    EXPECT_FALSE(nonSelfPaintingLayer.needsPaintPhaseDescendantOutlines());
    paint();
    EXPECT_TRUE(displayItemListContains(rootPaintController().getDisplayItemList(), outlineDiv, DisplayItem::paintPhaseToDrawingType(PaintPhaseSelfOutlineOnly)));
}

TEST_P(PaintLayerPainterTest, PaintPhaseFloat)
{
    AtomicString styleWithoutFloat = "width: 50px; height: 50px; background-color: green";
    AtomicString styleWithFloat = "float: left; " + styleWithoutFloat;
    setBodyInnerHTML(
        "<div id='self-painting-layer' style='position: absolute'>"
        "  <div id='non-self-painting-layer' style='overflow: hidden'>"
        "    <div>"
        "      <div id='float' style='width: 10px; height: 10px; background-color: blue'></div>"
        "    </div>"
        "  </div>"
        "</div>");
    LayoutObject& floatDiv = *document().getElementById("float")->layoutObject();
    toHTMLElement(floatDiv.node())->setAttribute(HTMLNames::styleAttr, styleWithoutFloat);
    document().view()->updateAllLifecyclePhases();

    LayoutBlock& selfPaintingLayerObject = *toLayoutBlock(document().getElementById("self-painting-layer")->layoutObject());
    PaintLayer& selfPaintingLayer = *selfPaintingLayerObject.layer();
    ASSERT_TRUE(selfPaintingLayer.isSelfPaintingLayer());
    PaintLayer& nonSelfPaintingLayer = *toLayoutBoxModelObject(document().getElementById("non-self-painting-layer")->layoutObject())->layer();
    ASSERT_FALSE(nonSelfPaintingLayer.isSelfPaintingLayer());
    ASSERT_TRUE(&nonSelfPaintingLayer == floatDiv.enclosingLayer());

    EXPECT_FALSE(selfPaintingLayer.needsPaintPhaseFloat());
    EXPECT_FALSE(nonSelfPaintingLayer.needsPaintPhaseFloat());

    // needsPaintPhaseFloat should be set when any descendant on the same layer has float.
    toHTMLElement(floatDiv.node())->setAttribute(HTMLNames::styleAttr, styleWithFloat);
    document().view()->updateAllLifecyclePhasesExceptPaint();
    EXPECT_TRUE(selfPaintingLayer.needsPaintPhaseFloat());
    EXPECT_FALSE(nonSelfPaintingLayer.needsPaintPhaseFloat());
    paint();
    EXPECT_TRUE(displayItemListContains(rootPaintController().getDisplayItemList(), floatDiv, DisplayItem::BoxDecorationBackground));
}

TEST_P(PaintLayerPainterTest, PaintPhaseFloatUnderInlineLayer)
{
    setBodyInnerHTML(
        "<div id='self-painting-layer' style='position: absolute'>"
        "  <div id='non-self-painting-layer' style='overflow: hidden'>"
        "    <span id='span' style='position: relative'>"
        "      <div id='float' style='width: 10px; height: 10px; background-color: blue; float: left'></div>"
        "    </span>"
        "  </div>"
        "</div>");
    document().view()->updateAllLifecyclePhases();

    LayoutObject& floatDiv = *document().getElementById("float")->layoutObject();
    LayoutInline& span = *toLayoutInline(document().getElementById("span")->layoutObject());
    PaintLayer& spanLayer = *span.layer();
    ASSERT_TRUE(&spanLayer == floatDiv.enclosingLayer());
    ASSERT_FALSE(spanLayer.needsPaintPhaseFloat());
    LayoutBlock& selfPaintingLayerObject = *toLayoutBlock(document().getElementById("self-painting-layer")->layoutObject());
    PaintLayer& selfPaintingLayer = *selfPaintingLayerObject.layer();
    ASSERT_TRUE(selfPaintingLayer.isSelfPaintingLayer());
    PaintLayer& nonSelfPaintingLayer = *toLayoutBoxModelObject(document().getElementById("non-self-painting-layer")->layoutObject())->layer();
    ASSERT_FALSE(nonSelfPaintingLayer.isSelfPaintingLayer());

    EXPECT_TRUE(selfPaintingLayer.needsPaintPhaseFloat());
    EXPECT_FALSE(nonSelfPaintingLayer.needsPaintPhaseFloat());
    EXPECT_FALSE(spanLayer.needsPaintPhaseFloat());
    EXPECT_TRUE(displayItemListContains(rootPaintController().getDisplayItemList(), floatDiv, DisplayItem::BoxDecorationBackground));
}

TEST_P(PaintLayerPainterTest, PaintPhaseBlockBackground)
{
    AtomicString styleWithoutBackground = "width: 50px; height: 50px";
    AtomicString styleWithBackground = "background: blue; " + styleWithoutBackground;
    setBodyInnerHTML(
        "<div id='self-painting-layer' style='position: absolute'>"
        "  <div id='non-self-painting-layer' style='overflow: hidden'>"
        "    <div>"
        "      <div id='background'></div>"
        "    </div>"
        "  </div>"
        "</div>");
    LayoutObject& backgroundDiv = *document().getElementById("background")->layoutObject();
    toHTMLElement(backgroundDiv.node())->setAttribute(HTMLNames::styleAttr, styleWithoutBackground);
    document().view()->updateAllLifecyclePhases();

    LayoutBlock& selfPaintingLayerObject = *toLayoutBlock(document().getElementById("self-painting-layer")->layoutObject());
    PaintLayer& selfPaintingLayer = *selfPaintingLayerObject.layer();
    ASSERT_TRUE(selfPaintingLayer.isSelfPaintingLayer());
    PaintLayer& nonSelfPaintingLayer = *toLayoutBoxModelObject(document().getElementById("non-self-painting-layer")->layoutObject())->layer();
    ASSERT_FALSE(nonSelfPaintingLayer.isSelfPaintingLayer());
    ASSERT_TRUE(&nonSelfPaintingLayer == backgroundDiv.enclosingLayer());

    EXPECT_FALSE(selfPaintingLayer.needsPaintPhaseDescendantBlockBackgrounds());
    EXPECT_FALSE(nonSelfPaintingLayer.needsPaintPhaseDescendantBlockBackgrounds());

    // Background on the self-painting-layer node itself doesn't affect PaintPhaseDescendantBlockBackgrounds.
    toHTMLElement(selfPaintingLayerObject.node())->setAttribute(HTMLNames::styleAttr, "position: absolute; background: green");
    document().view()->updateAllLifecyclePhases();
    EXPECT_FALSE(selfPaintingLayer.needsPaintPhaseDescendantBlockBackgrounds());
    EXPECT_FALSE(nonSelfPaintingLayer.needsPaintPhaseDescendantBlockBackgrounds());
    EXPECT_TRUE(displayItemListContains(rootPaintController().getDisplayItemList(), selfPaintingLayerObject, DisplayItem::BoxDecorationBackground));

    // needsPaintPhaseDescendantBlockBackgrounds should be set when any descendant on the same layer has Background.
    toHTMLElement(backgroundDiv.node())->setAttribute(HTMLNames::styleAttr, styleWithBackground);
    document().view()->updateAllLifecyclePhasesExceptPaint();
    EXPECT_TRUE(selfPaintingLayer.needsPaintPhaseDescendantBlockBackgrounds());
    EXPECT_FALSE(nonSelfPaintingLayer.needsPaintPhaseDescendantBlockBackgrounds());
    paint();
    EXPECT_TRUE(displayItemListContains(rootPaintController().getDisplayItemList(), backgroundDiv, DisplayItem::BoxDecorationBackground));
}

TEST_P(PaintLayerPainterTest, PaintPhasesUpdateOnLayerRemoval)
{
    setBodyInnerHTML(
        "<div id='layer' style='position: relative'>"
        "  <div style='height: 100px'>"
        "    <div style='height: 20px; outline: 1px solid red; background-color: green'>outline and background</div>"
        "    <div style='float: left'>float</div>"
        "  </div>"
        "</div>");

    LayoutBlock& layerDiv = *toLayoutBlock(document().getElementById("layer")->layoutObject());
    PaintLayer& layer = *layerDiv.layer();
    ASSERT_TRUE(layer.isSelfPaintingLayer());
    EXPECT_TRUE(layer.needsPaintPhaseDescendantOutlines());
    EXPECT_TRUE(layer.needsPaintPhaseFloat());
    EXPECT_TRUE(layer.needsPaintPhaseDescendantBlockBackgrounds());

    PaintLayer& htmlLayer = *toLayoutBlock(document().documentElement()->layoutObject())->layer();
    EXPECT_FALSE(htmlLayer.needsPaintPhaseDescendantOutlines());
    EXPECT_FALSE(htmlLayer.needsPaintPhaseFloat());
    EXPECT_FALSE(htmlLayer.needsPaintPhaseDescendantBlockBackgrounds());

    toHTMLElement(layerDiv.node())->setAttribute(HTMLNames::styleAttr, "");
    document().view()->updateAllLifecyclePhases();

    EXPECT_FALSE(layerDiv.hasLayer());
    EXPECT_TRUE(htmlLayer.needsPaintPhaseDescendantOutlines());
    EXPECT_TRUE(htmlLayer.needsPaintPhaseFloat());
    EXPECT_TRUE(htmlLayer.needsPaintPhaseDescendantBlockBackgrounds());
}

TEST_P(PaintLayerPainterTest, PaintPhasesUpdateOnLayerAddition)
{
    setBodyInnerHTML(
        "<div id='will-be-layer'>"
        "  <div style='height: 100px'>"
        "    <div style='height: 20px; outline: 1px solid red; background-color: green'>outline and background</div>"
        "    <div style='float: left'>float</div>"
        "  </div>"
        "</div>");

    LayoutBlock& layerDiv = *toLayoutBlock(document().getElementById("will-be-layer")->layoutObject());
    EXPECT_FALSE(layerDiv.hasLayer());

    PaintLayer& htmlLayer = *toLayoutBlock(document().documentElement()->layoutObject())->layer();
    EXPECT_TRUE(htmlLayer.needsPaintPhaseDescendantOutlines());
    EXPECT_TRUE(htmlLayer.needsPaintPhaseFloat());
    EXPECT_TRUE(htmlLayer.needsPaintPhaseDescendantBlockBackgrounds());

    toHTMLElement(layerDiv.node())->setAttribute(HTMLNames::styleAttr, "position: relative");
    document().view()->updateAllLifecyclePhases();
    ASSERT_TRUE(layerDiv.hasLayer());
    PaintLayer& layer = *layerDiv.layer();
    ASSERT_TRUE(layer.isSelfPaintingLayer());
    EXPECT_TRUE(layer.needsPaintPhaseDescendantOutlines());
    EXPECT_TRUE(layer.needsPaintPhaseFloat());
    EXPECT_TRUE(layer.needsPaintPhaseDescendantBlockBackgrounds());
}

TEST_P(PaintLayerPainterTest, PaintPhasesUpdateOnBecomingSelfPainting)
{
    setBodyInnerHTML(
        "<div id='will-be-self-painting' style='width: 100px; height: 100px; overflow: hidden'>"
        "  <div>"
        "    <div style='outline: 1px solid red; background-color: green'>outline and background</div>"
        "  </div>"
        "</div>");

    LayoutBlock& layerDiv = *toLayoutBlock(document().getElementById("will-be-self-painting")->layoutObject());
    ASSERT_TRUE(layerDiv.hasLayer());
    EXPECT_FALSE(layerDiv.layer()->isSelfPaintingLayer());

    PaintLayer& htmlLayer = *toLayoutBlock(document().documentElement()->layoutObject())->layer();
    EXPECT_TRUE(htmlLayer.needsPaintPhaseDescendantOutlines());
    EXPECT_TRUE(htmlLayer.needsPaintPhaseDescendantBlockBackgrounds());

    toHTMLElement(layerDiv.node())->setAttribute(HTMLNames::styleAttr, "width: 100px; height: 100px; overflow: hidden; position: relative");
    document().view()->updateAllLifecyclePhases();
    PaintLayer& layer = *layerDiv.layer();
    ASSERT_TRUE(layer.isSelfPaintingLayer());
    EXPECT_TRUE(layer.needsPaintPhaseDescendantOutlines());
    EXPECT_TRUE(layer.needsPaintPhaseDescendantBlockBackgrounds());
}

TEST_P(PaintLayerPainterTest, PaintPhasesUpdateOnBecomingNonSelfPainting)
{
    setBodyInnerHTML(
        "<div id='will-be-non-self-painting' style='width: 100px; height: 100px; overflow: hidden; position: relative'>"
        "  <div>"
        "    <div style='outline: 1px solid red; background-color: green'>outline and background</div>"
        "  </div>"
        "</div>");

    LayoutBlock& layerDiv = *toLayoutBlock(document().getElementById("will-be-non-self-painting")->layoutObject());
    ASSERT_TRUE(layerDiv.hasLayer());
    PaintLayer& layer = *layerDiv.layer();
    EXPECT_TRUE(layer.isSelfPaintingLayer());
    EXPECT_TRUE(layer.needsPaintPhaseDescendantOutlines());
    EXPECT_TRUE(layer.needsPaintPhaseDescendantBlockBackgrounds());

    PaintLayer& htmlLayer = *toLayoutBlock(document().documentElement()->layoutObject())->layer();
    EXPECT_FALSE(htmlLayer.needsPaintPhaseDescendantOutlines());
    EXPECT_FALSE(htmlLayer.needsPaintPhaseDescendantBlockBackgrounds());

    toHTMLElement(layerDiv.node())->setAttribute(HTMLNames::styleAttr, "width: 100px; height: 100px; overflow: hidden");
    document().view()->updateAllLifecyclePhases();
    EXPECT_FALSE(layer.isSelfPaintingLayer());
    EXPECT_TRUE(htmlLayer.needsPaintPhaseDescendantOutlines());
    EXPECT_TRUE(htmlLayer.needsPaintPhaseDescendantBlockBackgrounds());
}

TEST_P(PaintLayerPainterTest, TableCollapsedBorderNeedsPaintPhaseDescendantBlockBackgrounds)
{
    // "position: relative" makes the table and td self-painting layers.
    // The table's layer should be marked needsPaintPhaseDescendantBlockBackground because it
    // will paint collapsed borders in the phase.
    setBodyInnerHTML(
        "<table id='table' style='position: relative; border-collapse: collapse'>"
        "  <tr><td style='position: relative; border: 1px solid green'>Cell</td></tr>"
        "</table>");

    LayoutBlock& table = *toLayoutBlock(getLayoutObjectByElementId("table"));
    ASSERT_TRUE(table.hasLayer());
    PaintLayer& layer = *table.layer();
    EXPECT_TRUE(layer.isSelfPaintingLayer());
    EXPECT_TRUE(layer.needsPaintPhaseDescendantBlockBackgrounds());
}

TEST_P(PaintLayerPainterTest, TableCollapsedBorderNeedsPaintPhaseDescendantBlockBackgroundsDynamic)
{
    setBodyInnerHTML(
        "<table id='table' style='position: relative'>"
        "  <tr><td style='position: relative; border: 1px solid green'>Cell</td></tr>"
        "</table>");

    LayoutBlock& table = *toLayoutBlock(getLayoutObjectByElementId("table"));
    ASSERT_TRUE(table.hasLayer());
    PaintLayer& layer = *table.layer();
    EXPECT_TRUE(layer.isSelfPaintingLayer());
    EXPECT_FALSE(layer.needsPaintPhaseDescendantBlockBackgrounds());

    toHTMLElement(table.node())->setAttribute(HTMLNames::styleAttr, "position: relative; border-collapse: collapse");
    document().view()->updateAllLifecyclePhases();
    EXPECT_TRUE(layer.needsPaintPhaseDescendantBlockBackgrounds());
}

} // namespace blink
