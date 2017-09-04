// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/input/main_thread_event_queue.h"

#include "base/metrics/histogram_macros.h"
#include "content/common/input/event_with_latency_info.h"
#include "content/common/input_messages.h"

namespace content {

namespace {

const size_t kTenSeconds = 10 * 1000 * 1000;

bool IsContinuousEvent(const std::unique_ptr<EventWithDispatchType>& event) {
  switch (event->event().type) {
    case blink::WebInputEvent::MouseMove:
    case blink::WebInputEvent::MouseWheel:
      return true;
    case blink::WebInputEvent::TouchMove:
      // TouchMoves that are blocking end up blocking scroll. Do not treat
      // them as continuous events otherwise we will end up waiting up to an
      // additional frame.
      return static_cast<const blink::WebTouchEvent&>(event->event())
                 .dispatchType != blink::WebInputEvent::Blocking;
    default:
      return false;
  }
}

}  // namespace

EventWithDispatchType::EventWithDispatchType(
    ui::ScopedWebInputEvent event,
    const ui::LatencyInfo& latency,
    InputEventDispatchType dispatch_type)
    : ScopedWebInputEventWithLatencyInfo(std::move(event), latency),
      dispatch_type_(dispatch_type),
      creation_timestamp_(base::TimeTicks::Now()),
      last_coalesced_timestamp_(creation_timestamp_) {}

EventWithDispatchType::~EventWithDispatchType() {}

bool EventWithDispatchType::CanCoalesceWith(
    const EventWithDispatchType& other) const {
  return other.dispatch_type_ == dispatch_type_ &&
         ScopedWebInputEventWithLatencyInfo::CanCoalesceWith(other);
}

void EventWithDispatchType::CoalesceWith(const EventWithDispatchType& other) {
  coalesced_event_ids_.push_back(
      ui::WebInputEventTraits::GetUniqueTouchEventId(other.event()));
  ScopedWebInputEventWithLatencyInfo::CoalesceWith(other);
  last_coalesced_timestamp_ = base::TimeTicks::Now();
}

MainThreadEventQueue::SharedState::SharedState()
    : sent_main_frame_request_(false) {}

MainThreadEventQueue::SharedState::~SharedState() {}

MainThreadEventQueue::MainThreadEventQueue(
    int routing_id,
    MainThreadEventQueueClient* client,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner,
    blink::scheduler::RendererScheduler* renderer_scheduler)
    : routing_id_(routing_id),
      client_(client),
      last_touch_start_forced_nonblocking_due_to_fling_(false),
      enable_fling_passive_listener_flag_(base::FeatureList::IsEnabled(
          features::kPassiveEventListenersDueToFling)),
      handle_raf_aligned_touch_input_(
          base::FeatureList::IsEnabled(features::kRafAlignedTouchInputEvents)),
      handle_raf_aligned_mouse_input_(
          base::FeatureList::IsEnabled(features::kRafAlignedMouseInputEvents)),
      main_task_runner_(main_task_runner),
      renderer_scheduler_(renderer_scheduler) {}

MainThreadEventQueue::~MainThreadEventQueue() {}

bool MainThreadEventQueue::HandleEvent(
    ui::ScopedWebInputEvent event,
    const ui::LatencyInfo& latency,
    InputEventDispatchType original_dispatch_type,
    InputEventAckState ack_result) {
  DCHECK(original_dispatch_type == DISPATCH_TYPE_BLOCKING ||
         original_dispatch_type == DISPATCH_TYPE_NON_BLOCKING);
  DCHECK(ack_result == INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING ||
         ack_result == INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING_DUE_TO_FLING ||
         ack_result == INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  bool non_blocking = original_dispatch_type == DISPATCH_TYPE_NON_BLOCKING ||
                      ack_result == INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING;
  bool is_wheel = event->type == blink::WebInputEvent::MouseWheel;
  bool is_touch = blink::WebInputEvent::isTouchEventType(event->type);

  if (is_touch) {
    blink::WebTouchEvent* touch_event =
        static_cast<blink::WebTouchEvent*>(event.get());

    // Adjust the |dispatchType| on the event since the compositor
    // determined all event listeners are passive.
    if (non_blocking) {
      touch_event->dispatchType =
          blink::WebInputEvent::ListenersNonBlockingPassive;
    }
    if (touch_event->type == blink::WebInputEvent::TouchStart)
      last_touch_start_forced_nonblocking_due_to_fling_ = false;

    if (enable_fling_passive_listener_flag_ &&
        touch_event->touchStartOrFirstTouchMove &&
        touch_event->dispatchType == blink::WebInputEvent::Blocking) {
      // If the touch start is forced to be passive due to fling, its following
      // touch move should also be passive.
      if (ack_result == INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING_DUE_TO_FLING ||
          last_touch_start_forced_nonblocking_due_to_fling_) {
        touch_event->dispatchType =
            blink::WebInputEvent::ListenersForcedNonBlockingDueToFling;
        non_blocking = true;
        last_touch_start_forced_nonblocking_due_to_fling_ = true;
      }
    }
  }
  if (is_wheel && non_blocking) {
    // Adjust the |dispatchType| on the event since the compositor
    // determined all event listeners are passive.
    static_cast<blink::WebMouseWheelEvent*>(event.get())
        ->dispatchType = blink::WebInputEvent::ListenersNonBlockingPassive;
  }

  InputEventDispatchType dispatch_type =
      non_blocking ? DISPATCH_TYPE_NON_BLOCKING : DISPATCH_TYPE_BLOCKING;
  std::unique_ptr<EventWithDispatchType> event_with_dispatch_type(
      new EventWithDispatchType(std::move(event), latency, dispatch_type));

  QueueEvent(std::move(event_with_dispatch_type));

  // send an ack when we are non-blocking.
  return non_blocking;
}

void MainThreadEventQueue::DispatchInFlightEvent() {
  if (in_flight_event_) {
    // Report the coalesced count only for continuous events; otherwise
    // the zero value would be dominated by non-continuous events.
    base::TimeTicks now = base::TimeTicks::Now();
    if (IsContinuousEvent(in_flight_event_)) {
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Event.MainThreadEventQueue.Continuous.QueueingTime",
          (now - in_flight_event_->creationTimestamp()).InMicroseconds(), 1,
          kTenSeconds, 50);

      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Event.MainThreadEventQueue.Continuous.FreshnessTime",
          (now - in_flight_event_->lastCoalescedTimestamp()).InMicroseconds(),
          1, kTenSeconds, 50);

      UMA_HISTOGRAM_COUNTS_1000("Event.MainThreadEventQueue.CoalescedCount",
                                in_flight_event_->coalescedEventIds().size());
    } else {
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Event.MainThreadEventQueue.NonContinuous.QueueingTime",
          (now - in_flight_event_->creationTimestamp()).InMicroseconds(), 1,
          kTenSeconds, 50);
    }

    InputEventDispatchType dispatch_type = in_flight_event_->dispatchType();
    if (!in_flight_event_->coalescedEventIds().empty() &&
        dispatch_type == DISPATCH_TYPE_BLOCKING) {
      dispatch_type = DISPATCH_TYPE_BLOCKING_NOTIFY_MAIN;
    }

    client_->HandleEventOnMainThread(routing_id_, &in_flight_event_->event(),
                                     in_flight_event_->latencyInfo(),
                                     dispatch_type);
  }

  in_flight_event_.reset();
}

void MainThreadEventQueue::PossiblyScheduleMainFrame() {
  if (IsRafAlignedInputDisabled())
    return;
  bool needs_main_frame = false;
  {
    base::AutoLock lock(shared_state_lock_);
    if (!shared_state_.sent_main_frame_request_ &&
        !shared_state_.events_.empty() &&
        IsRafAlignedEvent(shared_state_.events_.front())) {
      needs_main_frame = !shared_state_.sent_main_frame_request_;
      shared_state_.sent_main_frame_request_ = false;
    }
  }
  if (needs_main_frame)
    client_->NeedsMainFrame(routing_id_);
}

void MainThreadEventQueue::DispatchSingleEvent() {
  {
    base::AutoLock lock(shared_state_lock_);
    if (shared_state_.events_.empty())
      return;

    in_flight_event_ = shared_state_.events_.Pop();
  }
  DispatchInFlightEvent();
  PossiblyScheduleMainFrame();
}

void MainThreadEventQueue::EventHandled(blink::WebInputEvent::Type type,
                                        InputEventAckState ack_result) {
  if (in_flight_event_ &&
      in_flight_event_->dispatchType() == DISPATCH_TYPE_BLOCKING) {
    for (const auto id : in_flight_event_->coalescedEventIds()) {
      client_->SendInputEventAck(routing_id_, type, ack_result, id);
      if (renderer_scheduler_) {
        renderer_scheduler_->DidHandleInputEventOnMainThread(
            in_flight_event_->event());
      }
    }
  }
}

void MainThreadEventQueue::DispatchRafAlignedInput() {
  if (IsRafAlignedInputDisabled())
    return;

  std::deque<std::unique_ptr<EventWithDispatchType>> events_to_process;
  {
    base::AutoLock lock(shared_state_lock_);
    shared_state_.sent_main_frame_request_ = false;

    while(!shared_state_.events_.empty()) {
      if (!IsRafAlignedEvent(shared_state_.events_.front()))
        break;
      events_to_process.emplace_back(shared_state_.events_.Pop());
    }
  }

  while(!events_to_process.empty()) {
    in_flight_event_ = std::move(events_to_process.front());
    events_to_process.pop_front();
    DispatchInFlightEvent();
  }
  PossiblyScheduleMainFrame();
}

void MainThreadEventQueue::SendEventNotificationToMainThread() {
  main_task_runner_->PostTask(
      FROM_HERE, base::Bind(&MainThreadEventQueue::DispatchSingleEvent, this));
}

void MainThreadEventQueue::QueueEvent(
    std::unique_ptr<EventWithDispatchType> event) {
  bool is_raf_aligned = IsRafAlignedEvent(event);
  size_t send_notification_count = 0;
  bool needs_main_frame = false;
  {
    base::AutoLock lock(shared_state_lock_);
    size_t size_before = shared_state_.events_.size();
    shared_state_.events_.Queue(std::move(event));
    size_t size_after = shared_state_.events_.size();

    if (size_before != size_after) {
      if (IsRafAlignedInputDisabled()) {
        send_notification_count = 1;
      } else if (!is_raf_aligned) {
        send_notification_count = 1;
        // If we had just enqueued a non-rAF input event we will send a series
        // of normal post messages to ensure they are all handled right away.
        for (size_t pos = size_after - 1; pos >= 1; --pos) {
          if (IsRafAlignedEvent(shared_state_.events_.at(pos - 1)))
            send_notification_count++;
          else
            break;
        }
      } else {
        needs_main_frame = !shared_state_.sent_main_frame_request_;
        shared_state_.sent_main_frame_request_ = true;
      }
    }
  }
  for (size_t i = 0; i < send_notification_count; ++i)
    SendEventNotificationToMainThread();
  if (needs_main_frame)
    client_->NeedsMainFrame(routing_id_);
}

bool MainThreadEventQueue::IsRafAlignedInputDisabled() {
  return !handle_raf_aligned_mouse_input_ && !handle_raf_aligned_touch_input_;
}

bool MainThreadEventQueue::IsRafAlignedEvent(
    const std::unique_ptr<EventWithDispatchType>& event) {
  switch (event->event().type) {
    case blink::WebInputEvent::MouseMove:
    case blink::WebInputEvent::MouseWheel:
      return handle_raf_aligned_mouse_input_;
    case blink::WebInputEvent::TouchMove:
      // TouchMoves that are blocking end up blocking scroll. Do not treat
      // them as continuous events otherwise we will end up waiting up to an
      // additional frame.
      return static_cast<const blink::WebTouchEvent&>(event->event())
                     .dispatchType != blink::WebInputEvent::Blocking &&
             handle_raf_aligned_touch_input_;
    default:
      return false;
  }
}

}  // namespace content
