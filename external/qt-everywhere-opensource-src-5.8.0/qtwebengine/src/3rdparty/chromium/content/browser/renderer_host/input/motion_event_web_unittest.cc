// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MSVC++ requires this to be set before any other includes to get M_PI.
#define _USE_MATH_DEFINES

#include <stddef.h>

#include <cmath>

#include "content/browser/renderer_host/input/motion_event_web.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/gesture_detection/motion_event_generic.h"
#include "ui/events/test/motion_event_test_utils.h"

using ui::MotionEvent;
using ui::MotionEventGeneric;
using ui::PointerProperties;

namespace content {

TEST(MotionEventWebTest, Constructor) {
  const float pi = static_cast<float>(M_PI);
  const float orientations[] = {
      -pi, -2.f * pi / 3, -pi / 2, -pi / 3, 0.f, pi / 3, pi / 2, 2.f * pi / 3};
  const float tilts[] = {0.f, pi / 4, pi / 3};
  const MotionEvent::ToolType tool_types[] = {MotionEvent::TOOL_TYPE_FINGER,
                                              MotionEvent::TOOL_TYPE_STYLUS,
                                              MotionEvent::TOOL_TYPE_MOUSE};

  base::TimeTicks event_time = base::TimeTicks::Now();
  PointerProperties pp;
  MotionEventGeneric generic_event(MotionEvent::ACTION_MOVE, event_time, pp);

  for (MotionEvent::ToolType tool_type : tool_types) {
    for (float orientation : orientations) {
      for (float tilt : tilts) {
        PointerProperties pp2;
        pp2.orientation = orientation;
        pp2.tilt = tilt;
        pp2.tool_type = tool_type;
        size_t pointer_index = generic_event.PushPointer(pp2);
        EXPECT_GT(pointer_index, 0u);

        blink::WebTouchEvent web_touch_event =
            CreateWebTouchEventFromMotionEvent(generic_event, true);

        MotionEventWeb event(web_touch_event);
        EXPECT_EQ(tool_type, event.GetToolType(pointer_index));
        if (tool_type == MotionEvent::TOOL_TYPE_STYLUS) {
          // Web touch event touch point tilt plane angles are stored as ints,
          // thus the tilt precision is 1 degree and the error should not be
          // greater than 0.5 degrees.
          EXPECT_NEAR(tilt, event.GetTilt(pointer_index), 0.5f * M_PI / 180.f)
              << " orientation=" << orientation;
        } else {
          EXPECT_EQ(0.f, event.GetTilt(pointer_index));
        }
        if (tool_type == MotionEvent::TOOL_TYPE_STYLUS && tilt > 0.f) {
          // Full stylus tilt orientation information survives above event
          // conversions only if there is a non-zero stylus tilt angle.
          // See: http://crbug.com/251330
          EXPECT_NEAR(orientation, event.GetOrientation(pointer_index), 1e-4)
              << " tilt=" << tilt;
        } else {
          // For non-stylus pointers and for styluses with a zero tilt angle,
          // orientation quadrant information is lost.
          EXPECT_NEAR(fmod(orientation + M_PI + 1e-4, M_PI_2) - 1e-4,
                      event.GetOrientation(pointer_index), 1e-4);
        }

        generic_event.RemovePointerAt(pointer_index);
      }
    }
  }
}

}  // namespace content
