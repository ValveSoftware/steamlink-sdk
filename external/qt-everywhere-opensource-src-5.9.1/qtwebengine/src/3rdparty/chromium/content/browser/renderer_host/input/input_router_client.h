// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_ROUTER_CLIENT_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_ROUTER_CLIENT_H_

#include "content/browser/renderer_host/event_with_latency_info.h"
#include "content/common/content_export.h"
#include "content/common/input/input_event_ack_source.h"
#include "content/common/input/input_event_ack_state.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"

namespace ui {
class LatencyInfo;
struct DidOverscrollParams;
}

namespace content {

class CONTENT_EXPORT InputRouterClient {
 public:
  virtual ~InputRouterClient() {}

  // Called just prior to events being sent to the renderer, giving the client
  // a chance to perform in-process event filtering.
  // The returned disposition will yield the following behavior:
  //   * |NOT_CONSUMED| will result in |input_event| being sent as usual.
  //   * |CONSUMED| or |NO_CONSUMER_EXISTS| will trigger the appropriate ack.
  //   * |UNKNOWN| will result in |input_event| being dropped.
  virtual InputEventAckState FilterInputEvent(
      const blink::WebInputEvent& input_event,
      const ui::LatencyInfo& latency_info) = 0;

  // Called each time a WebInputEvent IPC is sent.
  virtual void IncrementInFlightEventCount(blink::WebInputEvent::Type type) = 0;

  // Called each time a WebInputEvent ACK IPC is received.
  virtual void DecrementInFlightEventCount(InputEventAckSource ack_source) = 0;

  // Called when the renderer notifies that it has touch event handlers.
  virtual void OnHasTouchEventHandlers(bool has_handlers) = 0;

  // Called when the router has finished flushing all events queued at the time
  // of the call to Flush.  The call will typically be asynchronous with
  // respect to the call to |Flush| on the InputRouter.
  virtual void DidFlush() = 0;

  // Called when the router has received an overscroll notification from the
  // renderer.
  virtual void DidOverscroll(const ui::DidOverscrollParams& params) = 0;

  // Called when a renderer fling has terminated.
  virtual void DidStopFlinging() = 0;

  // Called when the input router generates an event. It is intended that the
  // client will do some processing on |gesture_event| and then send it back
  // to the InputRouter via SendGestureEvent.
  virtual void ForwardGestureEventWithLatencyInfo(
      const blink::WebGestureEvent& gesture_event,
      const ui::LatencyInfo& latency_info) = 0;
};

} // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_ROUTER_CLIENT_H_
