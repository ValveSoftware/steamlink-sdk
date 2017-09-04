// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_constraint_space.h"

#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_layout_opportunity_iterator.h"
#include "core/layout/ng/ng_physical_constraint_space.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

NGConstraintSpace* ConstructConstraintSpace(NGWritingMode writing_mode,
                                            TextDirection direction,
                                            NGPhysicalSize size) {
  return new NGConstraintSpace(
      writing_mode, direction,
      new NGPhysicalConstraintSpace(
          size, size, /* fixed_width */ true, /* fixed_height */ false,
          /* width_direction_triggers_scrollbar */ true,
          /* height_direction_triggers_scrollbar */ false, FragmentNone,
          FragmentColumn, /* is_new_fc */ false));
}

TEST(NGConstraintSpaceTest, WritingMode) {
  NGPhysicalConstraintSpace* phy_space = new NGPhysicalConstraintSpace(
      NGPhysicalSize(LayoutUnit(200), LayoutUnit(100)),
      NGPhysicalSize(LayoutUnit(200), LayoutUnit(100)), /* fixed_width */ true,
      /* fixed_height */ false, /* width_direction_triggers_scrollbar */ true,
      /* height_direction_triggers_scrollbar */ false, FragmentNone,
      FragmentColumn, /* is_new_fc */ false);

  NGConstraintSpace* horz_space =
      new NGConstraintSpace(HorizontalTopBottom, LTR, phy_space);

  NGConstraintSpace* vert_space =
      new NGConstraintSpace(VerticalRightLeft, LTR, phy_space);

  EXPECT_EQ(LayoutUnit(200), horz_space->AvailableSize().inline_size);
  EXPECT_EQ(LayoutUnit(200), vert_space->AvailableSize().block_size);

  EXPECT_EQ(LayoutUnit(100), horz_space->AvailableSize().block_size);
  EXPECT_EQ(LayoutUnit(100), vert_space->AvailableSize().inline_size);

  EXPECT_TRUE(horz_space->InlineTriggersScrollbar());
  EXPECT_TRUE(vert_space->BlockTriggersScrollbar());

  EXPECT_FALSE(horz_space->BlockTriggersScrollbar());
  EXPECT_FALSE(vert_space->InlineTriggersScrollbar());

  EXPECT_TRUE(horz_space->FixedInlineSize());
  EXPECT_TRUE(vert_space->FixedBlockSize());

  EXPECT_FALSE(horz_space->FixedBlockSize());
  EXPECT_FALSE(vert_space->FixedInlineSize());

  EXPECT_EQ(FragmentColumn, horz_space->BlockFragmentationType());
  EXPECT_EQ(FragmentNone, vert_space->BlockFragmentationType());
}

static String OpportunityToString(const NGLayoutOpportunity& opportunity) {
  return opportunity.IsEmpty() ? String("(empty)") : opportunity.ToString();
}

TEST(NGConstraintSpaceTest, LayoutOpportunitiesNoExclusions) {
  NGPhysicalSize physical_size;
  physical_size.width = LayoutUnit(600);
  physical_size.height = LayoutUnit(400);

  auto* space =
      ConstructConstraintSpace(HorizontalTopBottom, LTR, physical_size);
  auto* iterator = space->LayoutOpportunities();

  EXPECT_EQ("0,0 600x400", OpportunityToString(iterator->Next()));
  EXPECT_EQ("(empty)", OpportunityToString(iterator->Next()));
}

TEST(NGConstraintSpaceTest, LayoutOpportunitiesTopRightExclusion) {
  NGPhysicalSize physical_size;
  physical_size.width = LayoutUnit(600);
  physical_size.height = LayoutUnit(400);

  // Create a space with a 100x100 exclusion in the top right corner.
  auto* space =
      ConstructConstraintSpace(HorizontalTopBottom, LTR, physical_size);
  NGExclusion exclusion;
  exclusion.rect.size = {/* inline_size */ LayoutUnit(100),
                         /* block_size */ LayoutUnit(100)};
  exclusion.rect.offset = {/* inline_offset */ LayoutUnit(500),
                           /* block_offset */ LayoutUnit(0)};
  space->AddExclusion(exclusion);
  auto* iterator = space->LayoutOpportunities();

  // First opportunity should be to the left of the exclusion.
  EXPECT_EQ("0,0 500x400", OpportunityToString(iterator->Next()));

  // Second opportunity should be below the exclusion.
  EXPECT_EQ("0,100 600x300", OpportunityToString(iterator->Next()));

  // There should be no third opportunity.
  EXPECT_EQ("(empty)", OpportunityToString(iterator->Next()));
}

TEST(NGConstraintSpaceTest, LayoutOpportunitiesTopLeftExclusion) {
  NGPhysicalSize physical_size;
  physical_size.width = LayoutUnit(600);
  physical_size.height = LayoutUnit(400);

  // Create a space with a 100x100 exclusion in the top left corner.
  auto* space =
      ConstructConstraintSpace(HorizontalTopBottom, LTR, physical_size);
  NGExclusion exclusion;
  exclusion.rect.size = {/* inline_size */ LayoutUnit(100),
                         /* block_size */ LayoutUnit(100)};
  exclusion.rect.offset = {/* inline_offset */ LayoutUnit(0),
                           /* block_offset */ LayoutUnit(0)};
  space->AddExclusion(exclusion);

  auto* iterator = space->LayoutOpportunities();

  // First opportunity should be to the right of the exclusion.
  EXPECT_EQ("100,0 500x400", OpportunityToString(iterator->Next()));

  // Second opportunity should be below the exclusion.
  EXPECT_EQ("0,100 600x300", OpportunityToString(iterator->Next()));

  // There should be no third opportunity.
  EXPECT_EQ("(empty)", OpportunityToString(iterator->Next()));
}

// Verifies that Layout Opportunity iterator produces 7 layout opportunities
// from 4 start points created by 2 CSS exclusions positioned in the middle of
// the main constraint space.
//
// Test case visual representation:
//
//         100  200   300  400  500
//     (1)--|----|-(2)-|----|----|-(3)-+
//  50 |                               |
// 100 |                               |
// 150 |                               |
// 200 |       ******                  |
// 250 |       ******                  |
// 300 (4)                             |
// 350 |                         ***   |
//     +-------------------------------+
//
// Expected:
//   Layout opportunity iterator generates the next opportunities:
//   - 1st Start Point: 0,0 600x200; 0,0 150x400
//   - 2nd Start Point: 250,0 350x350; 250,0 250x400
//   - 3rd Start Point: 550,0 50x400
//   - 4th Start Point: 0,300 600x50; 0,300 500x100
TEST(NGConstraintSpaceTest, LayoutOpportunitiesTwoInMiddle) {
  NGPhysicalSize physical_size;
  physical_size.width = LayoutUnit(600);
  physical_size.height = LayoutUnit(400);

  auto* space =
      ConstructConstraintSpace(HorizontalTopBottom, LTR, physical_size);
  // Add exclusions
  NGExclusion exclusion1;
  exclusion1.rect.size = {/* inline_size */ LayoutUnit(100),
                          /* block_size */ LayoutUnit(100)};
  exclusion1.rect.offset = {/* inline_offset */ LayoutUnit(150),
                            /* block_offset */ LayoutUnit(200)};
  space->AddExclusion(exclusion1);
  NGExclusion exclusion2;
  exclusion2.rect.size = {/* inline_size */ LayoutUnit(50),
                          /* block_size */ LayoutUnit(50)};
  exclusion2.rect.offset = {/* inline_offset */ LayoutUnit(500),
                            /* block_offset */ LayoutUnit(350)};
  space->AddExclusion(exclusion2);

  auto* iterator = space->LayoutOpportunities();

  // 1st Start point
  EXPECT_EQ("0,0 600x200", OpportunityToString(iterator->Next()));
  EXPECT_EQ("0,0 150x400", OpportunityToString(iterator->Next()));

  // 2nd Start point
  EXPECT_EQ("250,0 350x350", OpportunityToString(iterator->Next()));
  EXPECT_EQ("250,0 250x400", OpportunityToString(iterator->Next()));

  // 3rd Start point
  EXPECT_EQ("550,0 50x400", OpportunityToString(iterator->Next()));

  // 4th Start point
  EXPECT_EQ("0,300 600x50", OpportunityToString(iterator->Next()));
  EXPECT_EQ("0,300 500x100", OpportunityToString(iterator->Next()));

  // Iterator is exhausted.
  EXPECT_EQ("(empty)", OpportunityToString(iterator->Next()));
}

// This test is the same as LayoutOpportunitiesTwoInMiddle with the only
// difference that NGLayoutOpportunityIterator takes 2 additional arguments:
// - origin_point that changes the iterator to return Layout Opportunities that
// lay after the origin point.
// - leader_point that together with origin_point creates a temporary exclusion
//
// Expected:
//   Layout opportunity iterator generates the next opportunities:
//   - 1st Start Point (0, 200): 350x150, 250x200
//   - 3rd Start Point (550, 200): 50x200
//   - 4th Start Point (0, 300): 600x50, 500x100
//   All other opportunities that are located before the origin point should be
//   filtered out.
TEST(NGConstraintSpaceTest, LayoutOpportunitiesTwoInMiddleWithOriginAndLeader) {
  NGPhysicalSize physical_size;
  physical_size.width = LayoutUnit(600);
  physical_size.height = LayoutUnit(400);

  auto* space =
      ConstructConstraintSpace(HorizontalTopBottom, LTR, physical_size);
  // Add exclusions
  NGExclusion exclusion1;
  exclusion1.rect.size = {/* inline_size */ LayoutUnit(100),
                          /* block_size */ LayoutUnit(100)};
  exclusion1.rect.offset = {/* inline_offset */ LayoutUnit(150),
                            /* block_offset */ LayoutUnit(200)};
  space->AddExclusion(exclusion1);
  NGExclusion exclusion2;
  exclusion2.rect.size = {/* inline_size */ LayoutUnit(50),
                          /* block_size */ LayoutUnit(50)};
  exclusion2.rect.offset = {/* inline_offset */ LayoutUnit(500),
                            /* block_offset */ LayoutUnit(350)};
  space->AddExclusion(exclusion2);

  const NGLogicalOffset origin_point = {LayoutUnit(0), LayoutUnit(200)};
  const NGLogicalOffset leader_point = {LayoutUnit(250), LayoutUnit(300)};
  auto* iterator =
      new NGLayoutOpportunityIterator(space, origin_point, leader_point);

  // 1st Start Point
  EXPECT_EQ("250,200 350x150", OpportunityToString(iterator->Next()));
  EXPECT_EQ("250,200 250x200", OpportunityToString(iterator->Next()));

  // 2nd Start Point
  EXPECT_EQ("550,200 50x200", OpportunityToString(iterator->Next()));

  // 3rd Start Point
  EXPECT_EQ("0,300 600x50", OpportunityToString(iterator->Next()));
  EXPECT_EQ("0,300 500x100", OpportunityToString(iterator->Next()));

  // Iterator is exhausted.
  EXPECT_EQ("(empty)", OpportunityToString(iterator->Next()));
}

// Verifies that Layout Opportunity iterator ignores the exclusion that is not
// within constraint space.
//
// Test case visual representation:
//
//         100  200  300  400  500
//     +----|----|----|----|----|----+
//  50 |                             |
// 100 |                             |
//     +-----------------------------+
//      ***  <- Exclusion
//
// Expected:
//   Layout opportunity iterator generates only one opportunity that equals to
//   available constraint space, i.e. 0,0 600x200
TEST(NGConstraintSpaceTest, LayoutOpportunitiesWithOutOfBoundsExclusions) {
  NGPhysicalSize physical_size;
  physical_size.width = LayoutUnit(600);
  physical_size.height = LayoutUnit(100);

  auto* space =
      ConstructConstraintSpace(HorizontalTopBottom, LTR, physical_size);
  NGExclusion exclusion;
  exclusion.rect.size = {/* inline_size */ LayoutUnit(100),
                         /* block_size */ LayoutUnit(100)};
  exclusion.rect.offset = {/* inline_offset */ LayoutUnit(0),
                           /* block_offset */ LayoutUnit(150)};
  space->AddExclusion(exclusion);

  auto* iterator = space->LayoutOpportunities();

  EXPECT_EQ("0,0 600x100", OpportunityToString(iterator->Next()));
  EXPECT_EQ("(empty)", OpportunityToString(iterator->Next()));
}

}  // namespace
}  // namespace blink
