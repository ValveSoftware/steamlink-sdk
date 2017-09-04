// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ScrollAnchor.h"

#include "core/dom/ClientRect.h"
#include "core/frame/VisualViewport.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/page/PrintContext.h"
#include "core/paint/PaintLayerScrollableArea.h"
#include "platform/testing/HistogramTester.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"

namespace blink {

using Corner = ScrollAnchor::Corner;

typedef bool TestParamRootLayerScrolling;
class ScrollAnchorTest
    : public testing::WithParamInterface<TestParamRootLayerScrolling>,
      private ScopedRootLayerScrollingForTest,
      public RenderingTest {
 public:
  ScrollAnchorTest() : ScopedRootLayerScrollingForTest(GetParam()) {
    RuntimeEnabledFeatures::setScrollAnchoringEnabled(true);
  }
  ~ScrollAnchorTest() {
    RuntimeEnabledFeatures::setScrollAnchoringEnabled(false);
  }

 protected:
  void update() {
    // TODO(skobes): Use SimTest instead of RenderingTest and move into
    // Source/web?
    document().view()->updateAllLifecyclePhases();
  }

  ScrollableArea* layoutViewport() {
    return document().view()->layoutViewportScrollableArea();
  }

  VisualViewport& visualViewport() {
    return document().view()->page()->frameHost().visualViewport();
  }

  ScrollableArea* scrollerForElement(Element* element) {
    return toLayoutBox(element->layoutObject())->getScrollableArea();
  }

  ScrollAnchor& scrollAnchor(ScrollableArea* scroller) {
    ASSERT(scroller->isFrameView() || scroller->isPaintLayerScrollableArea());
    return *(scroller->scrollAnchor());
  }

  void setHeight(Element* element, int height) {
    element->setAttribute(HTMLNames::styleAttr,
                          AtomicString(String::format("height: %dpx", height)));
    update();
  }

  void scrollLayoutViewport(ScrollOffset delta) {
    Element* scrollingElement = document().scrollingElement();
    if (delta.width())
      scrollingElement->setScrollLeft(scrollingElement->scrollLeft() +
                                      delta.width());
    if (delta.height())
      scrollingElement->setScrollTop(scrollingElement->scrollTop() +
                                     delta.height());
  }
};

INSTANTIATE_TEST_CASE_P(All, ScrollAnchorTest, ::testing::Bool());

// TODO(ymalik): Currently, this should be the first test in the file to avoid
// failure when running with other tests. Dig into this more and fix.
TEST_P(ScrollAnchorTest, UMAMetricUpdated) {
  HistogramTester histogramTester;
  setBodyInnerHTML(
      "<style> body { height: 1000px } div { height: 100px } </style>"
      "<div id='block1'>abc</div>"
      "<div id='block2'>def</div>");

  ScrollableArea* viewport = layoutViewport();

  // Scroll position not adjusted, metric not updated.
  scrollLayoutViewport(ScrollOffset(0, 150));
  histogramTester.expectTotalCount("Layout.ScrollAnchor.AdjustedScrollOffset",
                                   0);

  // Height changed, verify metric updated once.
  setHeight(document().getElementById("block1"), 200);
  histogramTester.expectUniqueSample("Layout.ScrollAnchor.AdjustedScrollOffset",
                                     1, 1);

  EXPECT_EQ(250, viewport->scrollOffsetInt().height());
  EXPECT_EQ(document().getElementById("block2")->layoutObject(),
            scrollAnchor(viewport).anchorObject());
}

TEST_P(ScrollAnchorTest, Basic) {
  setBodyInnerHTML(
      "<style> body { height: 1000px } div { height: 100px } </style>"
      "<div id='block1'>abc</div>"
      "<div id='block2'>def</div>");

  ScrollableArea* viewport = layoutViewport();

  // No anchor at origin (0,0).
  EXPECT_EQ(nullptr, scrollAnchor(viewport).anchorObject());

  scrollLayoutViewport(ScrollOffset(0, 150));
  setHeight(document().getElementById("block1"), 200);

  EXPECT_EQ(250, viewport->scrollOffsetInt().height());
  EXPECT_EQ(document().getElementById("block2")->layoutObject(),
            scrollAnchor(viewport).anchorObject());

  // ScrollableArea::userScroll should clear the anchor.
  viewport->userScroll(ScrollByPrecisePixel, FloatSize(0, 100));
  EXPECT_EQ(nullptr, scrollAnchor(viewport).anchorObject());
}

TEST_P(ScrollAnchorTest, VisualViewportAnchors) {
  setBodyInnerHTML(
      "<style>"
      "    * { font-size: 1.2em; font-family: sans-serif; }"
      "    div { height: 100px; width: 20px; background-color: pink; }"
      "</style>"
      "<div id='div'></div>"
      "<div id='text'><b>This is a scroll anchoring test</div>");

  ScrollableArea* lViewport = layoutViewport();
  VisualViewport& vViewport = visualViewport();

  vViewport.setScale(2.0);

  // No anchor at origin (0,0).
  EXPECT_EQ(nullptr, scrollAnchor(lViewport).anchorObject());

  // Scroll the visual viewport to bring #text to the top.
  int top = document().getElementById("text")->getBoundingClientRect()->top();
  vViewport.setLocation(FloatPoint(0, top));

  setHeight(document().getElementById("div"), 10);
  EXPECT_EQ(document().getElementById("text")->layoutObject(),
            scrollAnchor(lViewport).anchorObject());
  EXPECT_EQ(top - 90, vViewport.scrollOffsetInt().height());

  setHeight(document().getElementById("div"), 100);
  EXPECT_EQ(document().getElementById("text")->layoutObject(),
            scrollAnchor(lViewport).anchorObject());
  EXPECT_EQ(top, vViewport.scrollOffsetInt().height());

  // Scrolling the visual viewport should clear the anchor.
  vViewport.setLocation(FloatPoint(0, 0));
  EXPECT_EQ(nullptr, scrollAnchor(lViewport).anchorObject());
}

// Test that we ignore the clipped content when computing visibility otherwise
// we may end up with an anchor that we think is in the viewport but is not.
TEST_P(ScrollAnchorTest, ClippedScrollersSkipped) {
  setBodyInnerHTML(
      "<style>"
      "    body { height: 2000px; }"
      "    #scroller { overflow: scroll; width: 500px; height: 300px; }"
      "    .anchor {"
      "         position:relative; height: 100px; width: 150px;"
      "         background-color: #afa; border: 1px solid gray;"
      "    }"
      "    #forceScrolling { height: 500px; background-color: #fcc; }"
      "</style>"
      "<div id='scroller'>"
      "    <div id='innerChanger'></div>"
      "    <div id='innerAnchor' class='anchor'></div>"
      "    <div id='forceScrolling'></div>"
      "</div>"
      "<div id='outerChanger'></div>"
      "<div id='outerAnchor' class='anchor'></div>");

  ScrollableArea* scroller =
      scrollerForElement(document().getElementById("scroller"));
  ScrollableArea* viewport = layoutViewport();

  document().getElementById("scroller")->setScrollTop(100);
  scrollLayoutViewport(ScrollOffset(0, 350));

  setHeight(document().getElementById("innerChanger"), 200);
  setHeight(document().getElementById("outerChanger"), 150);

  EXPECT_EQ(300, scroller->scrollOffsetInt().height());
  EXPECT_EQ(document().getElementById("innerAnchor")->layoutObject(),
            scrollAnchor(scroller).anchorObject());
  EXPECT_EQ(500, viewport->scrollOffsetInt().height());
  EXPECT_EQ(document().getElementById("outerAnchor")->layoutObject(),
            scrollAnchor(viewport).anchorObject());
}

// Test that scroll anchoring causes no visible jump when a layout change
// (such as removal of a DOM element) changes the scroll bounds.
TEST_P(ScrollAnchorTest, AnchoringWhenContentRemoved) {
  setBodyInnerHTML(
      "<style>"
      "    #changer { height: 1500px; }"
      "    #anchor {"
      "        width: 150px; height: 1000px; background-color: pink;"
      "    }"
      "</style>"
      "<div id='changer'></div>"
      "<div id='anchor'></div>");

  ScrollableArea* viewport = layoutViewport();
  scrollLayoutViewport(ScrollOffset(0, 1600));

  setHeight(document().getElementById("changer"), 0);

  EXPECT_EQ(100, viewport->scrollOffsetInt().height());
  EXPECT_EQ(document().getElementById("anchor")->layoutObject(),
            scrollAnchor(viewport).anchorObject());
}

// Test that scroll anchoring causes no visible jump when a layout change
// (such as removal of a DOM element) changes the scroll bounds of a scrolling
// div.
TEST_P(ScrollAnchorTest, AnchoringWhenContentRemovedFromScrollingDiv) {
  setBodyInnerHTML(
      "<style>"
      "    #scroller { height: 500px; width: 200px; overflow: scroll; }"
      "    #changer { height: 1500px; }"
      "    #anchor {"
      "        width: 150px; height: 1000px; overflow: scroll;"
      "    }"
      "</style>"
      "<div id='scroller'>"
      "    <div id='changer'></div>"
      "    <div id='anchor'></div>"
      "</div>");

  ScrollableArea* scroller =
      scrollerForElement(document().getElementById("scroller"));

  document().getElementById("scroller")->setScrollTop(1600);

  setHeight(document().getElementById("changer"), 0);

  EXPECT_EQ(100, scroller->scrollOffsetInt().height());
  EXPECT_EQ(document().getElementById("anchor")->layoutObject(),
            scrollAnchor(scroller).anchorObject());
}

// Test that a non-anchoring scroll on scroller clears scroll anchors for all
// parent scrollers.
TEST_P(ScrollAnchorTest, ClearScrollAnchorsOnAncestors) {
  setBodyInnerHTML(
      "<style>"
      "    body { height: 1000px } div { height: 200px }"
      "    #scroller { height: 100px; width: 200px; overflow: scroll; }"
      "</style>"
      "<div id='changer'>abc</div>"
      "<div id='anchor'>def</div>"
      "<div id='scroller'><div></div></div>");

  ScrollableArea* viewport = layoutViewport();

  scrollLayoutViewport(ScrollOffset(0, 250));
  setHeight(document().getElementById("changer"), 300);

  EXPECT_EQ(350, viewport->scrollOffsetInt().height());
  EXPECT_EQ(document().getElementById("anchor")->layoutObject(),
            scrollAnchor(viewport).anchorObject());

  // Scrolling the nested scroller should clear the anchor on the main frame.
  ScrollableArea* scroller =
      scrollerForElement(document().getElementById("scroller"));
  scroller->scrollBy(ScrollOffset(0, 100), UserScroll);
  EXPECT_EQ(nullptr, scrollAnchor(viewport).anchorObject());
}

TEST_P(ScrollAnchorTest, AncestorClearingWithSiblingReference) {
  setBodyInnerHTML(
      "<style>"
      ".scroller {"
      "  overflow: scroll;"
      "  width: 400px;"
      "  height: 400px;"
      "}"
      ".space {"
      "  width: 100px;"
      "  height: 600px;"
      "}"
      "</style>"
      "<div id='s1' class='scroller'>"
      "  <div id='anchor' class='space'></div>"
      "</div>"
      "<div id='s2' class='scroller'>"
      "  <div class='space'></div>"
      "</div>");
  Element* s1 = document().getElementById("s1");
  Element* s2 = document().getElementById("s2");
  Element* anchor = document().getElementById("anchor");

  // Set non-zero scroll offsets for #s1 and #document
  s1->setScrollTop(100);
  scrollLayoutViewport(ScrollOffset(0, 100));

  // Invalidate layout.
  setHeight(anchor, 500);

  // This forces layout, during which both #s1 and #document will anchor to
  // #anchor. Then the scroll clears #s2 and #document.  Since #anchor is still
  // referenced by #s1, its IsScrollAnchorObject bit must remain set.
  s2->setScrollTop(100);

  // This should clear #s1.  If #anchor had its bit cleared already we would
  // crash in update().
  s1->removeChild(anchor);
  update();
}

TEST_P(ScrollAnchorTest, FractionalOffsetsAreRoundedBeforeComparing) {
  setBodyInnerHTML(
      "<style> body { height: 1000px } </style>"
      "<div id='block1' style='height: 50.4px'>abc</div>"
      "<div id='block2' style='height: 100px'>def</div>");

  ScrollableArea* viewport = layoutViewport();
  scrollLayoutViewport(ScrollOffset(0, 100));

  document().getElementById("block1")->setAttribute(HTMLNames::styleAttr,
                                                    "height: 50.6px");
  update();

  EXPECT_EQ(101, viewport->scrollOffsetInt().height());
}

TEST_P(ScrollAnchorTest, AnchorWithLayerInScrollingDiv) {
  setBodyInnerHTML(
      "<style>"
      "    #scroller { overflow: scroll; width: 500px; height: 400px; }"
      "    div { height: 100px }"
      "    #block2 { overflow: hidden }"
      "    #space { height: 1000px; }"
      "</style>"
      "<div id='scroller'><div id='space'>"
      "<div id='block1'>abc</div>"
      "<div id='block2'>def</div>"
      "</div></div>");

  ScrollableArea* scroller =
      scrollerForElement(document().getElementById("scroller"));
  Element* block1 = document().getElementById("block1");
  Element* block2 = document().getElementById("block2");

  scroller->scrollBy(ScrollOffset(0, 150), UserScroll);

  // In this layout pass we will anchor to #block2 which has its own PaintLayer.
  setHeight(block1, 200);
  EXPECT_EQ(250, scroller->scrollOffsetInt().height());
  EXPECT_EQ(block2->layoutObject(), scrollAnchor(scroller).anchorObject());

  // Test that the anchor object can be destroyed without affecting the scroll
  // position.
  block2->remove();
  update();
  EXPECT_EQ(250, scroller->scrollOffsetInt().height());
}

// Verify that a nested scroller with a div that has its own PaintLayer can be
// removed without causing a crash. This test passes if it doesn't crash.
TEST_P(ScrollAnchorTest, RemoveScrollerWithLayerInScrollingDiv) {
  setBodyInnerHTML(
      "<style>"
      "    body { height: 2000px }"
      "    #scroller { overflow: scroll; width: 500px; height: 400px}"
      "    #block1 { height: 100px; width: 100px; overflow: hidden}"
      "    #anchor { height: 1000px; }"
      "</style>"
      "<div id='changer1'></div>"
      "<div id='scroller'>"
      "  <div id='changer2'></div>"
      "  <div id='block1'></div>"
      "  <div id='anchor'></div>"
      "</div>");

  ScrollableArea* viewport = layoutViewport();
  ScrollableArea* scroller =
      scrollerForElement(document().getElementById("scroller"));
  Element* changer1 = document().getElementById("changer1");
  Element* changer2 = document().getElementById("changer2");
  Element* anchor = document().getElementById("anchor");

  scroller->scrollBy(ScrollOffset(0, 150), UserScroll);
  scrollLayoutViewport(ScrollOffset(0, 50));

  // In this layout pass both the inner and outer scroller will anchor to
  // #anchor.
  setHeight(changer1, 100);
  setHeight(changer2, 100);
  EXPECT_EQ(250, scroller->scrollOffsetInt().height());
  EXPECT_EQ(anchor->layoutObject(), scrollAnchor(scroller).anchorObject());
  EXPECT_EQ(anchor->layoutObject(), scrollAnchor(viewport).anchorObject());

  // Test that the inner scroller can be destroyed without crashing.
  document().getElementById("scroller")->remove();
  update();
}

TEST_P(ScrollAnchorTest, ExcludeAnonymousCandidates) {
  setBodyInnerHTML(
      "<style>"
      "    body { height: 3500px }"
      "    #div {"
      "        position: relative; background-color: pink;"
      "        top: 5px; left: 5px; width: 100px; height: 3500px;"
      "    }"
      "    #inline { padding-left: 10px }"
      "</style>"
      "<div id='div'>"
      "    <a id='inline'>text</a>"
      "    <p id='block'>Some text</p>"
      "</div>"
      "<div id=a>after</div>");

  ScrollableArea* viewport = layoutViewport();
  Element* inlineElem = document().getElementById("inline");
  EXPECT_TRUE(inlineElem->layoutObject()->parent()->isAnonymous());

  // Scroll #div into view, making anonymous block a viable candidate.
  document().getElementById("div")->scrollIntoView();

  // Trigger layout and verify that we don't anchor to the anonymous block.
  setHeight(document().getElementById("a"), 100);
  update();
  EXPECT_EQ(inlineElem->layoutObject()->slowFirstChild(),
            scrollAnchor(viewport).anchorObject());
}

TEST_P(ScrollAnchorTest, FullyContainedInlineBlock) {
  // Exercises every WalkStatus value:
  // html, body -> Constrain
  // #outer -> Continue
  // #ib1, br -> Skip
  // #ib2 -> Return
  setBodyInnerHTML(
      "<style>"
      "    body { height: 1000px }"
      "    #outer { line-height: 100px }"
      "    #ib1, #ib2 { display: inline-block }"
      "</style>"
      "<span id=outer>"
      "    <span id=ib1>abc</span>"
      "    <br><br>"
      "    <span id=ib2>def</span>"
      "</span>");

  scrollLayoutViewport(ScrollOffset(0, 150));

  Element* ib1 = document().getElementById("ib1");
  ib1->setAttribute(HTMLNames::styleAttr, "line-height: 150px");
  update();
  EXPECT_EQ(document().getElementById("ib2")->layoutObject(),
            scrollAnchor(layoutViewport()).anchorObject());
}

TEST_P(ScrollAnchorTest, TextBounds) {
  setBodyInnerHTML(
      "<style>"
      "    body {"
      "        position: absolute;"
      "        font-size: 100px;"
      "        width: 200px;"
      "        height: 1000px;"
      "        line-height: 100px;"
      "    }"
      "</style>"
      "abc <b id=b>def</b> ghi"
      "<div id=a>after</div>");

  scrollLayoutViewport(ScrollOffset(0, 150));

  setHeight(document().getElementById("a"), 100);
  EXPECT_EQ(document().getElementById("b")->layoutObject()->slowFirstChild(),
            scrollAnchor(layoutViewport()).anchorObject());
}

TEST_P(ScrollAnchorTest, ExcludeFixedPosition) {
  setBodyInnerHTML(
      "<style>"
      "    body { height: 1000px; padding: 20px; }"
      "    div { position: relative; top: 100px; }"
      "    #f { position: fixed }"
      "</style>"
      "<div id=f>fixed</div>"
      "<div id=c>content</div>"
      "<div id=a>after</div>");

  scrollLayoutViewport(ScrollOffset(0, 50));

  setHeight(document().getElementById("a"), 100);
  EXPECT_EQ(document().getElementById("c")->layoutObject(),
            scrollAnchor(layoutViewport()).anchorObject());
}

// This test verifies that position:absolute elements that stick to the viewport
// are not selected as anchors.
TEST_P(ScrollAnchorTest, ExcludeAbsolutePositionThatSticksToViewport) {
  setBodyInnerHTML(
      "<style>"
      "    body { margin: 0; }"
      "    #scroller { overflow: scroll; width: 500px; height: 400px; }"
      "    #space { height: 1000px; }"
      "    #abs {"
      "        position: absolute; background-color: red;"
      "        width: 100px; height: 100px;"
      "    }"
      "    #rel {"
      "        position: relative; background-color: green;"
      "        left: 50px; top: 100px; width: 100px; height: 75px;"
      "    }"
      "</style>"
      "<div id='scroller'><div id='space'>"
      "    <div id='abs'></div>"
      "    <div id='rel'></div>"
      "    <div id=a>after</div>"
      "</div></div>");

  Element* scrollerElement = document().getElementById("scroller");
  ScrollableArea* scroller = scrollerForElement(scrollerElement);
  Element* absPos = document().getElementById("abs");
  Element* relPos = document().getElementById("rel");

  scroller->scrollBy(ScrollOffset(0, 25), UserScroll);
  setHeight(document().getElementById("a"), 100);

  // When the scroller is position:static, the anchor cannot be
  // position:absolute.
  EXPECT_EQ(relPos->layoutObject(), scrollAnchor(scroller).anchorObject());

  scrollerElement->setAttribute(HTMLNames::styleAttr, "position: relative");
  update();
  scroller->scrollBy(ScrollOffset(0, 25), UserScroll);
  setHeight(document().getElementById("a"), 125);

  // When the scroller is position:relative, the anchor may be
  // position:absolute.
  EXPECT_EQ(absPos->layoutObject(), scrollAnchor(scroller).anchorObject());
}

// Test that we descend into zero-height containers that have overflowing
// content.
TEST_P(ScrollAnchorTest, DescendsIntoContainerWithOverflow) {
  setBodyInnerHTML(
      "<style>"
      "    body { height: 1000; }"
      "    #outer { width: 300px; }"
      "    #zeroheight { height: 0px; }"
      "    #changer { height: 100px; background-color: red; }"
      "    #bottom { margin-top: 600px; }"
      "</style>"
      "<div id='outer'>"
      "    <div id='zeroheight'>"
      "      <div id='changer'></div>"
      "      <div id='bottom'>bottom</div>"
      "    </div>"
      "</div>");

  ScrollableArea* viewport = layoutViewport();

  scrollLayoutViewport(ScrollOffset(0, 200));
  setHeight(document().getElementById("changer"), 200);

  EXPECT_EQ(300, viewport->scrollOffsetInt().height());
  EXPECT_EQ(document().getElementById("bottom")->layoutObject(),
            scrollAnchor(viewport).anchorObject());
}

// Test that we descend into zero-height containers that have floating content.
TEST_P(ScrollAnchorTest, DescendsIntoContainerWithFloat) {
  setBodyInnerHTML(
      "<style>"
      "    body { height: 1000; }"
      "    #outer { width: 300px; }"
      "    #outer:after { content: ' '; clear:both; display: table; }"
      "    #float {"
      "         float: left; background-color: #ccc;"
      "         height: 500px; width: 100%;"
      "    }"
      "    #inner { height: 21px; background-color:#7f0; }"
      "</style>"
      "<div id='outer'>"
      "    <div id='zeroheight'>"
      "      <div id='float'>"
      "         <div id='inner'></div>"
      "      </div>"
      "    </div>"
      "</div>"
      "<div id=a>after</div>");

  EXPECT_EQ(0,
            toLayoutBox(document().getElementById("zeroheight")->layoutObject())
                ->size()
                .height());

  ScrollableArea* viewport = layoutViewport();

  scrollLayoutViewport(ScrollOffset(0, 200));
  setHeight(document().getElementById("a"), 100);

  EXPECT_EQ(200, viewport->scrollOffsetInt().height());
  EXPECT_EQ(document().getElementById("float")->layoutObject(),
            scrollAnchor(viewport).anchorObject());
}

// This test verifies that scroll anchoring is disabled when any element within
// the main scroller changes its in-flow state.
TEST_P(ScrollAnchorTest, ChangeInFlowStateDisablesAnchoringForMainScroller) {
  setBodyInnerHTML(
      "<style>"
      "    body { height: 1000px; }"
      "    #header { background-color: #F5B335; height: 50px; width: 100%; }"
      "    #content { background-color: #D3D3D3; height: 200px; }"
      "</style>"
      "<div id='header'></div>"
      "<div id='content'></div>");

  ScrollableArea* viewport = layoutViewport();
  scrollLayoutViewport(ScrollOffset(0, 200));

  document().getElementById("header")->setAttribute(HTMLNames::styleAttr,
                                                    "position: fixed;");
  update();

  EXPECT_EQ(200, viewport->scrollOffsetInt().height());
}

// This test verifies that scroll anchoring is disabled when any element within
// a scrolling div changes its in-flow state.
TEST_P(ScrollAnchorTest, ChangeInFlowStateDisablesAnchoringForScrollingDiv) {
  setBodyInnerHTML(
      "<style>"
      "    #container { position: relative; width: 500px; }"
      "    #scroller { height: 200px; overflow: scroll; }"
      "    #changer { background-color: #F5B335; height: 50px; width: 100%; }"
      "    #anchor { background-color: #D3D3D3; height: 300px; }"
      "</style>"
      "<div id='container'>"
      "    <div id='scroller'>"
      "      <div id='changer'></div>"
      "      <div id='anchor'></div>"
      "    </div>"
      "</div>");

  ScrollableArea* scroller =
      scrollerForElement(document().getElementById("scroller"));
  document().getElementById("scroller")->setScrollTop(100);

  document().getElementById("changer")->setAttribute(HTMLNames::styleAttr,
                                                     "position: absolute;");
  update();

  EXPECT_EQ(100, scroller->scrollOffsetInt().height());
}

TEST_P(ScrollAnchorTest, FlexboxDelayedClampingAlsoDelaysAdjustment) {
  setBodyInnerHTML(
      "<style>"
      "    html { overflow: hidden; }"
      "    body {"
      "        position: absolute; display: flex;"
      "        top: 0; bottom: 0; margin: 0;"
      "    }"
      "    #scroller { overflow: auto; }"
      "    #spacer { width: 600px; height: 1200px; }"
      "    #before { height: 50px; }"
      "    #anchor {"
      "        width: 100px; height: 100px;"
      "        background-color: #8f8;"
      "    }"
      "</style>"
      "<div id='scroller'>"
      "    <div id='spacer'>"
      "        <div id='before'></div>"
      "        <div id='anchor'></div>"
      "    </div>"
      "</div>");

  Element* scroller = document().getElementById("scroller");
  scroller->setScrollTop(100);

  setHeight(document().getElementById("before"), 100);
  EXPECT_EQ(150, scrollerForElement(scroller)->scrollOffsetInt().height());
}

TEST_P(ScrollAnchorTest, FlexboxDelayedAdjustmentRespectsSANACLAP) {
  setBodyInnerHTML(
      "<style>"
      "    html { overflow: hidden; }"
      "    body {"
      "        position: absolute; display: flex;"
      "        top: 0; bottom: 0; margin: 0;"
      "    }"
      "    #scroller { overflow: auto; }"
      "    #spacer { width: 600px; height: 1200px; }"
      "    #anchor {"
      "        position: relative; top: 50px;"
      "        width: 100px; height: 100px;"
      "        background-color: #8f8;"
      "    }"
      "</style>"
      "<div id='scroller'>"
      "    <div id='spacer'>"
      "        <div id='anchor'></div>"
      "    </div>"
      "</div>");

  Element* scroller = document().getElementById("scroller");
  scroller->setScrollTop(100);

  document().getElementById("spacer")->setAttribute(HTMLNames::styleAttr,
                                                    "margin-top: 50px");
  update();
  EXPECT_EQ(100, scrollerForElement(scroller)->scrollOffsetInt().height());
}

// Test then an element and its children are not selected as the anchor when
// it has the overflow-anchor property set to none.
TEST_P(ScrollAnchorTest, OptOutElement) {
  setBodyInnerHTML(
      "<style>"
      "     body { height: 1000px }"
      "     .div {"
      "          height: 100px; width: 100px;"
      "          border: 1px solid gray; background-color: #afa;"
      "     }"
      "     #innerDiv {"
      "          height: 50px; width: 50px;"
      "          border: 1px solid gray; background-color: pink;"
      "     }"
      "</style>"
      "<div id='changer'></div>"
      "<div class='div' id='firstDiv'><div id='innerDiv'></div></div>"
      "<div class='div' id='secondDiv'></div>");

  ScrollableArea* viewport = layoutViewport();
  scrollLayoutViewport(ScrollOffset(0, 50));

  // No opt-out.
  setHeight(document().getElementById("changer"), 100);
  EXPECT_EQ(150, viewport->scrollOffsetInt().height());
  EXPECT_EQ(document().getElementById("innerDiv")->layoutObject(),
            scrollAnchor(viewport).anchorObject());

  // Clear anchor and opt-out element.
  scrollLayoutViewport(ScrollOffset(0, 10));
  document()
      .getElementById("firstDiv")
      ->setAttribute(HTMLNames::styleAttr,
                     AtomicString("overflow-anchor: none"));
  update();

  // Opted out element and it's children skipped.
  setHeight(document().getElementById("changer"), 200);
  EXPECT_EQ(260, viewport->scrollOffsetInt().height());
  EXPECT_EQ(document().getElementById("secondDiv")->layoutObject(),
            scrollAnchor(viewport).anchorObject());
}

TEST_P(ScrollAnchorTest,
       SuppressAnchorNodeAncestorChangingLayoutAffectingProperty) {
  setBodyInnerHTML(
      "<style> body { height: 1000px } div { height: 100px } </style>"
      "<div id='block1'>abc</div>");

  ScrollableArea* viewport = layoutViewport();

  scrollLayoutViewport(ScrollOffset(0, 50));
  document().body()->setAttribute(HTMLNames::styleAttr, "padding-top: 20px");
  update();

  EXPECT_EQ(50, viewport->scrollOffsetInt().height());
  EXPECT_EQ(nullptr, scrollAnchor(viewport).anchorObject());
}

TEST_P(ScrollAnchorTest, AnchorNodeAncestorChangingNonLayoutAffectingProperty) {
  setBodyInnerHTML(
      "<style> body { height: 1000px } div { height: 100px } </style>"
      "<div id='block1'>abc</div>"
      "<div id='block2'>def</div>");

  ScrollableArea* viewport = layoutViewport();
  scrollLayoutViewport(ScrollOffset(0, 150));

  document().body()->setAttribute(HTMLNames::styleAttr, "color: red");
  setHeight(document().getElementById("block1"), 200);

  EXPECT_EQ(250, viewport->scrollOffsetInt().height());
  EXPECT_EQ(document().getElementById("block2")->layoutObject(),
            scrollAnchor(viewport).anchorObject());
}

TEST_P(ScrollAnchorTest, TransformIsLayoutAffecting) {
  setBodyInnerHTML(
      "<style>"
      "    body { height: 1000px }"
      "    #block1 { height: 100px }"
      "</style>"
      "<div id='block1'>abc</div>"
      "<div id=a>after</div>");

  ScrollableArea* viewport = layoutViewport();

  scrollLayoutViewport(ScrollOffset(0, 50));
  document().getElementById("block1")->setAttribute(
      HTMLNames::styleAttr, "transform: matrix(1, 0, 0, 1, 25, 25);");
  update();

  document().getElementById("block1")->setAttribute(
      HTMLNames::styleAttr, "transform: matrix(1, 0, 0, 1, 50, 50);");
  setHeight(document().getElementById("a"), 100);
  update();

  EXPECT_EQ(50, viewport->scrollOffsetInt().height());
  EXPECT_EQ(nullptr, scrollAnchor(viewport).anchorObject());
}

TEST_P(ScrollAnchorTest, OptOutBody) {
  setBodyInnerHTML(
      "<style>"
      "    body { height: 2000px; overflow-anchor: none; }"
      "    #scroller { overflow: scroll; width: 500px; height: 300px; }"
      "    .anchor {"
      "        position:relative; height: 100px; width: 150px;"
      "        background-color: #afa; border: 1px solid gray;"
      "    }"
      "    #forceScrolling { height: 500px; background-color: #fcc; }"
      "</style>"
      "<div id='outerChanger'></div>"
      "<div id='outerAnchor' class='anchor'></div>"
      "<div id='scroller'>"
      "    <div id='innerChanger'></div>"
      "    <div id='innerAnchor' class='anchor'></div>"
      "    <div id='forceScrolling'></div>"
      "</div>");

  ScrollableArea* scroller =
      scrollerForElement(document().getElementById("scroller"));
  ScrollableArea* viewport = layoutViewport();

  document().getElementById("scroller")->setScrollTop(100);
  scrollLayoutViewport(ScrollOffset(0, 100));

  setHeight(document().getElementById("innerChanger"), 200);
  setHeight(document().getElementById("outerChanger"), 150);

  // Scroll anchoring should apply within #scroller.
  EXPECT_EQ(300, scroller->scrollOffsetInt().height());
  EXPECT_EQ(document().getElementById("innerAnchor")->layoutObject(),
            scrollAnchor(scroller).anchorObject());
  // Scroll anchoring should not apply within main frame.
  EXPECT_EQ(100, viewport->scrollOffsetInt().height());
  EXPECT_EQ(nullptr, scrollAnchor(viewport).anchorObject());
}

TEST_P(ScrollAnchorTest, OptOutScrollingDiv) {
  setBodyInnerHTML(
      "<style>"
      "    body { height: 2000px; }"
      "    #scroller {"
      "        overflow: scroll; width: 500px; height: 300px;"
      "        overflow-anchor: none;"
      "    }"
      "    .anchor {"
      "        position:relative; height: 100px; width: 150px;"
      "        background-color: #afa; border: 1px solid gray;"
      "    }"
      "    #forceScrolling { height: 500px; background-color: #fcc; }"
      "</style>"
      "<div id='outerChanger'></div>"
      "<div id='outerAnchor' class='anchor'></div>"
      "<div id='scroller'>"
      "    <div id='innerChanger'></div>"
      "    <div id='innerAnchor' class='anchor'></div>"
      "    <div id='forceScrolling'></div>"
      "</div>");

  ScrollableArea* scroller =
      scrollerForElement(document().getElementById("scroller"));
  ScrollableArea* viewport = layoutViewport();

  document().getElementById("scroller")->setScrollTop(100);
  scrollLayoutViewport(ScrollOffset(0, 100));

  setHeight(document().getElementById("innerChanger"), 200);
  setHeight(document().getElementById("outerChanger"), 150);

  // Scroll anchoring should not apply within #scroller.
  EXPECT_EQ(100, scroller->scrollOffsetInt().height());
  EXPECT_EQ(nullptr, scrollAnchor(scroller).anchorObject());
  // Scroll anchoring should apply within main frame.
  EXPECT_EQ(250, viewport->scrollOffsetInt().height());
  EXPECT_EQ(document().getElementById("outerAnchor")->layoutObject(),
            scrollAnchor(viewport).anchorObject());
}

TEST_P(ScrollAnchorTest, NonDefaultRootScroller) {
  setBodyInnerHTML(
      "<style>"
      "    ::-webkit-scrollbar {"
      "      width: 0px; height: 0px;"
      "    }"
      "    body, html {"
      "      margin: 0px; width: 100%; height: 100%;"
      "    }"
      "    #rootscroller {"
      "      overflow: scroll; width: 100%; height: 100%;"
      "    }"
      "    .spacer {"
      "      height: 600px; width: 100px;"
      "    }"
      "    #target {"
      "      height: 100px; width: 100px; background-color: red;"
      "    }"
      "</style>"
      "<div id='rootscroller'>"
      "    <div id='firstChild' class='spacer'></div>"
      "    <div id='target'></div>"
      "    <div class='spacer'></div>"
      "</div>"
      "<div class='spacer'></div>");

  Element* rootScrollerElement = document().getElementById("rootscroller");

  NonThrowableExceptionState nonThrow;
  document().setRootScroller(rootScrollerElement, nonThrow);
  document().view()->updateAllLifecyclePhases();

  ScrollableArea* scroller = scrollerForElement(rootScrollerElement);

  // By making the #rootScroller DIV the rootScroller, it should become the
  // layout viewport on the RootFrameViewport.
  ASSERT_EQ(scroller,
            &document().view()->getRootFrameViewport()->layoutViewport());

  // The #rootScroller DIV's anchor should have the RootFrameViewport set as
  // the scroller, rather than the FrameView's anchor.

  rootScrollerElement->setScrollTop(600);

  setHeight(document().getElementById("firstChild"), 1000);

  // Scroll anchoring should be applied to #rootScroller.
  EXPECT_EQ(1000, scroller->scrollOffset().height());
  EXPECT_EQ(document().getElementById("target")->layoutObject(),
            scrollAnchor(scroller).anchorObject());
  // Scroll anchoring should not apply within main frame.
  EXPECT_EQ(0, layoutViewport()->scrollOffset().height());
  EXPECT_EQ(nullptr, scrollAnchor(layoutViewport()).anchorObject());
}

// This test verifies that scroll anchoring is disabled when the document is in
// printing mode.
TEST_P(ScrollAnchorTest, AnchoringDisabledForPrinting) {
  setBodyInnerHTML(
      "<style> body { height: 1000px } div { height: 100px } </style>"
      "<div id='block1'>abc</div>"
      "<div id='block2'>def</div>");

  ScrollableArea* viewport = layoutViewport();
  scrollLayoutViewport(ScrollOffset(0, 150));

  // This will trigger printing and layout.
  PrintContext::numberOfPages(document().frame(), FloatSize(500, 500));

  EXPECT_EQ(150, viewport->scrollOffsetInt().height());
  EXPECT_EQ(nullptr, scrollAnchor(viewport).anchorObject());
}

class ScrollAnchorCornerTest : public ScrollAnchorTest {
 protected:
  void checkCorner(Corner corner,
                   ScrollOffset startOffset,
                   ScrollOffset expectedAdjustment) {
    ScrollableArea* viewport = layoutViewport();
    Element* element = document().getElementById("changer");

    viewport->setScrollOffset(startOffset, UserScroll);
    element->setAttribute(HTMLNames::classAttr, "change");
    update();

    ScrollOffset endPos = startOffset;
    endPos += expectedAdjustment;

    EXPECT_EQ(endPos, viewport->scrollOffset());
    EXPECT_EQ(document().getElementById("a")->layoutObject(),
              scrollAnchor(viewport).anchorObject());
    EXPECT_EQ(corner, scrollAnchor(viewport).corner());

    element->removeAttribute(HTMLNames::classAttr);
    update();
  }
};

// Verify that we anchor to the top left corner of an element for LTR.
TEST_P(ScrollAnchorCornerTest, CornersLTR) {
  setBodyInnerHTML(
      "<style>"
      "    body { position: relative; width: 1220px; height: 920px; }"
      "    #a { width: 400px; height: 300px; }"
      "    .change { height: 100px; }"
      "</style>"
      "<div id='changer'></div>"
      "<div id='a'></div>");

  checkCorner(Corner::TopLeft, ScrollOffset(20, 20), ScrollOffset(0, 100));
}

// Verify that we anchor to the top left corner of an anchor element for
// vertical-lr writing mode.
TEST_P(ScrollAnchorCornerTest, CornersVerticalLR) {
  setBodyInnerHTML(
      "<style>"
      "    html { writing-mode: vertical-lr; }"
      "    body { position: relative; width: 1220px; height: 920px; }"
      "    #a { width: 400px; height: 300px; }"
      "    .change { width: 100px; }"
      "</style>"
      "<div id='changer'></div>"
      "<div id='a'></div>");

  checkCorner(Corner::TopLeft, ScrollOffset(20, 20), ScrollOffset(100, 0));
}

// Verify that we anchor to the top right corner of an anchor element for RTL.
TEST_P(ScrollAnchorCornerTest, CornersRTL) {
  setBodyInnerHTML(
      "<style>"
      "    html { direction: rtl; }"
      "    body { position: relative; width: 1220px; height: 920px; }"
      "    #a { width: 400px; height: 300px; }"
      "    .change { height: 100px; }"
      "</style>"
      "<div id='changer'></div>"
      "<div id='a'></div>");

  checkCorner(Corner::TopRight, ScrollOffset(-20, 20), ScrollOffset(0, 100));
}

// Verify that we anchor to the top right corner of an anchor element for
// vertical-lr writing mode.
TEST_P(ScrollAnchorCornerTest, CornersVerticalRL) {
  setBodyInnerHTML(
      "<style>"
      "    html { writing-mode: vertical-rl; }"
      "    body { position: relative; width: 1220px; height: 920px; }"
      "    #a { width: 400px; height: 300px; }"
      "    .change { width: 100px; }"
      "</style>"
      "<div id='changer'></div>"
      "<div id='a'></div>");

  checkCorner(Corner::TopRight, ScrollOffset(-20, 20), ScrollOffset(-100, 0));
}

TEST_P(ScrollAnchorTest, IgnoreNonBlockLayoutAxis) {
  setBodyInnerHTML(
      "<style>"
      "    body {"
      "        margin: 0; line-height: 0;"
      "        width: 1200px; height: 1200px;"
      "    }"
      "    div {"
      "        width: 100px; height: 100px;"
      "        border: 5px solid gray;"
      "        display: inline-block;"
      "        box-sizing: border-box;"
      "    }"
      "</style>"
      "<div id='a'></div><br>"
      "<div id='b'></div><div id='c'></div>");

  ScrollableArea* viewport = layoutViewport();
  scrollLayoutViewport(ScrollOffset(150, 0));

  Element* a = document().getElementById("a");
  Element* b = document().getElementById("b");
  Element* c = document().getElementById("c");

  a->setAttribute(HTMLNames::styleAttr, "height: 150px");
  update();
  EXPECT_EQ(ScrollOffset(150, 0), viewport->scrollOffset());
  EXPECT_EQ(nullptr, scrollAnchor(viewport).anchorObject());

  scrollLayoutViewport(ScrollOffset(0, 50));

  a->setAttribute(HTMLNames::styleAttr, "height: 200px");
  b->setAttribute(HTMLNames::styleAttr, "width: 150px");
  update();
  EXPECT_EQ(ScrollOffset(150, 100), viewport->scrollOffset());
  EXPECT_EQ(c->layoutObject(), scrollAnchor(viewport).anchorObject());

  a->setAttribute(HTMLNames::styleAttr, "height: 100px");
  b->setAttribute(HTMLNames::styleAttr, "width: 100px");
  document().documentElement()->setAttribute(HTMLNames::styleAttr,
                                             "writing-mode: vertical-rl");
  document().scrollingElement()->setScrollLeft(0);
  document().scrollingElement()->setScrollTop(0);
  scrollLayoutViewport(ScrollOffset(0, 150));

  a->setAttribute(HTMLNames::styleAttr, "width: 150px");
  update();
  EXPECT_EQ(ScrollOffset(0, 150), viewport->scrollOffset());
  EXPECT_EQ(nullptr, scrollAnchor(viewport).anchorObject());

  scrollLayoutViewport(ScrollOffset(-50, 0));

  a->setAttribute(HTMLNames::styleAttr, "width: 200px");
  b->setAttribute(HTMLNames::styleAttr, "height: 150px");
  update();
  EXPECT_EQ(ScrollOffset(-100, 150), viewport->scrollOffset());
  EXPECT_EQ(c->layoutObject(), scrollAnchor(viewport).anchorObject());
}
}
