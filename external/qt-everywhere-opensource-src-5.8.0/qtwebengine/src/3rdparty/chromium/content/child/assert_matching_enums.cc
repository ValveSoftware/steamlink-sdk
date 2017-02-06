// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Use this file to assert that *_list.h enums that are meant to do the bridge
// from Blink are valid.

#include "base/macros.h"
#include "content/public/common/screen_orientation_values.h"
#include "media/base/mime_util.h"
#include "third_party/WebKit/public/platform/WebMimeRegistry.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebScreenOrientationLockType.h"
#include "third_party/WebKit/public/web/WebFrameSerializerCacheControlPolicy.h"

namespace content {

#define STATIC_ASSERT_ENUM(a, b)                            \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "mismatching enums: " #a)

// ScreenOrientationValues
STATIC_ASSERT_ENUM(blink::WebScreenOrientationLockDefault,
                   SCREEN_ORIENTATION_VALUES_DEFAULT);
STATIC_ASSERT_ENUM(blink::WebScreenOrientationLockPortraitPrimary,
                   SCREEN_ORIENTATION_VALUES_PORTRAIT_PRIMARY);
STATIC_ASSERT_ENUM(blink::WebScreenOrientationLockPortraitSecondary,
                   SCREEN_ORIENTATION_VALUES_PORTRAIT_SECONDARY);
STATIC_ASSERT_ENUM(blink::WebScreenOrientationLockLandscapePrimary,
                   SCREEN_ORIENTATION_VALUES_LANDSCAPE_PRIMARY);
STATIC_ASSERT_ENUM(blink::WebScreenOrientationLockLandscapeSecondary,
                   SCREEN_ORIENTATION_VALUES_LANDSCAPE_SECONDARY);
STATIC_ASSERT_ENUM(blink::WebScreenOrientationLockAny,
                   SCREEN_ORIENTATION_VALUES_ANY);
STATIC_ASSERT_ENUM(blink::WebScreenOrientationLockLandscape,
                   SCREEN_ORIENTATION_VALUES_LANDSCAPE);
STATIC_ASSERT_ENUM(blink::WebScreenOrientationLockPortrait,
                   SCREEN_ORIENTATION_VALUES_PORTRAIT);
STATIC_ASSERT_ENUM(blink::WebScreenOrientationLockNatural,
                   SCREEN_ORIENTATION_VALUES_NATURAL);

// SupportsType
STATIC_ASSERT_ENUM(blink::WebMimeRegistry::IsNotSupported,
                   media::IsNotSupported);
STATIC_ASSERT_ENUM(blink::WebMimeRegistry::IsSupported, media::IsSupported);
STATIC_ASSERT_ENUM(blink::WebMimeRegistry::MayBeSupported,
                   media::MayBeSupported);

} // namespace content
