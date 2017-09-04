// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/touch_event_stream_validator.h"

#include <stddef.h>

#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/common/input/web_touch_event_traits.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::WebInputEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace content {

TEST(TouchEventStreamValidator, ValidTouchStream) {
  TouchEventStreamValidator validator;
  SyntheticWebTouchEvent event;
  std::string error_msg;

  event.PressPoint(0, 1);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());
  event.ResetPoints();

  event.PressPoint(1, 0);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());
  event.ResetPoints();

  event.MovePoint(1, 1, 1);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());
  event.ResetPoints();

  event.ReleasePoint(1);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());
  event.ResetPoints();

  event.MovePoint(0, -1, 0);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());
  event.ResetPoints();

  event.CancelPoint(0);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());
  event.ResetPoints();

  event.PressPoint(-1, -1);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());
}

TEST(TouchEventStreamValidator, ResetOnNewTouchStream) {
  TouchEventStreamValidator validator;
  SyntheticWebTouchEvent event;
  std::string error_msg;

  event.PressPoint(0, 1);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());
  event.ResetPoints();

  event.CancelPoint(0);
  event.ResetPoints();
  event.PressPoint(1, 0);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());
}

TEST(TouchEventStreamValidator, MissedTouchStart) {
  TouchEventStreamValidator validator;
  SyntheticWebTouchEvent event;
  std::string error_msg;

  event.PressPoint(0, 1);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event.PressPoint(1, 0);
  event.ResetPoints();
  event.PressPoint(1, 1);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());
}

TEST(TouchEventStreamValidator, MissedTouchEnd) {
  TouchEventStreamValidator validator;
  SyntheticWebTouchEvent event;
  std::string error_msg;

  event.PressPoint(0, 1);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());
  event.ResetPoints();

  event.PressPoint(0, 1);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());
  event.ResetPoints();

  event.ReleasePoint(1);
  event.ResetPoints();
  event.PressPoint(1, 1);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());
}

TEST(TouchEventStreamValidator, EmptyEvent) {
  TouchEventStreamValidator validator;
  WebTouchEvent event;
  std::string error_msg;

  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());
}

TEST(TouchEventStreamValidator, InvalidEventType) {
  TouchEventStreamValidator validator;
  WebTouchEvent event;
  std::string error_msg;

  event.type = WebInputEvent::GestureScrollBegin;
  event.touchesLength = 1;
  event.touches[0].state = WebTouchPoint::StatePressed;

  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());
}

TEST(TouchEventStreamValidator, InvalidPointStates) {
  TouchEventStreamValidator validator;
  std::string error_msg;

  WebInputEvent::Type kTouchTypes[4] = {
      WebInputEvent::TouchStart, WebInputEvent::TouchMove,
      WebInputEvent::TouchEnd, WebInputEvent::TouchCancel,
  };

  WebTouchPoint::State kValidTouchPointStatesForType[4] = {
      WebTouchPoint::StatePressed, WebTouchPoint::StateMoved,
      WebTouchPoint::StateReleased, WebTouchPoint::StateCancelled,
  };

  SyntheticWebTouchEvent start;
  start.PressPoint(0, 0);
  for (size_t i = 0; i < 4; ++i) {
    // Always start with a touchstart to reset the stream validation.
    EXPECT_TRUE(validator.Validate(start, &error_msg));
    EXPECT_TRUE(error_msg.empty());

    WebTouchEvent event;
    event.touchesLength = 1;
    event.type = kTouchTypes[i];
    for (size_t j = WebTouchPoint::StateUndefined;
         j <= WebTouchPoint::StateCancelled;
         ++j) {
      event.touches[0].state = static_cast<WebTouchPoint::State>(j);
      if (event.touches[0].state == kValidTouchPointStatesForType[i]) {
        EXPECT_TRUE(validator.Validate(event, &error_msg));
        EXPECT_TRUE(error_msg.empty());
      } else {
        EXPECT_FALSE(validator.Validate(event, &error_msg));
        EXPECT_FALSE(error_msg.empty());
      }
    }
  }
}

}  // namespace content
