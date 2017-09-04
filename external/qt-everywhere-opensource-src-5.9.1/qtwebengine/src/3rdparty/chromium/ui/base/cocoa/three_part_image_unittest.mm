// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cocoa/three_part_image.h"

#include <memory>

#include "testing/gtest_mac.h"
#include "ui/base/resource/resource_bundle.h"
#import "ui/gfx/test/ui_cocoa_test_helper.h"
#include "ui/resources/grit/ui_resources.h"

namespace ui {
namespace test {

TEST(ThreePartImageTest, GetRects) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  base::scoped_nsobject<NSImage> leftImage(
      rb.GetNativeImageNamed(IDR_BROWSER_ACTION_BADGE_LEFT).CopyNSImage());
  base::scoped_nsobject<NSImage> middleImage(
      rb.GetNativeImageNamed(IDR_BROWSER_ACTION_BADGE_CENTER).CopyNSImage());
  base::scoped_nsobject<NSImage> rightImage(
      rb.GetNativeImageNamed(IDR_BROWSER_ACTION_BADGE_RIGHT).CopyNSImage());
  ThreePartImage image(leftImage, middleImage, rightImage);
  NSRect bounds = NSMakeRect(0, 0, 20, 11);
  EXPECT_NSRECT_EQ(NSMakeRect(0, 0, 4, 11), image.GetLeftRect(bounds));
  EXPECT_NSRECT_EQ(NSMakeRect(4, 0, 12, 11), image.GetMiddleRect(bounds));
  EXPECT_NSRECT_EQ(NSMakeRect(16, 0, 4, 11), image.GetRightRect(bounds));
}

TEST(ThreePartImageTest, GetRectsWithoutMiddle) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  base::scoped_nsobject<NSImage> leftImage(
      rb.GetNativeImageNamed(IDR_BROWSER_ACTION_BADGE_LEFT).CopyNSImage());
  base::scoped_nsobject<NSImage> rightImage(
      rb.GetNativeImageNamed(IDR_BROWSER_ACTION_BADGE_RIGHT).CopyNSImage());
  ThreePartImage image(leftImage, nullptr, rightImage);
  NSRect bounds = NSMakeRect(0, 0, 20, 11);
  EXPECT_NSRECT_EQ(NSMakeRect(0, 0, 4, 11), image.GetLeftRect(bounds));
  EXPECT_NSRECT_EQ(NSMakeRect(4, 0, 12, 11), image.GetMiddleRect(bounds));
  EXPECT_NSRECT_EQ(NSMakeRect(16, 0, 4, 11), image.GetRightRect(bounds));
}

TEST(ThreePartImageTest, HitTest) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  base::scoped_nsobject<NSImage> leftImage(
      rb.GetNativeImageNamed(IDR_BACK_ARROW).CopyNSImage());
  base::scoped_nsobject<NSImage> rightImage(
      rb.GetNativeImageNamed(IDR_FORWARD_ARROW).CopyNSImage());
  ThreePartImage image(leftImage, nullptr, rightImage);
  NSRect bounds = NSMakeRect(0, 0, 512, 128);

  // The middle of the arrows are hits.
  EXPECT_TRUE(image.HitTest(NSMakePoint(64, 64), bounds));
  EXPECT_TRUE(image.HitTest(NSMakePoint(448, 64), bounds));

  // No middle image means the middle rect is a hit.
  EXPECT_TRUE(image.HitTest(NSMakePoint(256, 64), bounds));

  // The corners are transparent.
  EXPECT_FALSE(image.HitTest(NSMakePoint(0, 0), bounds));
  EXPECT_FALSE(image.HitTest(NSMakePoint(0, 127), bounds));
  EXPECT_FALSE(image.HitTest(NSMakePoint(511, 0), bounds));
  EXPECT_FALSE(image.HitTest(NSMakePoint(511, 127), bounds));
}

}  // namespace test
}  // namespace ui
