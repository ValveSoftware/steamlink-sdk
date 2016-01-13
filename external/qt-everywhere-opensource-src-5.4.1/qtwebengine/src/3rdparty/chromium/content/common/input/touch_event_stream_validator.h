// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_TOUCH_EVENT_STREAM_VALIDATOR
#define CONTENT_COMMON_INPUT_TOUCH_EVENT_STREAM_VALIDATOR

#include <string>

#include "base/basictypes.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace content {

// Utility class for validating a stream of WebTouchEvents.
class TouchEventStreamValidator {
 public:
  TouchEventStreamValidator();
  ~TouchEventStreamValidator();

  // If |event| is valid for the current stream, returns true.
  // Otherwise, returns false with a corresponding error message.
  bool Validate(const blink::WebTouchEvent& event, std::string* error_msg);

 private:
  blink::WebTouchEvent previous_event_;

  DISALLOW_COPY_AND_ASSIGN(TouchEventStreamValidator);
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_TOUCH_EVENT_STREAM_VALIDATOR
