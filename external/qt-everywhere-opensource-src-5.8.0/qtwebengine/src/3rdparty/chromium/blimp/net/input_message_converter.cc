// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/input_message_converter.h"

#include "base/logging.h"
#include "blimp/common/proto/input.pb.h"
#include "third_party/WebKit/public/platform/WebGestureDevice.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace blimp {
namespace {

std::unique_ptr<blink::WebGestureEvent> BuildCommonWebGesture(
    const InputMessage& proto,
    blink::WebInputEvent::Type type) {
  std::unique_ptr<blink::WebGestureEvent> event(new blink::WebGestureEvent);
  event->type = type;
  event->timeStampSeconds = proto.timestamp_seconds();

  const GestureCommon& common = proto.gesture_common();
  event->x = common.x();
  event->y = common.y();
  event->globalX = common.global_x();
  event->globalY = common.global_y();
  event->sourceDevice = blink::WebGestureDevice::WebGestureDeviceTouchscreen;
  return event;
}

std::unique_ptr<blink::WebGestureEvent> ProtoToGestureScrollBegin(
    const InputMessage& proto) {
  std::unique_ptr<blink::WebGestureEvent> event(BuildCommonWebGesture(
      proto, blink::WebInputEvent::Type::GestureScrollBegin));

  const GestureScrollBegin& details = proto.gesture_scroll_begin();
  event->data.scrollBegin.deltaXHint = details.delta_x_hint();
  event->data.scrollBegin.deltaYHint = details.delta_y_hint();
  event->data.scrollBegin.targetViewport = details.target_viewport();

  return event;
}

std::unique_ptr<blink::WebGestureEvent> ProtoToGestureScrollEnd(
    const InputMessage& proto) {
  return BuildCommonWebGesture(proto,
                               blink::WebInputEvent::Type::GestureScrollEnd);
}

std::unique_ptr<blink::WebGestureEvent> ProtoToGestureScrollUpdate(
    const InputMessage& proto) {
  std::unique_ptr<blink::WebGestureEvent> event(BuildCommonWebGesture(
      proto, blink::WebInputEvent::Type::GestureScrollUpdate));

  const GestureScrollUpdate& details = proto.gesture_scroll_update();
  event->data.scrollUpdate.deltaX = details.delta_x();
  event->data.scrollUpdate.deltaY = details.delta_y();
  event->data.scrollUpdate.velocityX = details.velocity_x();
  event->data.scrollUpdate.velocityY = details.velocity_y();
  event->data.scrollUpdate.previousUpdateInSequencePrevented =
      details.previous_update_in_sequence_prevented();
  event->data.scrollUpdate.preventPropagation = details.prevent_propagation();
  event->data.scrollUpdate.inertialPhase =
      details.inertial() ? blink::WebGestureEvent::MomentumPhase
                         : blink::WebGestureEvent::UnknownMomentumPhase;

  return event;
}

std::unique_ptr<blink::WebGestureEvent> ProtoToGestureFlingStart(
    const InputMessage& proto) {
  std::unique_ptr<blink::WebGestureEvent> event(BuildCommonWebGesture(
      proto, blink::WebInputEvent::Type::GestureFlingStart));

  const GestureFlingStart& details = proto.gesture_fling_start();
  event->data.flingStart.velocityX = details.velocity_x();
  event->data.flingStart.velocityY = details.velocity_y();
  event->data.flingStart.targetViewport = details.target_viewport();

  return event;
}

std::unique_ptr<blink::WebGestureEvent> ProtoToGestureFlingCancel(
    const InputMessage& proto) {
  std::unique_ptr<blink::WebGestureEvent> event(BuildCommonWebGesture(
      proto, blink::WebInputEvent::Type::GestureFlingCancel));

  const GestureFlingCancel& details = proto.gesture_fling_cancel();
  event->data.flingCancel.preventBoosting = details.prevent_boosting();

  return event;
}

std::unique_ptr<blink::WebGestureEvent> ProtoToGestureTap(
    const InputMessage& proto) {
  std::unique_ptr<blink::WebGestureEvent> event(
      BuildCommonWebGesture(proto, blink::WebInputEvent::Type::GestureTap));

  const GestureTap& details = proto.gesture_tap();
  event->data.tap.tapCount = details.tap_count();
  event->data.tap.width = details.width();
  event->data.tap.height = details.height();

  return event;
}

std::unique_ptr<blink::WebGestureEvent> ProtoToGesturePinchBegin(
    const InputMessage& proto) {
  return BuildCommonWebGesture(proto,
                               blink::WebInputEvent::Type::GesturePinchBegin);
}

std::unique_ptr<blink::WebGestureEvent> ProtoToGesturePinchEnd(
    const InputMessage& proto) {
  return BuildCommonWebGesture(proto,
                               blink::WebInputEvent::Type::GesturePinchEnd);
}

std::unique_ptr<blink::WebGestureEvent> ProtoToGesturePinchUpdate(
    const InputMessage& proto) {
  std::unique_ptr<blink::WebGestureEvent> event(BuildCommonWebGesture(
      proto, blink::WebInputEvent::Type::GesturePinchUpdate));

  const GesturePinchUpdate& details = proto.gesture_pinch_update();
  event->data.pinchUpdate.zoomDisabled = details.zoom_disabled();
  event->data.pinchUpdate.scale = details.scale();

  return event;
}

std::unique_ptr<blink::WebGestureEvent> ProtoToGestureTapDown(
    const InputMessage& proto) {
  std::unique_ptr<blink::WebGestureEvent> event(
      BuildCommonWebGesture(proto, blink::WebInputEvent::Type::GestureTapDown));

  const GestureTapDown& details = proto.gesture_tap_down();
  event->data.tapDown.width = details.width();
  event->data.tapDown.height = details.height();

  return event;
}

std::unique_ptr<blink::WebGestureEvent> ProtoToGestureTapCancel(
    const InputMessage& proto) {
  return BuildCommonWebGesture(proto,
                               blink::WebInputEvent::Type::GestureTapCancel);
}

std::unique_ptr<blink::WebGestureEvent> ProtoToGestureTapUnconfirmed(
    const InputMessage& proto) {
  std::unique_ptr<blink::WebGestureEvent> event(BuildCommonWebGesture(
      proto, blink::WebInputEvent::Type::GestureTapUnconfirmed));

  const GestureTap& details = proto.gesture_tap();
  event->data.tap.tapCount = details.tap_count();
  event->data.tap.width = details.width();
  event->data.tap.height = details.height();

  return event;
}

std::unique_ptr<blink::WebGestureEvent> ProtoToGestureShowPress(
    const InputMessage& proto) {
  std::unique_ptr<blink::WebGestureEvent> event(BuildCommonWebGesture(
      proto, blink::WebInputEvent::Type::GestureShowPress));

  const GestureShowPress& details = proto.gesture_show_press();
  event->data.showPress.width = details.width();
  event->data.showPress.height = details.height();

  return event;
}

}  // namespace

InputMessageConverter::InputMessageConverter() {}

InputMessageConverter::~InputMessageConverter() {}

std::unique_ptr<blink::WebGestureEvent> InputMessageConverter::ProcessMessage(
    const InputMessage& message) {
  std::unique_ptr<blink::WebGestureEvent> event;

  switch (message.type()) {
    case InputMessage::Type_GestureScrollBegin:
      event = ProtoToGestureScrollBegin(message);
      break;
    case InputMessage::Type_GestureScrollEnd:
      event = ProtoToGestureScrollEnd(message);
      break;
    case InputMessage::Type_GestureScrollUpdate:
      event = ProtoToGestureScrollUpdate(message);
      break;
    case InputMessage::Type_GestureFlingStart:
      event = ProtoToGestureFlingStart(message);
      break;
    case InputMessage::Type_GestureFlingCancel:
      event = ProtoToGestureFlingCancel(message);
      break;
    case InputMessage::Type_GestureTap:
      event = ProtoToGestureTap(message);
      break;
    case InputMessage::Type_GesturePinchBegin:
      event = ProtoToGesturePinchBegin(message);
      break;
    case InputMessage::Type_GesturePinchEnd:
      event = ProtoToGesturePinchEnd(message);
      break;
    case InputMessage::Type_GesturePinchUpdate:
      event = ProtoToGesturePinchUpdate(message);
      break;
    case InputMessage::Type_GestureTapDown:
      event = ProtoToGestureTapDown(message);
      break;
    case InputMessage::Type_GestureTapCancel:
      event = ProtoToGestureTapCancel(message);
      break;
    case InputMessage::Type_GestureTapUnconfirmed:
      event = ProtoToGestureTapUnconfirmed(message);
      break;
    case InputMessage::Type_GestureShowPress:
      event = ProtoToGestureShowPress(message);
      break;
    case InputMessage::UNKNOWN:
      DLOG(FATAL) << "Received an InputMessage with an unknown type.";
      return nullptr;
  }

  return event;
}

ui::TextInputType InputMessageConverter::TextInputTypeFromProto(
    ImeMessage_InputType type) {
  switch (type) {
    case ImeMessage_InputType_NONE:
      return ui::TEXT_INPUT_TYPE_NONE;
    case ImeMessage_InputType_TEXT:
      return ui::TEXT_INPUT_TYPE_TEXT;
    case ImeMessage_InputType_PASSWORD:
      return ui::TEXT_INPUT_TYPE_PASSWORD;
    case ImeMessage_InputType_SEARCH:
      return ui::TEXT_INPUT_TYPE_SEARCH;
    case ImeMessage_InputType_EMAIL:
      return ui::TEXT_INPUT_TYPE_EMAIL;
    case ImeMessage_InputType_NUMBER:
      return ui::TEXT_INPUT_TYPE_NUMBER;
    case ImeMessage_InputType_TELEPHONE:
      return ui::TEXT_INPUT_TYPE_TELEPHONE;
    case ImeMessage_InputType_URL:
      return ui::TEXT_INPUT_TYPE_URL;
    case ImeMessage_InputType_DATE:
      return ui::TEXT_INPUT_TYPE_DATE;
    case ImeMessage_InputType_DATE_TIME:
      return ui::TEXT_INPUT_TYPE_DATE_TIME;
    case ImeMessage_InputType_DATE_TIME_LOCAL:
      return ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL;
    case ImeMessage_InputType_MONTH:
      return ui::TEXT_INPUT_TYPE_MONTH;
    case ImeMessage_InputType_TIME:
      return ui::TEXT_INPUT_TYPE_TIME;
    case ImeMessage_InputType_WEEK:
      return ui::TEXT_INPUT_TYPE_WEEK;
    case ImeMessage_InputType_TEXT_AREA:
      return ui::TEXT_INPUT_TYPE_TEXT_AREA;
    case ImeMessage_InputType_CONTENT_EDITABLE:
      return ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE;
    case ImeMessage_InputType_DATE_TIME_FIELD:
      return ui::TEXT_INPUT_TYPE_DATE_TIME_FIELD;
  }
  return ui::TEXT_INPUT_TYPE_NONE;
}

ImeMessage_InputType InputMessageConverter::TextInputTypeToProto(
    ui::TextInputType type) {
  switch (type) {
    case ui::TEXT_INPUT_TYPE_NONE:
      NOTREACHED() << "IME needs an editable TextInputType";
      return ImeMessage_InputType_NONE;
    case ui::TEXT_INPUT_TYPE_TEXT:
      return ImeMessage_InputType_TEXT;
    case ui::TEXT_INPUT_TYPE_PASSWORD:
      return ImeMessage_InputType_PASSWORD;
    case ui::TEXT_INPUT_TYPE_SEARCH:
      return ImeMessage_InputType_SEARCH;
    case ui::TEXT_INPUT_TYPE_EMAIL:
      return ImeMessage_InputType_EMAIL;
    case ui::TEXT_INPUT_TYPE_NUMBER:
      return ImeMessage_InputType_NUMBER;
    case ui::TEXT_INPUT_TYPE_TELEPHONE:
      return ImeMessage_InputType_TELEPHONE;
    case ui::TEXT_INPUT_TYPE_URL:
      return ImeMessage_InputType_URL;
    case ui::TEXT_INPUT_TYPE_DATE:
      return ImeMessage_InputType_DATE;
    case ui::TEXT_INPUT_TYPE_DATE_TIME:
      return ImeMessage_InputType_DATE_TIME;
    case ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL:
      return ImeMessage_InputType_DATE_TIME_LOCAL;
    case ui::TEXT_INPUT_TYPE_MONTH:
      return ImeMessage_InputType_MONTH;
    case ui::TEXT_INPUT_TYPE_TIME:
      return ImeMessage_InputType_TIME;
    case ui::TEXT_INPUT_TYPE_WEEK:
      return ImeMessage_InputType_WEEK;
    case ui::TEXT_INPUT_TYPE_TEXT_AREA:
      return ImeMessage_InputType_TEXT_AREA;
    case ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE:
      return ImeMessage_InputType_CONTENT_EDITABLE;
    case ui::TEXT_INPUT_TYPE_DATE_TIME_FIELD:
      return ImeMessage_InputType_DATE_TIME_FIELD;
  }
  return ImeMessage_InputType_NONE;
}

}  // namespace blimp
