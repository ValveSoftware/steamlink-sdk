// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_MOCK_INPUT_ACK_HANDLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_MOCK_INPUT_ACK_HANDLER_H_

#include "base/memory/scoped_ptr.h"
#include "content/browser/renderer_host/input/input_ack_handler.h"

namespace content {

class InputRouter;

class MockInputAckHandler : public InputAckHandler {
 public:
  MockInputAckHandler();
  virtual ~MockInputAckHandler();

  // InputAckHandler
  virtual void OnKeyboardEventAck(const NativeWebKeyboardEvent& event,
                                  InputEventAckState ack_result) OVERRIDE;
  virtual void OnWheelEventAck(const MouseWheelEventWithLatencyInfo& event,
                               InputEventAckState ack_result) OVERRIDE;
  virtual void OnTouchEventAck(const TouchEventWithLatencyInfo& event,
                               InputEventAckState ack_result) OVERRIDE;
  virtual void OnGestureEventAck(const GestureEventWithLatencyInfo& event,
                                 InputEventAckState ack_result) OVERRIDE;
  virtual void OnUnexpectedEventAck(UnexpectedEventAckType type) OVERRIDE;

  size_t GetAndResetAckCount();

  void set_input_router(InputRouter* input_router) {
    input_router_ = input_router;
  }

  void set_followup_touch_event(scoped_ptr<GestureEventWithLatencyInfo> event) {
    gesture_followup_event_ = event.Pass();
  }

  void set_followup_touch_event(scoped_ptr<TouchEventWithLatencyInfo> event) {
    touch_followup_event_ = event.Pass();
  }

  bool unexpected_event_ack_called() const {
    return unexpected_event_ack_called_;
  }
  InputEventAckState ack_state() const { return ack_state_; }

  blink::WebInputEvent::Type ack_event_type() const { return ack_event_type_; }

  const NativeWebKeyboardEvent& acked_keyboard_event() const {
    return acked_key_event_;
  }
  const blink::WebMouseWheelEvent& acked_wheel_event() const {
    return acked_wheel_event_;
  }
  const TouchEventWithLatencyInfo& acked_touch_event() const {
    return acked_touch_event_;
  }
  const blink::WebGestureEvent& acked_gesture_event() const {
    return acked_gesture_event_;
  }

 private:
  void RecordAckCalled(blink::WebInputEvent::Type eventType,
                       InputEventAckState ack_result);

  InputRouter* input_router_;

  size_t ack_count_;
  bool unexpected_event_ack_called_;
  blink::WebInputEvent::Type ack_event_type_;
  InputEventAckState ack_state_;
  NativeWebKeyboardEvent acked_key_event_;
  blink::WebMouseWheelEvent acked_wheel_event_;
  TouchEventWithLatencyInfo acked_touch_event_;
  blink::WebGestureEvent acked_gesture_event_;

  scoped_ptr<GestureEventWithLatencyInfo> gesture_followup_event_;
  scoped_ptr<TouchEventWithLatencyInfo> touch_followup_event_;
};

}  // namespace content

#endif // CONTENT_BROWSER_RENDERER_HOST_INPUT_MOCK_INPUT_ACK_HANDLER_H_
