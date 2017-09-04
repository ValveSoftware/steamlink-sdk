// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_BLINK_BLINK_EVENT_UTIL_H_
#define UI_EVENTS_BLINK_BLINK_EVENT_UTIL_H_

#include <memory>

#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "ui/events/gesture_detection/motion_event.h"

namespace blink {
class WebGestureEvent;
class WebInputEvent;
class WebTouchEvent;
}

namespace gfx {
class PointF;
class Vector2d;
}

namespace ui {
enum class DomCode;
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

// Transforms coordinates and other properties of |event|, by
// 1) translating / shifting by |delta| and
// 2) scaling by |scale|.
// If |event| does not need to change, returns nullptr.
// Otherwise, returns the transformed version of |event|.
std::unique_ptr<blink::WebInputEvent> TranslateAndScaleWebInputEvent(
    const blink::WebInputEvent& event,
    const gfx::Vector2d& delta,
    float scale);

blink::WebInputEvent::Type ToWebMouseEventType(MotionEvent::Action action);

void SetWebPointerPropertiesFromMotionEventData(
    blink::WebPointerProperties& webPointerProperties,
    int pointer_id,
    float pressure,
    float orientation_rad,
    float tilt_rad,
    int android_buttons_changed,
    int tool_type);

int WebEventModifiersToEventFlags(int modifiers);

blink::WebInputEvent::Modifiers DomCodeToWebInputEventModifiers(
    ui::DomCode code);

}  // namespace ui

#endif  // UI_EVENTS_BLINK_BLINK_EVENT_UTIL_H_
