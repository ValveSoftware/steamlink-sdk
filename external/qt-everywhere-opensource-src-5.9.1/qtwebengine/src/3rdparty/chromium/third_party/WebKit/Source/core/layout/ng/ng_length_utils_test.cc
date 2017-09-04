// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_length_utils.h"

#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_physical_fragment.h"
#include "core/layout/ng/ng_units.h"
#include "core/style/ComputedStyle.h"
#include "platform/CalculationValue.h"
#include "platform/LayoutUnit.h"
#include "platform/Length.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/RefPtr.h"

namespace blink {
namespace {

class NGLengthUtilsTest : public ::testing::Test {
 protected:
  void SetUp() override { style_ = ComputedStyle::create(); }

  static NGConstraintSpace* ConstructConstraintSpace(int inline_size,
                                                     int block_size,
                                                     bool fixed_inline = false,
                                                     bool fixed_block = false) {
    NGConstraintSpaceBuilder builder(HorizontalTopBottom);
    builder
        .SetAvailableSize(
            NGLogicalSize(LayoutUnit(inline_size), LayoutUnit(block_size)))
        .SetPercentageResolutionSize(
            NGLogicalSize(LayoutUnit(inline_size), LayoutUnit(block_size)))
        .SetIsFixedSizeInline(fixed_inline)
        .SetIsFixedSizeBlock(fixed_block);

    return new NGConstraintSpace(HorizontalTopBottom, LTR,
                                 builder.ToConstraintSpace());
  }

  LayoutUnit ResolveInlineLength(
      const Length& length,
      LengthResolveType type = LengthResolveType::ContentSize,
      const WTF::Optional<MinAndMaxContentSizes>& sizes = WTF::nullopt) {
    NGConstraintSpace* constraintSpace = ConstructConstraintSpace(200, 300);
    return ::blink::ResolveInlineLength(*constraintSpace, *style_, sizes,
                                        length, type);
  }

  LayoutUnit ResolveBlockLength(
      const Length& length,
      LengthResolveType type = LengthResolveType::ContentSize,
      LayoutUnit contentSize = LayoutUnit()) {
    NGConstraintSpace* constraintSpace = ConstructConstraintSpace(200, 300);
    return ::blink::ResolveBlockLength(*constraintSpace, *style_, length,
                                       contentSize, type);
  }

  LayoutUnit ComputeInlineSizeForFragment(
      const NGConstraintSpace* constraintSpace = ConstructConstraintSpace(200,
                                                                          300),
      const MinAndMaxContentSizes& sizes = MinAndMaxContentSizes()) {
    return ::blink::ComputeInlineSizeForFragment(*constraintSpace, *style_,
                                                 sizes);
  }

  LayoutUnit ComputeBlockSizeForFragment(
      const NGConstraintSpace* constraintSpace = ConstructConstraintSpace(200,
                                                                          300),
      LayoutUnit contentSize = LayoutUnit()) {
    return ::blink::ComputeBlockSizeForFragment(*constraintSpace, *style_,
                                                contentSize);
  }

  RefPtr<ComputedStyle> style_;
};

TEST_F(NGLengthUtilsTest, testResolveInlineLength) {
  EXPECT_EQ(LayoutUnit(60), ResolveInlineLength(Length(30, Percent)));
  EXPECT_EQ(LayoutUnit(150), ResolveInlineLength(Length(150, Fixed)));
  EXPECT_EQ(LayoutUnit(0),
            ResolveInlineLength(Length(Auto), LengthResolveType::MinSize));
  EXPECT_EQ(LayoutUnit(200), ResolveInlineLength(Length(Auto)));
  EXPECT_EQ(LayoutUnit(200), ResolveInlineLength(Length(FillAvailable)));

  EXPECT_EQ(LayoutUnit(200),
            ResolveInlineLength(Length(Auto), LengthResolveType::MaxSize));
  EXPECT_EQ(LayoutUnit(200), ResolveInlineLength(Length(FillAvailable),
                                                 LengthResolveType::MaxSize));
  MinAndMaxContentSizes sizes;
  sizes.min_content = LayoutUnit(30);
  sizes.max_content = LayoutUnit(40);
  EXPECT_EQ(LayoutUnit(30),
            ResolveInlineLength(Length(MinContent),
                                LengthResolveType::ContentSize, sizes));
  EXPECT_EQ(LayoutUnit(40),
            ResolveInlineLength(Length(MaxContent),
                                LengthResolveType::ContentSize, sizes));
  EXPECT_EQ(LayoutUnit(40),
            ResolveInlineLength(Length(FitContent),
                                LengthResolveType::ContentSize, sizes));
  sizes.max_content = LayoutUnit(800);
  EXPECT_EQ(LayoutUnit(200),
            ResolveInlineLength(Length(FitContent),
                                LengthResolveType::ContentSize, sizes));
#ifndef NDEBUG
  // This should fail a DCHECK.
  EXPECT_DEATH(ResolveInlineLength(Length(FitContent)), "Check failed");
#endif
}

TEST_F(NGLengthUtilsTest, testResolveBlockLength) {
  EXPECT_EQ(LayoutUnit(90), ResolveBlockLength(Length(30, Percent)));
  EXPECT_EQ(LayoutUnit(150), ResolveBlockLength(Length(150, Fixed)));
  EXPECT_EQ(LayoutUnit(0), ResolveBlockLength(Length(Auto)));
  EXPECT_EQ(LayoutUnit(300), ResolveBlockLength(Length(FillAvailable)));

  EXPECT_EQ(LayoutUnit(0),
            ResolveBlockLength(Length(Auto), LengthResolveType::ContentSize));
  EXPECT_EQ(LayoutUnit(300),
            ResolveBlockLength(Length(FillAvailable),
                               LengthResolveType::ContentSize));
}

TEST_F(NGLengthUtilsTest, testComputeInlineSizeForFragment) {
  MinAndMaxContentSizes sizes;
  sizes.min_content = LayoutUnit(30);
  sizes.max_content = LayoutUnit(40);

  style_->setLogicalWidth(Length(30, Percent));
  EXPECT_EQ(LayoutUnit(60), ComputeInlineSizeForFragment());

  style_->setLogicalWidth(Length(150, Fixed));
  EXPECT_EQ(LayoutUnit(150), ComputeInlineSizeForFragment());

  style_->setLogicalWidth(Length(Auto));
  EXPECT_EQ(LayoutUnit(200), ComputeInlineSizeForFragment());

  style_->setLogicalWidth(Length(FillAvailable));
  EXPECT_EQ(LayoutUnit(200), ComputeInlineSizeForFragment());

  style_->setLogicalWidth(Length(CalculationValue::create(
      PixelsAndPercent(100, -10), ValueRangeNonNegative)));
  EXPECT_EQ(LayoutUnit(80), ComputeInlineSizeForFragment());

  NGConstraintSpace* constraintSpace =
      ConstructConstraintSpace(120, 120, true, true);
  style_->setLogicalWidth(Length(150, Fixed));
  EXPECT_EQ(LayoutUnit(120), ComputeInlineSizeForFragment(constraintSpace));

  style_->setLogicalWidth(Length(200, Fixed));
  style_->setMaxWidth(Length(80, Percent));
  EXPECT_EQ(LayoutUnit(160), ComputeInlineSizeForFragment());

  style_->setLogicalWidth(Length(100, Fixed));
  style_->setMinWidth(Length(80, Percent));
  EXPECT_EQ(LayoutUnit(160), ComputeInlineSizeForFragment());

  style_ = ComputedStyle::create();
  style_->setMarginRight(Length(20, Fixed));
  EXPECT_EQ(LayoutUnit(180), ComputeInlineSizeForFragment());

  style_->setLogicalWidth(Length(100, Fixed));
  style_->setPaddingLeft(Length(50, Fixed));
  EXPECT_EQ(LayoutUnit(150), ComputeInlineSizeForFragment());

  style_->setBoxSizing(BoxSizingBorderBox);
  EXPECT_EQ(LayoutUnit(100), ComputeInlineSizeForFragment());

  // Content size should never be below zero, even with box-sizing: border-box
  // and a large padding...
  style_->setPaddingLeft(Length(400, Fixed));
  EXPECT_EQ(LayoutUnit(400), ComputeInlineSizeForFragment());

  // ...and the same goes for fill-available with a large padding.
  style_->setLogicalWidth(Length(FillAvailable));
  EXPECT_EQ(LayoutUnit(400), ComputeInlineSizeForFragment());

  constraintSpace = ConstructConstraintSpace(120, 140);
  style_->setLogicalWidth(Length(MinContent));
  EXPECT_EQ(LayoutUnit(430),
            ComputeInlineSizeForFragment(constraintSpace, sizes));
  style_->setLogicalWidth(Length(100, Fixed));
  style_->setMaxWidth(Length(MaxContent));
  // Due to padding and box-sizing, width computes to 400px and max-width to
  // 440px, so the result is 400.
  EXPECT_EQ(LayoutUnit(400),
            ComputeInlineSizeForFragment(constraintSpace, sizes));
  style_->setPaddingLeft(Length(0, Fixed));
  EXPECT_EQ(LayoutUnit(40),
            ComputeInlineSizeForFragment(constraintSpace, sizes));
}

TEST_F(NGLengthUtilsTest, testComputeBlockSizeForFragment) {
  style_->setLogicalHeight(Length(30, Percent));
  EXPECT_EQ(LayoutUnit(90), ComputeBlockSizeForFragment());

  style_->setLogicalHeight(Length(150, Fixed));
  EXPECT_EQ(LayoutUnit(150), ComputeBlockSizeForFragment());

  style_->setLogicalHeight(Length(Auto));
  EXPECT_EQ(LayoutUnit(0), ComputeBlockSizeForFragment());

  style_->setLogicalHeight(Length(Auto));
  EXPECT_EQ(LayoutUnit(120),
            ComputeBlockSizeForFragment(ConstructConstraintSpace(200, 300),
                                        LayoutUnit(120)));

  style_->setLogicalHeight(Length(FillAvailable));
  EXPECT_EQ(LayoutUnit(300), ComputeBlockSizeForFragment());

  style_->setLogicalHeight(Length(CalculationValue::create(
      PixelsAndPercent(100, -10), ValueRangeNonNegative)));
  EXPECT_EQ(LayoutUnit(70), ComputeBlockSizeForFragment());

  NGConstraintSpace* constraintSpace =
      ConstructConstraintSpace(200, 200, true, true);
  style_->setLogicalHeight(Length(150, Fixed));
  EXPECT_EQ(LayoutUnit(200), ComputeBlockSizeForFragment(constraintSpace));

  style_->setLogicalHeight(Length(300, Fixed));
  style_->setMaxHeight(Length(80, Percent));
  EXPECT_EQ(LayoutUnit(240), ComputeBlockSizeForFragment());

  style_->setLogicalHeight(Length(100, Fixed));
  style_->setMinHeight(Length(80, Percent));
  EXPECT_EQ(LayoutUnit(240), ComputeBlockSizeForFragment());

  style_ = ComputedStyle::create();
  style_->setMarginTop(Length(20, Fixed));
  style_->setLogicalHeight(Length(FillAvailable));
  EXPECT_EQ(LayoutUnit(280), ComputeBlockSizeForFragment());

  style_->setLogicalHeight(Length(100, Fixed));
  style_->setPaddingBottom(Length(50, Fixed));
  EXPECT_EQ(LayoutUnit(150), ComputeBlockSizeForFragment());

  style_->setBoxSizing(BoxSizingBorderBox);
  EXPECT_EQ(LayoutUnit(100), ComputeBlockSizeForFragment());

  // Content size should never be below zero, even with box-sizing: border-box
  // and a large padding...
  style_->setPaddingBottom(Length(400, Fixed));
  EXPECT_EQ(LayoutUnit(400), ComputeBlockSizeForFragment());

  // ...and the same goes for fill-available with a large padding.
  style_->setLogicalHeight(Length(FillAvailable));
  EXPECT_EQ(LayoutUnit(400), ComputeBlockSizeForFragment());

  // TODO(layout-ng): test {min,max}-content on max-height.
}

TEST_F(NGLengthUtilsTest, testIndefinitePercentages) {
  style_->setMinHeight(Length(20, Fixed));
  style_->setHeight(Length(20, Percent));

  EXPECT_EQ(NGSizeIndefinite,
            ComputeBlockSizeForFragment(ConstructConstraintSpace(200, -1),
                                        LayoutUnit(-1)));
  EXPECT_EQ(LayoutUnit(20),
            ComputeBlockSizeForFragment(ConstructConstraintSpace(200, -1),
                                        LayoutUnit(10)));
  EXPECT_EQ(LayoutUnit(120),
            ComputeBlockSizeForFragment(ConstructConstraintSpace(200, -1),
                                        LayoutUnit(120)));
}

TEST_F(NGLengthUtilsTest, testMargins) {
  style_->setMarginTop(Length(10, Percent));
  style_->setMarginRight(Length(52, Fixed));
  style_->setMarginBottom(Length(Auto));
  style_->setMarginLeft(Length(11, Percent));

  NGConstraintSpace* constraintSpace(ConstructConstraintSpace(200, 300));

  NGBoxStrut margins =
      ComputeMargins(*constraintSpace, *style_, HorizontalTopBottom, LTR);

  EXPECT_EQ(LayoutUnit(20), margins.block_start);
  EXPECT_EQ(LayoutUnit(52), margins.inline_end);
  EXPECT_EQ(LayoutUnit(), margins.block_end);
  EXPECT_EQ(LayoutUnit(22), margins.inline_start);
}

TEST_F(NGLengthUtilsTest, testBorders) {
  style_->setBorderTopWidth(1);
  style_->setBorderRightWidth(2);
  style_->setBorderBottomWidth(3);
  style_->setBorderLeftWidth(4);
  style_->setBorderTopStyle(BorderStyleSolid);
  style_->setBorderRightStyle(BorderStyleSolid);
  style_->setBorderBottomStyle(BorderStyleSolid);
  style_->setBorderLeftStyle(BorderStyleSolid);
  style_->setWritingMode(LeftToRightWritingMode);

  NGBoxStrut borders = ComputeBorders(*style_);

  EXPECT_EQ(LayoutUnit(4), borders.block_start);
  EXPECT_EQ(LayoutUnit(3), borders.inline_end);
  EXPECT_EQ(LayoutUnit(2), borders.block_end);
  EXPECT_EQ(LayoutUnit(1), borders.inline_start);
}

TEST_F(NGLengthUtilsTest, testPadding) {
  style_->setPaddingTop(Length(10, Percent));
  style_->setPaddingRight(Length(52, Fixed));
  style_->setPaddingBottom(Length(Auto));
  style_->setPaddingLeft(Length(11, Percent));
  style_->setWritingMode(RightToLeftWritingMode);

  NGConstraintSpace* constraintSpace(ConstructConstraintSpace(200, 300));

  NGBoxStrut padding = ComputePadding(*constraintSpace, *style_);

  EXPECT_EQ(LayoutUnit(52), padding.block_start);
  EXPECT_EQ(LayoutUnit(), padding.inline_end);
  EXPECT_EQ(LayoutUnit(22), padding.block_end);
  EXPECT_EQ(LayoutUnit(20), padding.inline_start);
}

TEST_F(NGLengthUtilsTest, testAutoMargins) {
  style_->setMarginRight(Length(Auto));
  style_->setMarginLeft(Length(Auto));

  NGFragmentBuilder builder(NGPhysicalFragmentBase::FragmentBox);
  builder.SetInlineSize(LayoutUnit(150));
  NGPhysicalFragment* physical_fragment = builder.ToFragment();
  NGFragment* fragment =
      new NGFragment(HorizontalTopBottom, LTR, physical_fragment);

  NGConstraintSpace* constraint_space(ConstructConstraintSpace(200, 300));

  NGBoxStrut margins;
  ApplyAutoMargins(*constraint_space, *style_, *fragment, &margins);

  EXPECT_EQ(LayoutUnit(), margins.block_start);
  EXPECT_EQ(LayoutUnit(), margins.block_end);
  EXPECT_EQ(LayoutUnit(25), margins.inline_start);
  EXPECT_EQ(LayoutUnit(25), margins.inline_end);

  style_->setMarginLeft(Length(0, Fixed));
  margins = NGBoxStrut();
  ApplyAutoMargins(*constraint_space, *style_, *fragment, &margins);
  EXPECT_EQ(LayoutUnit(0), margins.inline_start);
  EXPECT_EQ(LayoutUnit(50), margins.inline_end);

  style_->setMarginLeft(Length(Auto));
  style_->setMarginRight(Length(0, Fixed));
  margins = NGBoxStrut();
  ApplyAutoMargins(*constraint_space, *style_, *fragment, &margins);
  EXPECT_EQ(LayoutUnit(50), margins.inline_start);
  EXPECT_EQ(LayoutUnit(0), margins.inline_end);

  // Test that we don't end up with negative "auto" margins when the box is too
  // big.
  style_->setMarginLeft(Length(Auto));
  style_->setMarginRight(Length(5000, Fixed));
  margins = NGBoxStrut();
  margins.inline_end = LayoutUnit(5000);
  ApplyAutoMargins(*constraint_space, *style_, *fragment, &margins);
  EXPECT_EQ(LayoutUnit(0), margins.inline_start);
  EXPECT_EQ(LayoutUnit(5000), margins.inline_end);
}

}  // namespace
}  // namespace blink
