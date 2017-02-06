// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/ui_events_helper.h"

#include <stdint.h>

#include "content/browser/renderer_host/input/web_input_event_util.h"
#include "content/common/input/web_touch_event_traits.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"

namespace {

ui::EventType WebTouchPointStateToEventType(
    blink::WebTouchPoint::State state) {
  switch (state) {
    case blink::WebTouchPoint::StateReleased:
      return ui::ET_TOUCH_RELEASED;

    case blink::WebTouchPoint::StatePressed:
      return ui::ET_TOUCH_PRESSED;

    case blink::WebTouchPoint::StateMoved:
      return ui::ET_TOUCH_MOVED;

    case blink::WebTouchPoint::StateCancelled:
      return ui::ET_TOUCH_CANCELLED;

    default:
      return ui::ET_UNKNOWN;
  }
}

}  // namespace

namespace content {

bool MakeUITouchEventsFromWebTouchEvents(
    const TouchEventWithLatencyInfo& touch_with_latency,
    ScopedVector<ui::TouchEvent>* list,
    TouchEventCoordinateSystem coordinate_system) {
  const blink::WebTouchEvent& touch = touch_with_latency.event;
  ui::EventType type = ui::ET_UNKNOWN;
  switch (touch.type) {
    case blink::WebInputEvent::TouchStart:
      type = ui::ET_TOUCH_PRESSED;
      break;
    case blink::WebInputEvent::TouchEnd:
      type = ui::ET_TOUCH_RELEASED;
      break;
    case blink::WebInputEvent::TouchMove:
      type = ui::ET_TOUCH_MOVED;
      break;
    case blink::WebInputEvent::TouchCancel:
      type = ui::ET_TOUCH_CANCELLED;
      break;
    default:
      NOTREACHED();
      return false;
  }

  int flags = WebEventModifiersToEventFlags(touch.modifiers);
  base::TimeTicks timestamp =
      ui::EventTimeStampFromSeconds(touch.timeStampSeconds);
  for (unsigned i = 0; i < touch.touchesLength; ++i) {
    const blink::WebTouchPoint& point = touch.touches[i];
    if (WebTouchPointStateToEventType(point.state) != type)
      continue;
    // ui events start in the co-ordinate space of the EventDispatcher.
    gfx::PointF location;
    if (coordinate_system == LOCAL_COORDINATES)
      location = point.position;
    else
      location = point.screenPosition;
    ui::TouchEvent* uievent = new ui::TouchEvent(
        type, gfx::Point(), flags, point.id, timestamp, point.radiusX,
        point.radiusY, point.rotationAngle, point.force);
    uievent->set_location_f(location);
    uievent->set_root_location_f(location);
    uievent->set_latency(touch_with_latency.latency);
    list->push_back(uievent);
  }
  return true;
}

}  // namespace content
