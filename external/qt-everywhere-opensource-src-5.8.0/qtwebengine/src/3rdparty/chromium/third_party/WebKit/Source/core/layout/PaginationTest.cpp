// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/LayoutMultiColumnFlowThread.h"

#include "core/layout/LayoutTestHelper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class PaginationStrutTest : public RenderingTest {
public:
    int strutForBox(const char* blockId)
    {
        LayoutObject* layoutObject = getLayoutObjectByElementId(blockId);
        if (!layoutObject || !layoutObject->isBox())
            return std::numeric_limits<int>::min();
        LayoutBox* box = toLayoutBox(layoutObject);
        return box->paginationStrut().toInt();
    }

    int strutForLine(const char* containerId, int lineIndex)
    {
        LayoutObject* layoutObject = getLayoutObjectByElementId(containerId);
        if (!layoutObject || !layoutObject->isLayoutBlockFlow())
            return std::numeric_limits<int>::min();
        LayoutBlockFlow* block = toLayoutBlockFlow(layoutObject);
        if (block->multiColumnFlowThread())
            block = block->multiColumnFlowThread();
        for (RootInlineBox* line = block->firstRootBox(); line; line = line->nextRootBox(), lineIndex--) {
            if (lineIndex)
                continue;
            return line->paginationStrut().toInt();
        }
        return std::numeric_limits<int>::min();
    }
};

TEST_F(PaginationStrutTest, LineWithStrut)
{
    setBodyInnerHTML(
        "<div id='paged' style='overflow:-webkit-paged-y; height:200px; line-height:150px;'>"
        "    line1<br>"
        "    line2<br>"
        "</div>");
    EXPECT_EQ(0, strutForLine("paged", 0));
    EXPECT_EQ(50, strutForLine("paged", 1));
}

TEST_F(PaginationStrutTest, BlockWithStrut)
{
    setBodyInnerHTML(
        "<div style='overflow:-webkit-paged-y; height:200px; line-height:150px;'>"
        "    <div id='block1'>line1</div>"
        "    <div id='block2'>line2</div>"
        "</div>");
    EXPECT_EQ(0, strutForBox("block1"));
    EXPECT_EQ(0, strutForLine("block1", 0));
    EXPECT_EQ(50, strutForBox("block2"));
    EXPECT_EQ(0, strutForLine("block2", 0));
}

TEST_F(PaginationStrutTest, FloatWithStrut)
{
    setBodyInnerHTML(
        "<div style='overflow:-webkit-paged-y; height:200px; line-height:150px;'>"
        "    <div style='height:120px;'></div>"
        "    <div id='float' style='float:left;'>line</div>"
        "</div>");
    EXPECT_EQ(80, strutForBox("float"));
    EXPECT_EQ(0, strutForLine("float", 0));
}

TEST_F(PaginationStrutTest, UnbreakableBlockWithStrut)
{
    setBodyInnerHTML(
        "<style>img { display:block; width:100px; height:150px; outline:4px solid blue; padding:0; border:none; margin:0; }</style>"
        "<div style='overflow:-webkit-paged-y; height:200px;'>"
        "    <img id='img1'>"
        "    <img id='img2'>"
        "</div>");
    EXPECT_EQ(0, strutForBox("img1"));
    EXPECT_EQ(50, strutForBox("img2"));
}

TEST_F(PaginationStrutTest, BreakBefore)
{
    setBodyInnerHTML(
        "<div style='overflow:-webkit-paged-y; height:400px; line-height:50px;'>"
        "    <div id='block1'>line1</div>"
        "    <div id='block2' style='break-before:page;'>line2</div>"
        "</div>");
    EXPECT_EQ(0, strutForBox("block1"));
    EXPECT_EQ(0, strutForLine("block1", 0));
    EXPECT_EQ(350, strutForBox("block2"));
    EXPECT_EQ(0, strutForLine("block2", 0));
}

TEST_F(PaginationStrutTest, BlockWithStrutPropagatedFromInnerBlock)
{
    setBodyInnerHTML(
        "<div style='overflow:-webkit-paged-y; height:200px; line-height:150px;'>"
        "    <div id='block1'>line1</div>"
        "    <div id='block2' style='padding-top:2px;'>"
        "        <div id='innerBlock' style='padding-top:2px;'>line2</div>"
        "    </div>"
        "</div>");
    EXPECT_EQ(0, strutForBox("block1"));
    EXPECT_EQ(0, strutForLine("block1", 0));
    EXPECT_EQ(50, strutForBox("block2"));
    EXPECT_EQ(0, strutForBox("innerBlock"));
    EXPECT_EQ(0, strutForLine("innerBlock", 0));
}

TEST_F(PaginationStrutTest, BlockWithStrutPropagatedFromUnbreakableInnerBlock)
{
    setBodyInnerHTML(
        "<div style='overflow:-webkit-paged-y; height:400px; line-height:150px;'>"
        "    <div id='block1'>line1</div>"
        "    <div id='block2' style='padding-top:2px;'>"
        "        <div id='innerBlock' style='padding-top:2px; break-inside:avoid;'>"
        "            line2<br>"
        "            line3<br>"
        "        </div>"
        "    </div>"
        "</div>");
    EXPECT_EQ(0, strutForBox("block1"));
    EXPECT_EQ(0, strutForLine("block1", 0));
    EXPECT_EQ(250, strutForBox("block2"));
    EXPECT_EQ(0, strutForBox("innerBlock"));
    EXPECT_EQ(0, strutForLine("innerBlock", 0));
    EXPECT_EQ(0, strutForLine("innerBlock", 1));
}

TEST_F(PaginationStrutTest, InnerBlockWithBreakBefore)
{
    setBodyInnerHTML(
        "<div style='overflow:-webkit-paged-y; height:200px; line-height:150px;'>"
        "    <div id='block1'>line1</div>"
        "    <div id='block2' style='padding-top:2px;'>"
        "        <div id='innerBlock' style='padding-top:2px; break-before:page;'>line2</div>"
        "    </div>"
        "</div>");
    EXPECT_EQ(0, strutForBox("block1"));
    EXPECT_EQ(0, strutForLine("block1", 0));
    // There's no class A break point before #innerBlock (they only exist *between* siblings), so
    // the break is propagated and applied before #block2.
    EXPECT_EQ(50, strutForBox("block2"));
    EXPECT_EQ(0, strutForBox("innerBlock"));
    EXPECT_EQ(0, strutForLine("innerBlock", 0));
}

} // namespace blink
