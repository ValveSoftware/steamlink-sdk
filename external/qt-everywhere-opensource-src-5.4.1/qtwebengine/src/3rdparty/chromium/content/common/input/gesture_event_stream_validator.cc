// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/gesture_event_stream_validator.h"

#include "base/logging.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

using blink::WebInputEvent;

namespace content {

GestureEventStreamValidator::GestureEventStreamValidator()
    : scrolling_(false), pinching_(false), waiting_for_tap_end_(false) {
}

GestureEventStreamValidator::~GestureEventStreamValidator() {
}

bool GestureEventStreamValidator::Validate(const blink::WebGestureEvent& event,
                                           std::string* error_msg) {
  DCHECK(error_msg);
  error_msg->clear();
  switch (event.type) {
    case WebInputEvent::GestureScrollBegin:
      if (scrolling_ || pinching_)
        error_msg->append("Scroll begin during scroll\n");
      scrolling_ = true;
      break;
    case WebInputEvent::GestureScrollUpdate:
    case WebInputEvent::GestureScrollUpdateWithoutPropagation:
      if (!scrolling_)
        error_msg->append("Scroll update outside of scroll\n");
      break;
    case WebInputEvent::GestureScrollEnd:
    case WebInputEvent::GestureFlingStart:
      if (!scrolling_)
        error_msg->append("Scroll end outside of scroll\n");
      if (pinching_)
        error_msg->append("Ending scroll while pinching\n");
      scrolling_ = false;
      break;
    case WebInputEvent::GesturePinchBegin:
      if (!scrolling_)
        error_msg->append("Pinch begin outside of scroll\n");
      if (pinching_)
        error_msg->append("Pinch begin during pinch\n");
      pinching_ = true;
      break;
    case WebInputEvent::GesturePinchUpdate:
      if (!pinching_ || !scrolling_)
        error_msg->append("Pinch update outside of pinch\n");
      break;
    case WebInputEvent::GesturePinchEnd:
      if (!pinching_ || !scrolling_)
        error_msg->append("Pinch end outside of pinch\n");
      pinching_ = false;
      break;
    case WebInputEvent::GestureTapDown:
      if (waiting_for_tap_end_)
        error_msg->append("Missing tap end event\n");
      waiting_for_tap_end_ = true;
      break;
    case WebInputEvent::GestureTap:
    case WebInputEvent::GestureTapCancel:
    case WebInputEvent::GestureDoubleTap:
      if (!waiting_for_tap_end_)
        error_msg->append("Missing GestureTapDown event\n");
      waiting_for_tap_end_ = false;
      break;
    default:
      break;
  }
  return error_msg->empty();
}

}  // namespace content
