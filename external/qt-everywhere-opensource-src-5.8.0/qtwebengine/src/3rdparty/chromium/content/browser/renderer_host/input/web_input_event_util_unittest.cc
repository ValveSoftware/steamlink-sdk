// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Needed on Windows to get |M_PI| from <cmath>.
#ifdef _WIN32
#define _USE_MATH_DEFINES
#endif

#include <stddef.h>

#include <cmath>

#include "content/browser/renderer_host/input/web_input_event_util.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/common/input/web_input_event_traits.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/event_constants.h"
#include "ui/events/gesture_detection/gesture_event_data.h"
#include "ui/events/gesture_detection/motion_event_generic.h"
#include "ui/events/gesture_event_details.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"

using blink::WebInputEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;
using ui::MotionEvent;
using ui::MotionEventGeneric;

namespace content {

TEST(WebInputEventUtilTest, MotionEventConversion) {

  const MotionEvent::ToolType tool_types[] = {MotionEvent::TOOL_TYPE_FINGER,
                                              MotionEvent::TOOL_TYPE_STYLUS,
                                              MotionEvent::TOOL_TYPE_MOUSE};
  ui::PointerProperties pointer(5, 10, 40);
  pointer.id = 15;
  pointer.raw_x = 20;
  pointer.raw_y = 25;
  pointer.pressure = 30;
  pointer.touch_minor = 35;
  pointer.orientation = static_cast<float>(-M_PI / 2);
  pointer.tilt = static_cast<float>(M_PI / 3);
  for (MotionEvent::ToolType tool_type : tool_types) {
    pointer.tool_type = tool_type;
    MotionEventGeneric event(
        MotionEvent::ACTION_DOWN, base::TimeTicks::Now(), pointer);
    event.set_flags(ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN);
    event.set_unique_event_id(123456U);

    WebTouchEvent expected_event;
    expected_event.type = WebInputEvent::TouchStart;
    expected_event.touchesLength = 1;
    expected_event.timeStampSeconds =
        (event.GetEventTime() - base::TimeTicks()).InSecondsF();
    expected_event.modifiers = WebInputEvent::ShiftKey | WebInputEvent::AltKey;
    WebTouchPoint expected_pointer;
    expected_pointer.id = pointer.id;
    expected_pointer.state = WebTouchPoint::StatePressed;
    expected_pointer.position = blink::WebFloatPoint(pointer.x, pointer.y);
    expected_pointer.screenPosition =
        blink::WebFloatPoint(pointer.raw_x, pointer.raw_y);
    expected_pointer.radiusX = pointer.touch_major / 2.f;
    expected_pointer.radiusY = pointer.touch_minor / 2.f;
    expected_pointer.rotationAngle = 0.f;
    expected_pointer.force = pointer.pressure;
    if (tool_type == MotionEvent::TOOL_TYPE_STYLUS) {
      expected_pointer.tiltX = 60;
      expected_pointer.tiltY = 0;
    } else {
      expected_pointer.tiltX = 0;
      expected_pointer.tiltY = 0;
    }
    expected_event.touches[0] = expected_pointer;
    expected_event.uniqueTouchEventId = 123456U;

    WebTouchEvent actual_event =
        ui::CreateWebTouchEventFromMotionEvent(event, false);
    EXPECT_EQ(WebInputEventTraits::ToString(expected_event),
              WebInputEventTraits::ToString(actual_event));
  }
}

TEST(WebInputEventUtilTest, ScrollUpdateConversion) {
  int motion_event_id = 0;
  MotionEvent::ToolType tool_type = MotionEvent::TOOL_TYPE_UNKNOWN;
  base::TimeTicks timestamp = base::TimeTicks::Now();
  gfx::Vector2dF delta(-5.f, 10.f);
  gfx::PointF pos(1.f, 2.f);
  gfx::PointF raw_pos(11.f, 12.f);
  size_t touch_points = 1;
  gfx::RectF rect(pos, gfx::SizeF());
  int flags = 0;
  ui::GestureEventDetails details(ui::ET_GESTURE_SCROLL_UPDATE,
                                  delta.x(),
                                  delta.y());
  details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  details.mark_previous_scroll_update_in_sequence_prevented();
  ui::GestureEventData event(details,
                             motion_event_id,
                             tool_type,
                             timestamp,
                             pos.x(),
                             pos.y(),
                             raw_pos.x(),
                             raw_pos.y(),
                             touch_points,
                             rect,
                             flags,
                             0U);

  blink::WebGestureEvent web_event =
      ui::CreateWebGestureEventFromGestureEventData(event);
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, web_event.type);
  EXPECT_EQ(0, web_event.modifiers);
  EXPECT_EQ((timestamp - base::TimeTicks()).InSecondsF(),
            web_event.timeStampSeconds);
  EXPECT_EQ(gfx::ToFlooredInt(pos.x()), web_event.x);
  EXPECT_EQ(gfx::ToFlooredInt(pos.y()), web_event.y);
  EXPECT_EQ(gfx::ToFlooredInt(raw_pos.x()), web_event.globalX);
  EXPECT_EQ(gfx::ToFlooredInt(raw_pos.y()), web_event.globalY);
  EXPECT_EQ(blink::WebGestureDeviceTouchscreen, web_event.sourceDevice);
  EXPECT_EQ(delta.x(), web_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(delta.y(), web_event.data.scrollUpdate.deltaY);
  EXPECT_TRUE(web_event.data.scrollUpdate.previousUpdateInSequencePrevented);
}

}  // namespace content
