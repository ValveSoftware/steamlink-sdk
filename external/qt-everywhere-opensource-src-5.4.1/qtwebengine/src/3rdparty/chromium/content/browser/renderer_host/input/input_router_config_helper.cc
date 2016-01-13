// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/input_router_config_helper.h"

#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "ui/events/gesture_detection/gesture_detector.h"

#if defined(USE_AURA)
#include "ui/events/gestures/gesture_configuration.h"
#include "ui/events/gestures/unified_gesture_detector_enabled.h"
#elif defined(OS_ANDROID)
#include "ui/gfx/android/view_configuration.h"
#include "ui/gfx/screen.h"
#endif

namespace content {
namespace {

#if defined(USE_AURA)

GestureEventQueue::Config GetGestureEventQueueConfig() {
  GestureEventQueue::Config config;

  config.debounce_interval = base::TimeDelta::FromMilliseconds(
      ui::GestureConfiguration::scroll_debounce_interval_in_ms());

  config.touchscreen_tap_suppression_config.enabled = true;
  config.touchscreen_tap_suppression_config.max_cancel_to_down_time =
      base::TimeDelta::FromMilliseconds(
          ui::GestureConfiguration::fling_max_cancel_to_down_time_in_ms());

  config.touchscreen_tap_suppression_config.max_tap_gap_time =
      base::TimeDelta::FromMilliseconds(static_cast<int>(
          ui::GestureConfiguration::semi_long_press_time_in_seconds() * 1000));

  config.touchpad_tap_suppression_config.enabled = true;
  config.touchpad_tap_suppression_config.max_cancel_to_down_time =
      base::TimeDelta::FromMilliseconds(
          ui::GestureConfiguration::fling_max_cancel_to_down_time_in_ms());

  config.touchpad_tap_suppression_config.max_tap_gap_time =
      base::TimeDelta::FromMilliseconds(static_cast<int>(
          ui::GestureConfiguration::fling_max_tap_gap_time_in_ms() * 1000));

  return config;
}

TouchEventQueue::Config GetTouchEventQueueConfig() {
  TouchEventQueue::Config config;

  config.touchmove_slop_suppression_length_dips =
      ui::GestureConfiguration::max_touch_move_in_pixels_for_click();

  config.touchmove_slop_suppression_region_includes_boundary =
      ui::IsUnifiedGestureDetectorEnabled();

  return config;
}

#elif defined(OS_ANDROID)

// Default time allowance for the touch ack delay before the touch sequence is
// cancelled.
const int kTouchAckTimeoutDelayMs = 200;

GestureEventQueue::Config GetGestureEventQueueConfig() {
  GestureEventQueue::Config config;

  config.touchscreen_tap_suppression_config.enabled = true;
  config.touchscreen_tap_suppression_config.max_cancel_to_down_time =
      base::TimeDelta::FromMilliseconds(
          gfx::ViewConfiguration::GetTapTimeoutInMs());
  config.touchscreen_tap_suppression_config.max_tap_gap_time =
      base::TimeDelta::FromMilliseconds(
          gfx::ViewConfiguration::GetLongPressTimeoutInMs());

  return config;
}

TouchEventQueue::Config GetTouchEventQueueConfig() {
  TouchEventQueue::Config config;

  config.touch_ack_timeout_delay =
      base::TimeDelta::FromMilliseconds(kTouchAckTimeoutDelayMs);
  config.touch_ack_timeout_supported = true;

  const double touch_slop_length_pixels =
      static_cast<double>(gfx::ViewConfiguration::GetTouchSlopInPixels());
  const double device_scale_factor =
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().device_scale_factor();
  config.touchmove_slop_suppression_length_dips =
      touch_slop_length_pixels / device_scale_factor;

  return config;
}

#else

GestureEventQueue::Config GetGestureEventQueueConfig() {
  return GestureEventQueue::Config();
}

TouchEventQueue::Config GetTouchEventQueueConfig() {
  TouchEventQueue::Config config;
  config.touchmove_slop_suppression_length_dips =
      ui::GestureDetector::Config().touch_slop;
  return config;
}

#endif

TouchEventQueue::TouchScrollingMode GetTouchScrollingMode() {
  std::string modeString =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kTouchScrollingMode);
  if (modeString == switches::kTouchScrollingModeAsyncTouchmove)
    return TouchEventQueue::TOUCH_SCROLLING_MODE_ASYNC_TOUCHMOVE;
  if (modeString == switches::kTouchScrollingModeSyncTouchmove)
    return TouchEventQueue::TOUCH_SCROLLING_MODE_SYNC_TOUCHMOVE;
  if (modeString == switches::kTouchScrollingModeTouchcancel)
    return TouchEventQueue::TOUCH_SCROLLING_MODE_TOUCHCANCEL;
  if (modeString != "")
    LOG(ERROR) << "Invalid --touch-scrolling-mode option: " << modeString;
  return TouchEventQueue::TOUCH_SCROLLING_MODE_DEFAULT;
}

}  // namespace

InputRouterImpl::Config GetInputRouterConfigForPlatform() {
  InputRouterImpl::Config config;
  config.gesture_config = GetGestureEventQueueConfig();
  config.touch_config = GetTouchEventQueueConfig();
  config.touch_config.touch_scrolling_mode = GetTouchScrollingMode();
  return config;
}

}  // namespace content
