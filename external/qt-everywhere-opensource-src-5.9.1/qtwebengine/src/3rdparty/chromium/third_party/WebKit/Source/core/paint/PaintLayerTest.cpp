// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintLayer.h"

#include "core/layout/LayoutBoxModelObject.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutView.h"

namespace blink {

using PaintLayerTest = RenderingTest;

TEST_F(PaintLayerTest, PaintingExtentReflection) {
  setBodyInnerHTML(
      "<div id='target' style='background-color: blue; position: absolute;"
      "    width: 110px; height: 120px; top: 40px; left: 60px;"
      "    -webkit-box-reflect: below 3px'>"
      "</div>");

  PaintLayer* layer =
      toLayoutBoxModelObject(getLayoutObjectByElementId("target"))->layer();
  EXPECT_EQ(
      LayoutRect(60, 40, 110, 243),
      layer->paintingExtent(document().layoutView()->layer(), LayoutSize(), 0));
}

TEST_F(PaintLayerTest, PaintingExtentReflectionWithTransform) {
  setBodyInnerHTML(
      "<div id='target' style='background-color: blue; position: absolute;"
      "    width: 110px; height: 120px; top: 40px; left: 60px;"
      "    -webkit-box-reflect: below 3px; transform: translateX(30px)'>"
      "</div>");

  PaintLayer* layer =
      toLayoutBoxModelObject(getLayoutObjectByElementId("target"))->layer();
  EXPECT_EQ(
      LayoutRect(90, 40, 110, 243),
      layer->paintingExtent(document().layoutView()->layer(), LayoutSize(), 0));
}

TEST_F(PaintLayerTest, CompositedScrollingNoNeedsRepaint) {
  enableCompositing();
  setBodyInnerHTML(
      "<div id='scroll' style='width: 100px; height: 100px; overflow: scroll;"
      "    will-change: transform'>"
      "  <div id='content' style='position: relative; background: blue;"
      "      width: 2000px; height: 2000px'></div>"
      "</div>");

  PaintLayer* scrollLayer =
      toLayoutBoxModelObject(getLayoutObjectByElementId("scroll"))->layer();
  EXPECT_EQ(PaintsIntoOwnBacking, scrollLayer->compositingState());

  PaintLayer* contentLayer =
      toLayoutBoxModelObject(getLayoutObjectByElementId("content"))->layer();
  EXPECT_EQ(NotComposited, contentLayer->compositingState());
  EXPECT_EQ(LayoutPoint(), contentLayer->location());

  scrollLayer->getScrollableArea()->setScrollOffset(ScrollOffset(1000, 1000),
                                                    ProgrammaticScroll);
  document().view()->updateAllLifecyclePhasesExceptPaint();
  EXPECT_EQ(LayoutPoint(-1000, -1000), contentLayer->location());
  EXPECT_FALSE(contentLayer->needsRepaint());
  EXPECT_FALSE(scrollLayer->needsRepaint());
  document().view()->updateAllLifecyclePhases();
}

TEST_F(PaintLayerTest, NonCompositedScrollingNeedsRepaint) {
  setBodyInnerHTML(
      "<div id='scroll' style='width: 100px; height: 100px; overflow: scroll'>"
      "  <div id='content' style='position: relative; background: blue;"
      "      width: 2000px; height: 2000px'></div>"
      "</div>");

  PaintLayer* scrollLayer =
      toLayoutBoxModelObject(getLayoutObjectByElementId("scroll"))->layer();
  EXPECT_EQ(NotComposited, scrollLayer->compositingState());

  PaintLayer* contentLayer =
      toLayoutBoxModelObject(getLayoutObjectByElementId("content"))->layer();
  EXPECT_EQ(NotComposited, scrollLayer->compositingState());
  EXPECT_EQ(LayoutPoint(), contentLayer->location());

  scrollLayer->getScrollableArea()->setScrollOffset(ScrollOffset(1000, 1000),
                                                    ProgrammaticScroll);
  document().view()->updateAllLifecyclePhasesExceptPaint();
  EXPECT_EQ(LayoutPoint(-1000, -1000), contentLayer->location());
  EXPECT_TRUE(contentLayer->needsRepaint());
  EXPECT_TRUE(scrollLayer->needsRepaint());
  document().view()->updateAllLifecyclePhases();
}

}  // namespace blink
