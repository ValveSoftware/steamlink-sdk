// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touch_event_queue.h"

#include "base/auto_reset.h"
#include "base/debug/trace_event.h"
#include "base/stl_util.h"
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

// A slop region just larger than that used by many web applications. When
// touchmove's are being sent asynchronously, movement outside this region will
// trigger an immediate async touchmove to cancel potential tap-related logic.
const double kApplicationSlopRegionLengthDipsSqared = 15. * 15.;

// Using a small epsilon when comparing slop distances allows pixel perfect
// slop determination when using fractional DIP coordinates (assuming the slop
// region and DPI scale are reasonably proportioned).
const float kSlopEpsilon = .05f;

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
         !WebInputEventTraits::IgnoresAckDisposition(event);
}

bool OutsideApplicationSlopRegion(const WebTouchEvent& event,
                                  const gfx::PointF& anchor) {
  return (gfx::PointF(event.touches[0].position) - anchor).LengthSquared() >
         kApplicationSlopRegionLengthDipsSqared;
}

}  // namespace


// Cancels a touch sequence if a touchstart or touchmove ack response is
// sufficiently delayed.
class TouchEventQueue::TouchTimeoutHandler {
 public:
  TouchTimeoutHandler(TouchEventQueue* touch_queue,
                      base::TimeDelta timeout_delay)
      : touch_queue_(touch_queue),
        timeout_delay_(timeout_delay),
        pending_ack_state_(PENDING_ACK_NONE),
        timeout_monitor_(base::Bind(&TouchTimeoutHandler::OnTimeOut,
                                    base::Unretained(this))) {
    DCHECK(timeout_delay != base::TimeDelta());
  }

  ~TouchTimeoutHandler() {}

  void Start(const TouchEventWithLatencyInfo& event) {
    DCHECK_EQ(pending_ack_state_, PENDING_ACK_NONE);
    DCHECK(ShouldTouchTriggerTimeout(event.event));
    timeout_event_ = event;
    timeout_monitor_.Restart(timeout_delay_);
  }

  bool ConfirmTouchEvent(InputEventAckState ack_result) {
    switch (pending_ack_state_) {
      case PENDING_ACK_NONE:
        timeout_monitor_.Stop();
        return false;
      case PENDING_ACK_ORIGINAL_EVENT:
        if (AckedTimeoutEventRequiresCancel(ack_result)) {
          SetPendingAckState(PENDING_ACK_CANCEL_EVENT);
          TouchEventWithLatencyInfo cancel_event =
              ObtainCancelEventForTouchEvent(timeout_event_);
          touch_queue_->SendTouchEventImmediately(cancel_event);
        } else {
          SetPendingAckState(PENDING_ACK_NONE);
          touch_queue_->UpdateTouchAckStates(timeout_event_.event, ack_result);
        }
        return true;
      case PENDING_ACK_CANCEL_EVENT:
        SetPendingAckState(PENDING_ACK_NONE);
        return true;
    }
    return false;
  }

  bool FilterEvent(const WebTouchEvent& event) {
    return HasTimeoutEvent();
  }

  bool IsTimeoutTimerRunning() const {
    return timeout_monitor_.IsRunning();
  }

  void Reset() {
    pending_ack_state_ = PENDING_ACK_NONE;
    timeout_monitor_.Stop();
  }

  void set_timeout_delay(base::TimeDelta timeout_delay) {
    timeout_delay_ = timeout_delay;
  }

 private:
  enum PendingAckState {
    PENDING_ACK_NONE,
    PENDING_ACK_ORIGINAL_EVENT,
    PENDING_ACK_CANCEL_EVENT,
  };

  void OnTimeOut() {
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

  bool HasTimeoutEvent() const {
    return pending_ack_state_ != PENDING_ACK_NONE;
  }


  TouchEventQueue* touch_queue_;

  // How long to wait on a touch ack before cancelling the touch sequence.
  base::TimeDelta timeout_delay_;

  // The touch event source for which we expect the next ack.
  PendingAckState pending_ack_state_;

  // The event for which the ack timeout is triggered.
  TouchEventWithLatencyInfo timeout_event_;

  // Provides timeout-based callback behavior.
  TimeoutMonitor timeout_monitor_;
};

// Provides touchmove slop suppression for a single touch that remains within
// a given slop region, unless the touchstart is preventDefault'ed.
// TODO(jdduke): Use a flag bundled with each TouchEvent declaring whether it
// has exceeded the slop region, removing duplicated slop determination logic.
class TouchEventQueue::TouchMoveSlopSuppressor {
 public:
  TouchMoveSlopSuppressor(double slop_suppression_length_dips)
      : slop_suppression_length_dips_squared_(slop_suppression_length_dips *
                                              slop_suppression_length_dips),
        suppressing_touchmoves_(false) {}

  bool FilterEvent(const WebTouchEvent& event) {
    if (WebTouchEventTraits::IsTouchSequenceStart(event)) {
      touch_sequence_start_position_ =
          gfx::PointF(event.touches[0].position);
      suppressing_touchmoves_ = slop_suppression_length_dips_squared_ != 0;
    }

    if (event.type == WebInputEvent::TouchEnd ||
        event.type == WebInputEvent::TouchCancel)
      suppressing_touchmoves_ = false;

    if (event.type != WebInputEvent::TouchMove)
      return false;

    if (suppressing_touchmoves_) {
      // Movement with a secondary pointer should terminate suppression.
      if (event.touchesLength > 1) {
        suppressing_touchmoves_ = false;
      } else if (event.touchesLength == 1) {
        // Movement outside of the slop region should terminate suppression.
        gfx::PointF position(event.touches[0].position);
        if ((position - touch_sequence_start_position_).LengthSquared() >
            slop_suppression_length_dips_squared_)
          suppressing_touchmoves_ = false;
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
  double slop_suppression_length_dips_squared_;
  gfx::PointF touch_sequence_start_position_;
  bool suppressing_touchmoves_;

  DISALLOW_COPY_AND_ASSIGN(TouchMoveSlopSuppressor);
};

// This class represents a single coalesced touch event. However, it also keeps
// track of all the original touch-events that were coalesced into a single
// event. The coalesced event is forwarded to the renderer, while the original
// touch-events are sent to the Client (on ACK for the coalesced event) so that
// the Client receives the event with their original timestamp.
class CoalescedWebTouchEvent {
 public:
  // Events for which |async| is true will not be ack'ed to the client after the
  // corresponding ack is received following dispatch.
  CoalescedWebTouchEvent(const TouchEventWithLatencyInfo& event, bool async)
      : coalesced_event_(event) {
    if (async)
      coalesced_event_.event.cancelable = false;
    else
      events_to_ack_.push_back(event);

    TRACE_EVENT_ASYNC_BEGIN0("input", "TouchEventQueue::QueueEvent", this);
  }

  ~CoalescedWebTouchEvent() {
    TRACE_EVENT_ASYNC_END0("input", "TouchEventQueue::QueueEvent", this);
  }

  // Coalesces the event with the existing event if possible. Returns whether
  // the event was coalesced.
  bool CoalesceEventIfPossible(
      const TouchEventWithLatencyInfo& event_with_latency) {
    if (!WillDispatchAckToClient())
      return false;

    if (!coalesced_event_.CanCoalesceWith(event_with_latency))
      return false;

    TRACE_EVENT_INSTANT0(
        "input", "TouchEventQueue::MoveCoalesced", TRACE_EVENT_SCOPE_THREAD);
    coalesced_event_.CoalesceWith(event_with_latency);
    events_to_ack_.push_back(event_with_latency);
    return true;
  }

  void UpdateLatencyInfoForAck(const ui::LatencyInfo& renderer_latency_info) {
    if (!WillDispatchAckToClient())
      return;

    for (WebTouchEventWithLatencyList::iterator iter = events_to_ack_.begin(),
                                                end = events_to_ack_.end();
         iter != end;
         ++iter) {
      iter->latency.AddNewLatencyFrom(renderer_latency_info);
    }
  }

  void DispatchAckToClient(InputEventAckState ack_result,
                           TouchEventQueueClient* client) {
    DCHECK(client);
    if (!WillDispatchAckToClient())
      return;

    for (WebTouchEventWithLatencyList::const_iterator
             iter = events_to_ack_.begin(),
             end = events_to_ack_.end();
         iter != end;
         ++iter) {
      client->OnTouchEventAck(*iter, ack_result);
    }
  }

  const TouchEventWithLatencyInfo& coalesced_event() const {
    return coalesced_event_;
  }

 private:
  bool WillDispatchAckToClient() const { return !events_to_ack_.empty(); }

  // This is the event that is forwarded to the renderer.
  TouchEventWithLatencyInfo coalesced_event_;

  // This is the list of the original events that were coalesced, each requiring
  // future ack dispatch to the client.
  typedef std::vector<TouchEventWithLatencyInfo> WebTouchEventWithLatencyList;
  WebTouchEventWithLatencyList events_to_ack_;

  DISALLOW_COPY_AND_ASSIGN(CoalescedWebTouchEvent);
};

TouchEventQueue::Config::Config()
    : touchmove_slop_suppression_length_dips(0),
      touchmove_slop_suppression_region_includes_boundary(true),
      touch_scrolling_mode(TOUCH_SCROLLING_MODE_DEFAULT),
      touch_ack_timeout_delay(base::TimeDelta::FromMilliseconds(200)),
      touch_ack_timeout_supported(false) {
}

TouchEventQueue::TouchEventQueue(TouchEventQueueClient* client,
                                 const Config& config)
    : client_(client),
      dispatching_touch_ack_(NULL),
      dispatching_touch_(false),
      touch_filtering_state_(TOUCH_FILTERING_STATE_DEFAULT),
      ack_timeout_enabled_(config.touch_ack_timeout_supported),
      touchmove_slop_suppressor_(new TouchMoveSlopSuppressor(
          config.touchmove_slop_suppression_length_dips +
          (config.touchmove_slop_suppression_region_includes_boundary
               ? kSlopEpsilon
               : -kSlopEpsilon))),
      send_touch_events_async_(false),
      needs_async_touchmove_for_outer_slop_region_(false),
      last_sent_touch_timestamp_sec_(0),
      touch_scrolling_mode_(config.touch_scrolling_mode) {
  DCHECK(client);
  if (ack_timeout_enabled_) {
    timeout_handler_.reset(
        new TouchTimeoutHandler(this, config.touch_ack_timeout_delay));
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

void TouchEventQueue::ProcessTouchAck(InputEventAckState ack_result,
                                      const LatencyInfo& latency_info) {
  TRACE_EVENT0("input", "TouchEventQueue::ProcessTouchAck");

  DCHECK(!dispatching_touch_ack_);
  dispatching_touch_ = false;

  if (timeout_handler_ && timeout_handler_->ConfirmTouchEvent(ack_result))
    return;

  touchmove_slop_suppressor_->ConfirmTouchEvent(ack_result);

  if (touch_queue_.empty())
    return;

  if (ack_result == INPUT_EVENT_ACK_STATE_CONSUMED &&
      touch_filtering_state_ == FORWARD_TOUCHES_UNTIL_TIMEOUT) {
    touch_filtering_state_ = FORWARD_ALL_TOUCHES;
  }

  PopTouchEventToClient(ack_result, latency_info);
  TryForwardNextEventToRenderer();
}

void TouchEventQueue::TryForwardNextEventToRenderer() {
  DCHECK(!dispatching_touch_ack_);
  // If there are queued touch events, then try to forward them to the renderer
  // immediately, or ACK the events back to the client if appropriate.
  while (!touch_queue_.empty()) {
    PreFilterResult filter_result =
        FilterBeforeForwarding(touch_queue_.front()->coalesced_event().event);
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
  DCHECK_NE(touch_filtering_state_, DROP_ALL_TOUCHES);
  TouchEventWithLatencyInfo touch = touch_queue_.front()->coalesced_event();

  if (WebTouchEventTraits::IsTouchSequenceStart(touch.event)) {
    touch_filtering_state_ =
        ack_timeout_enabled_ ? FORWARD_TOUCHES_UNTIL_TIMEOUT
                             : FORWARD_ALL_TOUCHES;
    touch_ack_states_.clear();
    send_touch_events_async_ = false;
    touch_sequence_start_position_ =
        gfx::PointF(touch.event.touches[0].position);
  }

  if (send_touch_events_async_ &&
      touch.event.type == WebInputEvent::TouchMove) {
    // Throttling touchmove's in a continuous touchmove stream while scrolling
    // reduces the risk of jank. However, it's still important that the web
    // application be sent touches at key points in the gesture stream,
    // e.g., when the application slop region is exceeded or touchmove
    // coalescing fails because of different modifiers.
    const bool send_touchmove_now =
        size() > 1 ||
        (touch.event.timeStampSeconds >=
         last_sent_touch_timestamp_sec_ + kAsyncTouchMoveIntervalSec) ||
        (needs_async_touchmove_for_outer_slop_region_ &&
         OutsideApplicationSlopRegion(touch.event,
                                      touch_sequence_start_position_)) ||
        (pending_async_touchmove_ &&
         !pending_async_touchmove_->CanCoalesceWith(touch));

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
      pending_async_touchmove_->event.cancelable = !send_touch_events_async_;
      touch = *pending_async_touchmove_.Pass();
    } else {
      scoped_ptr<TouchEventWithLatencyInfo> async_move =
          pending_async_touchmove_.Pass();
      async_move->event.cancelable = false;
      touch_queue_.push_front(new CoalescedWebTouchEvent(*async_move, true));
      SendTouchEventImmediately(*async_move);
      return;
    }
  }

  // Note: Marking touchstart events as not-cancelable prevents them from
  // blocking subsequent gestures, but it may not be the best long term solution
  // for tracking touch point dispatch.
  if (send_touch_events_async_)
    touch.event.cancelable = false;

  // A synchronous ack will reset |dispatching_touch_|, in which case
  // the touch timeout should not be started.
  base::AutoReset<bool> dispatching_touch(&dispatching_touch_, true);
  SendTouchEventImmediately(touch);
  if (dispatching_touch_ &&
      touch_filtering_state_ == FORWARD_TOUCHES_UNTIL_TIMEOUT &&
      ShouldTouchTriggerTimeout(touch.event)) {
    DCHECK(timeout_handler_);
    timeout_handler_->Start(touch);
  }
}

void TouchEventQueue::OnGestureScrollEvent(
    const GestureEventWithLatencyInfo& gesture_event) {
  if (gesture_event.event.type != blink::WebInputEvent::GestureScrollBegin)
    return;

  if (touch_filtering_state_ != DROP_ALL_TOUCHES &&
      touch_filtering_state_ != DROP_TOUCHES_IN_SEQUENCE) {
    DCHECK(!touchmove_slop_suppressor_->suppressing_touchmoves())
        << "The renderer should be offered a touchmove before scrolling begins";
  }

  if (touch_scrolling_mode_ == TOUCH_SCROLLING_MODE_ASYNC_TOUCHMOVE) {
    if (touch_filtering_state_ != DROP_ALL_TOUCHES &&
        touch_filtering_state_ != DROP_TOUCHES_IN_SEQUENCE) {
      // If no touch points have a consumer, prevent all subsequent touch events
      // received during the scroll from reaching the renderer. This ensures
      // that the first touchstart the renderer sees in any given sequence can
      // always be preventDefault'ed (cancelable == true).
      // TODO(jdduke): Revisit if touchstarts during scroll are made cancelable.
      if (touch_ack_states_.empty() ||
          AllTouchAckStatesHaveState(
              INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS)) {
        touch_filtering_state_ = DROP_TOUCHES_IN_SEQUENCE;
        return;
      }
    }

    pending_async_touchmove_.reset();
    send_touch_events_async_ = true;
    needs_async_touchmove_for_outer_slop_region_ = true;
    return;
  }

  if (touch_scrolling_mode_ != TOUCH_SCROLLING_MODE_TOUCHCANCEL)
    return;

  // We assume that scroll events are generated synchronously from
  // dispatching a touch event ack. This allows us to generate a synthetic
  // cancel event that has the same touch ids as the touch event that
  // is being acked. Otherwise, we don't perform the touch-cancel optimization.
  if (!dispatching_touch_ack_)
    return;

  if (touch_filtering_state_ == DROP_TOUCHES_IN_SEQUENCE)
    return;

  touch_filtering_state_ = DROP_TOUCHES_IN_SEQUENCE;

  // Fake a TouchCancel to cancel the touch points of the touch event
  // that is currently being acked.
  // Note: |dispatching_touch_ack_| is non-null when we reach here, meaning we
  // are in the scope of PopTouchEventToClient() and that no touch event
  // in the queue is waiting for ack from renderer. So we can just insert
  // the touch cancel at the beginning of the queue.
  touch_queue_.push_front(new CoalescedWebTouchEvent(
      ObtainCancelEventForTouchEvent(
          dispatching_touch_ack_->coalesced_event()), true));
}

void TouchEventQueue::OnGestureEventAck(
    const GestureEventWithLatencyInfo& event,
    InputEventAckState ack_result) {
  if (touch_scrolling_mode_ != TOUCH_SCROLLING_MODE_ASYNC_TOUCHMOVE)
    return;

  if (event.event.type != blink::WebInputEvent::GestureScrollUpdate)
    return;

  // Throttle sending touchmove events as long as the scroll events are handled.
  // Note that there's no guarantee that this ACK is for the most recent
  // gesture event (or even part of the current sequence).  Worst case, the
  // delay in updating the absorption state will result in minor UI glitches.
  // A valid |pending_async_touchmove_| will be flushed when the next event is
  // forwarded.
  send_touch_events_async_ = (ack_result == INPUT_EVENT_ACK_STATE_CONSUMED);
  if (!send_touch_events_async_)
    needs_async_touchmove_for_outer_slop_region_ = false;
}

void TouchEventQueue::OnHasTouchEventHandlers(bool has_handlers) {
  DCHECK(!dispatching_touch_ack_);
  DCHECK(!dispatching_touch_);

  if (has_handlers) {
    if (touch_filtering_state_ == DROP_ALL_TOUCHES) {
      // If no touch handler was previously registered, ensure that we don't
      // send a partial touch sequence to the renderer.
      DCHECK(touch_queue_.empty());
      touch_filtering_state_ = DROP_TOUCHES_IN_SEQUENCE;
    }
  } else {
    // TODO(jdduke): Synthesize a TouchCancel if necessary to update Blink touch
    // state tracking and/or touch-action filtering (e.g., if the touch handler
    // was removed mid-sequence), crbug.com/375940.
    touch_filtering_state_ = DROP_ALL_TOUCHES;
    pending_async_touchmove_.reset();
    if (timeout_handler_)
      timeout_handler_->Reset();
    if (!touch_queue_.empty())
      ProcessTouchAck(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS, LatencyInfo());
    // As there is no touch handler, ack'ing the event should flush the queue.
    DCHECK(touch_queue_.empty());
  }
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
  // The timeout handler is valid only if explicitly supported in the config.
  if (!timeout_handler_)
    return;

  if (ack_timeout_enabled_ == enabled)
    return;

  ack_timeout_enabled_ = enabled;

  if (enabled)
    return;

  if (touch_filtering_state_ == FORWARD_TOUCHES_UNTIL_TIMEOUT)
    touch_filtering_state_ = FORWARD_ALL_TOUCHES;
  // Only reset the |timeout_handler_| if the timer is running and has not yet
  // timed out. This ensures that an already timed out sequence is properly
  // flushed by the handler.
  if (timeout_handler_ && timeout_handler_->IsTimeoutTimerRunning())
    timeout_handler_->Reset();
}

bool TouchEventQueue::HasPendingAsyncTouchMoveForTesting() const {
  return pending_async_touchmove_;
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
  if (touch_filtering_state_ != DROP_ALL_TOUCHES)
    touch_filtering_state_ = DROP_TOUCHES_IN_SEQUENCE;
  while (!touch_queue_.empty())
    PopTouchEventToClient(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
}

void TouchEventQueue::PopTouchEventToClient(InputEventAckState ack_result) {
  AckTouchEventToClient(ack_result, PopTouchEvent());
}

void TouchEventQueue::PopTouchEventToClient(
    InputEventAckState ack_result,
    const LatencyInfo& renderer_latency_info) {
  scoped_ptr<CoalescedWebTouchEvent> acked_event = PopTouchEvent();
  acked_event->UpdateLatencyInfoForAck(renderer_latency_info);
  AckTouchEventToClient(ack_result, acked_event.Pass());
}

void TouchEventQueue::AckTouchEventToClient(
    InputEventAckState ack_result,
    scoped_ptr<CoalescedWebTouchEvent> acked_event) {
  DCHECK(acked_event);
  DCHECK(!dispatching_touch_ack_);
  UpdateTouchAckStates(acked_event->coalesced_event().event, ack_result);

  // Note that acking the touch-event may result in multiple gestures being sent
  // to the renderer, or touch-events being queued.
  base::AutoReset<const CoalescedWebTouchEvent*> dispatching_touch_ack(
      &dispatching_touch_ack_, acked_event.get());
  acked_event->DispatchAckToClient(ack_result, client_);
}

scoped_ptr<CoalescedWebTouchEvent> TouchEventQueue::PopTouchEvent() {
  DCHECK(!touch_queue_.empty());
  scoped_ptr<CoalescedWebTouchEvent> event(touch_queue_.front());
  touch_queue_.pop_front();
  return event.Pass();
}

void TouchEventQueue::SendTouchEventImmediately(
    const TouchEventWithLatencyInfo& touch) {
  if (needs_async_touchmove_for_outer_slop_region_) {
    // Any event other than a touchmove (e.g., touchcancel or secondary
    // touchstart) after a scroll has started will interrupt the need to send a
    // an outer slop-region exceeding touchmove.
    if (touch.event.type != WebInputEvent::TouchMove ||
        OutsideApplicationSlopRegion(touch.event,
                                     touch_sequence_start_position_))
      needs_async_touchmove_for_outer_slop_region_ = false;
  }

  client_->SendTouchEventImmediately(touch);
}

TouchEventQueue::PreFilterResult
TouchEventQueue::FilterBeforeForwarding(const WebTouchEvent& event) {
  if (timeout_handler_ && timeout_handler_->FilterEvent(event))
    return ACK_WITH_NO_CONSUMER_EXISTS;

  if (touchmove_slop_suppressor_->FilterEvent(event))
    return ACK_WITH_NOT_CONSUMED;

  if (touch_filtering_state_ == DROP_ALL_TOUCHES)
    return ACK_WITH_NO_CONSUMER_EXISTS;

  if (touch_filtering_state_ == DROP_TOUCHES_IN_SEQUENCE &&
      event.type != WebInputEvent::TouchCancel) {
    if (WebTouchEventTraits::IsTouchSequenceStart(event))
      return FORWARD_TO_RENDERER;
    return ACK_WITH_NO_CONSUMER_EXISTS;
  }

  // Touch press events should always be forwarded to the renderer.
  if (event.type == WebInputEvent::TouchStart)
    return FORWARD_TO_RENDERER;

  for (unsigned int i = 0; i < event.touchesLength; ++i) {
    const WebTouchPoint& point = event.touches[i];
    // If a point has been stationary, then don't take it into account.
    if (point.state == WebTouchPoint::StateStationary)
      continue;

    if (touch_ack_states_.count(point.id) > 0) {
      if (touch_ack_states_.find(point.id)->second !=
          INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS)
        return FORWARD_TO_RENDERER;
    } else {
      // If the ACK status of a point is unknown, then the event should be
      // forwarded to the renderer.
      return FORWARD_TO_RENDERER;
    }
  }

  return ACK_WITH_NO_CONSUMER_EXISTS;
}

void TouchEventQueue::UpdateTouchAckStates(const WebTouchEvent& event,
                                           InputEventAckState ack_result) {
  // Update the ACK status for each touch point in the ACKed event.
  if (event.type == WebInputEvent::TouchEnd ||
      event.type == WebInputEvent::TouchCancel) {
    // The points have been released. Erase the ACK states.
    for (unsigned i = 0; i < event.touchesLength; ++i) {
      const WebTouchPoint& point = event.touches[i];
      if (point.state == WebTouchPoint::StateReleased ||
          point.state == WebTouchPoint::StateCancelled)
        touch_ack_states_.erase(point.id);
    }
  } else if (event.type == WebInputEvent::TouchStart) {
    for (unsigned i = 0; i < event.touchesLength; ++i) {
      const WebTouchPoint& point = event.touches[i];
      if (point.state == WebTouchPoint::StatePressed)
        touch_ack_states_[point.id] = ack_result;
    }
  }
}

bool TouchEventQueue::AllTouchAckStatesHaveState(
    InputEventAckState ack_state) const {
  if (touch_ack_states_.empty())
    return false;

  for (TouchPointAckStates::const_iterator iter = touch_ack_states_.begin(),
                                           end = touch_ack_states_.end();
       iter != end;
       ++iter) {
    if (iter->second != ack_state)
      return false;
  }

  return true;
}

}  // namespace content
