// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_ACK_HANDLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_ACK_HANDLER_H_

#include "base/basictypes.h"
#include "content/browser/renderer_host/event_with_latency_info.h"
#include "content/common/input/input_event_ack_state.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace content {

// Provided customized ack response for input events.
class CONTENT_EXPORT InputAckHandler {
 public:
  virtual ~InputAckHandler() {}

  // Called upon event ack receipt from the renderer.
  virtual void OnKeyboardEventAck(const NativeWebKeyboardEvent& event,
                                  InputEventAckState ack_result) = 0;
  virtual void OnWheelEventAck(const MouseWheelEventWithLatencyInfo& event,
                               InputEventAckState ack_result) = 0;
  virtual void OnTouchEventAck(const TouchEventWithLatencyInfo& event,
                               InputEventAckState ack_result) = 0;
  virtual void OnGestureEventAck(const GestureEventWithLatencyInfo& event,
                                 InputEventAckState ack_result) = 0;

  enum UnexpectedEventAckType {
    UNEXPECTED_ACK,
    UNEXPECTED_EVENT_TYPE,
    BAD_ACK_MESSAGE
  };
  virtual void OnUnexpectedEventAck(UnexpectedEventAckType type) = 0;
};

} // namespace content

#endif // CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_ACK_HANDLER_H_
