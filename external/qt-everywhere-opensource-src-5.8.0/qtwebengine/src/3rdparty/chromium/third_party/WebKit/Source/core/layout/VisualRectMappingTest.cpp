// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutView.h"
#include "core/layout/PaintInvalidationState.h"
#include "core/paint/PaintLayer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class VisualRectMappingTest : public RenderingTest {
public:
    VisualRectMappingTest()
        : RenderingTest(SingleChildFrameLoaderClient::create()) {}
protected:
    LayoutView& layoutView() const { return *document().layoutView(); }

    void checkPaintInvalidationStateRectMapping(const LayoutRect& expectedRect, const LayoutRect& rect, const LayoutObject& object, const LayoutView& layoutView, const LayoutObject& paintInvalidationContainer)
    {
        Vector<const LayoutObject*> ancestors;
        for (const LayoutObject* ancestor = &object; ancestor != layoutView; ancestor = ancestor->paintInvalidationParent())
            ancestors.append(ancestor);

        Vector<Optional<PaintInvalidationState>> paintInvalidationStates(ancestors.size() + 1);
        Vector<const LayoutObject*> pendingDelayedPaintInvalidations;
        paintInvalidationStates[0].emplace(layoutView, pendingDelayedPaintInvalidations);
        if (layoutView != object)
            paintInvalidationStates[0]->updateForChildren(PaintInvalidationFull);
        for (size_t i = 1; i < paintInvalidationStates.size(); ++i) {
            paintInvalidationStates[i].emplace(*paintInvalidationStates[i - 1], *ancestors[ancestors.size() - i]);
            if (paintInvalidationStates[i]->m_currentObject != object)
                paintInvalidationStates[i]->updateForChildren(PaintInvalidationFull);
        }

        const PaintInvalidationState& paintInvalidationState = *paintInvalidationStates.last();
        ASSERT_EQ(paintInvalidationState.m_currentObject, object);
        ASSERT_EQ(&paintInvalidationState.paintInvalidationContainer(), &paintInvalidationContainer);

        LayoutRect r = rect;
        paintInvalidationState.mapLocalRectToPaintInvalidationContainer(r);
        EXPECT_EQ(expectedRect, r);
    }
};

TEST_F(VisualRectMappingTest, LayoutText)
{
    setBodyInnerHTML(
        "<style>body { margin: 0; }</style>"
        "<div id='container' style='overflow: scroll; width: 50px; height: 50px'>"
        "  <span><img style='width: 20px; height: 100px'></span>"
        "  text text text text text text text"
        "</div>");

    LayoutBlock* container = toLayoutBlock(getLayoutObjectByElementId("container"));
    LayoutText* text = toLayoutText(container->lastChild());

    container->setScrollTop(LayoutUnit(50));
    LayoutRect originalRect(0, 60, 20, 80);
    LayoutRect rect = originalRect;
    EXPECT_TRUE(text->mapToVisualRectInAncestorSpace(container, rect));
    EXPECT_EQ(rect, LayoutRect(0, 10, 20, 80));

    rect = originalRect;
    EXPECT_TRUE(text->mapToVisualRectInAncestorSpace(&layoutView(), rect));
    EXPECT_EQ(rect, LayoutRect(0, 10, 20, 40));
    checkPaintInvalidationStateRectMapping(rect, originalRect, *text, layoutView(), layoutView());

    rect = LayoutRect(0, 60, 80, 0);
    EXPECT_TRUE(text->mapToVisualRectInAncestorSpace(container, rect, EdgeInclusive));
    EXPECT_EQ(rect, LayoutRect(0, 10, 80, 0));
}

TEST_F(VisualRectMappingTest, LayoutInline)
{
    document().setBaseURLOverride(KURL(ParsedURLString, "http://test.com"));
    setBodyInnerHTML(
        "<style>body { margin: 0; }</style>"
        "<div id='container' style='overflow: scroll; width: 50px; height: 50px'>"
        "  <span><img style='width: 20px; height: 100px'></span>"
        "  <span id=leaf></span></div>");

    LayoutBlock* container = toLayoutBlock(getLayoutObjectByElementId("container"));
    LayoutObject* leaf = container->lastChild();

    container->setScrollTop(LayoutUnit(50));
    LayoutRect originalRect(0, 60, 20, 80);
    LayoutRect rect = originalRect;
    EXPECT_TRUE(leaf->mapToVisualRectInAncestorSpace(container, rect));
    EXPECT_EQ(rect, LayoutRect(0, 10, 20, 80));

    rect = originalRect;
    EXPECT_TRUE(leaf->mapToVisualRectInAncestorSpace(&layoutView(), rect));
    EXPECT_EQ(rect, LayoutRect(0, 10, 20, 40));
    checkPaintInvalidationStateRectMapping(rect, originalRect, *leaf, layoutView(), layoutView());

    rect = LayoutRect(0, 60, 80, 0);
    EXPECT_TRUE(leaf->mapToVisualRectInAncestorSpace(container, rect, EdgeInclusive));
    EXPECT_EQ(rect, LayoutRect(0, 10, 80, 0));
}

TEST_F(VisualRectMappingTest, LayoutView)
{
    document().setBaseURLOverride(KURL(ParsedURLString, "http://test.com"));
    setBodyInnerHTML(
        "<style>body { margin: 0; }</style>"
        "<div id=frameContainer>"
        "  <iframe id=frame src='http://test.com' width='50' height='50' frameBorder='0'></iframe>"
        "</div>");

    Document& frameDocument = setupChildIframe("frame", "<style>body { margin: 0; }</style><span><img style='width: 20px; height: 100px'></span>text text text");
    document().view()->updateAllLifecyclePhases();

    LayoutBlock* frameContainer = toLayoutBlock(getLayoutObjectByElementId("frameContainer"));
    LayoutBlock* frameBody = toLayoutBlock(frameDocument.body()->layoutObject());
    LayoutText* frameText = toLayoutText(frameBody->lastChild());

    // This case involves clipping: frame height is 50, y-coordinate of result rect is 13,
    // so height should be clipped to (50 - 13) == 37.
    frameDocument.view()->setScrollPosition(DoublePoint(0, 47), ProgrammaticScroll);
    LayoutRect originalRect(4, 60, 20, 80);
    LayoutRect rect = originalRect;
    EXPECT_TRUE(frameText->mapToVisualRectInAncestorSpace(frameContainer, rect));
    EXPECT_EQ(rect, LayoutRect(4, 13, 20, 37));

    rect = originalRect;
    EXPECT_TRUE(frameText->mapToVisualRectInAncestorSpace(&layoutView(), rect));
    EXPECT_EQ(rect, LayoutRect(4, 13, 20, 37));
    checkPaintInvalidationStateRectMapping(rect, originalRect, *frameText, layoutView(), layoutView());

    rect = LayoutRect(4, 60, 0, 80);
    EXPECT_TRUE(frameText->mapToVisualRectInAncestorSpace(frameContainer, rect, EdgeInclusive));
    EXPECT_EQ(rect, LayoutRect(4, 13, 0, 37));
}

TEST_F(VisualRectMappingTest, LayoutViewSubpixelRounding)
{
    document().setBaseURLOverride(KURL(ParsedURLString, "http://test.com"));
    setBodyInnerHTML(
        "<style>body { margin: 0; }</style>"
        "<div id=frameContainer style='position: relative; left: 0.5px'>"
        "  <iframe id=frame style='position: relative; left: 0.5px' src='http://test.com' width='200' height='200' frameBorder='0'></iframe>"
        "</div>");

    Document& frameDocument = setupChildIframe(
        "frame", "<style>body { margin: 0; }</style><div id='target' style='position: relative; width: 100px; height: 100px; left: 0.5px'>");
    document().view()->updateAllLifecyclePhases();

    LayoutBlock* frameContainer = toLayoutBlock(getLayoutObjectByElementId("frameContainer"));
    LayoutObject* target = frameDocument.getElementById("target")->layoutObject();
    LayoutRect rect(0, 0, 100, 100);
    EXPECT_TRUE(target->mapToVisualRectInAncestorSpace(frameContainer, rect));
    // When passing from the iframe to the parent frame, the rect of (0.5, 0, 100, 100) is expanded to (0, 0, 100, 100), and then offset by
    // the 0.5 offset of frameContainer.
    EXPECT_EQ(LayoutRect(LayoutPoint(DoublePoint(0.5, 0)), LayoutSize(101, 100)), rect);
}

TEST_F(VisualRectMappingTest, LayoutViewDisplayNone)
{
    document().setBaseURLOverride(KURL(ParsedURLString, "http://test.com"));
    setBodyInnerHTML(
        "<style>body { margin: 0; }</style>"
        "<div id=frameContainer>"
        "  <iframe id=frame src='http://test.com' width='50' height='50' frameBorder='0'></iframe>"
        "</div>");

    Document& frameDocument = setupChildIframe("frame", "<style>body { margin: 0; }</style><div style='width:100px;height:100px;'></div>");
    document().view()->updateAllLifecyclePhases();

    LayoutBlock* frameContainer = toLayoutBlock(getLayoutObjectByElementId("frameContainer"));
    LayoutBlock* frameBody = toLayoutBlock(frameDocument.body()->layoutObject());
    LayoutBlock* frameDiv = toLayoutBlock(frameBody->lastChild());

    // This part is copied from the LayoutView test, just to ensure that the mapped
    // rect is valid before display:none is set on the iframe.
    frameDocument.view()->setScrollPosition(DoublePoint(0, 47), ProgrammaticScroll);
    LayoutRect originalRect(4, 60, 20, 80);
    LayoutRect rect = originalRect;
    EXPECT_TRUE(frameDiv->mapToVisualRectInAncestorSpace(frameContainer, rect));
    EXPECT_EQ(rect, LayoutRect(4, 13, 20, 37));

    Element* frameElement = document().getElementById("frame");
    frameElement->setInlineStyleProperty(CSSPropertyDisplay, "none");
    document().view()->updateAllLifecyclePhases();

    rect = originalRect;
    EXPECT_FALSE(frameDiv->mapToVisualRectInAncestorSpace(&layoutView(), rect));
    EXPECT_EQ(rect, LayoutRect());
}

TEST_F(VisualRectMappingTest, SelfFlippedWritingMode)
{
    setBodyInnerHTML(
        "<div id='target' style='writing-mode: vertical-rl; box-shadow: 40px 20px black;"
        "    width: 100px; height: 50px; position: absolute; top: 111px; left: 222px'>"
        "</div>");

    LayoutBlock* target = toLayoutBlock(getLayoutObjectByElementId("target"));
    LayoutRect overflowRect = target->localOverflowRectForPaintInvalidation();
    // -40 = -box_shadow_offset_x(40) (with target's top-right corner as the origin)
    // 140 = width(100) + box_shadow_offset_x(40)
    // 70 = height(50) + box_shadow_offset_y(20)
    EXPECT_EQ(LayoutRect(-40, 0, 140, 70), overflowRect);

    LayoutRect rect = overflowRect;
    EXPECT_TRUE(target->mapToVisualRectInAncestorSpace(target, rect));
    // This rect is in physical coordinates of target.
    EXPECT_EQ(LayoutRect(0, 0, 140, 70), rect);

    rect = overflowRect;
    EXPECT_TRUE(target->mapToVisualRectInAncestorSpace(&layoutView(), rect));
    EXPECT_EQ(LayoutRect(222, 111, 140, 70), rect);
    checkPaintInvalidationStateRectMapping(rect, overflowRect, *target, layoutView(), layoutView());
}

TEST_F(VisualRectMappingTest, ContainerFlippedWritingMode)
{
    setBodyInnerHTML(
        "<div id='container' style='writing-mode: vertical-rl; position: absolute; top: 111px; left: 222px'>"
        "    <div id='target' style='box-shadow: 40px 20px black; width: 100px; height: 90px'></div>"
        "    <div style='width: 100px; height: 100px'></div>"
        "</div>");

    LayoutBlock* target = toLayoutBlock(getLayoutObjectByElementId("target"));
    LayoutRect targetOverflowRect = target->localOverflowRectForPaintInvalidation();
    // -40 = -box_shadow_offset_x(40) (with target's top-right corner as the origin)
    // 140 = width(100) + box_shadow_offset_x(40)
    // 110 = height(90) + box_shadow_offset_y(20)
    EXPECT_EQ(LayoutRect(-40, 0, 140, 110), targetOverflowRect);

    LayoutRect rect = targetOverflowRect;
    EXPECT_TRUE(target->mapToVisualRectInAncestorSpace(target, rect));
    // This rect is in physical coordinates of target.
    EXPECT_EQ(LayoutRect(0, 0, 140, 110), rect);

    LayoutBlock* container = toLayoutBlock(getLayoutObjectByElementId("container"));
    rect = targetOverflowRect;
    EXPECT_TRUE(target->mapToVisualRectInAncestorSpace(container, rect));
    // 100 is the physical x location of target in container.
    EXPECT_EQ(LayoutRect(100, 0, 140, 110), rect);
    rect = targetOverflowRect;
    EXPECT_TRUE(target->mapToVisualRectInAncestorSpace(&layoutView(), rect));
    EXPECT_EQ(LayoutRect(322, 111, 140, 110), rect);
    checkPaintInvalidationStateRectMapping(rect, targetOverflowRect, *target, layoutView(), layoutView());

    LayoutRect containerOverflowRect = container->localOverflowRectForPaintInvalidation();
    EXPECT_EQ(LayoutRect(0, 0, 200, 100), containerOverflowRect);
    rect = containerOverflowRect;
    EXPECT_TRUE(container->mapToVisualRectInAncestorSpace(container, rect));
    EXPECT_EQ(LayoutRect(0, 0, 200, 100), rect);
    rect = containerOverflowRect;
    EXPECT_TRUE(container->mapToVisualRectInAncestorSpace(&layoutView(), rect));
    EXPECT_EQ(LayoutRect(222, 111, 200, 100), rect);
    checkPaintInvalidationStateRectMapping(rect, containerOverflowRect, *container, layoutView(), layoutView());
}

TEST_F(VisualRectMappingTest, ContainerOverflowScroll)
{
    setBodyInnerHTML(
        "<div id='container' style='position: absolute; top: 111px; left: 222px;"
        "    border: 10px solid red; overflow: scroll; width: 50px; height: 80px;'>"
        "    <div id='target' style='box-shadow: 40px 20px black; width: 100px; height: 90px'></div>"
        "</div>");

    LayoutBlock* container = toLayoutBlock(getLayoutObjectByElementId("container"));
    EXPECT_EQ(LayoutUnit(), container->scrollTop());
    EXPECT_EQ(LayoutUnit(), container->scrollLeft());
    container->setScrollTop(LayoutUnit(7));
    container->setScrollLeft(LayoutUnit(8));
    document().view()->updateAllLifecyclePhases();

    LayoutBlock* target = toLayoutBlock(getLayoutObjectByElementId("target"));
    LayoutRect targetOverflowRect = target->localOverflowRectForPaintInvalidation();
    // 140 = width(100) + box_shadow_offset_x(40)
    // 110 = height(90) + box_shadow_offset_y(20)
    EXPECT_EQ(LayoutRect(0, 0, 140, 110), targetOverflowRect);
    LayoutRect rect = targetOverflowRect;
    EXPECT_TRUE(target->mapToVisualRectInAncestorSpace(target, rect));
    EXPECT_EQ(LayoutRect(0, 0, 140, 110), rect);

    rect = targetOverflowRect;
    EXPECT_TRUE(target->mapToVisualRectInAncestorSpace(container, rect));
    // 2 = target_x(0) + container_border_left(10) - scroll_left(8)
    // 3 = target_y(0) + container_border_top(10) - scroll_top(7)
    // Rect is not clipped by container's overflow clip because of overflow:scroll.
    EXPECT_EQ(LayoutRect(2, 3, 140, 110), rect);

    rect = targetOverflowRect;
    EXPECT_TRUE(target->mapToVisualRectInAncestorSpace(&layoutView(), rect));
    // (2, 3, 140, 100) is first clipped by container's overflow clip, to (10, 10, 50, 80),
    // then is by added container's offset in LayoutView (111, 222).
    EXPECT_EQ(LayoutRect(232, 121, 50, 80), rect);
    checkPaintInvalidationStateRectMapping(rect, targetOverflowRect, *target, layoutView(), layoutView());

    LayoutRect containerOverflowRect = container->localOverflowRectForPaintInvalidation();
    // Because container has overflow clip, its visual overflow doesn't include overflow from children.
    // 70 = width(50) + border_left_width(10) + border_right_width(10)
    // 100 = height(80) + border_top_width(10) + border_bottom_width(10)
    EXPECT_EQ(LayoutRect(0, 0, 70, 100), containerOverflowRect);
    rect = containerOverflowRect;
    EXPECT_TRUE(container->mapToVisualRectInAncestorSpace(container, rect));
    // Container should not apply overflow clip on its own overflow rect.
    EXPECT_EQ(LayoutRect(0, 0, 70, 100), rect);

    rect = containerOverflowRect;
    EXPECT_TRUE(container->mapToVisualRectInAncestorSpace(&layoutView(), rect));
    EXPECT_EQ(LayoutRect(222, 111, 70, 100), rect);
    checkPaintInvalidationStateRectMapping(rect, containerOverflowRect, *container, layoutView(), layoutView());
}

TEST_F(VisualRectMappingTest, ContainerFlippedWritingModeAndOverflowScroll)
{
    setBodyInnerHTML(
        "<div id='container' style='writing-mode: vertical-rl; position: absolute; top: 111px; left: 222px;"
        "    border: solid red; border-width: 10px 20px 30px 40px;"
        "    overflow: scroll; width: 50px; height: 80px'>"
        "    <div id='target' style='box-shadow: 40px 20px black; width: 100px; height: 90px'></div>"
        "    <div style='width: 100px; height: 100px'></div>"
        "</div>");

    LayoutBlock* container = toLayoutBlock(getLayoutObjectByElementId("container"));
    EXPECT_EQ(LayoutUnit(), container->scrollTop());
    // The initial scroll offset is to the left-most because of flipped blocks writing mode.
    // 150 = total_layout_overflow(100 + 100) - width(50)
    EXPECT_EQ(LayoutUnit(150), container->scrollLeft());
    container->setScrollTop(LayoutUnit(7));
    container->setScrollLeft(LayoutUnit(142)); // Scroll to the right by 8 pixels.
    document().view()->updateAllLifecyclePhases();

    LayoutBlock* target = toLayoutBlock(getLayoutObjectByElementId("target"));
    LayoutRect targetOverflowRect = target->localOverflowRectForPaintInvalidation();
    // -40 = -box_shadow_offset_x(40) (with target's top-right corner as the origin)
    // 140 = width(100) + box_shadow_offset_x(40)
    // 110 = height(90) + box_shadow_offset_y(20)
    EXPECT_EQ(LayoutRect(-40, 0, 140, 110), targetOverflowRect);

    LayoutRect rect = targetOverflowRect;
    EXPECT_TRUE(target->mapToVisualRectInAncestorSpace(target, rect));
    // This rect is in physical coordinates of target.
    EXPECT_EQ(LayoutRect(0, 0, 140, 110), rect);

    rect = targetOverflowRect;
    EXPECT_TRUE(target->mapToVisualRectInAncestorSpace(container, rect));
    // -2 = target_physical_x(100) + container_border_left(40) - scroll_left(142)
    // 3 = target_y(0) + container_border_top(10) - scroll_top(7)
    // Rect is clipped by container's overflow clip because of overflow:scroll.
    EXPECT_EQ(LayoutRect(-2, 3, 140, 110), rect);

    rect = targetOverflowRect;
    EXPECT_TRUE(target->mapToVisualRectInAncestorSpace(&layoutView(), rect));
    // (-2, 3, 140, 100) is first clipped by container's overflow clip, to (40, 10, 50, 80),
    // then is added by container's offset in LayoutView (111, 222).
    // TODO(crbug.com/600039): rect.x() should be 262 (left + border-left), but is offset
    // by extra horizontal border-widths because of layout error.
    EXPECT_EQ(LayoutRect(322, 121, 50, 80), rect);
    checkPaintInvalidationStateRectMapping(rect, targetOverflowRect, *target, layoutView(), layoutView());

    LayoutRect containerOverflowRect = container->localOverflowRectForPaintInvalidation();
    // Because container has overflow clip, its visual overflow doesn't include overflow from children.
    // 110 = width(50) + border_left_width(40) + border_right_width(20)
    // 120 = height(80) + border_top_width(10) + border_bottom_width(30)
    EXPECT_EQ(LayoutRect(0, 0, 110, 120), containerOverflowRect);

    rect = containerOverflowRect;
    EXPECT_TRUE(container->mapToVisualRectInAncestorSpace(container, rect));
    EXPECT_EQ(LayoutRect(0, 0, 110, 120), rect);

    rect = containerOverflowRect;
    EXPECT_TRUE(container->mapToVisualRectInAncestorSpace(&layoutView(), rect));
    // TODO(crbug.com/600039): rect.x() should be 222 (left), but is offset by extra horizontal
    // border-widths because of layout error.
    EXPECT_EQ(LayoutRect(282, 111, 110, 120), rect);
    checkPaintInvalidationStateRectMapping(rect, containerOverflowRect, *container, layoutView(), layoutView());
}

TEST_F(VisualRectMappingTest, ContainerOverflowHidden)
{
    setBodyInnerHTML(
        "<div id='container' style='position: absolute; top: 111px; left: 222px;"
        "    border: 10px solid red; overflow: hidden; width: 50px; height: 80px;'>"
        "    <div id='target' style='box-shadow: 40px 20px black; width: 100px; height: 90px'></div>"
        "</div>");

    LayoutBlock* container = toLayoutBlock(getLayoutObjectByElementId("container"));
    EXPECT_EQ(LayoutUnit(), container->scrollTop());
    EXPECT_EQ(LayoutUnit(), container->scrollLeft());
    container->setScrollTop(LayoutUnit(27));
    container->setScrollLeft(LayoutUnit(28));
    document().view()->updateAllLifecyclePhases();

    LayoutBlock* target = toLayoutBlock(getLayoutObjectByElementId("target"));
    LayoutRect targetOverflowRect = target->localOverflowRectForPaintInvalidation();
    // 140 = width(100) + box_shadow_offset_x(40)
    // 110 = height(90) + box_shadow_offset_y(20)
    EXPECT_EQ(LayoutRect(0, 0, 140, 110), targetOverflowRect);
    LayoutRect rect = targetOverflowRect;
    EXPECT_TRUE(target->mapToVisualRectInAncestorSpace(target, rect));
    EXPECT_EQ(LayoutRect(0, 0, 140, 110), rect);

    rect = targetOverflowRect;
    EXPECT_TRUE(target->mapToVisualRectInAncestorSpace(container, rect));
    // Rect is clipped by container's overflow clip.
    EXPECT_EQ(LayoutRect(10, 10, 50, 80), rect);
}

TEST_F(VisualRectMappingTest, ContainerFlippedWritingModeAndOverflowHidden)
{
    setBodyInnerHTML(
        "<div id='container' style='writing-mode: vertical-rl; position: absolute; top: 111px; left: 222px;"
        "    border: solid red; border-width: 10px 20px 30px 40px;"
        "    overflow: hidden; width: 50px; height: 80px'>"
        "    <div id='target' style='box-shadow: 40px 20px black; width: 100px; height: 90px'></div>"
        "    <div style='width: 100px; height: 100px'></div>"
        "</div>");

    LayoutBlock* container = toLayoutBlock(getLayoutObjectByElementId("container"));
    EXPECT_EQ(LayoutUnit(), container->scrollTop());
    // The initial scroll offset is to the left-most because of flipped blocks writing mode.
    // 150 = total_layout_overflow(100 + 100) - width(50)
    EXPECT_EQ(LayoutUnit(150), container->scrollLeft());
    container->setScrollTop(LayoutUnit(7));
    container->setScrollLeft(LayoutUnit(82)); // Scroll to the right by 8 pixels.
    document().view()->updateAllLifecyclePhases();

    LayoutBlock* target = toLayoutBlock(getLayoutObjectByElementId("target"));
    LayoutRect targetOverflowRect = target->localOverflowRectForPaintInvalidation();
    // -40 = -box_shadow_offset_x(40) (with target's top-right corner as the origin)
    // 140 = width(100) + box_shadow_offset_x(40)
    // 110 = height(90) + box_shadow_offset_y(20)
    EXPECT_EQ(LayoutRect(-40, 0, 140, 110), targetOverflowRect);

    LayoutRect rect = targetOverflowRect;
    EXPECT_TRUE(target->mapToVisualRectInAncestorSpace(target, rect));
    // This rect is in physical coordinates of target.
    EXPECT_EQ(LayoutRect(0, 0, 140, 110), rect);

    rect = targetOverflowRect;
    EXPECT_TRUE(target->mapToVisualRectInAncestorSpace(container, rect));
    // 58 = target_physical_x(100) + container_border_left(40) - scroll_left(58)
    // The other sides of the rect are clipped by container's overflow clip.
    EXPECT_EQ(LayoutRect(58, 10, 32, 80), rect);
}

TEST_F(VisualRectMappingTest, ContainerAndTargetDifferentFlippedWritingMode)
{
    setBodyInnerHTML(
        "<div id='container' style='writing-mode: vertical-rl; position: absolute; top: 111px; left: 222px;"
        "    border: solid red; border-width: 10px 20px 30px 40px;"
        "    overflow: scroll; width: 50px; height: 80px'>"
        "    <div id='target' style='writing-mode: vertical-lr; box-shadow: 40px 20px black; width: 100px; height: 90px'></div>"
        "    <div style='width: 100px; height: 100px'></div>"
        "</div>");

    LayoutBlock* container = toLayoutBlock(getLayoutObjectByElementId("container"));
    EXPECT_EQ(LayoutUnit(), container->scrollTop());
    // The initial scroll offset is to the left-most because of flipped blocks writing mode.
    // 150 = total_layout_overflow(100 + 100) - width(50)
    EXPECT_EQ(LayoutUnit(150), container->scrollLeft());
    container->setScrollTop(LayoutUnit(7));
    container->setScrollLeft(LayoutUnit(142)); // Scroll to the right by 8 pixels.
    document().view()->updateAllLifecyclePhases();

    LayoutBlock* target = toLayoutBlock(getLayoutObjectByElementId("target"));
    LayoutRect targetOverflowRect = target->localOverflowRectForPaintInvalidation();
    // 140 = width(100) + box_shadow_offset_x(40)
    // 110 = height(90) + box_shadow_offset_y(20)
    EXPECT_EQ(LayoutRect(0, 0, 140, 110), targetOverflowRect);

    LayoutRect rect = targetOverflowRect;
    EXPECT_TRUE(target->mapToVisualRectInAncestorSpace(target, rect));
    // This rect is in physical coordinates of target.
    EXPECT_EQ(LayoutRect(0, 0, 140, 110), rect);

    rect = targetOverflowRect;
    EXPECT_TRUE(target->mapToVisualRectInAncestorSpace(container, rect));
    // -2 = target_physical_x(100) + container_border_left(40) - scroll_left(142)
    // 3 = target_y(0) + container_border_top(10) - scroll_top(7)
    // Rect is not clipped by container's overflow clip.
    EXPECT_EQ(LayoutRect(-2, 3, 140, 110), rect);
}

TEST_F(VisualRectMappingTest, DifferentPaintInvalidaitionContainerForAbsolutePosition)
{
    enableCompositing();
    document().frame()->settings()->setPreferCompositingToLCDTextEnabled(true);

    setBodyInnerHTML(
        "<div id='stacking-context' style='opacity: 0.9; background: blue; will-change: transform'>"
        "    <div id='scroller' style='overflow: scroll; width: 80px; height: 80px'>"
        "        <div id='absolute' style='position: absolute; top: 111px; left: 222px; width: 50px; height: 50px; background: green'></div>"
        "        <div id='normal-flow' style='width: 2000px; height: 2000px; background: yellow'></div>"
        "    </div>"
        "</div>");

    LayoutBlock* scroller = toLayoutBlock(getLayoutObjectByElementId("scroller"));
    scroller->setScrollTop(LayoutUnit(77));
    scroller->setScrollLeft(LayoutUnit(88));
    document().view()->updateAllLifecyclePhases();

    LayoutBlock* normalFlow = toLayoutBlock(getLayoutObjectByElementId("normal-flow"));
    EXPECT_EQ(scroller, &normalFlow->containerForPaintInvalidation());

    LayoutRect normalFlowOverflowRect = normalFlow->localOverflowRectForPaintInvalidation();
    EXPECT_EQ(LayoutRect(0, 0, 2000, 2000), normalFlowOverflowRect);
    LayoutRect rect = normalFlowOverflowRect;
    EXPECT_TRUE(normalFlow->mapToVisualRectInAncestorSpace(scroller, rect));
    EXPECT_EQ(LayoutRect(-88, -77, 2000, 2000), rect);
    checkPaintInvalidationStateRectMapping(rect, normalFlowOverflowRect, *normalFlow, layoutView(), *scroller);

    LayoutBlock* stackingContext = toLayoutBlock(getLayoutObjectByElementId("stacking-context"));
    LayoutBlock* absolute = toLayoutBlock(getLayoutObjectByElementId("absolute"));
    EXPECT_EQ(stackingContext, &absolute->containerForPaintInvalidation());
    EXPECT_EQ(stackingContext, absolute->container());

    LayoutRect absoluteOverflowRect = absolute->localOverflowRectForPaintInvalidation();
    EXPECT_EQ(LayoutRect(0, 0, 50, 50), absoluteOverflowRect);
    rect = absoluteOverflowRect;
    EXPECT_TRUE(absolute->mapToVisualRectInAncestorSpace(stackingContext, rect));
    EXPECT_EQ(LayoutRect(222, 111, 50, 50), rect);
    checkPaintInvalidationStateRectMapping(rect, absoluteOverflowRect, *absolute, layoutView(), *stackingContext);
}

TEST_F(VisualRectMappingTest, ContainerOfAbsoluteAbovePaintInvalidationContainer)
{
    enableCompositing();
    document().frame()->settings()->setPreferCompositingToLCDTextEnabled(true);

    setBodyInnerHTML(
        "<div id='container' style='position: absolute; top: 88px; left: 99px'>"
        "    <div style='height: 222px'></div>"
        // This div makes stacking-context composited.
        "    <div style='position: absolute; width: 1px; height: 1px; background:yellow; will-change: transform'></div>"
        // This stacking context is paintInvalidationContainer of the absolute child, but not a container of it.
        "    <div id='stacking-context' style='opacity: 0.9'>"
        "        <div id='absolute' style='position: absolute; top: 50px; left: 50px; width: 50px; height: 50px; background: green'></div>"
        "    </div>"
        "</div>");

    LayoutBlock* stackingContext = toLayoutBlock(getLayoutObjectByElementId("stacking-context"));
    LayoutBlock* absolute = toLayoutBlock(getLayoutObjectByElementId("absolute"));
    LayoutBlock* container = toLayoutBlock(getLayoutObjectByElementId("container"));
    EXPECT_EQ(stackingContext, &absolute->containerForPaintInvalidation());
    EXPECT_EQ(container, absolute->container());

    LayoutRect absoluteOverflowRect = absolute->localOverflowRectForPaintInvalidation();
    EXPECT_EQ(LayoutRect(0, 0, 50, 50), absoluteOverflowRect);
    LayoutRect rect = absoluteOverflowRect;
    EXPECT_TRUE(absolute->mapToVisualRectInAncestorSpace(stackingContext, rect));
    // -172 = top(50) - y_offset_of_stacking_context(222)
    EXPECT_EQ(LayoutRect(50, -172, 50, 50), rect);
    checkPaintInvalidationStateRectMapping(rect, absoluteOverflowRect, *absolute, layoutView(), *stackingContext);
}

} // namespace blink
