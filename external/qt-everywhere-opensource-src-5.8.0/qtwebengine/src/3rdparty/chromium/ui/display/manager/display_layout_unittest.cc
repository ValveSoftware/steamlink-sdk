// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/display.h"

#include <tuple>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/manager/display_layout.h"
#include "ui/display/manager/display_layout_builder.h"

namespace display {

TEST(DisplayLayoutTest, Empty) {
  DisplayList display_list;
  std::vector<int64_t> updated_ids;

  DisplayLayout display_layout;
  display_layout.ApplyToDisplayList(&display_list, &updated_ids, 0);

  EXPECT_EQ(0u, updated_ids.size());
  EXPECT_EQ(0u, display_list.size());
}

TEST(DisplayLayoutTest, SingleDisplayNoPlacements) {
  DisplayList display_list;
  display_list.emplace_back(0, gfx::Rect(0, 0, 800, 600));
  std::vector<int64_t> updated_ids;

  DisplayLayout display_layout;
  display_layout.ApplyToDisplayList(&display_list, &updated_ids, 0);

  EXPECT_EQ(0u, updated_ids.size());
  ASSERT_EQ(1u, display_list.size());
  EXPECT_EQ(gfx::Rect(0, 0, 800, 600), display_list[0].bounds());
}

TEST(DisplayLayoutTest, SingleDisplayNonRelevantPlacement) {
  DisplayList display_list;
  display_list.emplace_back(0, gfx::Rect(0, 0, 800, 600));
  std::vector<int64_t> updated_ids;

  DisplayLayoutBuilder builder(20);
  builder.AddDisplayPlacement(20, 40, DisplayPlacement::Position::LEFT, 150);
  std::unique_ptr<DisplayLayout> display_layout(builder.Build());
  display_layout->ApplyToDisplayList(&display_list, &updated_ids, 0);

  EXPECT_EQ(0u, updated_ids.size());
  ASSERT_EQ(1u, display_list.size());
  EXPECT_EQ(gfx::Rect(0, 0, 800, 600), display_list[0].bounds());
}

namespace {

class TwoDisplays
    : public testing::TestWithParam<std::tuple<
          // Primary Display Bounds
          gfx::Rect,
          // Secondary Display Bounds
          gfx::Rect,
          // Secondary Layout Position
          DisplayPlacement::Position,
          // Secondary Layout Offset
          int,
          // Minimum Offset Overlap
          int,
          // Expected Primary Display Bounds
          gfx::Rect,
          // Expected Secondary Display Bounds
          gfx::Rect>> {
 public:
  TwoDisplays() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoDisplays);
};

}  // namespace

TEST_P(TwoDisplays, Placement) {
  gfx::Rect primary_display_bounds = std::get<0>(GetParam());
  gfx::Rect secondary_display_bounds = std::get<1>(GetParam());
  DisplayPlacement::Position position = std::get<2>(GetParam());
  int offset = std::get<3>(GetParam());
  int minimum_offset_overlap = std::get<4>(GetParam());
  gfx::Rect expected_primary_display_bounds = std::get<5>(GetParam());
  gfx::Rect expected_secondary_display_bounds = std::get<6>(GetParam());

  DisplayList display_list;
  display_list.emplace_back(0, primary_display_bounds);
  display_list.emplace_back(1, secondary_display_bounds);
  std::vector<int64_t> updated_ids;

  DisplayLayoutBuilder builder(0);
  builder.AddDisplayPlacement(1, 0, position, offset);
  std::unique_ptr<DisplayLayout> display_layout(builder.Build());
  display_layout->ApplyToDisplayList(
      &display_list, &updated_ids, minimum_offset_overlap);

  ASSERT_EQ(1u, updated_ids.size());
  EXPECT_EQ(1u, updated_ids[0]);
  ASSERT_EQ(2u, display_list.size());
  EXPECT_EQ(expected_primary_display_bounds, display_list[0].bounds());
  EXPECT_EQ(expected_secondary_display_bounds, display_list[1].bounds());
}

INSTANTIATE_TEST_CASE_P(DisplayLayoutTestZero, TwoDisplays, testing::Values(
    std::make_tuple(
        gfx::Rect(0, 0, 800, 600), gfx::Rect(0, 0, 1024, 768),
        DisplayPlacement::Position::LEFT, 0, 0,
        gfx::Rect(0, 0, 800, 600), gfx::Rect(-1024, 0, 1024, 768)),
    std::make_tuple(
        gfx::Rect(0, 0, 800, 600), gfx::Rect(0, 0, 1024, 768),
        DisplayPlacement::Position::TOP, 0, 0,
        gfx::Rect(0, 0, 800, 600), gfx::Rect(0, -768, 1024, 768)),
    std::make_tuple(
        gfx::Rect(0, 0, 800, 600), gfx::Rect(0, 0, 1024, 768),
        DisplayPlacement::Position::RIGHT, 0, 0,
        gfx::Rect(0, 0, 800, 600), gfx::Rect(800, 0, 1024, 768)),
    std::make_tuple(
        gfx::Rect(0, 0, 800, 600), gfx::Rect(0, 0, 1024, 768),
        DisplayPlacement::Position::BOTTOM, 0, 0,
        gfx::Rect(0, 0, 800, 600), gfx::Rect(0, 600, 1024, 768))));

INSTANTIATE_TEST_CASE_P(DisplayLayoutTestOffset, TwoDisplays, testing::Values(
    std::make_tuple(
        gfx::Rect(0, 0, 800, 600), gfx::Rect(0, 0, 1024, 768),
        DisplayPlacement::Position::LEFT, 37, 0,
        gfx::Rect(0, 0, 800, 600), gfx::Rect(-1024, 37, 1024, 768)),
    std::make_tuple(
        gfx::Rect(0, 0, 800, 600), gfx::Rect(0, 0, 1024, 768),
        DisplayPlacement::Position::TOP, 37, 0,
        gfx::Rect(0, 0, 800, 600), gfx::Rect(37, -768, 1024, 768)),
    std::make_tuple(
        gfx::Rect(0, 0, 800, 600), gfx::Rect(0, 0, 1024, 768),
        DisplayPlacement::Position::RIGHT, 37, 0,
        gfx::Rect(0, 0, 800, 600), gfx::Rect(800, 37, 1024, 768)),
    std::make_tuple(
        gfx::Rect(0, 0, 800, 600), gfx::Rect(0, 0, 1024, 768),
        DisplayPlacement::Position::BOTTOM, 37, 0,
        gfx::Rect(0, 0, 800, 600), gfx::Rect(37, 600, 1024, 768))));

INSTANTIATE_TEST_CASE_P(DisplayLayoutTestCorner, TwoDisplays, testing::Values(
    // Top-Left
    std::make_tuple(
        gfx::Rect(0, 0, 20, 40), gfx::Rect(0, 0, 30, 60),
        DisplayPlacement::Position::LEFT, -60, 0,
        gfx::Rect(0, 0, 20, 40), gfx::Rect(-30, -60, 30, 60)),
    std::make_tuple(
        gfx::Rect(0, 0, 20, 40), gfx::Rect(0, 0, 30, 60),
        DisplayPlacement::Position::TOP, -30, 0,
        gfx::Rect(0, 0, 20, 40), gfx::Rect(-30, -60, 30, 60)),
    // Top-Right
    std::make_tuple(
        gfx::Rect(0, 0, 20, 40), gfx::Rect(0, 0, 30, 60),
        DisplayPlacement::Position::RIGHT, -60, 0,
        gfx::Rect(0, 0, 20, 40), gfx::Rect(20, -60, 30, 60)),
    std::make_tuple(
        gfx::Rect(0, 0, 20, 40), gfx::Rect(0, 0, 30, 60),
        DisplayPlacement::Position::TOP, 20, 0,
        gfx::Rect(0, 0, 20, 40), gfx::Rect(20, -60, 30, 60)),
    // Bottom-Right
    std::make_tuple(
        gfx::Rect(0, 0, 20, 40), gfx::Rect(0, 0, 30, 60),
        DisplayPlacement::Position::RIGHT, 40, 0,
        gfx::Rect(0, 0, 20, 40), gfx::Rect(20, 40, 30, 60)),
    std::make_tuple(
        gfx::Rect(0, 0, 20, 40), gfx::Rect(0, 0, 30, 60),
        DisplayPlacement::Position::BOTTOM, 20, 0,
        gfx::Rect(0, 0, 20, 40), gfx::Rect(20, 40, 30, 60)),
    // Bottom-Left
    std::make_tuple(
        gfx::Rect(0, 0, 20, 40), gfx::Rect(0, 0, 30, 60),
        DisplayPlacement::Position::LEFT, 40, 0,
        gfx::Rect(0, 0, 20, 40), gfx::Rect(-30, 40, 30, 60)),
    std::make_tuple(
        gfx::Rect(0, 0, 20, 40), gfx::Rect(0, 0, 30, 60),
        DisplayPlacement::Position::BOTTOM, -30, 0,
        gfx::Rect(0, 0, 20, 40), gfx::Rect(-30, 40, 30, 60))));

INSTANTIATE_TEST_CASE_P(DisplayLayoutTestZeroMinimumOverlap, TwoDisplays,
    testing::Values(
        std::make_tuple(
            gfx::Rect(0, 0, 800, 600), gfx::Rect(0, 0, 1024, 768),
            DisplayPlacement::Position::LEFT, 0, 14,
            gfx::Rect(0, 0, 800, 600), gfx::Rect(-1024, 0, 1024, 768)),
        std::make_tuple(
            gfx::Rect(0, 0, 800, 600), gfx::Rect(0, 0, 1024, 768),
            DisplayPlacement::Position::TOP, 0, 14,
            gfx::Rect(0, 0, 800, 600), gfx::Rect(0, -768, 1024, 768)),
        std::make_tuple(
            gfx::Rect(0, 0, 800, 600), gfx::Rect(0, 0, 1024, 768),
            DisplayPlacement::Position::RIGHT, 0, 14,
            gfx::Rect(0, 0, 800, 600), gfx::Rect(800, 0, 1024, 768)),
        std::make_tuple(
            gfx::Rect(0, 0, 800, 600), gfx::Rect(0, 0, 1024, 768),
            DisplayPlacement::Position::BOTTOM, 0, 14,
            gfx::Rect(0, 0, 800, 600), gfx::Rect(0, 600, 1024, 768))));

INSTANTIATE_TEST_CASE_P(DisplayLayoutTestOffsetMinimumOverlap, TwoDisplays,
    testing::Values(
        std::make_tuple(
            gfx::Rect(0, 0, 800, 600), gfx::Rect(0, 0, 1024, 768),
            DisplayPlacement::Position::LEFT, 37, 14,
            gfx::Rect(0, 0, 800, 600), gfx::Rect(-1024, 37, 1024, 768)),
        std::make_tuple(
            gfx::Rect(0, 0, 800, 600), gfx::Rect(0, 0, 1024, 768),
            DisplayPlacement::Position::TOP, 37, 14,
            gfx::Rect(0, 0, 800, 600), gfx::Rect(37, -768, 1024, 768)),
        std::make_tuple(
            gfx::Rect(0, 0, 800, 600), gfx::Rect(0, 0, 1024, 768),
            DisplayPlacement::Position::RIGHT, 37, 14,
            gfx::Rect(0, 0, 800, 600), gfx::Rect(800, 37, 1024, 768)),
        std::make_tuple(
            gfx::Rect(0, 0, 800, 600), gfx::Rect(0, 0, 1024, 768),
            DisplayPlacement::Position::BOTTOM, 37, 14,
            gfx::Rect(0, 0, 800, 600), gfx::Rect(37, 600, 1024, 768))));

INSTANTIATE_TEST_CASE_P(DisplayLayoutTestMinimumOverlap, TwoDisplays,
    testing::Values(
        // Top-Left
        std::make_tuple(
            gfx::Rect(0, 0, 20, 40), gfx::Rect(0, 0, 30, 60),
            DisplayPlacement::Position::LEFT, -60, 14,
            gfx::Rect(0, 0, 20, 40), gfx::Rect(-30, -46, 30, 60)),
        std::make_tuple(
            gfx::Rect(0, 0, 20, 40), gfx::Rect(0, 0, 30, 60),
            DisplayPlacement::Position::TOP, -30, 14,
            gfx::Rect(0, 0, 20, 40), gfx::Rect(-16, -60, 30, 60)),
        // Top-Right
        std::make_tuple(
            gfx::Rect(0, 0, 20, 40), gfx::Rect(0, 0, 30, 60),
            DisplayPlacement::Position::RIGHT, -60, 14,
            gfx::Rect(0, 0, 20, 40), gfx::Rect(20, -46, 30, 60)),
        std::make_tuple(
            gfx::Rect(0, 0, 20, 40), gfx::Rect(0, 0, 30, 60),
            DisplayPlacement::Position::TOP, 20, 14,
            gfx::Rect(0, 0, 20, 40), gfx::Rect(6, -60, 30, 60)),
        // Bottom-Right
        std::make_tuple(
            gfx::Rect(0, 0, 20, 40), gfx::Rect(0, 0, 30, 60),
            DisplayPlacement::Position::RIGHT, 40, 14,
            gfx::Rect(0, 0, 20, 40), gfx::Rect(20, 26, 30, 60)),
        std::make_tuple(
            gfx::Rect(0, 0, 20, 40), gfx::Rect(0, 0, 30, 60),
            DisplayPlacement::Position::BOTTOM, 20, 14,
            gfx::Rect(0, 0, 20, 40), gfx::Rect(6, 40, 30, 60)),
        // Bottom-Left
        std::make_tuple(
            gfx::Rect(0, 0, 20, 40), gfx::Rect(0, 0, 30, 60),
            DisplayPlacement::Position::LEFT, 40, 14,
            gfx::Rect(0, 0, 20, 40), gfx::Rect(-30, 26, 30, 60)),
        std::make_tuple(
            gfx::Rect(0, 0, 20, 40), gfx::Rect(0, 0, 30, 60),
            DisplayPlacement::Position::BOTTOM, -30, 14,
            gfx::Rect(0, 0, 20, 40), gfx::Rect(-16, 40, 30, 60))));

// Display Layout
//     [1]  [4]
//    [0][3]   [6]
// [2]  [5]
TEST(DisplayLayoutTest, MultipleDisplays) {
  DisplayList display_list;
  display_list.emplace_back(0, gfx::Rect(0, 0, 100, 100));
  display_list.emplace_back(1, gfx::Rect(0, 0, 100, 100));
  display_list.emplace_back(2, gfx::Rect(0, 0, 100, 100));
  display_list.emplace_back(3, gfx::Rect(0, 0, 100, 100));
  display_list.emplace_back(4, gfx::Rect(0, 0, 100, 100));
  display_list.emplace_back(5, gfx::Rect(0, 0, 100, 100));
  display_list.emplace_back(6, gfx::Rect(0, 0, 100, 100));
  std::vector<int64_t> updated_ids;

  DisplayLayoutBuilder builder(0);
  builder.AddDisplayPlacement(1, 0, DisplayPlacement::Position::TOP, 50);
  builder.AddDisplayPlacement(2, 0, DisplayPlacement::Position::LEFT, 100);
  builder.AddDisplayPlacement(3, 0, DisplayPlacement::Position::RIGHT, 0);
  builder.AddDisplayPlacement(4, 3, DisplayPlacement::Position::RIGHT, -100);
  builder.AddDisplayPlacement(5, 3, DisplayPlacement::Position::BOTTOM, -50);
  builder.AddDisplayPlacement(6, 4, DisplayPlacement::Position::BOTTOM, 100);
  std::unique_ptr<DisplayLayout> display_layout(builder.Build());
  display_layout->ApplyToDisplayList(&display_list, &updated_ids, 0);

  ASSERT_EQ(6u, updated_ids.size());
  std::sort(updated_ids.begin(), updated_ids.end());
  EXPECT_EQ(1u, updated_ids[0]);
  EXPECT_EQ(2u, updated_ids[1]);
  EXPECT_EQ(3u, updated_ids[2]);
  EXPECT_EQ(4u, updated_ids[3]);
  EXPECT_EQ(5u, updated_ids[4]);
  EXPECT_EQ(6u, updated_ids[5]);
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), display_list[0].bounds());
  EXPECT_EQ(gfx::Rect(50, -100, 100, 100), display_list[1].bounds());
  EXPECT_EQ(gfx::Rect(-100, 100, 100, 100), display_list[2].bounds());
  EXPECT_EQ(gfx::Rect(100, 0, 100, 100), display_list[3].bounds());
  EXPECT_EQ(gfx::Rect(200, -100, 100, 100), display_list[4].bounds());
  EXPECT_EQ(gfx::Rect(50, 100, 100, 100), display_list[5].bounds());
  EXPECT_EQ(gfx::Rect(300, 0, 100, 100), display_list[6].bounds());
}

namespace {

class TwoDisplaysBottomRightReference
  : public testing::TestWithParam<std::tuple<
        // Primary Display Bounds
        gfx::Rect,
        // Secondary Display Bounds
        gfx::Rect,
        // Secondary Layout Position
        DisplayPlacement::Position,
        // Secondary Layout Offset
        int,
        // Minimum Offset Overlap
        int,
        // Expected Primary Display Bounds
        gfx::Rect,
        // Expected Secondary Display Bounds
        gfx::Rect>> {
 public:
  TwoDisplaysBottomRightReference() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoDisplaysBottomRightReference);
};

}  // namespace

TEST_P(TwoDisplaysBottomRightReference, Placement) {
  gfx::Rect primary_display_bounds = std::get<0>(GetParam());
  gfx::Rect secondary_display_bounds = std::get<1>(GetParam());
  DisplayPlacement::Position position = std::get<2>(GetParam());
  int offset = std::get<3>(GetParam());
  int minimum_offset_overlap = std::get<4>(GetParam());
  gfx::Rect expected_primary_display_bounds = std::get<5>(GetParam());
  gfx::Rect expected_secondary_display_bounds = std::get<6>(GetParam());

  DisplayList display_list;
  display_list.emplace_back(0, primary_display_bounds);
  display_list.emplace_back(1, secondary_display_bounds);
  std::vector<int64_t> updated_ids;

  DisplayLayoutBuilder builder(0);
  DisplayPlacement placement;
  placement.display_id = 1;
  placement.parent_display_id = 0;
  placement.position = position;
  placement.offset = offset;
  placement.offset_reference = DisplayPlacement::OffsetReference::BOTTOM_RIGHT;
  builder.AddDisplayPlacement(placement);
  std::unique_ptr<DisplayLayout> display_layout(builder.Build());
  display_layout->ApplyToDisplayList(
      &display_list, &updated_ids, minimum_offset_overlap);

  ASSERT_EQ(1u, updated_ids.size());
  EXPECT_EQ(1u, updated_ids[0]);
  ASSERT_EQ(2u, display_list.size());
  EXPECT_EQ(expected_primary_display_bounds, display_list[0].bounds());
  EXPECT_EQ(expected_secondary_display_bounds, display_list[1].bounds());
}

INSTANTIATE_TEST_CASE_P(DisplayLayoutTestZero, TwoDisplaysBottomRightReference,
    testing::Values(
        std::make_tuple(
            gfx::Rect(0, 0, 800, 600), gfx::Rect(0, 0, 1024, 768),
            DisplayPlacement::Position::LEFT, 0, 0,
            gfx::Rect(0, 0, 800, 600), gfx::Rect(-1024, -168, 1024, 768)),
        std::make_tuple(
            gfx::Rect(0, 0, 800, 600), gfx::Rect(0, 0, 1024, 768),
            DisplayPlacement::Position::TOP, 0, 0,
            gfx::Rect(0, 0, 800, 600), gfx::Rect(-224, -768, 1024, 768)),
        std::make_tuple(
            gfx::Rect(0, 0, 800, 600), gfx::Rect(0, 0, 1024, 768),
            DisplayPlacement::Position::RIGHT, 0, 0,
            gfx::Rect(0, 0, 800, 600), gfx::Rect(800, -168, 1024, 768)),
        std::make_tuple(
            gfx::Rect(0, 0, 800, 600), gfx::Rect(0, 0, 1024, 768),
            DisplayPlacement::Position::BOTTOM, 0, 0,
            gfx::Rect(0, 0, 800, 600), gfx::Rect(-224, 600, 1024, 768))));

INSTANTIATE_TEST_CASE_P(DisplayLayoutTestOffset,
    TwoDisplaysBottomRightReference, testing::Values(
        std::make_tuple(
            gfx::Rect(0, 0, 800, 600), gfx::Rect(0, 0, 1024, 768),
            DisplayPlacement::Position::LEFT, 7, 0,
            gfx::Rect(0, 0, 800, 600), gfx::Rect(-1024, -175, 1024, 768)),
        std::make_tuple(
            gfx::Rect(0, 0, 800, 600), gfx::Rect(0, 0, 1024, 768),
            DisplayPlacement::Position::TOP, 7, 0,
            gfx::Rect(0, 0, 800, 600), gfx::Rect(-231, -768, 1024, 768)),
        std::make_tuple(
            gfx::Rect(0, 0, 800, 600), gfx::Rect(0, 0, 1024, 768),
            DisplayPlacement::Position::RIGHT, 7, 0,
            gfx::Rect(0, 0, 800, 600), gfx::Rect(800, -175, 1024, 768)),
        std::make_tuple(
            gfx::Rect(0, 0, 800, 600), gfx::Rect(0, 0, 1024, 768),
            DisplayPlacement::Position::BOTTOM, 7, 0,
            gfx::Rect(0, 0, 800, 600), gfx::Rect(-231, 600, 1024, 768))));

}  // namespace display
