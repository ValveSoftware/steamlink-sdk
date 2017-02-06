// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutTableSection.h"

#include "core/layout/LayoutTestHelper.h"

namespace blink {

namespace {

using LayoutTableSectionTest = RenderingTest;

TEST_F(LayoutTableSectionTest, BackgroundIsKnownToBeOpaqueWithLayerAndCollapsedBorder)
{
    setBodyInnerHTML("<table style='border-collapse: collapse'>"
        "  <thead style='will-change: transform; background-color: blue'>"
        "    <tr><td>Cell</td></tr>"
        "  </thead>"
        "</table>");

    LayoutTableSection* section = toLayoutTableSection(document().body()->firstChild()->firstChild()->layoutObject());
    EXPECT_FALSE(section->backgroundIsKnownToBeOpaqueInRect(LayoutRect(0, 0, 1, 1)));
}

TEST_F(LayoutTableSectionTest, BackgroundIsKnownToBeOpaqueWithBorderSpacing)
{
    setBodyInnerHTML("<table style='border-spacing: 10px'>"
        "  <thead style='background-color: blue'>"
        "    <tr><td>Cell</td></tr>"
        "  </thead>"
        "</table>");

    LayoutTableSection* section = toLayoutTableSection(document().body()->firstChild()->firstChild()->layoutObject());
    EXPECT_FALSE(section->backgroundIsKnownToBeOpaqueInRect(LayoutRect(0, 0, 1, 1)));
}

TEST_F(LayoutTableSectionTest, BackgroundIsKnownToBeOpaqueWithEmptyCell)
{
    setBodyInnerHTML("<table style='border-spacing: 10px'>"
        "  <thead style='background-color: blue'>"
        "    <tr><td>Cell</td></tr>"
        "    <tr><td>Cell</td><td>Cell</td></tr>"
        "  </thead>"
        "</table>");

    LayoutTableSection* section = toLayoutTableSection(document().body()->firstChild()->firstChild()->layoutObject());
    EXPECT_FALSE(section->backgroundIsKnownToBeOpaqueInRect(LayoutRect(0, 0, 1, 1)));
}

TEST_F(LayoutTableSectionTest, EmptySectionDirtiedRows)
{
    setBodyInnerHTML(
        "<table style='border: 100px solid red'>"
        "  <thead id='section'></thead>"
        "</table>");

    LayoutTableSection* section = toLayoutTableSection(getLayoutObjectByElementId("section"));
    CellSpan cellSpan = section->dirtiedRows(LayoutRect(50, 50, 100, 100));
    EXPECT_EQ(0u, cellSpan.start());
    EXPECT_EQ(0u, cellSpan.end());
}

TEST_F(LayoutTableSectionTest, EmptySectionDirtiedEffectiveColumns)
{
    setBodyInnerHTML(
        "<table style='border: 100px solid red'>"
        "  <thead id='section'></thead>"
        "</table>");

    LayoutTableSection* section = toLayoutTableSection(getLayoutObjectByElementId("section"));
    CellSpan cellSpan = section->dirtiedEffectiveColumns(LayoutRect(50, 50, 100, 100));
    // The table has at least 1 column even if there is no cell.
    EXPECT_EQ(1u, section->table()->numEffectiveColumns());
    EXPECT_EQ(1u, cellSpan.start());
    EXPECT_EQ(1u, cellSpan.end());
}

} // anonymous namespace

} // namespace blink
