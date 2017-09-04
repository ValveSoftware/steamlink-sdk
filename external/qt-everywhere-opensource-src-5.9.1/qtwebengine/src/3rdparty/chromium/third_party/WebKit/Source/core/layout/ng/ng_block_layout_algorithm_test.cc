// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_block_layout_algorithm.h"

#include "core/layout/ng/ng_box.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_physical_fragment_base.h"
#include "core/layout/ng/ng_physical_fragment.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/layout/ng/ng_units.h"
#include "core/style/ComputedStyle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

NGConstraintSpace* ConstructConstraintSpace(NGWritingMode writing_mode,
                                            TextDirection direction,
                                            NGLogicalSize size) {
  NGConstraintSpaceBuilder builder(writing_mode);
  builder.SetAvailableSize(size).SetPercentageResolutionSize(size);

  return new NGConstraintSpace(writing_mode, direction,
                               builder.ToConstraintSpace());
}

class NGBlockLayoutAlgorithmTest : public ::testing::Test {
 protected:
  void SetUp() override { style_ = ComputedStyle::create(); }

  NGPhysicalFragment* RunBlockLayoutAlgorithm(NGConstraintSpace* space,
                                              NGBox* first_child) {
    NGBlockLayoutAlgorithm algorithm(style_, first_child, space);
    NGPhysicalFragmentBase* frag;
    while (!algorithm.Layout(nullptr, &frag, nullptr))
      continue;
    return toNGPhysicalFragment(frag);
  }

  RefPtr<ComputedStyle> style_;
};

TEST_F(NGBlockLayoutAlgorithmTest, FixedSize) {
  style_->setWidth(Length(30, Fixed));
  style_->setHeight(Length(40, Fixed));

  auto* space = ConstructConstraintSpace(
      HorizontalTopBottom, LTR,
      NGLogicalSize(LayoutUnit(100), NGSizeIndefinite));
  NGPhysicalFragmentBase* frag = RunBlockLayoutAlgorithm(space, nullptr);

  EXPECT_EQ(LayoutUnit(30), frag->Width());
  EXPECT_EQ(LayoutUnit(40), frag->Height());
}

// Verifies that two children are laid out with the correct size and position.
TEST_F(NGBlockLayoutAlgorithmTest, LayoutBlockChildren) {
  const int kWidth = 30;
  const int kHeight1 = 20;
  const int kHeight2 = 30;
  const int kMarginTop = 5;
  const int kMarginBottom = 20;
  style_->setWidth(Length(kWidth, Fixed));

  RefPtr<ComputedStyle> first_style = ComputedStyle::create();
  first_style->setHeight(Length(kHeight1, Fixed));
  NGBox* first_child = new NGBox(first_style.get());

  RefPtr<ComputedStyle> second_style = ComputedStyle::create();
  second_style->setHeight(Length(kHeight2, Fixed));
  second_style->setMarginTop(Length(kMarginTop, Fixed));
  second_style->setMarginBottom(Length(kMarginBottom, Fixed));
  NGBox* second_child = new NGBox(second_style.get());

  first_child->SetNextSibling(second_child);

  auto* space = ConstructConstraintSpace(
      HorizontalTopBottom, LTR,
      NGLogicalSize(LayoutUnit(100), NGSizeIndefinite));
  NGPhysicalFragment* frag = RunBlockLayoutAlgorithm(space, first_child);

  EXPECT_EQ(LayoutUnit(kWidth), frag->Width());
  EXPECT_EQ(LayoutUnit(kHeight1 + kHeight2 + kMarginTop), frag->Height());
  EXPECT_EQ(NGPhysicalFragmentBase::FragmentBox, frag->Type());
  ASSERT_EQ(frag->Children().size(), 2UL);

  const NGPhysicalFragmentBase* child = frag->Children()[0];
  EXPECT_EQ(kHeight1, child->Height());
  EXPECT_EQ(0, child->TopOffset());

  child = frag->Children()[1];
  EXPECT_EQ(kHeight2, child->Height());
  EXPECT_EQ(kHeight1 + kMarginTop, child->TopOffset());
}

// Verifies that a child is laid out correctly if it's writing mode is different
// from the parent's one.
//
// Test case's HTML representation:
// <div style="writing-mode: vertical-lr;">
//   <div style="width:50px;
//       height: 50px; margin-left: 100px;
//       writing-mode: horizontal-tb;"></div>
// </div>
TEST_F(NGBlockLayoutAlgorithmTest, LayoutBlockChildrenWithWritingMode) {
  const int kWidth = 50;
  const int kHeight = 50;
  const int kMarginLeft = 100;

  RefPtr<ComputedStyle> div1_style = ComputedStyle::create();
  div1_style->setWritingMode(LeftToRightWritingMode);
  NGBox* div1 = new NGBox(div1_style.get());

  RefPtr<ComputedStyle> div2_style = ComputedStyle::create();
  div2_style->setHeight(Length(kHeight, Fixed));
  div2_style->setWidth(Length(kWidth, Fixed));
  div1_style->setWritingMode(TopToBottomWritingMode);
  div2_style->setMarginLeft(Length(kMarginLeft, Fixed));
  NGBox* div2 = new NGBox(div2_style.get());

  div1->SetFirstChild(div2);

  auto* space =
      ConstructConstraintSpace(HorizontalTopBottom, LTR,
                               NGLogicalSize(LayoutUnit(500), LayoutUnit(500)));
  NGPhysicalFragment* frag = RunBlockLayoutAlgorithm(space, div1);

  const NGPhysicalFragmentBase* child = frag->Children()[0];
  // DIV2
  child = static_cast<const NGPhysicalFragment*>(child)->Children()[0];

  EXPECT_EQ(kHeight, child->Height());
  EXPECT_EQ(0, child->TopOffset());
  EXPECT_EQ(kMarginLeft, child->LeftOffset());
}

// Verifies the collapsing margins case for the next pair:
// - top margin of a box and top margin of its first in-flow child.
//
// Test case's HTML representation:
// <div style="margin-top: 20px; height: 50px;">  <!-- DIV1 -->
//    <div style="margin-top: 10px"></div>        <!-- DIV2 -->
// </div>
//
// Expected:
// - Empty margin strut of the fragment that establishes new formatting context
// - Margins are collapsed resulting a single margin 20px = max(20px, 10px)
// - The top offset of DIV2 == 20px
TEST_F(NGBlockLayoutAlgorithmTest, CollapsingMarginsCase1) {
  const int kHeight = 50;
  const int kDiv1MarginTop = 20;
  const int kDiv2MarginTop = 10;

  // DIV1
  RefPtr<ComputedStyle> div1_style = ComputedStyle::create();
  div1_style->setHeight(Length(kHeight, Fixed));
  div1_style->setMarginTop(Length(kDiv1MarginTop, Fixed));
  NGBox* div1 = new NGBox(div1_style.get());

  // DIV2
  RefPtr<ComputedStyle> div2_style = ComputedStyle::create();
  div2_style->setMarginTop(Length(kDiv2MarginTop, Fixed));
  NGBox* div2 = new NGBox(div2_style.get());

  div1->SetFirstChild(div2);

  auto* space = ConstructConstraintSpace(
      HorizontalTopBottom, LTR,
      NGLogicalSize(LayoutUnit(100), NGSizeIndefinite));
  space->SetIsNewFormattingContext(true);
  NGPhysicalFragment* frag = RunBlockLayoutAlgorithm(space, div1);

  EXPECT_TRUE(frag->MarginStrut().IsEmpty());
  ASSERT_EQ(frag->Children().size(), 1UL);
  const NGPhysicalFragment* div2_fragment =
      static_cast<const NGPhysicalFragment*>(frag->Children()[0].get());
  EXPECT_EQ(NGMarginStrut({LayoutUnit(kDiv2MarginTop)}),
            div2_fragment->MarginStrut());
  EXPECT_EQ(kDiv1MarginTop, div2_fragment->TopOffset());
}

// Verifies the collapsing margins case for the next pair:
// - bottom margin of box and top margin of its next in-flow following sibling.
//
// Test case's HTML representation:
// <div style="margin-bottom: 20px; height: 50px;">  <!-- DIV1 -->
//    <div style="margin-bottom: -15px"></div>       <!-- DIV2 -->
//    <div></div>                                    <!-- DIV3 -->
// </div>
// <div></div>                                       <!-- DIV4 -->
// <div style="margin-top: 10px; height: 50px;">     <!-- DIV5 -->
//    <div></div>                                    <!-- DIV6 -->
//    <div style="margin-top: -30px"></div>          <!-- DIV7 -->
// </div>
//
// Expected:
//   Margins are collapsed resulting an overlap
//   -10px = max(20px, 10px) - max(abs(-15px), abs(-30px))
//   between DIV2 and DIV3. Zero-height blocks are ignored.
TEST_F(NGBlockLayoutAlgorithmTest, CollapsingMarginsCase2) {
  const int kHeight = 50;
  const int kDiv1MarginBottom = 20;
  const int kDiv2MarginBottom = -15;
  const int kDiv5MarginTop = 10;
  const int kDiv7MarginTop = -30;
  const int kExpectedCollapsedMargin = -10;

  // DIV1
  RefPtr<ComputedStyle> div1_style = ComputedStyle::create();
  div1_style->setHeight(Length(kHeight, Fixed));
  div1_style->setMarginBottom(Length(kDiv1MarginBottom, Fixed));
  NGBox* div1 = new NGBox(div1_style.get());

  // DIV2
  RefPtr<ComputedStyle> div2_style = ComputedStyle::create();
  div2_style->setMarginBottom(Length(kDiv2MarginBottom, Fixed));
  NGBox* div2 = new NGBox(div2_style.get());

  // Empty DIVs: DIV3, DIV4, DIV6
  NGBox* div3 = new NGBox(ComputedStyle::create().get());
  NGBox* div4 = new NGBox(ComputedStyle::create().get());
  NGBox* div6 = new NGBox(ComputedStyle::create().get());

  // DIV5
  RefPtr<ComputedStyle> div5_style = ComputedStyle::create();
  div5_style->setHeight(Length(kHeight, Fixed));
  div5_style->setMarginTop(Length(kDiv5MarginTop, Fixed));
  NGBox* div5 = new NGBox(div5_style.get());

  // DIV7
  RefPtr<ComputedStyle> div7_style = ComputedStyle::create();
  div7_style->setMarginTop(Length(kDiv7MarginTop, Fixed));
  NGBox* div7 = new NGBox(div7_style.get());

  div1->SetFirstChild(div2);
  div2->SetNextSibling(div3);
  div1->SetNextSibling(div4);
  div4->SetNextSibling(div5);
  div5->SetFirstChild(div6);
  div6->SetNextSibling(div7);

  auto* space = ConstructConstraintSpace(
      HorizontalTopBottom, LTR,
      NGLogicalSize(LayoutUnit(100), NGSizeIndefinite));
  NGPhysicalFragment* frag = RunBlockLayoutAlgorithm(space, div1);

  ASSERT_EQ(frag->Children().size(), 3UL);

  // DIV1
  const NGPhysicalFragmentBase* child = frag->Children()[0];
  EXPECT_EQ(kHeight, child->Height());
  EXPECT_EQ(0, child->TopOffset());

  // DIV5
  child = frag->Children()[2];
  EXPECT_EQ(kHeight, child->Height());
  EXPECT_EQ(kHeight + kExpectedCollapsedMargin, child->TopOffset());
}

// Verifies the collapsing margins case for the next pair:
// - bottom margin of a last in-flow child and bottom margin of its parent if
//   the parent has 'auto' computed height
//
// Test case's HTML representation:
// <div style="margin-bottom: 20px; height: 50px;">            <!-- DIV1 -->
//   <div style="margin-bottom: 200px; height: 50px;"/>        <!-- DIV2 -->
// </div>
//
// Expected:
//   1) Margins are collapsed with the result = std::max(20, 200)
//      if DIV1.height == auto
//   2) Margins are NOT collapsed if DIV1.height != auto
TEST_F(NGBlockLayoutAlgorithmTest, CollapsingMarginsCase3) {
  const int kHeight = 50;
  const int kDiv1MarginBottom = 20;
  const int kDiv2MarginBottom = 200;

  // DIV1
  RefPtr<ComputedStyle> div1_style = ComputedStyle::create();
  div1_style->setMarginBottom(Length(kDiv1MarginBottom, Fixed));
  NGBox* div1 = new NGBox(div1_style.get());

  // DIV2
  RefPtr<ComputedStyle> div2_style = ComputedStyle::create();
  div2_style->setHeight(Length(kHeight, Fixed));
  div2_style->setMarginBottom(Length(kDiv2MarginBottom, Fixed));
  NGBox* div2 = new NGBox(div2_style.get());

  div1->SetFirstChild(div2);

  auto* space = ConstructConstraintSpace(
      HorizontalTopBottom, LTR,
      NGLogicalSize(LayoutUnit(100), NGSizeIndefinite));
  NGPhysicalFragment* frag = RunBlockLayoutAlgorithm(space, div1);

  // Verify that margins are collapsed.
  EXPECT_EQ(NGMarginStrut({LayoutUnit(0), LayoutUnit(kDiv2MarginBottom)}),
            frag->MarginStrut());

  // Verify that margins are NOT collapsed.
  div1_style->setHeight(Length(kHeight, Fixed));
  frag = RunBlockLayoutAlgorithm(space, div1);
  EXPECT_EQ(NGMarginStrut({LayoutUnit(0), LayoutUnit(kDiv1MarginBottom)}),
            frag->MarginStrut());
}

// Verifies that 2 adjoining margins are not collapsed if there is padding or
// border that separates them.
//
// Test case's HTML representation:
// <div style="margin: 30px 0px; padding: 20px 0px;">    <!-- DIV1 -->
//   <div style="margin: 200px 0px; height: 50px;"/>     <!-- DIV2 -->
// </div>
//
// Expected:
// Margins do NOT collapse if there is an interfering padding or border.
TEST_F(NGBlockLayoutAlgorithmTest, CollapsingMarginsCase4) {
  const int kHeight = 50;
  const int kDiv1Margin = 30;
  const int kDiv1Padding = 20;
  const int kDiv2Margin = 200;

  // DIV1
  RefPtr<ComputedStyle> div1_style = ComputedStyle::create();
  div1_style->setMarginTop(Length(kDiv1Margin, Fixed));
  div1_style->setMarginBottom(Length(kDiv1Margin, Fixed));
  div1_style->setPaddingTop(Length(kDiv1Padding, Fixed));
  div1_style->setPaddingBottom(Length(kDiv1Padding, Fixed));
  NGBox* div1 = new NGBox(div1_style.get());

  // DIV2
  RefPtr<ComputedStyle> div2_style = ComputedStyle::create();
  div2_style->setHeight(Length(kHeight, Fixed));
  div2_style->setMarginTop(Length(kDiv2Margin, Fixed));
  div2_style->setMarginBottom(Length(kDiv2Margin, Fixed));
  NGBox* div2 = new NGBox(div2_style.get());

  div1->SetFirstChild(div2);

  auto* space = ConstructConstraintSpace(
      HorizontalTopBottom, LTR,
      NGLogicalSize(LayoutUnit(100), NGSizeIndefinite));
  NGPhysicalFragment* frag = RunBlockLayoutAlgorithm(space, div1);

  // Verify that margins do NOT collapse.
  frag = RunBlockLayoutAlgorithm(space, div1);
  EXPECT_EQ(NGMarginStrut({LayoutUnit(kDiv1Margin), LayoutUnit(kDiv1Margin)}),
            frag->MarginStrut());
  ASSERT_EQ(frag->Children().size(), 1UL);

  EXPECT_EQ(NGMarginStrut({LayoutUnit(kDiv2Margin), LayoutUnit(kDiv2Margin)}),
            static_cast<const NGPhysicalFragment*>(frag->Children()[0].get())
                ->MarginStrut());

  // Reset padding and verify that margins DO collapse.
  div1_style->setPaddingTop(Length(0, Fixed));
  div1_style->setPaddingBottom(Length(0, Fixed));
  frag = RunBlockLayoutAlgorithm(space, div1);
  EXPECT_EQ(NGMarginStrut({LayoutUnit(kDiv2Margin), LayoutUnit(kDiv2Margin)}),
            frag->MarginStrut());
}

// Verifies that margins of 2 adjoining blocks with different writing modes
// get collapsed.
//
// Test case's HTML representation:
//   <div style="writing-mode: vertical-lr;">
//     <div style="margin-right: 60px; width: 60px;">vertical</div>
//     <div style="margin-left: 100px; writing-mode: horizontal-tb;">
//       horizontal
//     </div>
//   </div>
TEST_F(NGBlockLayoutAlgorithmTest, CollapsingMarginsCase5) {
  const int kVerticalDivMarginRight = 60;
  const int kVerticalDivWidth = 60;
  const int kHorizontalDivMarginLeft = 100;

  style_->setWidth(Length(500, Fixed));
  style_->setHeight(Length(500, Fixed));
  style_->setWritingMode(LeftToRightWritingMode);

  // Vertical DIV
  RefPtr<ComputedStyle> vertical_style = ComputedStyle::create();
  vertical_style->setMarginRight(Length(kVerticalDivMarginRight, Fixed));
  vertical_style->setWidth(Length(kVerticalDivWidth, Fixed));
  NGBox* vertical_div = new NGBox(vertical_style.get());

  // Horizontal DIV
  RefPtr<ComputedStyle> horizontal_style = ComputedStyle::create();
  horizontal_style->setMarginLeft(Length(kHorizontalDivMarginLeft, Fixed));
  horizontal_style->setWritingMode(TopToBottomWritingMode);
  NGBox* horizontal_div = new NGBox(horizontal_style.get());

  vertical_div->SetNextSibling(horizontal_div);

  auto* space = ConstructConstraintSpace(
      VerticalLeftRight, LTR, NGLogicalSize(LayoutUnit(500), LayoutUnit(500)));
  NGPhysicalFragment* frag = RunBlockLayoutAlgorithm(space, vertical_div);

  ASSERT_EQ(frag->Children().size(), 2UL);
  const NGPhysicalFragmentBase* child = frag->Children()[1];
  // Horizontal div
  EXPECT_EQ(0, child->TopOffset());
  EXPECT_EQ(kVerticalDivWidth + kHorizontalDivMarginLeft, child->LeftOffset());
}

// Verifies that the margin strut of a child with a different writing mode does
// not get used in the collapsing margins calculation.
//
// Test case's HTML representation:
//   <style>
//     #div1 { margin-bottom: 10px; height: 60px; writing-mode: vertical-rl; }
//     #div2 { margin-left: -20px; width: 10px; }
//     #div3 { margin-top: 40px; height: 60px; }
//   </style>
//   <div id="div1">
//      <div id="div2">vertical</div>
//   </div>
//   <div id="div3"></div>
TEST_F(NGBlockLayoutAlgorithmTest, CollapsingMarginsCase6) {
  const int kHeight = 60;
  const int kWidth = 10;
  const int kMarginBottom = 10;
  const int kMarginLeft = -20;
  const int kMarginTop = 40;

  style_->setWidth(Length(500, Fixed));
  style_->setHeight(Length(500, Fixed));

  // DIV1
  RefPtr<ComputedStyle> div1_style = ComputedStyle::create();
  div1_style->setWidth(Length(kWidth, Fixed));
  div1_style->setHeight(Length(kHeight, Fixed));
  div1_style->setWritingMode(RightToLeftWritingMode);
  div1_style->setMarginBottom(Length(kMarginBottom, Fixed));
  NGBox* div1 = new NGBox(div1_style.get());

  // DIV2
  RefPtr<ComputedStyle> div2_style = ComputedStyle::create();
  div2_style->setWidth(Length(kWidth, Fixed));
  div2_style->setMarginLeft(Length(kMarginLeft, Fixed));
  NGBox* div2 = new NGBox(div2_style.get());

  // DIV3
  RefPtr<ComputedStyle> div3_style = ComputedStyle::create();
  div3_style->setHeight(Length(kHeight, Fixed));
  div3_style->setMarginTop(Length(kMarginTop, Fixed));
  NGBox* div3 = new NGBox(div3_style.get());

  div1->SetFirstChild(div2);
  div1->SetNextSibling(div3);

  auto* space =
      ConstructConstraintSpace(HorizontalTopBottom, LTR,
                               NGLogicalSize(LayoutUnit(500), LayoutUnit(500)));
  NGPhysicalFragment* frag = RunBlockLayoutAlgorithm(space, div1);

  ASSERT_EQ(frag->Children().size(), 2UL);

  const NGPhysicalFragmentBase* child1 = frag->Children()[0];
  EXPECT_EQ(0, child1->TopOffset());
  EXPECT_EQ(kHeight, child1->Height());

  const NGPhysicalFragmentBase* child2 = frag->Children()[1];
  EXPECT_EQ(kHeight + std::max(kMarginBottom, kMarginTop), child2->TopOffset());
}

// Verifies that a box's size includes its borders and padding, and that
// children are positioned inside the content box.
//
// Test case's HTML representation:
// <style>
//   #div1 { width:100px; height:100px; }
//   #div1 { border-style:solid; border-width:1px 2px 3px 4px; }
//   #div1 { padding:5px 6px 7px 8px; }
// </style>
// <div id="div1">
//    <div id="div2"></div>
// </div>
TEST_F(NGBlockLayoutAlgorithmTest, BorderAndPadding) {
  const int kWidth = 100;
  const int kHeight = 100;
  const int kBorderTop = 1;
  const int kBorderRight = 2;
  const int kBorderBottom = 3;
  const int kBorderLeft = 4;
  const int kPaddingTop = 5;
  const int kPaddingRight = 6;
  const int kPaddingBottom = 7;
  const int kPaddingLeft = 8;
  RefPtr<ComputedStyle> div1_style = ComputedStyle::create();

  div1_style->setWidth(Length(kWidth, Fixed));
  div1_style->setHeight(Length(kHeight, Fixed));

  div1_style->setBorderTopWidth(kBorderTop);
  div1_style->setBorderTopStyle(BorderStyleSolid);
  div1_style->setBorderRightWidth(kBorderRight);
  div1_style->setBorderRightStyle(BorderStyleSolid);
  div1_style->setBorderBottomWidth(kBorderBottom);
  div1_style->setBorderBottomStyle(BorderStyleSolid);
  div1_style->setBorderLeftWidth(kBorderLeft);
  div1_style->setBorderLeftStyle(BorderStyleSolid);

  div1_style->setPaddingTop(Length(kPaddingTop, Fixed));
  div1_style->setPaddingRight(Length(kPaddingRight, Fixed));
  div1_style->setPaddingBottom(Length(kPaddingBottom, Fixed));
  div1_style->setPaddingLeft(Length(kPaddingLeft, Fixed));
  NGBox* div1 = new NGBox(div1_style.get());

  RefPtr<ComputedStyle> div2_style = ComputedStyle::create();
  NGBox* div2 = new NGBox(div2_style.get());

  div1->SetFirstChild(div2);

  auto* space = ConstructConstraintSpace(
      HorizontalTopBottom, LTR,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  NGPhysicalFragment* frag = RunBlockLayoutAlgorithm(space, div1);

  ASSERT_EQ(frag->Children().size(), 1UL);

  // div1
  const NGPhysicalFragmentBase* child = frag->Children()[0];
  EXPECT_EQ(kBorderLeft + kPaddingLeft + kWidth + kPaddingRight + kBorderRight,
            child->Width());
  EXPECT_EQ(kBorderTop + kPaddingTop + kHeight + kPaddingBottom + kBorderBottom,
            child->Height());

  ASSERT_TRUE(child->Type() == NGPhysicalFragmentBase::FragmentBox);
  ASSERT_EQ(static_cast<const NGPhysicalFragment*>(child)->Children().size(),
            1UL);

  // div2
  child = static_cast<const NGPhysicalFragment*>(child)->Children()[0];
  EXPECT_EQ(kBorderTop + kPaddingTop, child->TopOffset());
  EXPECT_EQ(kBorderLeft + kPaddingLeft, child->LeftOffset());
}

TEST_F(NGBlockLayoutAlgorithmTest, PercentageResolutionSize) {
  const int kPaddingLeft = 10;
  const int kWidth = 30;
  style_->setWidth(Length(kWidth, Fixed));
  style_->setPaddingLeft(Length(kPaddingLeft, Fixed));

  RefPtr<ComputedStyle> first_style = ComputedStyle::create();
  first_style->setWidth(Length(40, Percent));
  NGBox* first_child = new NGBox(first_style.get());

  auto* space = ConstructConstraintSpace(
      HorizontalTopBottom, LTR,
      NGLogicalSize(LayoutUnit(100), NGSizeIndefinite));
  NGPhysicalFragment* frag = RunBlockLayoutAlgorithm(space, first_child);

  EXPECT_EQ(frag->Width(), LayoutUnit(kWidth + kPaddingLeft));
  EXPECT_EQ(frag->Type(), NGPhysicalFragmentBase::FragmentBox);
  ASSERT_EQ(frag->Children().size(), 1UL);

  const NGPhysicalFragmentBase* child = frag->Children()[0];
  EXPECT_EQ(child->Width(), LayoutUnit(12));
}

// A very simple auto margin case. We rely on the tests in ng_length_utils_test
// for the more complex cases; just make sure we handle auto at all here.
TEST_F(NGBlockLayoutAlgorithmTest, AutoMargin) {
  const int kPaddingLeft = 10;
  const int kWidth = 30;
  style_->setWidth(Length(kWidth, Fixed));
  style_->setPaddingLeft(Length(kPaddingLeft, Fixed));

  RefPtr<ComputedStyle> first_style = ComputedStyle::create();
  const int kChildWidth = 10;
  first_style->setWidth(Length(kChildWidth, Fixed));
  first_style->setMarginLeft(Length(Auto));
  first_style->setMarginRight(Length(Auto));
  NGBox* first_child = new NGBox(first_style.get());

  auto* space = ConstructConstraintSpace(
      HorizontalTopBottom, LTR,
      NGLogicalSize(LayoutUnit(100), NGSizeIndefinite));
  NGPhysicalFragment* frag = RunBlockLayoutAlgorithm(space, first_child);

  EXPECT_EQ(LayoutUnit(kWidth + kPaddingLeft), frag->Width());
  EXPECT_EQ(NGPhysicalFragmentBase::FragmentBox, frag->Type());
  EXPECT_EQ(LayoutUnit(kWidth + kPaddingLeft), frag->WidthOverflow());
  ASSERT_EQ(1UL, frag->Children().size());

  const NGPhysicalFragmentBase* child = frag->Children()[0];
  EXPECT_EQ(LayoutUnit(kChildWidth), child->Width());
  EXPECT_EQ(LayoutUnit(kPaddingLeft + 10), child->LeftOffset());
  EXPECT_EQ(LayoutUnit(0), child->TopOffset());
}

// Verifies that 3 Left/Right float fragments and one regular block fragment
// are correctly positioned by the algorithm.
//
// Test case's HTML representation:
//  <div id="parent" style="width: 200px; height: 200px;">
//    <div style="float:left; width: 30px; height: 30px;
//        margin-top: 10px;"/>   <!-- DIV1 -->
//    <div style="width: 30px; height: 30px;"/>   <!-- DIV2 -->
//    <div style="float:right; width: 50px; height: 50px;"/>  <!-- DIV3 -->
//    <div style="float:left; width: 120px; height: 120px;
//        margin-left: 30px;"/>  <!-- DIV4 -->
//  </div>
//
// Expected:
// - Left float(DIV1) is positioned at the left.
// - Regular block (DIV2) is positioned behind DIV1.
// - Right float(DIV3) is positioned at the right below DIV2
// - Left float(DIV4) is positioned at the left below DIV3.
TEST_F(NGBlockLayoutAlgorithmTest, PositionFloatFragments) {
  const int kParentLeftPadding = 10;
  const int kDiv1TopMargin = 10;
  const int kParentSize = 200;
  const int kDiv1Size = 30;
  const int kDiv2Size = 30;
  const int kDiv3Size = 50;
  const int kDiv4Size = kParentSize - kDiv3Size;
  const int kDiv4LeftMargin = kDiv1Size;

  style_->setHeight(Length(kParentSize, Fixed));
  style_->setWidth(Length(kParentSize, Fixed));
  style_->setPaddingLeft(Length(kParentLeftPadding, Fixed));

  // DIV1
  RefPtr<ComputedStyle> div1_style = ComputedStyle::create();
  div1_style->setWidth(Length(kDiv1Size, Fixed));
  div1_style->setHeight(Length(kDiv1Size, Fixed));
  div1_style->setFloating(EFloat::Left);
  div1_style->setMarginTop(Length(kDiv1TopMargin, Fixed));
  NGBox* div1 = new NGBox(div1_style.get());

  // DIV2
  RefPtr<ComputedStyle> div2_style = ComputedStyle::create();
  div2_style->setWidth(Length(kDiv2Size, Fixed));
  div2_style->setHeight(Length(kDiv2Size, Fixed));
  NGBox* div2 = new NGBox(div2_style.get());

  // DIV3
  RefPtr<ComputedStyle> div3_style = ComputedStyle::create();
  div3_style->setWidth(Length(kDiv3Size, Fixed));
  div3_style->setHeight(Length(kDiv3Size, Fixed));
  div3_style->setFloating(EFloat::Right);
  NGBox* div3 = new NGBox(div3_style.get());

  // DIV4
  RefPtr<ComputedStyle> div4_style = ComputedStyle::create();
  div4_style->setWidth(Length(kDiv4Size, Fixed));
  div4_style->setHeight(Length(kDiv4Size, Fixed));
  div4_style->setMarginLeft(Length(kDiv4LeftMargin, Fixed));
  div4_style->setFloating(EFloat::Left);
  NGBox* div4 = new NGBox(div4_style.get());

  div1->SetNextSibling(div2);
  div2->SetNextSibling(div3);
  div3->SetNextSibling(div4);

  auto* space = ConstructConstraintSpace(
      HorizontalTopBottom, LTR,
      NGLogicalSize(LayoutUnit(kParentSize), LayoutUnit(kParentSize)));
  NGPhysicalFragment* frag = RunBlockLayoutAlgorithm(space, div1);
  ASSERT_EQ(frag->Children().size(), 4UL);

  // DIV1
  const NGPhysicalFragmentBase* child1 = frag->Children()[0];
  EXPECT_EQ(kDiv1TopMargin, child1->TopOffset());
  EXPECT_EQ(kParentLeftPadding, child1->LeftOffset());

  // DIV2
  const NGPhysicalFragmentBase* child2 = frag->Children()[1];
  EXPECT_EQ(0, child2->TopOffset());
  EXPECT_EQ(kParentLeftPadding, child2->LeftOffset());

  // DIV3
  const NGPhysicalFragmentBase* child3 = frag->Children()[2];
  EXPECT_EQ(kDiv2Size, child3->TopOffset());
  EXPECT_EQ(kParentLeftPadding + kParentSize - kDiv3Size, child3->LeftOffset());

  // DIV4
  const NGPhysicalFragmentBase* child4 = frag->Children()[3];
  EXPECT_EQ(kDiv2Size + kDiv3Size, child4->TopOffset());
  EXPECT_EQ(kParentLeftPadding + kDiv4LeftMargin, child4->LeftOffset());
}

// Verifies that NG block layout algorithm respects "clear" CSS property.
//
// Test case's HTML representation:
//  <div id="parent" style="width: 200px; height: 200px;">
//    <div style="float: left; width: 30px; height: 30px;"/>   <!-- DIV1 -->
//    <div style="float: right; width: 40px; height: 40px;
//        clear: left;"/>  <!-- DIV2 -->
//    <div style="clear: ...; width: 50px; height: 50px;"/>    <!-- DIV3 -->
//  </div>
//
// Expected:
// - DIV2 is positioned below DIV1 because it has clear: left;
// - DIV3 is positioned below DIV1 if clear: left;
// - DIV3 is positioned below DIV2 if clear: right;
// - DIV3 is positioned below DIV2 if clear: both;
TEST_F(NGBlockLayoutAlgorithmTest, PositionFragmentsWithClear) {
  const int kParentSize = 200;
  const int kDiv1Size = 30;
  const int kDiv2Size = 40;
  const int kDiv3Size = 50;

  style_->setHeight(Length(kParentSize, Fixed));
  style_->setWidth(Length(kParentSize, Fixed));

  // DIV1
  RefPtr<ComputedStyle> div1_style = ComputedStyle::create();
  div1_style->setWidth(Length(kDiv1Size, Fixed));
  div1_style->setHeight(Length(kDiv1Size, Fixed));
  div1_style->setFloating(EFloat::Left);
  NGBox* div1 = new NGBox(div1_style.get());

  // DIV2
  RefPtr<ComputedStyle> div2_style = ComputedStyle::create();
  div2_style->setWidth(Length(kDiv2Size, Fixed));
  div2_style->setHeight(Length(kDiv2Size, Fixed));
  div2_style->setClear(EClear::ClearLeft);
  div2_style->setFloating(EFloat::Right);
  NGBox* div2 = new NGBox(div2_style.get());

  // DIV3
  RefPtr<ComputedStyle> div3_style = ComputedStyle::create();
  div3_style->setWidth(Length(kDiv3Size, Fixed));
  div3_style->setHeight(Length(kDiv3Size, Fixed));
  NGBox* div3 = new NGBox(div3_style.get());

  div1->SetNextSibling(div2);
  div2->SetNextSibling(div3);

  // clear: left;
  div3_style->setClear(EClear::ClearLeft);
  auto* space = ConstructConstraintSpace(
      HorizontalTopBottom, LTR,
      NGLogicalSize(LayoutUnit(kParentSize), LayoutUnit(kParentSize)));
  NGPhysicalFragment* frag = RunBlockLayoutAlgorithm(space, div1);
  const NGPhysicalFragmentBase* child3 = frag->Children()[2];
  EXPECT_EQ(kDiv1Size, child3->TopOffset());

  // clear: right;
  div3_style->setClear(EClear::ClearRight);
  space = ConstructConstraintSpace(
      HorizontalTopBottom, LTR,
      NGLogicalSize(LayoutUnit(kParentSize), LayoutUnit(kParentSize)));
  frag = RunBlockLayoutAlgorithm(space, div1);
  child3 = frag->Children()[2];
  EXPECT_EQ(kDiv1Size + kDiv2Size, child3->TopOffset());

  // clear: both;
  div3_style->setClear(EClear::ClearBoth);
  space = ConstructConstraintSpace(
      HorizontalTopBottom, LTR,
      NGLogicalSize(LayoutUnit(kParentSize), LayoutUnit(kParentSize)));
  frag = RunBlockLayoutAlgorithm(space, div1);
  space = ConstructConstraintSpace(
      HorizontalTopBottom, LTR,
      NGLogicalSize(LayoutUnit(kParentSize), LayoutUnit(kParentSize)));
  child3 = frag->Children()[2];
  EXPECT_EQ(kDiv1Size + kDiv2Size, child3->TopOffset());
}

}  // namespace
}  // namespace blink
