// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_EVENT_WITH_LATENCY_INFO_H_
#define CONTENT_BROWSER_RENDERER_HOST_EVENT_WITH_LATENCY_INFO_H_

#include "ui/events/latency_info.h"

#include "content/common/input/web_input_event_traits.h"

namespace blink {
class WebGestureEvent;
class WebMouseEvent;
class WebMouseWheelEvent;
class WebTouchEvent;
}

namespace content {

template <typename T>
class EventWithLatencyInfo {
 public:
  T event;
  ui::LatencyInfo latency;

  EventWithLatencyInfo(const T& e, const ui::LatencyInfo& l)
      : event(e), latency(l) {}

  EventWithLatencyInfo() {}

  bool CanCoalesceWith(const EventWithLatencyInfo& other)
      const WARN_UNUSED_RESULT {
    return WebInputEventTraits::CanCoalesce(other.event, event);
  }

  void CoalesceWith(const EventWithLatencyInfo& other) {
    WebInputEventTraits::Coalesce(other.event, &event);
    // When coalescing two input events, we keep the oldest LatencyInfo
    // for Telemetry latency test since it will represent the longest
    // latency.
    if (other.latency.trace_id >= 0 &&
        (latency.trace_id < 0 || other.latency.trace_id < latency.trace_id))
      latency = other.latency;
  }
};

typedef EventWithLatencyInfo<blink::WebGestureEvent>
    GestureEventWithLatencyInfo;
typedef EventWithLatencyInfo<blink::WebMouseWheelEvent>
    MouseWheelEventWithLatencyInfo;
typedef EventWithLatencyInfo<blink::WebMouseEvent>
    MouseEventWithLatencyInfo;
typedef EventWithLatencyInfo<blink::WebTouchEvent>
    TouchEventWithLatencyInfo;

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_EVENT_WITH_LATENCY_INFO_H_
