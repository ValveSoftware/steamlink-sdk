// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_GESTURE_EVENT_QUEUE_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_GESTURE_EVENT_QUEUE_H_

#include <deque>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer/timer.h"
#include "content/browser/renderer_host/event_with_latency_info.h"
#include "content/browser/renderer_host/input/tap_suppression_controller.h"
#include "content/browser/renderer_host/input/touchpad_tap_suppression_controller.h"
#include "content/browser/renderer_host/input/touchscreen_tap_suppression_controller.h"
#include "content/common/content_export.h"
#include "content/common/input/input_event_ack_state.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/gfx/transform.h"

namespace content {
class GestureEventQueueTest;
class InputRouter;
class MockRenderWidgetHost;

// Interface with which the GestureEventQueue can forward gesture events, and
// dispatch gesture event responses.
class CONTENT_EXPORT GestureEventQueueClient {
 public:
  virtual ~GestureEventQueueClient() {}

  virtual void SendGestureEventImmediately(
      const GestureEventWithLatencyInfo& event) = 0;

  virtual void OnGestureEventAck(
      const GestureEventWithLatencyInfo& event,
      InputEventAckState ack_result) = 0;
};

// Maintains WebGestureEvents in a queue before forwarding them to the renderer
// to apply a sequence of filters on them:
// 1. The sequence is filtered for bounces. A bounce is when the finger lifts
//    from the screen briefly during an in-progress scroll. Ifco this happens,
//    non-GestureScrollUpdate events are queued until the de-bounce interval
//    passes or another GestureScrollUpdate event occurs.
// 2. Unnecessary GestureFlingCancel events are filtered. These are
//    GestureFlingCancels that have no corresponding GestureFlingStart in the
//    queue.
// 3. Taps immediately after a GestureFlingCancel (caused by the same tap) are
//    filtered.
// 4. Whenever possible, events in the queue are coalesced to have as few events
//    as possible and therefore maximize the chance that the event stream can be
//    handled entirely by the compositor thread.
// Events in the queue are forwarded to the renderer one by one; i.e., each
// event is sent after receiving the ACK for previous one. The only exception is
// that if a GestureScrollUpdate is followed by a GesturePinchUpdate, they are
// sent together.
// TODO(rjkroege): Possibly refactor into a filter chain:
// http://crbug.com/148443.
class CONTENT_EXPORT GestureEventQueue {
 public:
  struct CONTENT_EXPORT Config {
    Config();

    // Controls touchpad-related tap suppression, disabled by default.
    TapSuppressionController::Config touchpad_tap_suppression_config;

    // Controls touchscreen-related tap suppression, disabled by default.
    TapSuppressionController::Config touchscreen_tap_suppression_config;

    // Determines whether non-scroll gesture events are "debounced" during an
    // active scroll sequence, suppressing brief scroll interruptions.
    // Zero by default (disabled).
    base::TimeDelta debounce_interval;
  };

  // Both |client| and |touchpad_client| must outlive the GestureEventQueue.
  GestureEventQueue(GestureEventQueueClient* client,
                    TouchpadTapSuppressionControllerClient* touchpad_client,
                    const Config& config);
  ~GestureEventQueue();

  // Returns |true| if the caller should immediately forward the provided
  // |GestureEventWithLatencyInfo| argument to the renderer.
  // If this function returns false, then the event may be queued and forwared
  // at a later point.
  bool ShouldForward(const GestureEventWithLatencyInfo&);

  // Indicates that the caller has received an acknowledgement from the renderer
  // with state |ack_result| and event |type|. May send events if the queue is
  // not empty.
  void ProcessGestureAck(InputEventAckState ack_result,
                         blink::WebInputEvent::Type type,
                         const ui::LatencyInfo& latency);

  // Sets the state of the |fling_in_progress_| field to indicate that a fling
  // is definitely not in progress.
  void FlingHasBeenHalted();

  // Returns the |TouchpadTapSuppressionController| instance.
  TouchpadTapSuppressionController* GetTouchpadTapSuppressionController();

  void ForwardGestureEvent(const GestureEventWithLatencyInfo& gesture_event);

  // Whether the queue is expecting a gesture event ack.
  bool ExpectingGestureAck() const;

  bool empty() const {
    return coalesced_gesture_events_.empty() &&
           debouncing_deferral_queue_.empty();
  }

  void set_debounce_interval_time_ms_for_testing(int interval_ms) {
    debounce_interval_ = base::TimeDelta::FromMilliseconds(interval_ms);
  }

 private:
  friend class GestureEventQueueTest;
  friend class MockRenderWidgetHost;

  // TODO(mohsen): There are a bunch of ShouldForward.../ShouldDiscard...
  // methods that are getting confusing. This should be somehow fixed. Maybe
  // while refactoring GEQ: http://crbug.com/148443.

  // Inovked on the expiration of the debounce interval to release
  // deferred events.
  void SendScrollEndingEventsNow();

  // Returns |true| if the given GestureFlingCancel should be discarded
  // as unnecessary.
  bool ShouldDiscardFlingCancelEvent(
      const GestureEventWithLatencyInfo& gesture_event) const;

  // Returns |true| if the only event in the queue is the current event and
  // hence that event should be handled now.
  bool ShouldHandleEventNow() const;

  // Merge or append a GestureScrollUpdate or GesturePinchUpdate into
  // the coalescing queue.
  void MergeOrInsertScrollAndPinchEvent(
      const GestureEventWithLatencyInfo& gesture_event);

  // Sub-filter for removing bounces from in-progress scrolls.
  bool ShouldForwardForBounceReduction(
      const GestureEventWithLatencyInfo& gesture_event);

  // Sub-filter for removing unnecessary GestureFlingCancels.
  bool ShouldForwardForGFCFiltering(
      const GestureEventWithLatencyInfo& gesture_event) const;

  // Sub-filter for suppressing taps immediately after a GestureFlingCancel.
  bool ShouldForwardForTapSuppression(
      const GestureEventWithLatencyInfo& gesture_event);

  // Puts the events in a queue to forward them one by one; i.e., forward them
  // whenever ACK for previous event is received. This queue also tries to
  // coalesce events as much as possible.
  bool ShouldForwardForCoalescing(
      const GestureEventWithLatencyInfo& gesture_event);

  // Whether the event_in_queue is GesturePinchUpdate or
  // GestureScrollUpdate and it has the same modifiers as the
  // new event.
  bool ShouldTryMerging(
      const GestureEventWithLatencyInfo& new_event,
      const GestureEventWithLatencyInfo& event_in_queue)const;

  // Returns the transform matrix corresponding to the gesture event.
  // Assumes the gesture event sent is either GestureScrollUpdate or
  // GesturePinchUpdate. Returns the identity matrix otherwise.
  gfx::Transform GetTransformForEvent(
      const GestureEventWithLatencyInfo& gesture_event) const;

  // The number of sent events for which we're awaiting an ack.  These events
  // remain at the head of the queue until ack'ed.
  size_t EventsInFlightCount() const;

  // The receiver of all forwarded gesture events.
  GestureEventQueueClient* client_;

  // True if a GestureFlingStart is in progress on the renderer or
  // queued without a subsequent queued GestureFlingCancel event.
  bool fling_in_progress_;

  // True if a GestureScrollUpdate sequence is in progress.
  bool scrolling_in_progress_;

  // True if two related gesture events were sent before without waiting
  // for an ACK, so the next gesture ACK should be ignored.
  bool ignore_next_ack_;

  // An object tracking the state of touchpad on the delivery of mouse events to
  // the renderer to filter mouse immediately after a touchpad fling canceling
  // tap.
  // TODO(mohsen): Move touchpad tap suppression out of GestureEventQueue since
  // GEQ is meant to only be used for touchscreen gesture events.
  TouchpadTapSuppressionController touchpad_tap_suppression_controller_;

  // An object tracking the state of touchscreen on the delivery of gesture tap
  // events to the renderer to filter taps immediately after a touchscreen fling
  // canceling tap.
  TouchscreenTapSuppressionController touchscreen_tap_suppression_controller_;

  typedef std::deque<GestureEventWithLatencyInfo> GestureQueue;

  // Queue of coalesced gesture events not yet sent to the renderer. If
  // |ignore_next_ack_| is false, then the event at the front of the queue has
  // been sent and is awaiting an ACK, and all other events have yet to be sent.
  // If |ignore_next_ack_| is true, then the two events at the front of the
  // queue have been sent, and the second is awaiting an ACK. All other events
  // have yet to be sent.
  GestureQueue coalesced_gesture_events_;

  // Timer to release a previously deferred gesture event.
  base::OneShotTimer<GestureEventQueue> debounce_deferring_timer_;

  // Queue of events that have been deferred for debounce.
  GestureQueue debouncing_deferral_queue_;

  // Time window in which to debounce scroll/fling ends. Note that an interval
  // of zero effectively disables debouncing.
  base::TimeDelta debounce_interval_;

  DISALLOW_COPY_AND_ASSIGN(GestureEventQueue);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_GESTURE_EVENT_QUEUE_H_
