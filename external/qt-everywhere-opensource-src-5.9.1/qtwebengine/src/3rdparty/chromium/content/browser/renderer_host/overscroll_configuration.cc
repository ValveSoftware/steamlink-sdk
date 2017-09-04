// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/overscroll_configuration.h"

#include "base/logging.h"

namespace {

float g_horiz_threshold_complete = 0.25f;
float g_vert_threshold_complete = 0.20f;

float g_horiz_threshold_start_touchscreen = 50.f;
float g_horiz_threshold_start_touchpad = 50.f;
float g_vert_threshold_start = 0.f;

float g_horiz_resist_after = 30.f;
float g_vert_resist_after = 30.f;

}

namespace content {

float GetOverscrollConfig(OverscrollConfig config) {
  switch (config) {
    case OVERSCROLL_CONFIG_HORIZ_THRESHOLD_COMPLETE:
      return g_horiz_threshold_complete;

    case OVERSCROLL_CONFIG_VERT_THRESHOLD_COMPLETE:
      return g_vert_threshold_complete;

    case OVERSCROLL_CONFIG_HORIZ_THRESHOLD_START_TOUCHSCREEN:
      return g_horiz_threshold_start_touchscreen;

    case OVERSCROLL_CONFIG_HORIZ_THRESHOLD_START_TOUCHPAD:
      return g_horiz_threshold_start_touchpad;

    case OVERSCROLL_CONFIG_VERT_THRESHOLD_START:
      return g_vert_threshold_start;

    case OVERSCROLL_CONFIG_HORIZ_RESIST_AFTER:
      return g_horiz_resist_after;

    case OVERSCROLL_CONFIG_VERT_RESIST_AFTER:
      return g_vert_resist_after;

    case OVERSCROLL_CONFIG_NONE:
    case OVERSCROLL_CONFIG_COUNT:
      NOTREACHED();
  }

  return -1.f;
}

}  // namespace content
