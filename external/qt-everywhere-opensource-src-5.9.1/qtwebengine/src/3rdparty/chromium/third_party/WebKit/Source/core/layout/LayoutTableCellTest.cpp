/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/layout/LayoutTableCell.h"

#include "core/layout/LayoutTestHelper.h"

namespace blink {

namespace {

class LayoutTableCellDeathTest : public RenderingTest {
 protected:
  virtual void SetUp() {
    RenderingTest::SetUp();
    m_cell = LayoutTableCell::createAnonymous(&document());
  }

  virtual void TearDown() {
    m_cell->destroy();
    RenderingTest::TearDown();
  }

  LayoutTableCell* m_cell;
};

TEST_F(LayoutTableCellDeathTest, CanSetColumn) {
  static const unsigned columnIndex = 10;
  m_cell->setAbsoluteColumnIndex(columnIndex);
  EXPECT_EQ(columnIndex, m_cell->absoluteColumnIndex());
}

TEST_F(LayoutTableCellDeathTest, CanSetColumnToMaxColumnIndex) {
  m_cell->setAbsoluteColumnIndex(maxColumnIndex);
  EXPECT_EQ(maxColumnIndex, m_cell->absoluteColumnIndex());
}

// FIXME: Re-enable these tests once ASSERT_DEATH is supported for Android.
// See: https://bugs.webkit.org/show_bug.cgi?id=74089
// TODO(dgrogan): These tests started flaking on Mac try bots around 2016-07-28.
// https://crbug.com/632816
#if !OS(ANDROID) && !OS(MACOSX)

TEST_F(LayoutTableCellDeathTest, CrashIfColumnOverflowOnSetting) {
  ASSERT_DEATH(m_cell->setAbsoluteColumnIndex(maxColumnIndex + 1), "");
}

TEST_F(LayoutTableCellDeathTest, CrashIfSettingUnsetColumnIndex) {
  ASSERT_DEATH(m_cell->setAbsoluteColumnIndex(unsetColumnIndex), "");
}

#endif

using LayoutTableCellTest = RenderingTest;

TEST_F(LayoutTableCellTest, ResetColspanIfTooBig) {
  setBodyInnerHTML("<table><td colspan='14000'></td></table>");

  LayoutTableCell* cell = toLayoutTableCell(document()
                                                .body()
                                                ->firstChild()
                                                ->firstChild()
                                                ->firstChild()
                                                ->firstChild()
                                                ->layoutObject());
  ASSERT_EQ(cell->colSpan(), 8190U);
}

TEST_F(LayoutTableCellTest, DoNotResetColspanJustBelowBoundary) {
  setBodyInnerHTML("<table><td colspan='8190'></td></table>");

  LayoutTableCell* cell = toLayoutTableCell(document()
                                                .body()
                                                ->firstChild()
                                                ->firstChild()
                                                ->firstChild()
                                                ->firstChild()
                                                ->layoutObject());
  ASSERT_EQ(cell->colSpan(), 8190U);
}

TEST_F(LayoutTableCellTest, ResetRowspanIfTooBig) {
  setBodyInnerHTML("<table><td rowspan='70000'></td></table>");

  LayoutTableCell* cell = toLayoutTableCell(document()
                                                .body()
                                                ->firstChild()
                                                ->firstChild()
                                                ->firstChild()
                                                ->firstChild()
                                                ->layoutObject());
  ASSERT_EQ(cell->rowSpan(), 65534U);
}

TEST_F(LayoutTableCellTest, DoNotResetRowspanJustBelowBoundary) {
  setBodyInnerHTML("<table><td rowspan='65534'></td></table>");

  LayoutTableCell* cell = toLayoutTableCell(document()
                                                .body()
                                                ->firstChild()
                                                ->firstChild()
                                                ->firstChild()
                                                ->firstChild()
                                                ->layoutObject());
  ASSERT_EQ(cell->rowSpan(), 65534U);
}

TEST_F(LayoutTableCellTest,
       BackgroundIsKnownToBeOpaqueWithLayerAndCollapsedBorder) {
  setBodyInnerHTML(
      "<table style='border-collapse: collapse'>"
      "<td style='will-change: transform; background-color: blue'>Cell></td>"
      "</table>");

  LayoutTableCell* cell = toLayoutTableCell(document()
                                                .body()
                                                ->firstChild()
                                                ->firstChild()
                                                ->firstChild()
                                                ->firstChild()
                                                ->layoutObject());
  EXPECT_FALSE(cell->backgroundIsKnownToBeOpaqueInRect(LayoutRect(0, 0, 1, 1)));
}

TEST_F(LayoutTableCellTest, RepaintContentInTableCell) {
  const char* bodyContent =
      "<table id='table' style='position: absolute; left: 1px;'>"
      "<tr>"
      "<td id='cell'>"
      "<div style='display: inline-block; height: 20px; width: 20px'>"
      "</td>"
      "</tr>"
      "</table>";
  setBodyInnerHTML(bodyContent);

  // Create an overflow recalc.
  Element* cell = document().getElementById(AtomicString("cell"));
  cell->setAttribute(HTMLNames::styleAttr, "outline: 1px solid black;");
  // Trigger a layout on the table that doesn't require cell layout.
  Element* table = document().getElementById(AtomicString("table"));
  table->setAttribute(HTMLNames::styleAttr, "position: absolute; left: 2px;");
  document().view()->updateAllLifecyclePhases();

  // Check that overflow was calculated on the cell.
  LayoutBlock* inputBlock = toLayoutBlock(getLayoutObjectByElementId("cell"));
  LayoutRect rect = inputBlock->localVisualRect();
  EXPECT_EQ(LayoutRect(-1, -1, 24, 24), rect);
}
}  // namespace

}  // namespace blink
