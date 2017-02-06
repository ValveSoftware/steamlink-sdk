// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_EVENT_QUEUE_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_EVENT_QUEUE_H_

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <list>

#include "base/macros.h"
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

  virtual void OnFilteringTouchEvent(
      const blink::WebTouchEvent& touch_event) = 0;
};

// A queue for throttling and coalescing touch-events.
class CONTENT_EXPORT TouchEventQueue {
 public:
  struct CONTENT_EXPORT Config {
    Config();

    // Touch ack timeout delay for desktop sites. If zero, timeout behavior
    // is disabled for such sites. Defaults to 200ms.
    base::TimeDelta desktop_touch_ack_timeout_delay;

    // Touch ack timeout delay for mobile sites. If zero, timeout behavior
    // is disabled for such sites. Defaults to 1000ms.
    base::TimeDelta mobile_touch_ack_timeout_delay;

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

  // Insert a TouchScrollStarted event in the queue ahead of all not-in-flight
  // events.
  void PrependTouchScrollNotification();

  // Notifies the queue that a touch-event has been processed by the renderer.
  // At this point, if the ack is for async touchmove, remove the uncancelable
  // touchmove from the front of the queue and decide if it should dispatch the
  // next pending async touch move event, otherwise the queue may send one or
  // more gesture events and/or additional queued touch-events to the renderer.
  void ProcessTouchAck(InputEventAckState ack_result,
                       const ui::LatencyInfo& latency_info,
                       const uint32_t unique_touch_event_id);

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

  // Sets whether the current site has a mobile friendly viewport. This
  // determines which ack timeout delay will be used for *future* touch events.
  // The default assumption is that the site is *not* mobile-optimized.
  void SetIsMobileOptimizedSite(bool mobile_optimized_site);

  // Whether ack timeout behavior is supported and enabled for the current site.
  bool IsAckTimeoutEnabled() const;

  bool empty() const WARN_UNUSED_RESULT {
    return touch_queue_.empty();
  }

  size_t size() const {
    return touch_queue_.size();
  }

  bool has_handlers() const { return has_handlers_; }

  size_t uncancelable_touch_moves_pending_ack_count() const {
    return ack_pending_async_touchmove_ids_.size();
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

  // Ack all coalesced events at the head of the queue to the client with
  // |ack_result|, updating the acked events with |optional_latency_info| if it
  // exists, and popping the head of the queue.
  void AckTouchEventToClient(InputEventAckState ack_result,
                             const ui::LatencyInfo* optional_latency_info);

  // Dispatch |touch| to the client. Before dispatching, updates pointer
  // states in touchmove events for pointers that have not changed position.
  void SendTouchEventImmediately(TouchEventWithLatencyInfo* touch);

  enum PreFilterResult {
    ACK_WITH_NO_CONSUMER_EXISTS,
    ACK_WITH_NOT_CONSUMED,
    FORWARD_TO_RENDERER,
  };
  // Filter touches prior to forwarding to the renderer, e.g., if the renderer
  // has no touch handler.
  PreFilterResult FilterBeforeForwarding(const blink::WebTouchEvent& event);
  void ForwardToRenderer(const TouchEventWithLatencyInfo& event);
  void UpdateTouchConsumerStates(const blink::WebTouchEvent& event,
                                 InputEventAckState ack_result);
  void FlushPendingAsyncTouchmove();

  // Handles touch event forwarding and ack'ed event dispatch.
  TouchEventQueueClient* client_;

  typedef std::list<CoalescedWebTouchEvent*> TouchQueue;
  TouchQueue touch_queue_;

  // Position of the first touch in the most recent sequence forwarded to the
  // client.
  gfx::PointF touch_sequence_start_position_;

  // Used to defer touch forwarding when ack dispatch triggers |QueueEvent()|.
  // True within the scope of |AckTouchEventToClient()|.
  bool dispatching_touch_ack_;

  // Used to prevent touch timeout scheduling and increase the count for async
  // touchmove if we receive a synchronous ack after forwarding a touch event
  // to the client.
  bool dispatching_touch_;

  // Whether the renderer has at least one touch handler.
  bool has_handlers_;

  // Whether any pointer in the touch sequence reported having a consumer.
  bool has_handler_for_current_sequence_;

  // Whether to allow any remaining touches for the current sequence. Note that
  // this is a stricter condition than an empty |touch_consumer_states_|, as it
  // also prevents forwarding of touchstart events for new pointers in the
  // current sequence. This is only used when the event is synthetically
  // cancelled after a touch timeout.
  bool drop_remaining_touches_in_sequence_;

  // Optional handler for timed-out touch event acks.
  std::unique_ptr<TouchTimeoutHandler> timeout_handler_;

  // Suppression of TouchMove's within a slop region when a sequence has not yet
  // been preventDefaulted.
  std::unique_ptr<TouchMoveSlopSuppressor> touchmove_slop_suppressor_;

  // Whether touch events should remain buffered and dispatched asynchronously
  // while a scroll sequence is active.  In this mode, touchmove's are throttled
  // and ack'ed immediately, but remain buffered in |pending_async_touchmove_|
  // until a sufficient time period has elapsed since the last sent touch event.
  // For details see the design doc at http://goo.gl/lVyJAa.
  bool send_touch_events_async_;
  std::unique_ptr<TouchEventWithLatencyInfo> pending_async_touchmove_;

  // For uncancelable touch moves, not only we send a fake ack, but also a real
  // ack from render, which we use to decide when to send the next async
  // touchmove. This can help avoid the touch event queue keep growing when
  // render handles touchmove slow. We use a queue
  // ack_pending_async_touchmove_ids to store the recent dispatched
  // uncancelable touchmoves which are still waiting for their acks back from
  // render. We do not put them back to the front the touch_event_queue any
  // more.
  std::deque<uint32_t> ack_pending_async_touchmove_ids_;

  double last_sent_touch_timestamp_sec_;

  // Event is saved to compare pointer positions for new touchmove events.
  std::unique_ptr<blink::WebTouchEvent> last_sent_touchevent_;

  DISALLOW_COPY_AND_ASSIGN(TouchEventQueue);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_EVENT_QUEUE_H_
