// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_base.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebScreenOrientationType.h"
#include "ui/display/display.h"

namespace content {

namespace {

display::Display CreateDisplay(int width, int height, int angle) {
  display::Display display;
  display.SetRotationAsDegree(angle);
  display.set_bounds(gfx::Rect(width, height));

  return display;
}

} // anonymous namespace

TEST(RenderWidgetHostViewBaseTest, OrientationTypeForMobile) {
  // Square display (width == height).
  {
    display::Display display = CreateDisplay(100, 100, 0);
    EXPECT_EQ(blink::WebScreenOrientationPortraitPrimary,
              RenderWidgetHostViewBase::GetOrientationTypeForMobile(display));

    display = CreateDisplay(200, 200, 90);
    EXPECT_EQ(blink::WebScreenOrientationLandscapePrimary,
              RenderWidgetHostViewBase::GetOrientationTypeForMobile(display));

    display = CreateDisplay(0, 0, 180);
    EXPECT_EQ(blink::WebScreenOrientationPortraitSecondary,
              RenderWidgetHostViewBase::GetOrientationTypeForMobile(display));

    display = CreateDisplay(10000, 10000, 270);
    EXPECT_EQ(blink::WebScreenOrientationLandscapeSecondary,
              RenderWidgetHostViewBase::GetOrientationTypeForMobile(display));
  }

  // natural width > natural height.
  {
    display::Display display = CreateDisplay(1, 0, 0);
    EXPECT_EQ(blink::WebScreenOrientationLandscapePrimary,
              RenderWidgetHostViewBase::GetOrientationTypeForMobile(display));

    display = CreateDisplay(19999, 20000, 90);
    EXPECT_EQ(blink::WebScreenOrientationPortraitSecondary,
              RenderWidgetHostViewBase::GetOrientationTypeForMobile(display));

    display = CreateDisplay(200, 100, 180);
    EXPECT_EQ(blink::WebScreenOrientationLandscapeSecondary,
              RenderWidgetHostViewBase::GetOrientationTypeForMobile(display));

    display = CreateDisplay(1, 10000, 270);
    EXPECT_EQ(blink::WebScreenOrientationPortraitPrimary,
              RenderWidgetHostViewBase::GetOrientationTypeForMobile(display));
  }

  // natural width < natural height.
  {
    display::Display display = CreateDisplay(0, 1, 0);
    EXPECT_EQ(blink::WebScreenOrientationPortraitPrimary,
              RenderWidgetHostViewBase::GetOrientationTypeForMobile(display));

    display = CreateDisplay(20000, 19999, 90);
    EXPECT_EQ(blink::WebScreenOrientationLandscapePrimary,
              RenderWidgetHostViewBase::GetOrientationTypeForMobile(display));

    display = CreateDisplay(100, 200, 180);
    EXPECT_EQ(blink::WebScreenOrientationPortraitSecondary,
              RenderWidgetHostViewBase::GetOrientationTypeForMobile(display));

    display = CreateDisplay(10000, 1, 270);
    EXPECT_EQ(blink::WebScreenOrientationLandscapeSecondary,
              RenderWidgetHostViewBase::GetOrientationTypeForMobile(display));
  }
}

TEST(RenderWidgetHostViewBaseTest, OrientationTypeForDesktop) {
  // On Desktop, the primary orientation is the first computed one so a test
  // similar to OrientationTypeForMobile is not possible.
  // Instead this test will only check one configuration and verify that the
  // method reports two landscape and two portrait orientations with one primary
  // and one secondary for each.

  // natural width > natural height.
  {
    display::Display display = CreateDisplay(1, 0, 0);
    blink::WebScreenOrientationType landscape_1 =
        RenderWidgetHostViewBase::GetOrientationTypeForDesktop(display);
    EXPECT_TRUE(landscape_1 == blink::WebScreenOrientationLandscapePrimary ||
                landscape_1 == blink::WebScreenOrientationLandscapeSecondary);

    display = CreateDisplay(200, 100, 180);
    blink::WebScreenOrientationType landscape_2 =
        RenderWidgetHostViewBase::GetOrientationTypeForDesktop(display);
    EXPECT_TRUE(landscape_2 == blink::WebScreenOrientationLandscapePrimary ||
                landscape_2 == blink::WebScreenOrientationLandscapeSecondary);

    EXPECT_NE(landscape_1, landscape_2);

    display = CreateDisplay(19999, 20000, 90);
    blink::WebScreenOrientationType portrait_1 =
        RenderWidgetHostViewBase::GetOrientationTypeForDesktop(display);
    EXPECT_TRUE(portrait_1 == blink::WebScreenOrientationPortraitPrimary ||
                portrait_1 == blink::WebScreenOrientationPortraitSecondary);

    display = CreateDisplay(1, 10000, 270);
    blink::WebScreenOrientationType portrait_2 =
        RenderWidgetHostViewBase::GetOrientationTypeForDesktop(display);
    EXPECT_TRUE(portrait_2 == blink::WebScreenOrientationPortraitPrimary ||
                portrait_2 == blink::WebScreenOrientationPortraitSecondary);

    EXPECT_NE(portrait_1, portrait_2);

  }
}

} // namespace content
