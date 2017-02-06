// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintControllerPaintTest.h"
#include "core/paint/PaintLayerPainter.h"

namespace blink {

using TableCellPainterTest = PaintControllerPaintTest;

TEST_F(TableCellPainterTest, Background)
{
    setBodyInnerHTML(
        "<style>"
        "  td { width: 200px; height: 200px; border: none; }"
        "  tr { background-color: blue; }"
        "  table { border: none; border-spacing: 0; border-collapse: collapse; }"
        "</style>"
        "<table>"
        "  <tr><td id='cell1'></td></tr>"
        "  <tr><td id='cell2'></td></tr>"
        "</table>");

    LayoutView& layoutView = *document().layoutView();
    LayoutObject& cell1 = *getLayoutObjectByElementId("cell1");
    LayoutObject& cell2 = *getLayoutObjectByElementId("cell2");

    rootPaintController().invalidateAll();
    document().view()->updateAllLifecyclePhasesExceptPaint();
    IntRect interestRect(0, 0, 200, 200);
    paint(&interestRect);

    EXPECT_DISPLAY_LIST(rootPaintController().getDisplayItemList(), 2,
        TestDisplayItem(layoutView, DisplayItem::DocumentBackground),
        TestDisplayItem(cell1, DisplayItem::TableCellBackgroundFromRow));

    document().view()->updateAllLifecyclePhasesExceptPaint();
    interestRect = IntRect(0, 300, 200, 1000);
    paint(&interestRect);

    EXPECT_DISPLAY_LIST(rootPaintController().getDisplayItemList(), 2,
        TestDisplayItem(layoutView, DisplayItem::DocumentBackground),
        TestDisplayItem(cell2, DisplayItem::TableCellBackgroundFromRow));
}

TEST_F(TableCellPainterTest, BackgroundWithCellSpacing)
{
    setBodyInnerHTML(
        "<style>"
        "  body { margin: 0; }"
        "  td { width: 200px; height: 200px; border: 0; background-color: green; }"
        "  tr { background-color: blue; }"
        "  table { border: none; border-spacing: 100px; border-collapse: separate; }"
        "</style>"
        "<table>"
        "  <tr><td id='cell1'></td></tr>"
        "  <tr><td id='cell2'></td></tr>"
        "</table>");

    LayoutView& layoutView = *document().layoutView();
    LayoutObject& cell1 = *getLayoutObjectByElementId("cell1");
    LayoutObject& cell2 = *getLayoutObjectByElementId("cell2");

    rootPaintController().invalidateAll();
    document().view()->updateAllLifecyclePhasesExceptPaint();
    // Intersects cell1 and the spacing between cell1 and cell2.
    IntRect interestRect(0, 200, 200, 200);
    paint(&interestRect);

    EXPECT_DISPLAY_LIST(rootPaintController().getDisplayItemList(), 3,
        TestDisplayItem(layoutView, DisplayItem::DocumentBackground),
        TestDisplayItem(cell1, DisplayItem::TableCellBackgroundFromRow),
        TestDisplayItem(cell1, DisplayItem::BoxDecorationBackground));

    document().view()->updateAllLifecyclePhasesExceptPaint();
    // Intersects the spacing only.
    interestRect = IntRect(0, 300, 100, 100);
    paint(&interestRect);

    EXPECT_DISPLAY_LIST(rootPaintController().getDisplayItemList(), 1,
        TestDisplayItem(layoutView, DisplayItem::DocumentBackground));

    document().view()->updateAllLifecyclePhasesExceptPaint();
    // Intersects cell2 only.
    interestRect = IntRect(0, 400, 200, 200);
    paint(&interestRect);

    EXPECT_DISPLAY_LIST(rootPaintController().getDisplayItemList(), 3,
        TestDisplayItem(layoutView, DisplayItem::DocumentBackground),
        TestDisplayItem(cell2, DisplayItem::TableCellBackgroundFromRow),
        TestDisplayItem(cell2, DisplayItem::BoxDecorationBackground));
}

TEST_F(TableCellPainterTest, BackgroundInSelfPaintingRow)
{
    setBodyInnerHTML(
        "<style>"
        "  body { margin: 0 }"
        "  td { width: 200px; height: 200px; border: 0; background-color: green; }"
        "  tr { background-color: blue; opacity: 0.5; }"
        "  table { border: none; border-spacing: 100px; border-collapse: separate; }"
        "</style>"
        "<table>"
        "  <tr id='row'><td id='cell1'><td id='cell2'></td></tr>"
        "</table>");

    LayoutView& layoutView = *document().layoutView();
    LayoutObject& cell1 = *getLayoutObjectByElementId("cell1");
    LayoutObject& cell2 = *getLayoutObjectByElementId("cell2");
    LayoutObject& row = *getLayoutObjectByElementId("row");
    PaintLayer& htmlLayer = *toLayoutBoxModelObject(document().documentElement()->layoutObject())->layer();

    rootPaintController().invalidateAll();
    document().view()->updateAllLifecyclePhasesExceptPaint();
    // Intersects cell1 and the spacing between cell1 and cell2.
    IntRect interestRect(200, 0, 200, 200);
    paint(&interestRect);

    EXPECT_DISPLAY_LIST(rootPaintController().getDisplayItemList(), 7,
        TestDisplayItem(layoutView, DisplayItem::DocumentBackground),
        TestDisplayItem(htmlLayer, DisplayItem::Subsequence),
        TestDisplayItem(row, DisplayItem::BeginCompositing),
        TestDisplayItem(cell1, DisplayItem::TableCellBackgroundFromRow),
        TestDisplayItem(cell1, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(row, DisplayItem::EndCompositing),
        TestDisplayItem(htmlLayer, DisplayItem::EndSubsequence));

    document().view()->updateAllLifecyclePhasesExceptPaint();
    // Intersects the spacing only.
    interestRect = IntRect(300, 0, 100, 100);
    paint(&interestRect);

    EXPECT_DISPLAY_LIST(rootPaintController().getDisplayItemList(), 3,
        TestDisplayItem(layoutView, DisplayItem::DocumentBackground),
        TestDisplayItem(htmlLayer, DisplayItem::Subsequence),
        TestDisplayItem(htmlLayer, DisplayItem::EndSubsequence));

    document().view()->updateAllLifecyclePhasesExceptPaint();
    // Intersects cell2 only.
    interestRect = IntRect(450, 0, 200, 200);
    paint(&interestRect);

    EXPECT_DISPLAY_LIST(rootPaintController().getDisplayItemList(), 7,
        TestDisplayItem(layoutView, DisplayItem::DocumentBackground),
        TestDisplayItem(htmlLayer, DisplayItem::Subsequence),
        TestDisplayItem(row, DisplayItem::BeginCompositing),
        TestDisplayItem(cell2, DisplayItem::TableCellBackgroundFromRow),
        TestDisplayItem(cell2, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(row, DisplayItem::EndCompositing),
        TestDisplayItem(htmlLayer, DisplayItem::EndSubsequence));
}

TEST_F(TableCellPainterTest, CollapsedBorderAndOverflow)
{
    setBodyInnerHTML(
        "<style>"
        "  body { margin: 0 }"
        "  td { width: 100px; height: 100px; border: 100px solid blue; outline: 100px solid yellow; background: green; }"
        "  table { margin: 100px; border-collapse: collapse; }"
        "</style>"
        "<table>"
        "  <tr><td id='cell'></td></tr>"
        "</table>");

    LayoutView& layoutView = *document().layoutView();
    LayoutObject& cell = *getLayoutObjectByElementId("cell");

    rootPaintController().invalidateAll();
    document().view()->updateAllLifecyclePhasesExceptPaint();
    // Intersects the overflowing part of cell but not border box.
    IntRect interestRect(0, 0, 100, 100);
    paint(&interestRect);

    // We should paint all display items of cell.
    EXPECT_DISPLAY_LIST(rootPaintController().getDisplayItemList(), 4,
        TestDisplayItem(layoutView, DisplayItem::DocumentBackground),
        TestDisplayItem(cell, DisplayItem::BoxDecorationBackground),
        TestDisplayItem(cell, DisplayItem::TableCollapsedBorderLast),
        TestDisplayItem(cell, DisplayItem::paintPhaseToDrawingType(PaintPhaseSelfOutlineOnly)));
}

} // namespace blink
