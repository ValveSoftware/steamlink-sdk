// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_EVENT_WITH_LATENCY_INFO_H_
#define CONTENT_COMMON_INPUT_EVENT_WITH_LATENCY_INFO_H_

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebGestureEvent.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "ui/events/blink/scoped_web_input_event.h"
#include "ui/events/latency_info.h"

namespace content {
namespace internal {

bool CONTENT_EXPORT CanCoalesce(const blink::WebMouseEvent& event_to_coalesce,
                                const blink::WebMouseEvent& event);
void CONTENT_EXPORT Coalesce(const blink::WebMouseEvent& event_to_coalesce,
                             blink::WebMouseEvent* event);
bool CONTENT_EXPORT
CanCoalesce(const blink::WebMouseWheelEvent& event_to_coalesce,
            const blink::WebMouseWheelEvent& event);
void CONTENT_EXPORT Coalesce(const blink::WebMouseWheelEvent& event_to_coalesce,
                             blink::WebMouseWheelEvent* event);
bool CONTENT_EXPORT CanCoalesce(const blink::WebTouchEvent& event_to_coalesce,
                                const blink::WebTouchEvent& event);
void CONTENT_EXPORT Coalesce(const blink::WebTouchEvent& event_to_coalesce,
                             blink::WebTouchEvent* event);
bool CONTENT_EXPORT CanCoalesce(const blink::WebGestureEvent& event_to_coalesce,
                                const blink::WebGestureEvent& event);
void CONTENT_EXPORT Coalesce(const blink::WebGestureEvent& event_to_coalesce,
                             blink::WebGestureEvent* event);

}  // namespace internal

class ScopedWebInputEventWithLatencyInfo {
 public:
  ScopedWebInputEventWithLatencyInfo(ui::ScopedWebInputEvent,
                                     const ui::LatencyInfo&);

  ~ScopedWebInputEventWithLatencyInfo();

  bool CanCoalesceWith(const ScopedWebInputEventWithLatencyInfo& other) const
      WARN_UNUSED_RESULT;

  const blink::WebInputEvent& event() const;
  blink::WebInputEvent& event();
  const ui::LatencyInfo latencyInfo() const { return latency_; }

  void CoalesceWith(const ScopedWebInputEventWithLatencyInfo& other);

 private:
  ui::ScopedWebInputEvent event_;
  mutable ui::LatencyInfo latency_;
};

template <typename T>
class EventWithLatencyInfo {
 public:
  T event;
  mutable ui::LatencyInfo latency;

  explicit EventWithLatencyInfo(const T& e) : event(e) {}

  EventWithLatencyInfo(const T& e, const ui::LatencyInfo& l)
      : event(e), latency(l) {}

  EventWithLatencyInfo() {}

  bool CanCoalesceWith(const EventWithLatencyInfo& other)
      const WARN_UNUSED_RESULT {
    if (other.event.type != event.type)
      return false;

    DCHECK_EQ(sizeof(T), event.size);
    DCHECK_EQ(sizeof(T), other.event.size);

    return internal::CanCoalesce(other.event, event);
  }

  void CoalesceWith(const EventWithLatencyInfo& other) {
    // |other| should be a newer event than |this|.
    if (other.latency.trace_id() >= 0 && latency.trace_id() >= 0)
      DCHECK_GT(other.latency.trace_id(), latency.trace_id());

    // New events get coalesced into older events, and the newer timestamp
    // should always be preserved.
    const double time_stamp_seconds = other.event.timeStampSeconds;
    internal::Coalesce(other.event, &event);
    event.timeStampSeconds = time_stamp_seconds;

    // When coalescing two input events, we keep the oldest LatencyInfo
    // for Telemetry latency tests, since it will represent the longest
    // latency.
    other.latency = latency;
    other.latency.set_coalesced();
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

#endif  // CONTENT_COMMON_INPUT_EVENT_WITH_LATENCY_INFO_H_
