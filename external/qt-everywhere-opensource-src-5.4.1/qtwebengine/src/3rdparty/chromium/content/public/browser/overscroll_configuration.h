// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_OVERSCROLL_CONFIGURATION_H_
#define CONTENT_PUBLIC_BROWSER_OVERSCROLL_CONFIGURATION_H_

#include "base/basictypes.h"
#include "content/common/content_export.h"

namespace content {

// Sets and retrieves various overscroll related configuration values.
enum OverscrollConfig {
  OVERSCROLL_CONFIG_NONE,
  OVERSCROLL_CONFIG_HORIZ_THRESHOLD_COMPLETE,
  OVERSCROLL_CONFIG_VERT_THRESHOLD_COMPLETE,
  OVERSCROLL_CONFIG_HORIZ_THRESHOLD_START_TOUCHPAD,
  OVERSCROLL_CONFIG_HORIZ_THRESHOLD_START_TOUCHSCREEN,
  OVERSCROLL_CONFIG_VERT_THRESHOLD_START,
  OVERSCROLL_CONFIG_HORIZ_RESIST_AFTER,
  OVERSCROLL_CONFIG_VERT_RESIST_AFTER,
  OVERSCROLL_CONFIG_COUNT
};

CONTENT_EXPORT void SetOverscrollConfig(OverscrollConfig config, float value);

CONTENT_EXPORT float GetOverscrollConfig(OverscrollConfig config);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_OVERSCROLL_CONFIGURATION_H_
