// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/logging.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/common/proto/input.pb.h"
#include "blimp/net/blimp_message_processor.h"
#include "blimp/net/input_message_converter.h"
#include "blimp/net/input_message_generator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebGestureDevice.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace blimp {
namespace {

void ValidateWebGestureEventRoundTripping(const blink::WebGestureEvent& event) {
  InputMessageGenerator generator;
  InputMessageConverter processor;

  std::unique_ptr<BlimpMessage> proto = generator.GenerateMessage(event);
  EXPECT_NE(nullptr, proto.get());
  EXPECT_EQ(BlimpMessage::kInput, proto->feature_case());

  std::unique_ptr<blink::WebGestureEvent> new_event =
      processor.ProcessMessage(proto->input());
  EXPECT_NE(nullptr, new_event.get());

  EXPECT_EQ(event.size, new_event->size);
  EXPECT_EQ(0, memcmp(&event, new_event.get(), event.size));
}

blink::WebGestureEvent BuildBaseTestEvent() {
  blink::WebGestureEvent event;
  event.timeStampSeconds = 1.23;
  event.x = 2;
  event.y = 3;
  event.globalX = 4;
  event.globalY = 5;
  event.sourceDevice = blink::WebGestureDevice::WebGestureDeviceTouchscreen;
  return event;
}

}  // namespace

TEST(InputMessageTest, TestGestureScrollBeginRoundTrip) {
  blink::WebGestureEvent event = BuildBaseTestEvent();
  event.type = blink::WebInputEvent::Type::GestureScrollBegin;
  event.data.scrollBegin.deltaXHint = 2.34f;
  event.data.scrollBegin.deltaYHint = 3.45f;
  event.data.scrollBegin.targetViewport = true;
  ValidateWebGestureEventRoundTripping(event);
}

TEST(InputMessageTest, TestGestureScrollEndRoundTrip) {
  blink::WebGestureEvent event = BuildBaseTestEvent();
  event.type = blink::WebInputEvent::Type::GestureScrollEnd;
  ValidateWebGestureEventRoundTripping(event);
}

TEST(InputMessageTest, TestGestureScrollUpdateRoundTrip) {
  blink::WebGestureEvent event = BuildBaseTestEvent();
  event.type = blink::WebInputEvent::Type::GestureScrollUpdate;
  event.data.scrollUpdate.deltaX = 2.34f;
  event.data.scrollUpdate.deltaY = 3.45f;
  event.data.scrollUpdate.velocityX = 4.56f;
  event.data.scrollUpdate.velocityY = 5.67f;
  event.data.scrollUpdate.previousUpdateInSequencePrevented = true;
  event.data.scrollUpdate.preventPropagation = true;
  event.data.scrollUpdate.inertialPhase = blink::WebGestureEvent::MomentumPhase;
  ValidateWebGestureEventRoundTripping(event);
}

TEST(InputMessageTest, TestGestureFlingStartRoundTrip) {
  blink::WebGestureEvent event = BuildBaseTestEvent();
  event.type = blink::WebInputEvent::Type::GestureFlingStart;
  event.data.flingStart.velocityX = 2.34f;
  event.data.flingStart.velocityY = 3.45f;
  event.data.flingStart.targetViewport = true;
  ValidateWebGestureEventRoundTripping(event);
}

TEST(InputMessageTest, TestGestureFlingCancelRoundTrip) {
  blink::WebGestureEvent event = BuildBaseTestEvent();
  event.type = blink::WebInputEvent::Type::GestureFlingCancel;
  event.data.flingCancel.preventBoosting = true;
  ValidateWebGestureEventRoundTripping(event);
}

TEST(InputMessageTest, TestGestureTapRoundTrip) {
  blink::WebGestureEvent event = BuildBaseTestEvent();
  event.type = blink::WebGestureEvent::Type::GestureTap;
  event.data.tap.tapCount = 3;
  event.data.tap.width = 2.34f;
  event.data.tap.height = 3.45f;
  ValidateWebGestureEventRoundTripping(event);
}

TEST(InputMessageTest, TestGesturePinchBeginRoundTrip) {
  blink::WebGestureEvent event = BuildBaseTestEvent();
  event.type = blink::WebInputEvent::Type::GesturePinchBegin;
  ValidateWebGestureEventRoundTripping(event);
}

TEST(InputMessageTest, TestGesturePinchEndRoundTrip) {
  blink::WebGestureEvent event = BuildBaseTestEvent();
  event.type = blink::WebInputEvent::Type::GesturePinchEnd;
  ValidateWebGestureEventRoundTripping(event);
}

TEST(InputMessageTest, TestGesturePinchUpdateRoundTrip) {
  blink::WebGestureEvent event = BuildBaseTestEvent();
  event.type = blink::WebInputEvent::Type::GesturePinchUpdate;
  event.data.pinchUpdate.zoomDisabled = true;
  event.data.pinchUpdate.scale = 2.34f;
  ValidateWebGestureEventRoundTripping(event);
}

TEST(InputMessageTest, TestUnsupportedInputEventSerializationFails) {
  // We currently do not support MouseWheel events.  If that changes update
  // this test.
  blink::WebGestureEvent event;
  event.type = blink::WebInputEvent::Type::MouseWheel;
  InputMessageGenerator generator;
  EXPECT_EQ(nullptr, generator.GenerateMessage(event).get());
}

TEST(InputMessageConverterTest, TestTextInputTypeToProtoConversion) {
  for (size_t i = ui::TextInputType::TEXT_INPUT_TYPE_TEXT;
       i < ui::TextInputType::TEXT_INPUT_TYPE_MAX; i++) {
    ui::TextInputType type = static_cast<ui::TextInputType>(i);
    EXPECT_EQ(type, InputMessageConverter::TextInputTypeFromProto(
                        InputMessageConverter::TextInputTypeToProto(type)));
  }
}

TEST(InputMessageTest, TestGestureTapDownRoundTrip) {
  blink::WebGestureEvent event = BuildBaseTestEvent();
  event.type = blink::WebGestureEvent::Type::GestureTapDown;
  event.data.tapDown.width = 2.3f;
  event.data.tapDown.height = 3.4f;
  ValidateWebGestureEventRoundTripping(event);
}

TEST(InputMessageTest, TestGestureTapCancelRoundTrip) {
  blink::WebGestureEvent event = BuildBaseTestEvent();
  event.type = blink::WebGestureEvent::Type::GestureTapCancel;
  ValidateWebGestureEventRoundTripping(event);
}

TEST(InputMessageTest, TestGestureTapUnconfirmedRoundTrip) {
  blink::WebGestureEvent event = BuildBaseTestEvent();
  event.type = blink::WebGestureEvent::Type::GestureTapUnconfirmed;
  event.data.tap.tapCount = 2;
  event.data.tap.width = 2.3f;
  event.data.tap.height = 3.4f;
  ValidateWebGestureEventRoundTripping(event);
}

TEST(InputMessageTest, TestGestureShowPressRoundTrip) {
  blink::WebGestureEvent event = BuildBaseTestEvent();
  event.type = blink::WebGestureEvent::Type::GestureShowPress;
  event.data.showPress.width = 2.3f;
  event.data.showPress.height = 3.4f;
  ValidateWebGestureEventRoundTripping(event);
}

}  // namespace blimp
