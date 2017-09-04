// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_BLINK_WEB_INPUT_EVENT_TRAITS_H_
#define UI_EVENTS_BLINK_WEB_INPUT_EVENT_TRAITS_H_

#include "third_party/WebKit/public/platform/WebGestureEvent.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "ui/events/blink/scoped_web_input_event.h"
#include "ui/events/latency_info.h"

namespace ui {

// Utility class for performing operations on and with WebInputEvents.
class WebInputEventTraits {
 public:
  static std::string ToString(const blink::WebInputEvent& event);
  static size_t GetSize(blink::WebInputEvent::Type type);
  static ScopedWebInputEvent Clone(const blink::WebInputEvent& event);
  static void Delete(blink::WebInputEvent* event);
  static bool ShouldBlockEventStream(const blink::WebInputEvent& event);

  static bool CanCauseScroll(const blink::WebMouseWheelEvent& event);

  // Return uniqueTouchEventId for WebTouchEvent, otherwise return 0.
  static uint32_t GetUniqueTouchEventId(const blink::WebInputEvent& event);
  static LatencyInfo CreateLatencyInfoForWebGestureEvent(
      blink::WebGestureEvent event);
};

}  // namespace ui

#endif  // UI_EVENTS_BLINK_WEB_INPUT_EVENT_TRAITS_H_
