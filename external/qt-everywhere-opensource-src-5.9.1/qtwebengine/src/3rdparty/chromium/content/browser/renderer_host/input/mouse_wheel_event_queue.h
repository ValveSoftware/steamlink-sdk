// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_MOUSE_WHEEL_EVENT_QUEUE_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_MOUSE_WHEEL_EVENT_QUEUE_H_

#include <deque>
#include <memory>

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/browser/renderer_host/event_with_latency_info.h"
#include "content/common/content_export.h"
#include "content/common/input/input_event_ack_state.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"

namespace content {

// The duration in which a ScrollEnd will be sent after the last
// ScrollUpdate was sent for wheel based gesture scrolls.
// This value is used only if |enable_scroll_latching_| is true.
const int64_t kDefaultWheelScrollLatchingTransactionMs = 100;

class QueuedWebMouseWheelEvent;

// Interface with which MouseWheelEventQueue can forward mouse wheel events,
// and dispatch mouse wheel event responses.
class CONTENT_EXPORT MouseWheelEventQueueClient {
 public:
  virtual ~MouseWheelEventQueueClient() {}

  virtual void SendMouseWheelEventImmediately(
      const MouseWheelEventWithLatencyInfo& event) = 0;
  virtual void ForwardGestureEventWithLatencyInfo(
      const blink::WebGestureEvent& event,
      const ui::LatencyInfo& latency_info) = 0;
  virtual void OnMouseWheelEventAck(const MouseWheelEventWithLatencyInfo& event,
                                    InputEventAckState ack_result) = 0;
};

// A queue for throttling and coalescing mouse wheel events.
class CONTENT_EXPORT MouseWheelEventQueue {
 public:
  // The |client| must outlive the MouseWheelEventQueue. |send_gestures|
  // indicates whether mouse wheel events should generate
  // Scroll[Begin|Update|End] on unhandled acknowledge events.
  // |scroll_transaction_ms| is the duration in which the
  // ScrollEnd should be sent after a ScrollUpdate.
  MouseWheelEventQueue(MouseWheelEventQueueClient* client,
                       bool enable_scroll_latching);

  ~MouseWheelEventQueue();

  // Adds an event to the queue. The event may be coalesced with previously
  // queued events (e.g. consecutive mouse-wheel events can be coalesced into a
  // single mouse-wheel event). The event may also be immediately forwarded to
  // the renderer (e.g. when there are no other queued mouse-wheel event).
  void QueueEvent(const MouseWheelEventWithLatencyInfo& event);

  // Notifies the queue that a mouse wheel event has been processed by the
  // renderer.
  void ProcessMouseWheelAck(InputEventAckState ack_result,
                            const ui::LatencyInfo& latency_info);

  // When GestureScrollBegin is received, and it is a different source
  // than mouse wheels terminate the current GestureScroll if there is one.
  // When Gesture{ScrollEnd,FlingStart} is received, resume generating
  // gestures.
  void OnGestureScrollEvent(const GestureEventWithLatencyInfo& gesture_event);

  bool has_pending() const WARN_UNUSED_RESULT {
    return !wheel_queue_.empty() || event_sent_for_gesture_ack_;
  }

  size_t queued_size() const { return wheel_queue_.size(); }
  bool event_in_flight() const {
    return event_sent_for_gesture_ack_ != nullptr;
  }

 private:
  void TryForwardNextEventToRenderer();
  void SendScrollEnd(blink::WebGestureEvent update_event, bool synthetic);
  void SendScrollBegin(const blink::WebGestureEvent& gesture_update,
                       bool synthetic);

  MouseWheelEventQueueClient* client_;
  base::OneShotTimer scroll_end_timer_;

  std::deque<std::unique_ptr<QueuedWebMouseWheelEvent>> wheel_queue_;
  std::unique_ptr<QueuedWebMouseWheelEvent> event_sent_for_gesture_ack_;

  // True if a non-synthetic GSB needs to be sent before a GSU is sent.
  bool needs_scroll_begin_;

  // True if a non-synthetic GSE needs to be sent because a non-synthetic
  // GSB has been sent in the past.
  bool needs_scroll_end_;

  // True if the touchpad and wheel scroll latching is enabled.
  bool enable_scroll_latching_;

  int64_t scroll_transaction_ms_;
  blink::WebGestureDevice scrolling_device_;

  DISALLOW_COPY_AND_ASSIGN(MouseWheelEventQueue);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_MOUSE_WHEEL_EVENT_QUEUE_H_
