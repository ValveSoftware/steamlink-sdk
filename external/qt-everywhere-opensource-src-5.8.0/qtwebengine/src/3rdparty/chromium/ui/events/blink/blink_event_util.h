// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_BLINK_BLINK_EVENT_UTIL_H_
#define UI_EVENTS_BLINK_BLINK_EVENT_UTIL_H_

#include <memory>

#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/gesture_detection/motion_event.h"

namespace base {
class TimeDelta;
}

namespace blink {
class WebGestureEvent;
class WebInputEvent;
class WebTouchEvent;
}

namespace gfx {
class PointF;
}

namespace ui {
struct GestureEventData;
struct GestureEventDetails;
class MotionEvent;

blink::WebTouchEvent CreateWebTouchEventFromMotionEvent(
    const MotionEvent& event,
    bool may_cause_scrolling);

blink::WebGestureEvent CreateWebGestureEvent(const GestureEventDetails& details,
                                             base::TimeTicks timestamp,
                                             const gfx::PointF& location,
                                             const gfx::PointF& raw_location,
                                             int flags,
                                             uint32_t unique_touch_event_id);

// Convenience wrapper for |CreateWebGestureEvent| using the supplied |data|.
blink::WebGestureEvent CreateWebGestureEventFromGestureEventData(
    const GestureEventData& data);

int EventFlagsToWebEventModifiers(int flags);

std::unique_ptr<blink::WebInputEvent> ScaleWebInputEvent(
    const blink::WebInputEvent& event,
    float scale);

blink::WebPointerProperties::PointerType ToWebPointerType(
    MotionEvent::ToolType tool_type);

}  // namespace ui

#endif  // UI_EVENTS_BLINK_BLINK_EVENT_UTIL_H_
