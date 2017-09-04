// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/gesture_event_stream_validator.h"

#include "content/common/input/synthetic_web_input_event_builders.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::WebInputEvent;
using blink::WebGestureEvent;

namespace content {
namespace {

const blink::WebGestureDevice kDefaultGestureDevice =
    blink::WebGestureDeviceTouchscreen;

blink::WebGestureEvent Build(WebInputEvent::Type type) {
  blink::WebGestureEvent event =
      SyntheticWebGestureEventBuilder::Build(type, kDefaultGestureDevice);
  // Default to providing a (valid) non-zero fling velocity.
  if (type == WebInputEvent::GestureFlingStart)
    event.data.flingStart.velocityX = 5;
  return event;
}

}  // namespace

TEST(GestureEventStreamValidator, ValidScroll) {
  GestureEventStreamValidator validator;
  std::string error_msg;
  WebGestureEvent event;

  event = Build(WebInputEvent::GestureScrollBegin);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::GestureScrollUpdate);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::GestureScrollEnd);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());
}

TEST(GestureEventStreamValidator, InvalidScroll) {
  GestureEventStreamValidator validator;
  std::string error_msg;
  WebGestureEvent event;

  // No preceding ScrollBegin.
  event = Build(WebInputEvent::GestureScrollUpdate);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());

  // No preceding ScrollBegin.
  event = Build(WebInputEvent::GestureScrollEnd);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());

  event = Build(WebInputEvent::GestureScrollBegin);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  // Already scrolling.
  event = Build(WebInputEvent::GestureScrollBegin);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());

  event = Build(WebInputEvent::GestureScrollEnd);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  // Scroll already ended.
  event = Build(WebInputEvent::GestureScrollEnd);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());
}

TEST(GestureEventStreamValidator, ValidFling) {
  GestureEventStreamValidator validator;
  std::string error_msg;
  WebGestureEvent event;

  event = Build(WebInputEvent::GestureScrollBegin);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::GestureFlingStart);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());
}

TEST(GestureEventStreamValidator, InvalidFling) {
  GestureEventStreamValidator validator;
  std::string error_msg;
  WebGestureEvent event;

  // No preceding ScrollBegin.
  event = Build(WebInputEvent::GestureFlingStart);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());

  // Zero velocity.
  event = Build(WebInputEvent::GestureScrollBegin);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::GestureFlingStart);
  event.data.flingStart.velocityX = 0;
  event.data.flingStart.velocityY = 0;
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());
}

TEST(GestureEventStreamValidator, ValidPinch) {
  GestureEventStreamValidator validator;
  std::string error_msg;
  WebGestureEvent event;

  event = Build(WebInputEvent::GesturePinchBegin);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::GesturePinchUpdate);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::GesturePinchEnd);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());
}

TEST(GestureEventStreamValidator, InvalidPinch) {
  GestureEventStreamValidator validator;
  std::string error_msg;
  WebGestureEvent event;

  // No preceding PinchBegin.
  event = Build(WebInputEvent::GesturePinchUpdate);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());

  event = Build(WebInputEvent::GesturePinchBegin);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  // ScrollBegin while pinching.
  event = Build(WebInputEvent::GestureScrollBegin);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());

  // ScrollEnd while pinching.
  event = Build(WebInputEvent::GestureScrollEnd);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());

  // Pinch already begun.
  event = Build(WebInputEvent::GesturePinchBegin);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());

  event = Build(WebInputEvent::GesturePinchEnd);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  // Pinch already ended.
  event = Build(WebInputEvent::GesturePinchEnd);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());
}

TEST(GestureEventStreamValidator, ValidTap) {
  GestureEventStreamValidator validator;
  std::string error_msg;
  WebGestureEvent event;

  event = Build(WebInputEvent::GestureTapDown);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::GestureTapCancel);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::GestureTapDown);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::GestureTapUnconfirmed);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::GestureTapCancel);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::GestureTapDown);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::GestureTap);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  // DoubleTap does not require a TapDown (unlike Tap, TapUnconfirmed and
  // TapCancel).
  event = Build(WebInputEvent::GestureDoubleTap);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());
}

TEST(GestureEventStreamValidator, InvalidTap) {
  GestureEventStreamValidator validator;
  std::string error_msg;
  WebGestureEvent event;

  // No preceding TapDown.
  event = Build(WebInputEvent::GestureTapUnconfirmed);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());

  event = Build(WebInputEvent::GestureTapCancel);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());

  event = Build(WebInputEvent::GestureTap);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());

  // TapDown already terminated.
  event = Build(WebInputEvent::GestureTapDown);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::GestureDoubleTap);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::GestureTapCancel);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());

  // TapDown already terminated.
  event = Build(WebInputEvent::GestureTapDown);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::GestureTap);
  EXPECT_TRUE(validator.Validate(event, &error_msg));
  EXPECT_TRUE(error_msg.empty());

  event = Build(WebInputEvent::GestureTapCancel);
  EXPECT_FALSE(validator.Validate(event, &error_msg));
  EXPECT_FALSE(error_msg.empty());
}

}  // namespace content
