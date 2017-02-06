// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touch_event_queue.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/renderer_host/input/timeout_monitor.h"
#include "content/common/input/web_touch_event_traits.h"
#include "ui/gfx/geometry/point_f.h"

using blink::WebInputEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;
using ui::LatencyInfo;

namespace content {
namespace {

// Time interval at which touchmove events will be forwarded to the client while
// scrolling is active and possible.
const double kAsyncTouchMoveIntervalSec = .2;

// A sanity check on touches received to ensure that touch movement outside
// the platform slop region will cause scrolling.
const double kMaxConceivablePlatformSlopRegionLengthDipsSquared = 60. * 60.;

TouchEventWithLatencyInfo ObtainCancelEventForTouchEvent(
    const TouchEventWithLatencyInfo& event_to_cancel) {
  TouchEventWithLatencyInfo event = event_to_cancel;
  WebTouchEventTraits::ResetTypeAndTouchStates(
      WebInputEvent::TouchCancel,
      // TODO(rbyers): Shouldn't we use a fresh timestamp?
      event.event.timeStampSeconds,
      &event.event);
  return event;
}

bool ShouldTouchTriggerTimeout(const WebTouchEvent& event) {
  return (event.type == WebInputEvent::TouchStart ||
          event.type == WebInputEvent::TouchMove) &&
         event.dispatchType == WebInputEvent::Blocking;
}

// Compare all properties of touch points to determine the state.
bool HasPointChanged(const WebTouchPoint& point_1,
    const WebTouchPoint& point_2) {
  DCHECK_EQ(point_1.id, point_2.id);
  if (point_1.screenPosition != point_2.screenPosition ||
      point_1.position != point_2.position ||
      point_1.radiusX != point_2.radiusX ||
      point_1.radiusY != point_2.radiusY ||
      point_1.rotationAngle != point_2.rotationAngle ||
      point_1.force != point_2.force ||
      point_1.tiltX != point_2.tiltX ||
      point_1.tiltY != point_2.tiltY) {
    return true;
  }
  return false;
}

}  // namespace


// Cancels a touch sequence if a touchstart or touchmove ack response is
// sufficiently delayed.
class TouchEventQueue::TouchTimeoutHandler {
 public:
  TouchTimeoutHandler(TouchEventQueue* touch_queue,
                      base::TimeDelta desktop_timeout_delay,
                      base::TimeDelta mobile_timeout_delay)
      : touch_queue_(touch_queue),
        desktop_timeout_delay_(desktop_timeout_delay),
        mobile_timeout_delay_(mobile_timeout_delay),
        use_mobile_timeout_(false),
        pending_ack_state_(PENDING_ACK_NONE),
        timeout_monitor_(base::Bind(&TouchTimeoutHandler::OnTimeOut,
                                    base::Unretained(this))),
        enabled_(true),
        enabled_for_current_sequence_(false),
        sequence_awaiting_uma_update_(false),
        sequence_using_mobile_timeout_(false) {
    SetUseMobileTimeout(false);
  }

  ~TouchTimeoutHandler() {
    LogSequenceEndForUMAIfNecessary(false);
  }

  void StartIfNecessary(const TouchEventWithLatencyInfo& event) {
    if (pending_ack_state_ != PENDING_ACK_NONE)
      return;

    if (!enabled_)
      return;

    const base::TimeDelta timeout_delay = GetTimeoutDelay();
    if (timeout_delay.is_zero())
      return;

    if (!ShouldTouchTriggerTimeout(event.event))
      return;

    if (WebTouchEventTraits::IsTouchSequenceStart(event.event)) {
      LogSequenceStartForUMA();
      enabled_for_current_sequence_ = true;
    }

    if (!enabled_for_current_sequence_)
      return;

    timeout_event_ = event;
    timeout_monitor_.Restart(timeout_delay);
  }

  bool ConfirmTouchEvent(InputEventAckState ack_result) {
    switch (pending_ack_state_) {
      case PENDING_ACK_NONE:
        if (ack_result == INPUT_EVENT_ACK_STATE_CONSUMED)
          enabled_for_current_sequence_ = false;
        timeout_monitor_.Stop();
        return false;
      case PENDING_ACK_ORIGINAL_EVENT:
        if (AckedTimeoutEventRequiresCancel(ack_result)) {
          SetPendingAckState(PENDING_ACK_CANCEL_EVENT);
          TouchEventWithLatencyInfo cancel_event =
              ObtainCancelEventForTouchEvent(timeout_event_);
          touch_queue_->SendTouchEventImmediately(&cancel_event);
        } else {
          SetPendingAckState(PENDING_ACK_NONE);
          touch_queue_->UpdateTouchConsumerStates(timeout_event_.event,
                                                  ack_result);
        }
        return true;
      case PENDING_ACK_CANCEL_EVENT:
        SetPendingAckState(PENDING_ACK_NONE);
        return true;
    }
    return false;
  }

  bool FilterEvent(const WebTouchEvent& event) {
    if (!HasTimeoutEvent())
      return false;

    if (WebTouchEventTraits::IsTouchSequenceStart(event)) {
      // If a new sequence is observed while we're still waiting on the
      // timed-out sequence response, also count the new sequence as timed-out.
      LogSequenceStartForUMA();
      LogSequenceEndForUMAIfNecessary(true);
    }

    return true;
  }

  void SetEnabled(bool enabled) {
    if (enabled_ == enabled)
      return;

    enabled_ = enabled;

    if (enabled_)
      return;

    enabled_for_current_sequence_ = false;
    // Only reset the |timeout_handler_| if the timer is running and has not
    // yet timed out. This ensures that an already timed out sequence is
    // properly flushed by the handler.
    if (IsTimeoutTimerRunning()) {
      pending_ack_state_ = PENDING_ACK_NONE;
      timeout_monitor_.Stop();
    }
  }

  void SetUseMobileTimeout(bool use_mobile_timeout) {
    use_mobile_timeout_ = use_mobile_timeout;
  }

  bool IsTimeoutTimerRunning() const { return timeout_monitor_.IsRunning(); }

  bool IsEnabled() const {
    return enabled_ && !GetTimeoutDelay().is_zero();
  }

 private:
  enum PendingAckState {
    PENDING_ACK_NONE,
    PENDING_ACK_ORIGINAL_EVENT,
    PENDING_ACK_CANCEL_EVENT,
  };

  void OnTimeOut() {
    LogSequenceEndForUMAIfNecessary(true);
    SetPendingAckState(PENDING_ACK_ORIGINAL_EVENT);
    touch_queue_->FlushQueue();
  }

  // Skip a cancel event if the timed-out event had no consumer and was the
  // initial event in the gesture.
  bool AckedTimeoutEventRequiresCancel(InputEventAckState ack_result) const {
    DCHECK(HasTimeoutEvent());
    if (ack_result != INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS)
      return true;
    return !WebTouchEventTraits::IsTouchSequenceStart(timeout_event_.event);
  }

  void SetPendingAckState(PendingAckState new_pending_ack_state) {
    DCHECK_NE(pending_ack_state_, new_pending_ack_state);
    switch (new_pending_ack_state) {
      case PENDING_ACK_ORIGINAL_EVENT:
        DCHECK_EQ(pending_ack_state_, PENDING_ACK_NONE);
        TRACE_EVENT_ASYNC_BEGIN0("input", "TouchEventTimeout", this);
        break;
      case PENDING_ACK_CANCEL_EVENT:
        DCHECK_EQ(pending_ack_state_, PENDING_ACK_ORIGINAL_EVENT);
        DCHECK(!timeout_monitor_.IsRunning());
        DCHECK(touch_queue_->empty());
        TRACE_EVENT_ASYNC_STEP_INTO0(
            "input", "TouchEventTimeout", this, "CancelEvent");
        break;
      case PENDING_ACK_NONE:
        DCHECK(!timeout_monitor_.IsRunning());
        DCHECK(touch_queue_->empty());
        TRACE_EVENT_ASYNC_END0("input", "TouchEventTimeout", this);
        break;
    }
    pending_ack_state_ = new_pending_ack_state;
  }

  void LogSequenceStartForUMA() {
    // Always flush any unlogged entries before starting a new one.
    LogSequenceEndForUMAIfNecessary(false);
    sequence_awaiting_uma_update_ = true;
    sequence_using_mobile_timeout_ = use_mobile_timeout_;
  }

  void LogSequenceEndForUMAIfNecessary(bool timed_out) {
    if (!sequence_awaiting_uma_update_)
      return;

    sequence_awaiting_uma_update_ = false;

    if (sequence_using_mobile_timeout_) {
      UMA_HISTOGRAM_BOOLEAN("Event.Touch.TimedOutOnMobileSite", timed_out);
    } else {
      UMA_HISTOGRAM_BOOLEAN("Event.Touch.TimedOutOnDesktopSite", timed_out);
    }
  }

  base::TimeDelta GetTimeoutDelay() const {
    return use_mobile_timeout_ ? mobile_timeout_delay_ : desktop_timeout_delay_;
  }

  bool HasTimeoutEvent() const {
    return pending_ack_state_ != PENDING_ACK_NONE;
  }


  TouchEventQueue* touch_queue_;

  // How long to wait on a touch ack before cancelling the touch sequence.
  const base::TimeDelta desktop_timeout_delay_;
  const base::TimeDelta mobile_timeout_delay_;
  bool use_mobile_timeout_;

  // The touch event source for which we expect the next ack.
  PendingAckState pending_ack_state_;

  // The event for which the ack timeout is triggered.
  TouchEventWithLatencyInfo timeout_event_;

  // Provides timeout-based callback behavior.
  TimeoutMonitor timeout_monitor_;

  bool enabled_;
  bool enabled_for_current_sequence_;

  // Bookkeeping to classify and log whether a touch sequence times out.
  bool sequence_awaiting_uma_update_;
  bool sequence_using_mobile_timeout_;
};

// Provides touchmove slop suppression for a touch sequence until a
// (unprevented) touch will trigger immediate scrolling.
class TouchEventQueue::TouchMoveSlopSuppressor {
 public:
  TouchMoveSlopSuppressor() : suppressing_touchmoves_(false) {}

  bool FilterEvent(const WebTouchEvent& event) {
    if (WebTouchEventTraits::IsTouchSequenceStart(event)) {
      suppressing_touchmoves_ = true;
      touch_start_location_ = gfx::PointF(event.touches[0].position);
    }

    if (event.type == WebInputEvent::TouchEnd ||
        event.type == WebInputEvent::TouchCancel)
      suppressing_touchmoves_ = false;

    if (event.type != WebInputEvent::TouchMove)
      return false;

    if (suppressing_touchmoves_) {
      if (event.touchesLength > 1) {
        suppressing_touchmoves_ = false;
      } else if (event.movedBeyondSlopRegion) {
        suppressing_touchmoves_ = false;
      } else {
        // No sane slop region should be larger than 60 DIPs.
        DCHECK_LT((gfx::PointF(event.touches[0].position) -
                   touch_start_location_).LengthSquared(),
                  kMaxConceivablePlatformSlopRegionLengthDipsSquared);
      }
    }

    return suppressing_touchmoves_;
  }

  void ConfirmTouchEvent(InputEventAckState ack_result) {
    if (ack_result == INPUT_EVENT_ACK_STATE_CONSUMED)
      suppressing_touchmoves_ = false;
  }

  bool suppressing_touchmoves() const { return suppressing_touchmoves_; }

 private:
  bool suppressing_touchmoves_;

  // Sanity check that the upstream touch provider is properly reporting whether
  // the touch sequence will cause scrolling.
  gfx::PointF touch_start_location_;

  DISALLOW_COPY_AND_ASSIGN(TouchMoveSlopSuppressor);
};

// This class represents a single coalesced touch event. However, it also keeps
// track of all the original touch-events that were coalesced into a single
// event. The coalesced event is forwarded to the renderer, while the original
// touch-events are sent to the Client (on ACK for the coalesced event) so that
// the Client receives the event with their original timestamp.
class CoalescedWebTouchEvent {
 public:
  CoalescedWebTouchEvent(const TouchEventWithLatencyInfo& event,
                         bool suppress_client_ack)
      : coalesced_event_(event), suppress_client_ack_(suppress_client_ack) {
    TRACE_EVENT_ASYNC_BEGIN0("input", "TouchEventQueue::QueueEvent", this);
  }

  ~CoalescedWebTouchEvent() {
    TRACE_EVENT_ASYNC_END0("input", "TouchEventQueue::QueueEvent", this);
  }

  // Coalesces the event with the existing event if possible. Returns whether
  // the event was coalesced.
  bool CoalesceEventIfPossible(
      const TouchEventWithLatencyInfo& event_with_latency) {
    if (suppress_client_ack_)
      return false;

    if (!coalesced_event_.CanCoalesceWith(event_with_latency))
      return false;

    // Addition of the first event to |uncoaleseced_events_to_ack_| is deferred
    // until the first coalesced event, optimizing the (common) case where the
    // event is not coalesced at all.
    if (uncoaleseced_events_to_ack_.empty())
      uncoaleseced_events_to_ack_.push_back(coalesced_event_);

    TRACE_EVENT_INSTANT0(
        "input", "TouchEventQueue::MoveCoalesced", TRACE_EVENT_SCOPE_THREAD);
    coalesced_event_.CoalesceWith(event_with_latency);
    uncoaleseced_events_to_ack_.push_back(event_with_latency);
    DCHECK_GE(uncoaleseced_events_to_ack_.size(), 2U);
    return true;
  }

  void DispatchAckToClient(InputEventAckState ack_result,
                           const ui::LatencyInfo* optional_latency_info,
                           TouchEventQueueClient* client) {
    DCHECK(client);
    if (suppress_client_ack_)
      return;

    if (uncoaleseced_events_to_ack_.empty()) {
      if (optional_latency_info)
        coalesced_event_.latency.AddNewLatencyFrom(*optional_latency_info);
      client->OnTouchEventAck(coalesced_event_, ack_result);
      return;
    }

    DCHECK_GE(uncoaleseced_events_to_ack_.size(), 2U);
    for (WebTouchEventWithLatencyList::iterator
             iter = uncoaleseced_events_to_ack_.begin(),
             end = uncoaleseced_events_to_ack_.end();
         iter != end;
         ++iter) {
      if (optional_latency_info)
        iter->latency.AddNewLatencyFrom(*optional_latency_info);
      client->OnTouchEventAck(*iter, ack_result);
    }
  }

  const TouchEventWithLatencyInfo& coalesced_event() const {
    return coalesced_event_;
  }

 private:
  // This is the event that is forwarded to the renderer.
  TouchEventWithLatencyInfo coalesced_event_;

  // This is the list of the original events that were coalesced, each requiring
  // future ack dispatch to the client.
  // Note that this will be empty if no coalescing has occurred.
  typedef std::vector<TouchEventWithLatencyInfo> WebTouchEventWithLatencyList;
  WebTouchEventWithLatencyList uncoaleseced_events_to_ack_;

  bool suppress_client_ack_;

  DISALLOW_COPY_AND_ASSIGN(CoalescedWebTouchEvent);
};

TouchEventQueue::Config::Config()
    : desktop_touch_ack_timeout_delay(base::TimeDelta::FromMilliseconds(200)),
      mobile_touch_ack_timeout_delay(base::TimeDelta::FromMilliseconds(1000)),
      touch_ack_timeout_supported(false) {
}

TouchEventQueue::TouchEventQueue(TouchEventQueueClient* client,
                                 const Config& config)
    : client_(client),
      dispatching_touch_ack_(false),
      dispatching_touch_(false),
      has_handlers_(true),
      has_handler_for_current_sequence_(false),
      drop_remaining_touches_in_sequence_(false),
      touchmove_slop_suppressor_(new TouchMoveSlopSuppressor),
      send_touch_events_async_(false),
      last_sent_touch_timestamp_sec_(0) {
  DCHECK(client);
  if (config.touch_ack_timeout_supported) {
    timeout_handler_.reset(
        new TouchTimeoutHandler(this,
                                config.desktop_touch_ack_timeout_delay,
                                config.mobile_touch_ack_timeout_delay));
  }
}

TouchEventQueue::~TouchEventQueue() {
  if (!touch_queue_.empty())
    STLDeleteElements(&touch_queue_);
}

void TouchEventQueue::QueueEvent(const TouchEventWithLatencyInfo& event) {
  TRACE_EVENT0("input", "TouchEventQueue::QueueEvent");

  // If the queueing of |event| was triggered by an ack dispatch, defer
  // processing the event until the dispatch has finished.
  if (touch_queue_.empty() && !dispatching_touch_ack_) {
    // Optimization of the case without touch handlers.  Removing this path
    // yields identical results, but this avoids unnecessary allocations.
    PreFilterResult filter_result = FilterBeforeForwarding(event.event);
    if (filter_result != FORWARD_TO_RENDERER) {
      client_->OnFilteringTouchEvent(event.event);
      client_->OnTouchEventAck(event,
                               filter_result == ACK_WITH_NO_CONSUMER_EXISTS
                                   ? INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS
                                   : INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
      return;
    }

    // There is no touch event in the queue. Forward it to the renderer
    // immediately.
    touch_queue_.push_back(new CoalescedWebTouchEvent(event, false));
    ForwardNextEventToRenderer();
    return;
  }

  // If the last queued touch-event was a touch-move, and the current event is
  // also a touch-move, then the events can be coalesced into a single event.
  if (touch_queue_.size() > 1) {
    CoalescedWebTouchEvent* last_event = touch_queue_.back();
    if (last_event->CoalesceEventIfPossible(event))
      return;
  }
  touch_queue_.push_back(new CoalescedWebTouchEvent(event, false));
}

void TouchEventQueue::PrependTouchScrollNotification() {
  TRACE_EVENT0("input", "TouchEventQueue::PrependTouchScrollNotification");

  // The queue should have an in-flight event when this method is called because
  // this method is triggered by InputRouterImpl::SendGestureEvent, which is
  // triggered by TouchEventQueue::AckTouchEventToClient, which has just
  // received an ack for the in-flight event. We leave the head of the queue
  // untouched since it is the in-flight event.
  //
  // However, for the (integration) tests in RenderWidgetHostTest that trigger
  // this method indirectly, they push the TouchScrollStarted event into
  // TouchEventQueue without any way to dispatch it. Below we added a check for
  // non-empty queue to keep those tests as-is w/o exposing internals of this
  // class all the way up.
  if (!touch_queue_.empty()) {
    TouchEventWithLatencyInfo touch;
    touch.event.type = WebInputEvent::TouchScrollStarted;
    touch.event.uniqueTouchEventId = 0;
    touch.event.touchesLength = 0;
    touch.event.dispatchType = WebInputEvent::EventNonBlocking;

    auto it = touch_queue_.begin();
    DCHECK(it != touch_queue_.end());
    touch_queue_.insert(++it, new CoalescedWebTouchEvent(touch, false));
  }
}

void TouchEventQueue::ProcessTouchAck(InputEventAckState ack_result,
                                      const LatencyInfo& latency_info,
                                      const uint32_t unique_touch_event_id) {
  TRACE_EVENT0("input", "TouchEventQueue::ProcessTouchAck");

  // We receive an ack for async touchmove from render.
  if (!ack_pending_async_touchmove_ids_.empty() &&
      ack_pending_async_touchmove_ids_.front() == unique_touch_event_id) {
    // Remove the first touchmove from the ack_pending_async_touchmove queue.
    ack_pending_async_touchmove_ids_.pop_front();
    // Send the next pending async touch move once we receive all acks back.
    if (pending_async_touchmove_ && ack_pending_async_touchmove_ids_.empty()) {
      DCHECK(touch_queue_.empty());

      // Dispatch the next pending async touch move when time expires.
      if (pending_async_touchmove_->event.timeStampSeconds >=
          last_sent_touch_timestamp_sec_ + kAsyncTouchMoveIntervalSec) {
        FlushPendingAsyncTouchmove();
      }
    }
    return;
  }

  DCHECK(!dispatching_touch_ack_);
  dispatching_touch_ = false;

  if (timeout_handler_ && timeout_handler_->ConfirmTouchEvent(ack_result))
    return;

  touchmove_slop_suppressor_->ConfirmTouchEvent(ack_result);

  if (touch_queue_.empty())
    return;

  // We don't care about the ordering of the acks vs the ordering of the
  // dispatched events because we can receive the ack for event B before the ack
  // for event A even though A was sent before B. This seems to be happening
  // when, for example, A is acked from renderer but B isn't, so the ack for B
  // is synthesized "locally" in InputRouter.
  //
  // TODO(crbug.com/600773): Bring the id checks back when dispatch triggering
  // is sane.

  PopTouchEventToClient(ack_result, latency_info);
  TryForwardNextEventToRenderer();
}

void TouchEventQueue::TryForwardNextEventToRenderer() {
  DCHECK(!dispatching_touch_ack_);
  // If there are queued touch events, then try to forward them to the renderer
  // immediately, or ACK the events back to the client if appropriate.
  while (!touch_queue_.empty()) {
    const WebTouchEvent& event = touch_queue_.front()->coalesced_event().event;
    PreFilterResult filter_result = FilterBeforeForwarding(event);
    if (filter_result != FORWARD_TO_RENDERER)
      client_->OnFilteringTouchEvent(event);
    switch (filter_result) {
      case ACK_WITH_NO_CONSUMER_EXISTS:
        PopTouchEventToClient(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
        break;
      case ACK_WITH_NOT_CONSUMED:
        PopTouchEventToClient(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
        break;
      case FORWARD_TO_RENDERER:
        ForwardNextEventToRenderer();
        return;
    }
  }
}

void TouchEventQueue::ForwardNextEventToRenderer() {
  TRACE_EVENT0("input", "TouchEventQueue::ForwardNextEventToRenderer");

  DCHECK(!empty());
  DCHECK(!dispatching_touch_);
  TouchEventWithLatencyInfo touch = touch_queue_.front()->coalesced_event();

  if (send_touch_events_async_ &&
      touch.event.type == WebInputEvent::TouchMove) {
    // Throttling touchmove's in a continuous touchmove stream while scrolling
    // reduces the risk of jank. However, it's still important that the web
    // application be sent touches at key points in the gesture stream,
    // e.g., when the application slop region is exceeded or touchmove
    // coalescing fails because of different modifiers.
    bool send_touchmove_now = size() > 1;
    send_touchmove_now |= pending_async_touchmove_ &&
                          !pending_async_touchmove_->CanCoalesceWith(touch);
    send_touchmove_now |=
        ack_pending_async_touchmove_ids_.empty() &&
        (touch.event.timeStampSeconds >=
         last_sent_touch_timestamp_sec_ + kAsyncTouchMoveIntervalSec);

    if (!send_touchmove_now) {
      if (!pending_async_touchmove_) {
        pending_async_touchmove_.reset(new TouchEventWithLatencyInfo(touch));
      } else {
        DCHECK(pending_async_touchmove_->CanCoalesceWith(touch));
        pending_async_touchmove_->CoalesceWith(touch);
      }
      DCHECK_EQ(1U, size());
      PopTouchEventToClient(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
      // It's possible (though unlikely) that ack'ing the current touch will
      // trigger the queueing of another touch event (e.g., a touchcancel). As
      // forwarding of the queued event will be deferred while the ack is being
      // dispatched (see |OnTouchEvent()|), try forwarding it now.
      TryForwardNextEventToRenderer();
      return;
    }
  }

  last_sent_touch_timestamp_sec_ = touch.event.timeStampSeconds;

  // Flush any pending async touch move. If it can be combined with the current
  // (touchmove) event, great, otherwise send it immediately but separately. Its
  // ack will trigger forwarding of the original |touch| event.
  if (pending_async_touchmove_) {
    if (pending_async_touchmove_->CanCoalesceWith(touch)) {
      pending_async_touchmove_->CoalesceWith(touch);
      pending_async_touchmove_->event.dispatchType =
          send_touch_events_async_ ? WebInputEvent::EventNonBlocking
                                   : WebInputEvent::Blocking;
      touch = *pending_async_touchmove_;
      pending_async_touchmove_.reset();
    } else {
      FlushPendingAsyncTouchmove();
      return;
    }
  }

  // Note: Touchstart events are marked cancelable to allow transitions between
  // platform scrolling and JS pinching. Touchend events, however, remain
  // uncancelable, mitigating the risk of jank when transitioning to a fling.
  if (send_touch_events_async_ && touch.event.type != WebInputEvent::TouchStart)
    touch.event.dispatchType = WebInputEvent::EventNonBlocking;

  SendTouchEventImmediately(&touch);
}

void TouchEventQueue::FlushPendingAsyncTouchmove() {
  DCHECK(!dispatching_touch_);
  std::unique_ptr<TouchEventWithLatencyInfo> touch =
      std::move(pending_async_touchmove_);
  touch->event.dispatchType = WebInputEvent::EventNonBlocking;
  touch_queue_.push_front(new CoalescedWebTouchEvent(*touch, true));
  SendTouchEventImmediately(touch.get());
}

void TouchEventQueue::OnGestureScrollEvent(
    const GestureEventWithLatencyInfo& gesture_event) {
  if (gesture_event.event.type == blink::WebInputEvent::GestureScrollBegin) {
    if (has_handler_for_current_sequence_ &&
        !drop_remaining_touches_in_sequence_) {
      DCHECK(!touchmove_slop_suppressor_->suppressing_touchmoves())
          <<  "A touch handler should be offered a touchmove before scrolling.";
    }

    pending_async_touchmove_.reset();

    return;
  }

  if (gesture_event.event.type == blink::WebInputEvent::GestureScrollUpdate &&
      gesture_event.event.resendingPluginId == -1) {
    send_touch_events_async_ = true;
  }
}

void TouchEventQueue::OnGestureEventAck(
    const GestureEventWithLatencyInfo& event,
    InputEventAckState ack_result) {
  // Throttle sending touchmove events as long as the scroll events are handled.
  // Note that there's no guarantee that this ACK is for the most recent
  // gesture event (or even part of the current sequence).  Worst case, the
  // delay in updating the absorption state will result in minor UI glitches.
  // A valid |pending_async_touchmove_| will be flushed when the next event is
  // forwarded. Scroll updates that are being resent from a GuestView are
  // ignored.
  if (event.event.type == blink::WebInputEvent::GestureScrollUpdate &&
      event.event.resendingPluginId == -1) {
    send_touch_events_async_ = (ack_result == INPUT_EVENT_ACK_STATE_CONSUMED);
  }
}

void TouchEventQueue::OnHasTouchEventHandlers(bool has_handlers) {
  DCHECK(!dispatching_touch_ack_);
  DCHECK(!dispatching_touch_);
  has_handlers_ = has_handlers;
}

bool TouchEventQueue::IsPendingAckTouchStart() const {
  DCHECK(!dispatching_touch_ack_);
  if (touch_queue_.empty())
    return false;

  const blink::WebTouchEvent& event =
      touch_queue_.front()->coalesced_event().event;
  return (event.type == WebInputEvent::TouchStart);
}

void TouchEventQueue::SetAckTimeoutEnabled(bool enabled) {
  if (timeout_handler_)
    timeout_handler_->SetEnabled(enabled);
}

void TouchEventQueue::SetIsMobileOptimizedSite(bool mobile_optimized_site) {
  if (timeout_handler_)
    timeout_handler_->SetUseMobileTimeout(mobile_optimized_site);
}

bool TouchEventQueue::IsAckTimeoutEnabled() const {
  return timeout_handler_ && timeout_handler_->IsEnabled();
}

bool TouchEventQueue::HasPendingAsyncTouchMoveForTesting() const {
  return !!pending_async_touchmove_;
}

bool TouchEventQueue::IsTimeoutRunningForTesting() const {
  return timeout_handler_ && timeout_handler_->IsTimeoutTimerRunning();
}

const TouchEventWithLatencyInfo&
TouchEventQueue::GetLatestEventForTesting() const {
  return touch_queue_.back()->coalesced_event();
}

void TouchEventQueue::FlushQueue() {
  DCHECK(!dispatching_touch_ack_);
  DCHECK(!dispatching_touch_);
  pending_async_touchmove_.reset();
  drop_remaining_touches_in_sequence_ = true;
  while (!touch_queue_.empty())
    PopTouchEventToClient(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
}

void TouchEventQueue::PopTouchEventToClient(InputEventAckState ack_result) {
  AckTouchEventToClient(ack_result, nullptr);
}

void TouchEventQueue::PopTouchEventToClient(
    InputEventAckState ack_result,
    const LatencyInfo& renderer_latency_info) {
  AckTouchEventToClient(ack_result, &renderer_latency_info);
}

void TouchEventQueue::AckTouchEventToClient(
    InputEventAckState ack_result,
    const ui::LatencyInfo* optional_latency_info) {
  DCHECK(!dispatching_touch_ack_);
  DCHECK(!touch_queue_.empty());
  std::unique_ptr<CoalescedWebTouchEvent> acked_event(touch_queue_.front());
  DCHECK(acked_event);

  UpdateTouchConsumerStates(acked_event->coalesced_event().event, ack_result);

  // Note that acking the touch-event may result in multiple gestures being sent
  // to the renderer, or touch-events being queued.
  base::AutoReset<bool> dispatching_touch_ack(&dispatching_touch_ack_, true);

  // Skip ack for TouchScrollStarted since it was synthesized within the queue.
  if (acked_event->coalesced_event().event.type !=
      WebInputEvent::TouchScrollStarted) {
    acked_event->DispatchAckToClient(ack_result, optional_latency_info,
                                     client_);
  }

  touch_queue_.pop_front();
}

void TouchEventQueue::SendTouchEventImmediately(
    TouchEventWithLatencyInfo* touch) {
  // TODO(crbug.com/600773): Hack to avoid cyclic reentry to this method.
  if (dispatching_touch_)
    return;

  // For touchmove events, compare touch points position from current event
  // to last sent event and update touch points state.
  if (touch->event.type == WebInputEvent::TouchMove) {
    CHECK(last_sent_touchevent_);
    for (unsigned int i = 0; i < last_sent_touchevent_->touchesLength; ++i) {
      const WebTouchPoint& last_touch_point =
          last_sent_touchevent_->touches[i];
      // Touches with same id may not have same index in Touches array.
      for (unsigned int j = 0; j < touch->event.touchesLength; ++j) {
        const WebTouchPoint& current_touchmove_point = touch->event.touches[j];
        if (current_touchmove_point.id != last_touch_point.id)
          continue;

        if (!HasPointChanged(last_touch_point, current_touchmove_point))
          touch->event.touches[j].state = WebTouchPoint::StateStationary;

        break;
      }
    }
  }

  if (touch->event.type != WebInputEvent::TouchScrollStarted) {
    if (last_sent_touchevent_)
      *last_sent_touchevent_ = touch->event;
    else
      last_sent_touchevent_.reset(new WebTouchEvent(touch->event));
  }

  base::AutoReset<bool> dispatching_touch(&dispatching_touch_, true);

  client_->SendTouchEventImmediately(*touch);

  // A synchronous ack will reset |dispatching_touch_|, in which case the touch
  // timeout should not be started and the count also should not be increased.
  if (dispatching_touch_) {
    if (touch->event.type == WebInputEvent::TouchMove &&
        touch->event.dispatchType != WebInputEvent::Blocking) {
      // When we send out a uncancelable touch move, we increase the count and
      // we do not process input event ack any more, we will just ack to client
      // and wait for the ack from render. Also we will remove it from the front
      // of the queue.
      ack_pending_async_touchmove_ids_.push_back(
          touch->event.uniqueTouchEventId);
      dispatching_touch_ = false;
      PopTouchEventToClient(INPUT_EVENT_ACK_STATE_IGNORED);
      TryForwardNextEventToRenderer();
      return;
    }

    if (timeout_handler_)
      timeout_handler_->StartIfNecessary(*touch);
  }
}

TouchEventQueue::PreFilterResult
TouchEventQueue::FilterBeforeForwarding(const WebTouchEvent& event) {
  if (event.type == WebInputEvent::TouchScrollStarted)
    return FORWARD_TO_RENDERER;

  if (WebTouchEventTraits::IsTouchSequenceStart(event)) {
    has_handler_for_current_sequence_ = false;
    send_touch_events_async_ = false;
    pending_async_touchmove_.reset();
    last_sent_touchevent_.reset();

    touch_sequence_start_position_ = gfx::PointF(event.touches[0].position);
    drop_remaining_touches_in_sequence_ = false;
    if (!has_handlers_) {
      drop_remaining_touches_in_sequence_ = true;
      return ACK_WITH_NO_CONSUMER_EXISTS;
    }
  }

  if (timeout_handler_ && timeout_handler_->FilterEvent(event))
    return ACK_WITH_NO_CONSUMER_EXISTS;

  if (touchmove_slop_suppressor_->FilterEvent(event))
    return ACK_WITH_NOT_CONSUMED;

  if (drop_remaining_touches_in_sequence_ &&
      event.type != WebInputEvent::TouchCancel) {
    return ACK_WITH_NO_CONSUMER_EXISTS;
  }

  if (event.type == WebInputEvent::TouchStart) {
    return (has_handlers_ || has_handler_for_current_sequence_)
               ? FORWARD_TO_RENDERER
               : ACK_WITH_NO_CONSUMER_EXISTS;
  }

  if (has_handler_for_current_sequence_) {
    // Only forward a touch if it has a non-stationary pointer that is active
    // in the current touch sequence.
    for (size_t i = 0; i < event.touchesLength; ++i) {
      const WebTouchPoint& point = event.touches[i];
      if (point.state == WebTouchPoint::StateStationary)
        continue;

      // |last_sent_touchevent_| will be non-null as long as there is an
      // active touch sequence being forwarded to the renderer.
      if (!last_sent_touchevent_)
        continue;

      for (size_t j = 0; j < last_sent_touchevent_->touchesLength; ++j) {
        if (point.id != last_sent_touchevent_->touches[j].id)
          continue;

        if (event.type != WebInputEvent::TouchMove)
          return FORWARD_TO_RENDERER;

        // All pointers in TouchMove events may have state as StateMoved,
        // even though none of the pointers have not changed in real.
        // Forward these events when at least one pointer has changed.
        if (HasPointChanged(last_sent_touchevent_->touches[j], point))
          return FORWARD_TO_RENDERER;

        // This is a TouchMove event for which we have yet to find a
        // non-stationary pointer. Continue checking the next pointers
        // in the |event|.
        break;
      }

    }
  }

  return ACK_WITH_NO_CONSUMER_EXISTS;
}

void TouchEventQueue::UpdateTouchConsumerStates(const WebTouchEvent& event,
                                                InputEventAckState ack_result) {
  if (event.type == WebInputEvent::TouchStart) {
    if (ack_result == INPUT_EVENT_ACK_STATE_CONSUMED)
      send_touch_events_async_ = false;
    has_handler_for_current_sequence_ |=
        ack_result != INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS;
  } else if (WebTouchEventTraits::IsTouchSequenceEnd(event)) {
    has_handler_for_current_sequence_ = false;
  }
}

}  // namespace content
