// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutBoxModelObject.h"

#include "core/html/HTMLElement.h"
#include "core/layout/ImageQualityController.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/page/scrolling/StickyPositionScrollingConstraints.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/PaintLayerScrollableArea.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class LayoutBoxModelObjectTest : public RenderingTest {
 protected:
  const FloatRect& getScrollContainerRelativeContainingBlockRect(
      const StickyPositionScrollingConstraints& constraints) const {
    return constraints.scrollContainerRelativeContainingBlockRect();
  }

  const FloatRect& getScrollContainerRelativeStickyBoxRect(
      const StickyPositionScrollingConstraints& constraints) const {
    return constraints.scrollContainerRelativeStickyBoxRect();
  }
};

// Verifies that the sticky constraints are correctly computed.
TEST_F(LayoutBoxModelObjectTest, StickyPositionConstraints) {
  setBodyInnerHTML(
      "<style>#sticky { position: sticky; top: 0; width: 100px; height: 100px; "
      "}"
      "#container { box-sizing: border-box; position: relative; top: 100px; "
      "height: 400px; width: 200px; padding: 10px; border: 5px solid black; }"
      "#scroller { width: 400px; height: 100px; overflow: auto; "
      "position: relative; top: 200px; border: 2px solid black; }"
      ".spacer { height: 1000px; }</style>"
      "<div id='scroller'><div id='container'><div "
      "id='sticky'></div></div><div class='spacer'></div></div>");
  LayoutBoxModelObject* scroller =
      toLayoutBoxModelObject(getLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollableArea = scroller->getScrollableArea();
  scrollableArea->scrollToAbsolutePosition(
      FloatPoint(scrollableArea->scrollOffsetInt().width(), 50));
  ASSERT_EQ(50.0, scrollableArea->scrollPosition().y());
  LayoutBoxModelObject* sticky =
      toLayoutBoxModelObject(getLayoutObjectByElementId("sticky"));
  sticky->updateStickyPositionConstraints();
  ASSERT_EQ(scroller->layer(), sticky->layer()->ancestorOverflowLayer());

  const StickyPositionScrollingConstraints& constraints =
      scrollableArea->stickyConstraintsMap().get(sticky->layer());
  ASSERT_EQ(0.f, constraints.topOffset());

  // The coordinates of the constraint rects should all be with respect to the
  // unscrolled scroller.
  ASSERT_EQ(IntRect(15, 115, 170, 370),
            enclosingIntRect(
                getScrollContainerRelativeContainingBlockRect(constraints)));
  ASSERT_EQ(
      IntRect(15, 115, 100, 100),
      enclosingIntRect(getScrollContainerRelativeStickyBoxRect(constraints)));

  // The sticky constraining rect also doesn't include the border offset.
  ASSERT_EQ(IntRect(0, 50, 400, 100),
            enclosingIntRect(sticky->computeStickyConstrainingRect()));
}

// Verifies that the sticky constraints are correctly computed in right to left.
TEST_F(LayoutBoxModelObjectTest, StickyPositionVerticalRLConstraints) {
  setBodyInnerHTML(
      "<style> html { -webkit-writing-mode: vertical-rl; } "
      "#sticky { position: sticky; top: 0; width: 100px; height: 100px; "
      "}"
      "#container { box-sizing: border-box; position: relative; top: 100px; "
      "height: 400px; width: 200px; padding: 10px; border: 5px solid black; }"
      "#scroller { width: 400px; height: 100px; overflow: auto; "
      "position: relative; top: 200px; border: 2px solid black; }"
      ".spacer { height: 1000px; }</style>"
      "<div id='scroller'><div id='container'><div "
      "id='sticky'></div></div><div class='spacer'></div></div>");
  LayoutBoxModelObject* scroller =
      toLayoutBoxModelObject(getLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollableArea = scroller->getScrollableArea();
  scrollableArea->scrollToAbsolutePosition(
      FloatPoint(scrollableArea->scrollOffsetInt().width(), 50));
  ASSERT_EQ(50.0, scrollableArea->scrollPosition().y());
  LayoutBoxModelObject* sticky =
      toLayoutBoxModelObject(getLayoutObjectByElementId("sticky"));
  sticky->updateStickyPositionConstraints();
  ASSERT_EQ(scroller->layer(), sticky->layer()->ancestorOverflowLayer());

  const StickyPositionScrollingConstraints& constraints =
      scrollableArea->stickyConstraintsMap().get(sticky->layer());
  ASSERT_EQ(0.f, constraints.topOffset());

  // The coordinates of the constraint rects should all be with respect to the
  // unscrolled scroller.
  ASSERT_EQ(IntRect(215, 115, 170, 370),
            enclosingIntRect(
                getScrollContainerRelativeContainingBlockRect(constraints)));
  ASSERT_EQ(
      IntRect(285, 115, 100, 100),
      enclosingIntRect(getScrollContainerRelativeStickyBoxRect(constraints)));

  // The sticky constraining rect also doesn't include the border offset.
  ASSERT_EQ(IntRect(0, 50, 400, 100),
            enclosingIntRect(sticky->computeStickyConstrainingRect()));
}

// Verifies that the sticky constraints are not affected by transforms
TEST_F(LayoutBoxModelObjectTest, StickyPositionTransforms) {
  setBodyInnerHTML(
      "<style>#sticky { position: sticky; top: 0; width: 100px; height: 100px; "
      "transform: scale(2); transform-origin: top left; }"
      "#container { box-sizing: border-box; position: relative; top: 100px; "
      "height: 400px; width: 200px; padding: 10px; border: 5px solid black; "
      "transform: scale(2); transform-origin: top left; }"
      "#scroller { height: 100px; overflow: auto; position: relative; top: "
      "200px; }"
      ".spacer { height: 1000px; }</style>"
      "<div id='scroller'><div id='container'><div "
      "id='sticky'></div></div><div class='spacer'></div></div>");
  LayoutBoxModelObject* scroller =
      toLayoutBoxModelObject(getLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollableArea = scroller->getScrollableArea();
  scrollableArea->scrollToAbsolutePosition(
      FloatPoint(scrollableArea->scrollOffsetInt().width(), 50));
  ASSERT_EQ(50.0, scrollableArea->scrollPosition().y());
  LayoutBoxModelObject* sticky =
      toLayoutBoxModelObject(getLayoutObjectByElementId("sticky"));
  sticky->updateStickyPositionConstraints();
  ASSERT_EQ(scroller->layer(), sticky->layer()->ancestorOverflowLayer());

  const StickyPositionScrollingConstraints& constraints =
      scrollableArea->stickyConstraintsMap().get(sticky->layer());
  ASSERT_EQ(0.f, constraints.topOffset());

  // The coordinates of the constraint rects should all be with respect to the
  // unscrolled scroller.
  ASSERT_EQ(IntRect(15, 115, 170, 370),
            enclosingIntRect(
                getScrollContainerRelativeContainingBlockRect(constraints)));
  ASSERT_EQ(
      IntRect(15, 115, 100, 100),
      enclosingIntRect(getScrollContainerRelativeStickyBoxRect(constraints)));
}

// Verifies that the sticky constraints are correctly computed.
TEST_F(LayoutBoxModelObjectTest, StickyPositionPercentageStyles) {
  setBodyInnerHTML(
      "<style>#sticky { position: sticky; margin-top: 10%; top: 0; width: "
      "100px; height: 100px; }"
      "#container { box-sizing: border-box; position: relative; top: 100px; "
      "height: 400px; width: 250px; padding: 5%; border: 5px solid black; }"
      "#scroller { width: 400px; height: 100px; overflow: auto; position: "
      "relative; top: 200px; }"
      ".spacer { height: 1000px; }</style>"
      "<div id='scroller'><div id='container'><div "
      "id='sticky'></div></div><div class='spacer'></div></div>");
  LayoutBoxModelObject* scroller =
      toLayoutBoxModelObject(getLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollableArea = scroller->getScrollableArea();
  scrollableArea->scrollToAbsolutePosition(
      FloatPoint(scrollableArea->scrollPosition().x(), 50));
  ASSERT_EQ(50.0, scrollableArea->scrollPosition().y());
  LayoutBoxModelObject* sticky =
      toLayoutBoxModelObject(getLayoutObjectByElementId("sticky"));
  sticky->updateStickyPositionConstraints();
  ASSERT_EQ(scroller->layer(), sticky->layer()->ancestorOverflowLayer());

  const StickyPositionScrollingConstraints& constraints =
      scrollableArea->stickyConstraintsMap().get(sticky->layer());
  ASSERT_EQ(0.f, constraints.topOffset());

  ASSERT_EQ(IntRect(25, 145, 200, 330),
            enclosingIntRect(
                getScrollContainerRelativeContainingBlockRect(constraints)));
  ASSERT_EQ(
      IntRect(25, 145, 100, 100),
      enclosingIntRect(getScrollContainerRelativeStickyBoxRect(constraints)));
}

// Verifies that the sticky constraints are correct when the sticky position
// container is also the ancestor scroller.
TEST_F(LayoutBoxModelObjectTest, StickyPositionContainerIsScroller) {
  setBodyInnerHTML(
      "<style>#sticky { position: sticky; top: 0; width: 100px; height: 100px; "
      "}"
      "#scroller { height: 100px; width: 400px; overflow: auto; position: "
      "relative; top: 200px; border: 2px solid black; }"
      ".spacer { height: 1000px; }</style>"
      "<div id='scroller'><div id='sticky'></div><div "
      "class='spacer'></div></div>");
  LayoutBoxModelObject* scroller =
      toLayoutBoxModelObject(getLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollableArea = scroller->getScrollableArea();
  scrollableArea->scrollToAbsolutePosition(
      FloatPoint(scrollableArea->scrollPosition().x(), 50));
  ASSERT_EQ(50.0, scrollableArea->scrollPosition().y());
  LayoutBoxModelObject* sticky =
      toLayoutBoxModelObject(getLayoutObjectByElementId("sticky"));
  sticky->updateStickyPositionConstraints();
  ASSERT_EQ(scroller->layer(), sticky->layer()->ancestorOverflowLayer());

  const StickyPositionScrollingConstraints& constraints =
      scrollableArea->stickyConstraintsMap().get(sticky->layer());
  ASSERT_EQ(IntRect(0, 0, 400, 1100),
            enclosingIntRect(
                getScrollContainerRelativeContainingBlockRect(constraints)));
  ASSERT_EQ(
      IntRect(0, 0, 100, 100),
      enclosingIntRect(getScrollContainerRelativeStickyBoxRect(constraints)));
}

// Verifies that the sticky constraints are correct when the sticky position
// object has an anonymous containing block.
TEST_F(LayoutBoxModelObjectTest, StickyPositionAnonymousContainer) {
  setBodyInnerHTML(
      "<style>#sticky { display: inline-block; position: sticky; top: 0; "
      "width: 100px; height: 100px; }"
      "#container { box-sizing: border-box; position: relative; top: 100px; "
      "height: 400px; width: 200px; padding: 10px; border: 5px solid black; }"
      "#scroller { height: 100px; overflow: auto; position: relative; top: "
      "200px; }"
      ".header { height: 50px; }"
      ".spacer { height: 1000px; }</style>"
      "<div id='scroller'><div id='container'><div class='header'></div><div "
      "id='sticky'></div></div><div class='spacer'></div></div>");
  LayoutBoxModelObject* scroller =
      toLayoutBoxModelObject(getLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollableArea = scroller->getScrollableArea();
  scrollableArea->scrollToAbsolutePosition(
      FloatPoint(scrollableArea->scrollPosition().x(), 50));
  ASSERT_EQ(50.0, scrollableArea->scrollPosition().y());
  LayoutBoxModelObject* sticky =
      toLayoutBoxModelObject(getLayoutObjectByElementId("sticky"));
  sticky->updateStickyPositionConstraints();
  ASSERT_EQ(scroller->layer(), sticky->layer()->ancestorOverflowLayer());

  const StickyPositionScrollingConstraints& constraints =
      scrollableArea->stickyConstraintsMap().get(sticky->layer());
  ASSERT_EQ(IntRect(15, 115, 170, 370),
            enclosingIntRect(
                getScrollContainerRelativeContainingBlockRect(constraints)));
  ASSERT_EQ(
      IntRect(15, 165, 100, 100),
      enclosingIntRect(getScrollContainerRelativeStickyBoxRect(constraints)));
}
}  // namespace blink
