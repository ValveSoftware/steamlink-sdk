// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintControllerPaintTest.h"

#include "core/editing/FrameCaret.h"
#include "core/editing/FrameSelection.h"
#include "core/layout/LayoutText.h"
#include "core/layout/line/InlineTextBox.h"
#include "core/page/FocusController.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/ObjectPaintProperties.h"
#include "core/paint/PaintLayerPainter.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"

namespace blink {

INSTANTIATE_TEST_CASE_P(All,
                        PaintControllerPaintTestForSlimmingPaintV1AndV2,
                        ::testing::Bool());

TEST_P(PaintControllerPaintTestForSlimmingPaintV1AndV2,
       FullDocumentPaintingWithCaret) {
  setBodyInnerHTML(
      "<div id='div' contentEditable='true' style='outline:none'>XYZ</div>");
  document().page()->focusController().setActive(true);
  document().page()->focusController().setFocused(true);
  Element& div = *toElement(document().body()->firstChild());
  InlineTextBox& textInlineBox =
      *toLayoutText(div.firstChild()->layoutObject())->firstTextBox();

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_DISPLAY_LIST(
        rootPaintController().getDisplayItemList(), 6,
        TestDisplayItem(layoutView(),
                        DisplayItem::kClipFrameToVisibleContentRect),
        TestDisplayItem(*layoutView().layer(), DisplayItem::kSubsequence),
        TestDisplayItem(layoutView(), documentBackgroundType),
        TestDisplayItem(textInlineBox, foregroundType),
        TestDisplayItem(*layoutView().layer(), DisplayItem::kEndSubsequence),
        TestDisplayItem(layoutView(),
                        DisplayItem::clipTypeToEndClipType(
                            DisplayItem::kClipFrameToVisibleContentRect)));
  } else {
    EXPECT_DISPLAY_LIST(rootPaintController().getDisplayItemList(), 2,
                        TestDisplayItem(layoutView(), documentBackgroundType),
                        TestDisplayItem(textInlineBox, foregroundType));
  }

  div.focus();
  document().view()->updateAllLifecyclePhases();

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_DISPLAY_LIST(
        rootPaintController().getDisplayItemList(), 7,
        TestDisplayItem(layoutView(),
                        DisplayItem::kClipFrameToVisibleContentRect),
        TestDisplayItem(*layoutView().layer(), DisplayItem::kSubsequence),
        TestDisplayItem(layoutView(), documentBackgroundType),
        TestDisplayItem(textInlineBox, foregroundType),
        TestDisplayItem(*document().frame()->selection().m_frameCaret,
                        DisplayItem::kCaret),  // New!
        TestDisplayItem(*layoutView().layer(), DisplayItem::kEndSubsequence),
        TestDisplayItem(layoutView(),
                        DisplayItem::clipTypeToEndClipType(
                            DisplayItem::kClipFrameToVisibleContentRect)));
  } else {
    EXPECT_DISPLAY_LIST(
        rootPaintController().getDisplayItemList(), 3,
        TestDisplayItem(layoutView(), documentBackgroundType),
        TestDisplayItem(textInlineBox, foregroundType),
        TestDisplayItem(*document().frame()->selection().m_frameCaret,
                        DisplayItem::kCaret));  // New!
  }
}

TEST_P(PaintControllerPaintTestForSlimmingPaintV1AndV2, InlineRelayout) {
  setBodyInnerHTML(
      "<div id='div' style='width:100px; height: 200px'>AAAAAAAAAA "
      "BBBBBBBBBB</div>");
  Element& div = *toElement(document().body()->firstChild());
  LayoutBlock& divBlock =
      *toLayoutBlock(document().body()->firstChild()->layoutObject());
  LayoutText& text = *toLayoutText(divBlock.firstChild());
  InlineTextBox& firstTextBox = *text.firstTextBox();

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_DISPLAY_LIST(
        rootPaintController().getDisplayItemList(), 6,
        TestDisplayItem(layoutView(),
                        DisplayItem::kClipFrameToVisibleContentRect),
        TestDisplayItem(*layoutView().layer(), DisplayItem::kSubsequence),
        TestDisplayItem(layoutView(), documentBackgroundType),
        TestDisplayItem(firstTextBox, foregroundType),
        TestDisplayItem(*layoutView().layer(), DisplayItem::kEndSubsequence),
        TestDisplayItem(layoutView(),
                        DisplayItem::clipTypeToEndClipType(
                            DisplayItem::kClipFrameToVisibleContentRect)));
  } else {
    EXPECT_DISPLAY_LIST(rootPaintController().getDisplayItemList(), 2,
                        TestDisplayItem(layoutView(), documentBackgroundType),
                        TestDisplayItem(firstTextBox, foregroundType));
  }

  div.setAttribute(HTMLNames::styleAttr, "width: 10px; height: 200px");
  document().view()->updateAllLifecyclePhases();

  LayoutText& newText = *toLayoutText(divBlock.firstChild());
  InlineTextBox& newFirstTextBox = *newText.firstTextBox();
  InlineTextBox& secondTextBox = *newText.firstTextBox()->nextTextBox();

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_DISPLAY_LIST(
        rootPaintController().getDisplayItemList(), 7,
        TestDisplayItem(layoutView(),
                        DisplayItem::kClipFrameToVisibleContentRect),
        TestDisplayItem(*layoutView().layer(), DisplayItem::kSubsequence),
        TestDisplayItem(layoutView(), documentBackgroundType),
        TestDisplayItem(newFirstTextBox, foregroundType),
        TestDisplayItem(secondTextBox, foregroundType),
        TestDisplayItem(*layoutView().layer(), DisplayItem::kEndSubsequence),
        TestDisplayItem(layoutView(),
                        DisplayItem::clipTypeToEndClipType(
                            DisplayItem::kClipFrameToVisibleContentRect)));
  } else {
    EXPECT_DISPLAY_LIST(rootPaintController().getDisplayItemList(), 3,
                        TestDisplayItem(layoutView(), documentBackgroundType),
                        TestDisplayItem(newFirstTextBox, foregroundType),
                        TestDisplayItem(secondTextBox, foregroundType));
  }
}

TEST_F(PaintControllerPaintTestForSlimmingPaintV2, ChunkIdClientCacheFlag) {
  setBodyInnerHTML(
      "<div id='div' style='width: 200px; height: 200px; opacity: 0.5'>"
      "  <div style='width: 100px; height: 100px; background-color: "
      "blue'></div>"
      "  <div style='width: 100px; height: 100px; background-color: "
      "blue'></div>"
      "</div>");
  PaintLayer& htmlLayer =
      *toLayoutBoxModelObject(document().documentElement()->layoutObject())
           ->layer();
  LayoutBlock& div = *toLayoutBlock(getLayoutObjectByElementId("div"));
  LayoutObject& subDiv = *div.firstChild();
  LayoutObject& subDiv2 = *subDiv.nextSibling();
  EXPECT_DISPLAY_LIST(
      rootPaintController().getDisplayItemList(), 11,
      TestDisplayItem(layoutView(),
                      DisplayItem::kClipFrameToVisibleContentRect),
      TestDisplayItem(*layoutView().layer(), DisplayItem::kSubsequence),
      TestDisplayItem(layoutView(), documentBackgroundType),
      TestDisplayItem(htmlLayer, DisplayItem::kSubsequence),
      TestDisplayItem(div, DisplayItem::kBeginCompositing),
      TestDisplayItem(subDiv, backgroundType),
      TestDisplayItem(subDiv2, backgroundType),
      TestDisplayItem(div, DisplayItem::kEndCompositing),
      TestDisplayItem(htmlLayer, DisplayItem::kEndSubsequence),
      TestDisplayItem(*layoutView().layer(), DisplayItem::kEndSubsequence),
      TestDisplayItem(layoutView(),
                      DisplayItem::clipTypeToEndClipType(
                          DisplayItem::kClipFrameToVisibleContentRect)));

  const PaintChunk& backgroundChunk = rootPaintController().paintChunks()[0];
  EXPECT_TRUE(backgroundChunk.properties.scroll->isRoot());

  const EffectPaintPropertyNode* effectNode = div.paintProperties()->effect();
  EXPECT_EQ(0.5f, effectNode->opacity());
  const PaintChunk& chunk = rootPaintController().paintChunks()[1];
  EXPECT_EQ(*div.layer(), chunk.id->client);
  EXPECT_EQ(effectNode, chunk.properties.effect.get());

  EXPECT_FALSE(div.layer()->isJustCreated());
  // Client used by only paint chunks and non-cachaeable display items but not
  // by any cacheable display items won't be marked as validly cached.
  EXPECT_FALSE(rootPaintController().clientCacheIsValid(*div.layer()));
  EXPECT_FALSE(rootPaintController().clientCacheIsValid(div));
  EXPECT_TRUE(rootPaintController().clientCacheIsValid(subDiv));
}

TEST_F(PaintControllerPaintTestForSlimmingPaintV2, CompositingFold) {
  setBodyInnerHTML(
      "<div id='div' style='width: 200px; height: 200px; opacity: 0.5'>"
      "  <div style='width: 100px; height: 100px; background-color: "
      "blue'></div>"
      "</div>");
  PaintLayer& htmlLayer =
      *toLayoutBoxModelObject(document().documentElement()->layoutObject())
           ->layer();
  LayoutBlock& div = *toLayoutBlock(getLayoutObjectByElementId("div"));
  LayoutObject& subDiv = *div.firstChild();

  EXPECT_DISPLAY_LIST(
      rootPaintController().getDisplayItemList(), 8,
      TestDisplayItem(layoutView(),
                      DisplayItem::kClipFrameToVisibleContentRect),
      TestDisplayItem(*layoutView().layer(), DisplayItem::kSubsequence),
      TestDisplayItem(layoutView(), documentBackgroundType),
      TestDisplayItem(htmlLayer, DisplayItem::kSubsequence),
      // The begin and end compositing display items have been folded into this
      // one.
      TestDisplayItem(subDiv, backgroundType),
      TestDisplayItem(htmlLayer, DisplayItem::kEndSubsequence),
      TestDisplayItem(*layoutView().layer(), DisplayItem::kEndSubsequence),
      TestDisplayItem(layoutView(),
                      DisplayItem::clipTypeToEndClipType(
                          DisplayItem::kClipFrameToVisibleContentRect)));
}

}  // namespace blink
