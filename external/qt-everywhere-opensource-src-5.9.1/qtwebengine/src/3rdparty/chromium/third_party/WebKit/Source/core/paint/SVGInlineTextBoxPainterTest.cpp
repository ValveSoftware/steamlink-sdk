// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/SVGInlineTextBoxPainter.h"

#include "core/dom/Document.h"
#include "core/dom/Range.h"
#include "core/editing/DOMSelection.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/layout/LayoutView.h"
#include "core/layout/line/InlineTextBox.h"
#include "core/layout/svg/LayoutSVGInlineText.h"
#include "core/layout/svg/LayoutSVGText.h"
#include "core/paint/PaintControllerPaintTest.h"
#include "core/paint/PaintLayer.h"
#include "platform/geometry/IntRectOutsets.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "platform/graphics/paint/PaintController.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

class SVGInlineTextBoxPainterTest : public PaintControllerPaintTest {
 public:
  const DrawingDisplayItem* getDrawingForSVGTextById(const char* elementName) {
    // Look up the inline text box that serves as the display item client for
    // the painted text.
    LayoutSVGText* targetSVGText = toLayoutSVGText(
        document().getElementById(AtomicString(elementName))->layoutObject());
    LayoutSVGInlineText* targetInlineText =
        targetSVGText->descendantTextNodes()[0];
    const DisplayItemClient* targetClient =
        static_cast<const DisplayItemClient*>(targetInlineText->firstTextBox());

    // Find the appropriate drawing in the display item list.
    const DisplayItemList& displayItemList =
        rootPaintController().getDisplayItemList();
    for (size_t i = 0; i < displayItemList.size(); i++) {
      if (displayItemList[i].client() == *targetClient)
        return static_cast<const DrawingDisplayItem*>(&displayItemList[i]);
    }

    return nullptr;
  }

  void selectAllText() {
    Range* range = document().createRange();
    range->selectNode(document().documentElement());
    LocalDOMWindow* window = document().domWindow();
    DOMSelection* selection = window->getSelection();
    selection->removeAllRanges();
    selection->addRange(range);
  }

 private:
  void SetUp() override {
    PaintControllerPaintTest::SetUp();
    enableCompositing();
  }
};

static void assertTextDrawingEquals(
    const DrawingDisplayItem* drawingDisplayItem,
    const char* str) {
  ASSERT_EQ(
      str,
      static_cast<const InlineTextBox*>(&drawingDisplayItem->client())->text());
}

bool ApproximatelyEqual(int a, int b, int delta) {
  return abs(a - b) <= delta;
}

const static int kPixelDelta = 4;

#define EXPECT_RECT_EQ(expected, actual)                                       \
  do {                                                                         \
    const FloatRect& actualRect = actual;                                      \
    EXPECT_TRUE(ApproximatelyEqual(expected.x(), actualRect.x(), kPixelDelta)) \
        << "actual: " << actualRect.x() << ", expected: " << expected.x();     \
    EXPECT_TRUE(ApproximatelyEqual(expected.y(), actualRect.y(), kPixelDelta)) \
        << "actual: " << actualRect.y() << ", expected: " << expected.y();     \
    EXPECT_TRUE(                                                               \
        ApproximatelyEqual(expected.width(), actualRect.width(), kPixelDelta)) \
        << "actual: " << actualRect.width()                                    \
        << ", expected: " << expected.width();                                 \
    EXPECT_TRUE(ApproximatelyEqual(expected.height(), actualRect.height(),     \
                                   kPixelDelta))                               \
        << "actual: " << actualRect.height()                                   \
        << ", expected: " << expected.height();                                \
  } while (false)

static IntRect cullRectFromDrawing(
    const DrawingDisplayItem& drawingDisplayItem) {
  return IntRect(drawingDisplayItem.picture()->cullRect());
}

TEST_F(SVGInlineTextBoxPainterTest, TextCullRect_DefaultWritingMode) {
  setBodyInnerHTML(
      "<svg width='400px' height='400px' font-family='Arial' font-size='30'>"
      "<text id='target' x='50' y='30'>x</text>"
      "</svg>");
  document().view()->updateAllLifecyclePhases();

  const DrawingDisplayItem* drawingDisplayItem =
      getDrawingForSVGTextById("target");
  assertTextDrawingEquals(drawingDisplayItem, "x");
  EXPECT_RECT_EQ(IntRect(50, 3, 15, 33),
                 cullRectFromDrawing(*drawingDisplayItem));

  selectAllText();
  document().view()->updateAllLifecyclePhases();

  drawingDisplayItem = getDrawingForSVGTextById("target");
  assertTextDrawingEquals(drawingDisplayItem, "x");
  EXPECT_RECT_EQ(IntRect(50, 3, 15, 33),
                 cullRectFromDrawing(*drawingDisplayItem));
}

TEST_F(SVGInlineTextBoxPainterTest, TextCullRect_WritingModeTopToBottom) {
  setBodyInnerHTML(
      "<svg width='400px' height='400px' font-family='Arial' font-size='30'>"
      "<text id='target' x='50' y='30' writing-mode='tb'>x</text>"
      "</svg>");
  document().view()->updateAllLifecyclePhases();

  const DrawingDisplayItem* drawingDisplayItem =
      getDrawingForSVGTextById("target");
  assertTextDrawingEquals(drawingDisplayItem, "x");
  EXPECT_RECT_EQ(IntRect(33, 30, 34, 15),
                 cullRectFromDrawing(*drawingDisplayItem));

  selectAllText();
  document().view()->updateAllLifecyclePhases();

  // The selection rect is one pixel taller due to sub-pixel difference
  // between the text bounds and selection bounds in combination with use of
  // enclosingIntRect() in SVGInlineTextBox::localSelectionRect().
  drawingDisplayItem = getDrawingForSVGTextById("target");
  assertTextDrawingEquals(drawingDisplayItem, "x");
  EXPECT_RECT_EQ(IntRect(33, 30, 34, 16),
                 cullRectFromDrawing(*drawingDisplayItem));
}

TEST_F(SVGInlineTextBoxPainterTest, TextCullRect_TextShadow) {
  setBodyInnerHTML(
      "<svg width='400px' height='400px' font-family='Arial' font-size='30'>"
      "<text id='target' x='50' y='30' style='text-shadow: 200px 200px 5px "
      "red'>x</text>"
      "</svg>");
  document().view()->updateAllLifecyclePhases();

  const DrawingDisplayItem* drawingDisplayItem =
      getDrawingForSVGTextById("target");
  assertTextDrawingEquals(drawingDisplayItem, "x");
  EXPECT_RECT_EQ(IntRect(50, 3, 220, 238),
                 cullRectFromDrawing(*drawingDisplayItem));
}

}  // namespace
}  // namespace blink
