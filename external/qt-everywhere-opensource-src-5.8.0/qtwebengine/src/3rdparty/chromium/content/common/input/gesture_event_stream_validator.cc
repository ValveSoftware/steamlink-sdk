// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/gesture_event_stream_validator.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "content/common/input/web_input_event_traits.h"
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
  if (!WebInputEvent::isGestureEventType(event.type)) {
    error_msg->append(base::StringPrintf(
        "Invalid gesture type: %s", WebInputEventTraits::GetName(event.type)));
  }
  switch (event.type) {
    case WebInputEvent::GestureScrollBegin:
      if (scrolling_)
        error_msg->append("Scroll begin during scroll\n");
      if (pinching_)
        error_msg->append("Scroll begin during pinch\n");
      scrolling_ = true;
      break;
    case WebInputEvent::GestureScrollUpdate:
      if (!scrolling_)
        error_msg->append("Scroll update outside of scroll\n");
      break;
    case WebInputEvent::GestureFlingStart:
      if (event.sourceDevice == blink::WebGestureDeviceTouchscreen &&
          !event.data.flingStart.velocityX &&
          !event.data.flingStart.velocityY) {
        error_msg->append("Zero velocity touchscreen fling\n");
      }
      if (!scrolling_)
        error_msg->append("Fling start outside of scroll\n");
      if (pinching_)
        error_msg->append("Flinging while pinching\n");
      scrolling_ = false;
      break;
    case WebInputEvent::GestureScrollEnd:
      if (!scrolling_)
        error_msg->append("Scroll end outside of scroll\n");
      if (pinching_)
        error_msg->append("Ending scroll while pinching\n");
      scrolling_ = false;
      break;
    case WebInputEvent::GesturePinchBegin:
      if (pinching_)
        error_msg->append("Pinch begin during pinch\n");
      pinching_ = true;
      break;
    case WebInputEvent::GesturePinchUpdate:
      if (!pinching_)
        error_msg->append("Pinch update outside of pinch\n");
      break;
    case WebInputEvent::GesturePinchEnd:
      if (!pinching_)
        error_msg->append("Pinch end outside of pinch\n");
      pinching_ = false;
      break;
    case WebInputEvent::GestureTapDown:
      if (waiting_for_tap_end_)
        error_msg->append("Missing tap ending event before TapDown\n");
      waiting_for_tap_end_ = true;
      break;
    case WebInputEvent::GestureTapUnconfirmed:
      if (!waiting_for_tap_end_)
        error_msg->append("Missing TapDown event before TapUnconfirmed\n");
      break;
    case WebInputEvent::GestureTapCancel:
      if (!waiting_for_tap_end_)
        error_msg->append("Missing TapDown event before TapCancel\n");
      waiting_for_tap_end_ = false;
      break;
    case WebInputEvent::GestureTap:
      if (!waiting_for_tap_end_)
        error_msg->append("Missing TapDown event before Tap\n");
      waiting_for_tap_end_ = false;
      break;
    case WebInputEvent::GestureDoubleTap:
      // DoubleTap gestures may be synthetically inserted, and do not require a
      // preceding TapDown.
      waiting_for_tap_end_ = false;
      break;
    default:
      break;
  }
  // TODO(wjmaclean): At some future point we may wish to consider adding a
  // 'continuity check', requiring that all events between an initial tap-down
  // and whatever terminates the sequence to have the same source device type,
  // and that touchpad gestures are only found on ScrollEvents.
  if (event.sourceDevice == blink::WebGestureDeviceUninitialized)
    error_msg->append("Gesture event source is uninitialized.\n");

  return error_msg->empty();
}

}  // namespace content
