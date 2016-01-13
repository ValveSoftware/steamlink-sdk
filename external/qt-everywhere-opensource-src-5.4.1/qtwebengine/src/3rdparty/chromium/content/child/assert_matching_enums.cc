// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Use this file to assert that *_list.h enums that are meant to do the bridge
// from Blink are valid.

#include "base/macros.h"
#include "cc/animation/animation.h"
#include "content/public/common/screen_orientation_values.h"
#include "third_party/WebKit/public/platform/WebAnimation.h"
#include "third_party/WebKit/public/platform/WebMimeRegistry.h"
#include "third_party/WebKit/public/platform/WebScreenOrientationLockType.h"

namespace content {

#define COMPILE_ASSERT_MATCHING_ENUM(expected, actual) \
  COMPILE_ASSERT(int(expected) == int(actual), mismatching_enums)

// ScreenOrientationValues
COMPILE_ASSERT_MATCHING_ENUM(blink::WebScreenOrientationLockDefault,
    DEFAULT);
COMPILE_ASSERT_MATCHING_ENUM(blink::WebScreenOrientationLockPortraitPrimary,
    PORTRAIT_PRIMARY);
COMPILE_ASSERT_MATCHING_ENUM(blink::WebScreenOrientationLockPortraitSecondary,
    PORTRAIT_SECONDARY);
COMPILE_ASSERT_MATCHING_ENUM(blink::WebScreenOrientationLockLandscapePrimary,
    LANDSCAPE_PRIMARY);
COMPILE_ASSERT_MATCHING_ENUM(blink::WebScreenOrientationLockLandscapeSecondary,
    LANDSCAPE_SECONDARY);
COMPILE_ASSERT_MATCHING_ENUM(blink::WebScreenOrientationLockAny,
    ANY);
COMPILE_ASSERT_MATCHING_ENUM(blink::WebScreenOrientationLockLandscape,
    LANDSCAPE);
COMPILE_ASSERT_MATCHING_ENUM(blink::WebScreenOrientationLockPortrait,
    PORTRAIT);

// TargetProperty
COMPILE_ASSERT_MATCHING_ENUM(blink::WebAnimation::TargetPropertyTransform,
    cc::Animation::Transform);
COMPILE_ASSERT_MATCHING_ENUM(blink::WebAnimation::TargetPropertyOpacity,
    cc::Animation::Opacity);
COMPILE_ASSERT_MATCHING_ENUM(blink::WebAnimation::TargetPropertyFilter,
    cc::Animation::Filter);
#if WEB_SCROLL_OFFSET_ANIMATION_CURVE_IS_DEFINED
COMPILE_ASSERT_MATCHING_ENUM(blink::WebAnimation::TargetPropertyScrollOffset,
    cc::Animation::ScrollOffset);
#endif

} // namespace content
