// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/intent_helper/activity_icon_loader.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

void OnIconsReady0(
    std::unique_ptr<ActivityIconLoader::ActivityToIconsMap> activity_to_icons) {
  EXPECT_EQ(3U, activity_to_icons->size());
  EXPECT_EQ(1U, activity_to_icons->count(
                    ActivityIconLoader::ActivityName("p0", "a0")));
  EXPECT_EQ(1U, activity_to_icons->count(
                    ActivityIconLoader::ActivityName("p1", "a1")));
  EXPECT_EQ(1U, activity_to_icons->count(
                    ActivityIconLoader::ActivityName("p1", "a0")));
}

void OnIconsReady1(
    std::unique_ptr<ActivityIconLoader::ActivityToIconsMap> activity_to_icons) {
  EXPECT_EQ(1U, activity_to_icons->size());
  EXPECT_EQ(1U, activity_to_icons->count(
                    ActivityIconLoader::ActivityName("p1", "a1")));
}

void OnIconsReady2(
    std::unique_ptr<ActivityIconLoader::ActivityToIconsMap> activity_to_icons) {
  EXPECT_TRUE(activity_to_icons->empty());
}

void OnIconsReady3(
    std::unique_ptr<ActivityIconLoader::ActivityToIconsMap> activity_to_icons) {
  EXPECT_EQ(2U, activity_to_icons->size());
  EXPECT_EQ(1U, activity_to_icons->count(
                    ActivityIconLoader::ActivityName("p1", "a1")));
  EXPECT_EQ(1U, activity_to_icons->count(
                    ActivityIconLoader::ActivityName("p2", "a2")));
}

}  // namespace

// Tests if InvalidateIcons properly cleans up the cache.
TEST(ActivityIconLoaderTest, TestInvalidateIcons) {
  scoped_refptr<ActivityIconLoader> loader(new ActivityIconLoader);
  loader->AddIconToCacheForTesting(ActivityIconLoader::ActivityName("p0", "a0"),
                                   gfx::Image());
  loader->AddIconToCacheForTesting(ActivityIconLoader::ActivityName("p0", "a1"),
                                   gfx::Image());
  loader->AddIconToCacheForTesting(ActivityIconLoader::ActivityName("p1", "a0"),
                                   gfx::Image());
  EXPECT_EQ(3U, loader->cached_icons_for_testing().size());

  loader->InvalidateIcons("p2");  // delete none.
  EXPECT_EQ(3U, loader->cached_icons_for_testing().size());

  loader->InvalidateIcons("p0");  // delete 2 entries.
  EXPECT_EQ(1U, loader->cached_icons_for_testing().size());

  loader->InvalidateIcons("p2");  // delete none.
  EXPECT_EQ(1U, loader->cached_icons_for_testing().size());

  loader->InvalidateIcons("p1");  // delete 1 entry.
  EXPECT_EQ(0U, loader->cached_icons_for_testing().size());
}

// Tests if GetActivityIcons immediately returns cached icons.
TEST(ActivityIconLoaderTest, TestGetActivityIcons) {
  scoped_refptr<ActivityIconLoader> loader(new ActivityIconLoader);
  loader->AddIconToCacheForTesting(ActivityIconLoader::ActivityName("p0", "a0"),
                                   gfx::Image());
  loader->AddIconToCacheForTesting(ActivityIconLoader::ActivityName("p1", "a1"),
                                   gfx::Image());
  loader->AddIconToCacheForTesting(ActivityIconLoader::ActivityName("p1", "a0"),
                                   gfx::Image());

  // Check that GetActivityIcons() immediately calls OnIconsReady0() with all
  // locally cached icons.
  std::vector<ActivityIconLoader::ActivityName> activities;
  activities.emplace_back("p0", "a0");
  activities.emplace_back("p1", "a1");
  activities.emplace_back("p1", "a0");
  EXPECT_EQ(ActivityIconLoader::GetResult::SUCCEEDED_SYNC,
            loader->GetActivityIcons(activities, base::Bind(&OnIconsReady0)));

  // Test with different |activities|.
  activities.clear();
  activities.emplace_back("p1", "a1");
  EXPECT_EQ(ActivityIconLoader::GetResult::SUCCEEDED_SYNC,
            loader->GetActivityIcons(activities, base::Bind(&OnIconsReady1)));
  activities.clear();
  EXPECT_EQ(ActivityIconLoader::GetResult::SUCCEEDED_SYNC,
            loader->GetActivityIcons(activities, base::Bind(&OnIconsReady2)));
  activities.emplace_back("p1", "a_unknown");
  EXPECT_EQ(ActivityIconLoader::GetResult::FAILED_ARC_NOT_SUPPORTED,
            loader->GetActivityIcons(activities, base::Bind(&OnIconsReady2)));
}

// Tests if OnIconsResized updates the cache.
TEST(ActivityIconLoaderTest, TestOnIconsResized) {
  scoped_refptr<ActivityIconLoader> loader(new ActivityIconLoader);
  std::unique_ptr<ActivityIconLoader::ActivityToIconsMap> activity_to_icons(
      new ActivityIconLoader::ActivityToIconsMap);
  activity_to_icons->insert(
      std::make_pair(ActivityIconLoader::ActivityName("p0", "a0"),
                     ActivityIconLoader::Icons(gfx::Image(), gfx::Image())));
  activity_to_icons->insert(
      std::make_pair(ActivityIconLoader::ActivityName("p1", "a1"),
                     ActivityIconLoader::Icons(gfx::Image(), gfx::Image())));
  activity_to_icons->insert(
      std::make_pair(ActivityIconLoader::ActivityName("p1", "a0"),
                     ActivityIconLoader::Icons(gfx::Image(), gfx::Image())));
  // Duplicated entey which should be ignored.
  activity_to_icons->insert(
      std::make_pair(ActivityIconLoader::ActivityName("p0", "a0"),
                     ActivityIconLoader::Icons(gfx::Image(), gfx::Image())));

  // Call OnIconsResized() and check that the cache is properly updated.
  loader->OnIconsResizedForTesting(base::Bind(&OnIconsReady0),
                                   std::move(activity_to_icons));
  EXPECT_EQ(3U, loader->cached_icons_for_testing().size());
  EXPECT_EQ(1U, loader->cached_icons_for_testing().count(
                    ActivityIconLoader::ActivityName("p0", "a0")));
  EXPECT_EQ(1U, loader->cached_icons_for_testing().count(
                    ActivityIconLoader::ActivityName("p1", "a1")));
  EXPECT_EQ(1U, loader->cached_icons_for_testing().count(
                    ActivityIconLoader::ActivityName("p1", "a0")));

  // Call OnIconsResized() again to make sure that the second call does not
  // remove the cache the previous call added.
  activity_to_icons.reset(new ActivityIconLoader::ActivityToIconsMap);
  // Duplicated entry.
  activity_to_icons->insert(
      std::make_pair(ActivityIconLoader::ActivityName("p1", "a1"),
                     ActivityIconLoader::Icons(gfx::Image(), gfx::Image())));
  // New entry.
  activity_to_icons->insert(
      std::make_pair(ActivityIconLoader::ActivityName("p2", "a2"),
                     ActivityIconLoader::Icons(gfx::Image(), gfx::Image())));
  loader->OnIconsResizedForTesting(base::Bind(&OnIconsReady3),
                                   std::move(activity_to_icons));
  EXPECT_EQ(4U, loader->cached_icons_for_testing().size());
  EXPECT_EQ(1U, loader->cached_icons_for_testing().count(
                    ActivityIconLoader::ActivityName("p0", "a0")));
  EXPECT_EQ(1U, loader->cached_icons_for_testing().count(
                    ActivityIconLoader::ActivityName("p1", "a1")));
  EXPECT_EQ(1U, loader->cached_icons_for_testing().count(
                    ActivityIconLoader::ActivityName("p1", "a0")));
  EXPECT_EQ(1U, loader->cached_icons_for_testing().count(
                    ActivityIconLoader::ActivityName("p2", "a2")));
}

}  // namespace arc
