// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_EVENT_QUEUE_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_EVENT_QUEUE_H_

#include <deque>
#include <map>

#include "base/basictypes.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/event_with_latency_info.h"
#include "content/common/content_export.h"
#include "content/common/input/input_event_ack_state.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/gfx/geometry/point_f.h"

namespace content {

class CoalescedWebTouchEvent;

// Interface with which TouchEventQueue can forward touch events, and dispatch
// touch event responses.
class CONTENT_EXPORT TouchEventQueueClient {
 public:
  virtual ~TouchEventQueueClient() {}

  virtual void SendTouchEventImmediately(
      const TouchEventWithLatencyInfo& event) = 0;

  virtual void OnTouchEventAck(
      const TouchEventWithLatencyInfo& event,
      InputEventAckState ack_result) = 0;
};

// A queue for throttling and coalescing touch-events.
class CONTENT_EXPORT TouchEventQueue {
 public:
  // Different ways of dealing with touch events during scrolling.
  // TODO(rbyers): Remove this once we're confident that touch move absorption
  // is OK. http://crbug.com/350430
  enum TouchScrollingMode {
    // Send a touchcancel on scroll start and no further touch events for the
    // duration of the scroll.  Chrome Android's traditional behavior.
    TOUCH_SCROLLING_MODE_TOUCHCANCEL,
    // Send touchmove events throughout a scroll, blocking on each ACK and
    // using the disposition to determine whether a scroll update should be
    // sent.  Mobile Safari's default overflow scroll behavior.
    TOUCH_SCROLLING_MODE_SYNC_TOUCHMOVE,
    // Send touchmove events throughout a scroll, but throttle sending and
    // ignore the ACK as long as scrolling remains possible.  Unconsumed scroll
    // events return touchmove events to being dispatched synchronously.
    TOUCH_SCROLLING_MODE_ASYNC_TOUCHMOVE,
    TOUCH_SCROLLING_MODE_DEFAULT = TOUCH_SCROLLING_MODE_ASYNC_TOUCHMOVE
  };

  struct CONTENT_EXPORT Config {
    Config();

    // Determines the bounds of the (square) touchmove slop suppression region.
    // Defaults to 0 (disabled).
    double touchmove_slop_suppression_length_dips;

    // Whether the touchmove slop suppression region is boundary inclusive.
    // Defaults to true.
    // TODO(jdduke): Remove when unified GR enabled, crbug.com/332418.
    bool touchmove_slop_suppression_region_includes_boundary;

    // Determines the type of touch scrolling.
    // Defaults to TouchEventQueue:::TOUCH_SCROLLING_MODE_DEFAULT.
    TouchEventQueue::TouchScrollingMode touch_scrolling_mode;

    // Controls whether touch ack timeouts will trigger touch cancellation.
    // Defaults to 200ms.
    base::TimeDelta touch_ack_timeout_delay;

    // Whether the platform supports touch ack timeout behavior.
    // Defaults to false (disabled).
    bool touch_ack_timeout_supported;
  };

  // The |client| must outlive the TouchEventQueue.
  TouchEventQueue(TouchEventQueueClient* client, const Config& config);

  ~TouchEventQueue();

  // Adds an event to the queue. The event may be coalesced with previously
  // queued events (e.g. consecutive touch-move events can be coalesced into a
  // single touch-move event). The event may also be immediately forwarded to
  // the renderer (e.g. when there are no other queued touch event).
  void QueueEvent(const TouchEventWithLatencyInfo& event);

  // Notifies the queue that a touch-event has been processed by the renderer.
  // At this point, the queue may send one or more gesture events and/or
  // additional queued touch-events to the renderer.
  void ProcessTouchAck(InputEventAckState ack_result,
                       const ui::LatencyInfo& latency_info);

  // When GestureScrollBegin is received, we send a touch cancel to renderer,
  // route all the following touch events directly to client, and ignore the
  // ack for the touch cancel. When Gesture{ScrollEnd,FlingStart} is received,
  // resume the normal flow of sending touch events to the renderer.
  void OnGestureScrollEvent(const GestureEventWithLatencyInfo& gesture_event);

  void OnGestureEventAck(
      const GestureEventWithLatencyInfo& event,
      InputEventAckState ack_result);

  // Notifies the queue whether the renderer has at least one touch handler.
  void OnHasTouchEventHandlers(bool has_handlers);

  // Returns whether the currently pending touch event (waiting ACK) is for
  // a touch start event.
  bool IsPendingAckTouchStart() const;

  // Sets whether a delayed touch ack will cancel and flush the current
  // touch sequence. Note that, if the timeout was previously disabled, enabling
  // it will take effect only for the following touch sequence.
  void SetAckTimeoutEnabled(bool enabled);

  bool empty() const WARN_UNUSED_RESULT {
    return touch_queue_.empty();
  }

  size_t size() const {
    return touch_queue_.size();
  }

  bool ack_timeout_enabled() const {
    return ack_timeout_enabled_;
  }

  bool has_handlers() const {
    return touch_filtering_state_ != DROP_ALL_TOUCHES;
  }

 private:
  class TouchTimeoutHandler;
  class TouchMoveSlopSuppressor;
  friend class TouchTimeoutHandler;
  friend class TouchEventQueueTest;

  bool HasPendingAsyncTouchMoveForTesting() const;
  bool IsTimeoutRunningForTesting() const;
  const TouchEventWithLatencyInfo& GetLatestEventForTesting() const;

  // Empties the queue of touch events. This may result in any number of gesture
  // events being sent to the renderer.
  void FlushQueue();

  // Walks the queue, checking each event with |FilterBeforeForwarding()|.
  // If allowed, forwards the touch event and stops processing further events.
  // Otherwise, acks the event with |INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS|.
  void TryForwardNextEventToRenderer();

  // Forwards the event at the head of the queue to the renderer.
  void ForwardNextEventToRenderer();

  // Pops the touch-event from the head of the queue and acks it to the client.
  void PopTouchEventToClient(InputEventAckState ack_result);

  // Pops the touch-event from the top of the queue and acks it to the client,
  // updating the event with |renderer_latency_info|.
  void PopTouchEventToClient(InputEventAckState ack_result,
                             const ui::LatencyInfo& renderer_latency_info);

  // Ack all coalesced events in |acked_event| to the client with |ack_result|.
  void AckTouchEventToClient(InputEventAckState ack_result,
                             scoped_ptr<CoalescedWebTouchEvent> acked_event);

  // Safely pop the head of the queue.
  scoped_ptr<CoalescedWebTouchEvent> PopTouchEvent();

  // Dispatch |touch| to the client.
  void SendTouchEventImmediately(const TouchEventWithLatencyInfo& touch);

  enum PreFilterResult {
    ACK_WITH_NO_CONSUMER_EXISTS,
    ACK_WITH_NOT_CONSUMED,
    FORWARD_TO_RENDERER,
  };
  // Filter touches prior to forwarding to the renderer, e.g., if the renderer
  // has no touch handler.
  PreFilterResult FilterBeforeForwarding(const blink::WebTouchEvent& event);
  void ForwardToRenderer(const TouchEventWithLatencyInfo& event);
  void UpdateTouchAckStates(const blink::WebTouchEvent& event,
                            InputEventAckState ack_result);
  bool AllTouchAckStatesHaveState(InputEventAckState ack_state) const;


  // Handles touch event forwarding and ack'ed event dispatch.
  TouchEventQueueClient* client_;

  typedef std::deque<CoalescedWebTouchEvent*> TouchQueue;
  TouchQueue touch_queue_;

  // Maintain the ACK status for each touch point.
  typedef std::map<int, InputEventAckState> TouchPointAckStates;
  TouchPointAckStates touch_ack_states_;

  // Position of the first touch in the most recent sequence forwarded to the
  // client.
  gfx::PointF touch_sequence_start_position_;

  // Used to defer touch forwarding when ack dispatch triggers |QueueEvent()|.
  // If not NULL, |dispatching_touch_ack_| is the touch event of which the ack
  // is being dispatched.
  const CoalescedWebTouchEvent* dispatching_touch_ack_;

  // Used to prevent touch timeout scheduling if we receive a synchronous
  // ack after forwarding a touch event to the client.
  bool dispatching_touch_;

  enum TouchFilteringState {
    FORWARD_ALL_TOUCHES,           // Don't filter at all - the default.
    FORWARD_TOUCHES_UNTIL_TIMEOUT, // Don't filter unless we get an ACK timeout.
    DROP_TOUCHES_IN_SEQUENCE,      // Filter all events until a new touch
                                   // sequence is received.
    DROP_ALL_TOUCHES,              // Filter all events, e.g., no touch handler.
    TOUCH_FILTERING_STATE_DEFAULT = FORWARD_ALL_TOUCHES,
  };
  TouchFilteringState touch_filtering_state_;

  // Optional handler for timed-out touch event acks.
  bool ack_timeout_enabled_;
  scoped_ptr<TouchTimeoutHandler> timeout_handler_;

  // Suppression of TouchMove's within a slop region when a sequence has not yet
  // been preventDefaulted.
  scoped_ptr<TouchMoveSlopSuppressor> touchmove_slop_suppressor_;

  // Whether touch events should remain buffered and dispatched asynchronously
  // while a scroll sequence is active.  In this mode, touchmove's are throttled
  // and ack'ed immediately, but remain buffered in |pending_async_touchmove_|
  // until a sufficient time period has elapsed since the last sent touch event.
  // For details see the design doc at http://goo.gl/lVyJAa.
  bool send_touch_events_async_;
  bool needs_async_touchmove_for_outer_slop_region_;
  scoped_ptr<TouchEventWithLatencyInfo> pending_async_touchmove_;
  double last_sent_touch_timestamp_sec_;

  // How touch events are handled during scrolling.  For now this is a global
  // setting for experimentation, but we may evolve it into an app-controlled
  // mode.
  const TouchScrollingMode touch_scrolling_mode_;

  DISALLOW_COPY_AND_ASSIGN(TouchEventQueue);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_EVENT_QUEUE_H_
