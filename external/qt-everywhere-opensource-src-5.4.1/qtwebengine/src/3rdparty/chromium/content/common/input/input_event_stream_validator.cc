// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/input_event_stream_validator.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "content/public/common/content_switches.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

using blink::WebInputEvent;
using blink::WebGestureEvent;
using blink::WebTouchEvent;

namespace content {

InputEventStreamValidator::InputEventStreamValidator()
    : enabled_(CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kValidateInputEventStream)) {
}

InputEventStreamValidator::~InputEventStreamValidator() {
}

void InputEventStreamValidator::Validate(const WebInputEvent& event) {
  if (!enabled_)
    return;

  DCHECK(ValidateImpl(event, &error_msg_)) << error_msg_;
}

bool InputEventStreamValidator::ValidateImpl(const blink::WebInputEvent& event,
                                             std::string* error_msg) {
  DCHECK(error_msg);
  if (WebInputEvent::isGestureEventType(event.type)) {
    const WebGestureEvent& gesture = static_cast<const WebGestureEvent&>(event);
    // TODO(jdduke): Validate touchpad gesture streams.
    if (gesture.sourceDevice == blink::WebGestureDeviceTouchscreen)
      return gesture_validator_.Validate(gesture, error_msg);
  } else if (WebInputEvent::isTouchEventType(event.type)) {
    const WebTouchEvent& touch = static_cast<const WebTouchEvent&>(event);
    return touch_validator_.Validate(touch, error_msg);
  }
  return true;
}

}  // namespace content
