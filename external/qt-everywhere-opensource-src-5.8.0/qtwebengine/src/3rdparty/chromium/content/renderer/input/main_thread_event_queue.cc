// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/input/main_thread_event_queue.h"

namespace content {

MainThreadEventQueue::MainThreadEventQueue(int routing_id,
                                           MainThreadEventQueueClient* client)
    : routing_id_(routing_id), client_(client), is_flinging_(false) {}

MainThreadEventQueue::~MainThreadEventQueue() {}

bool MainThreadEventQueue::HandleEvent(
    const blink::WebInputEvent* event,
    const ui::LatencyInfo& latency,
    InputEventDispatchType original_dispatch_type,
    InputEventAckState ack_result) {
  DCHECK(original_dispatch_type == DISPATCH_TYPE_BLOCKING ||
         original_dispatch_type == DISPATCH_TYPE_NON_BLOCKING);
  DCHECK(ack_result == INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING ||
         ack_result == INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  bool non_blocking = original_dispatch_type == DISPATCH_TYPE_NON_BLOCKING ||
                      ack_result == INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING;

  InputEventDispatchType dispatch_type =
      non_blocking ? DISPATCH_TYPE_NON_BLOCKING_NOTIFY_MAIN
                   : DISPATCH_TYPE_BLOCKING_NOTIFY_MAIN;

  if (event->type == blink::WebInputEvent::MouseWheel) {
    PendingMouseWheelEvent modified_dispatch_type_event =
        PendingMouseWheelEvent(
            *static_cast<const blink::WebMouseWheelEvent*>(event), latency,
            dispatch_type);

    // Adjust the |dispatchType| on the event since the compositor
    // determined all event listeners are passive.
    if (non_blocking) {
      modified_dispatch_type_event.event.dispatchType =
          blink::WebInputEvent::ListenersNonBlockingPassive;
    }

    if (wheel_events_.state() == WebInputEventQueueState::ITEM_PENDING) {
      wheel_events_.Queue(modified_dispatch_type_event);
    } else {
      if (non_blocking) {
        wheel_events_.set_state(WebInputEventQueueState::ITEM_PENDING);
        client_->SendEventToMainThread(routing_id_,
                                       &modified_dispatch_type_event.event,
                                       latency, dispatch_type);
      } else {
        // If there is nothing in the event queue and the event is
        // blocking pass the |original_dispatch_type| to avoid
        // having the main thread call us back as an optimization.
        client_->SendEventToMainThread(routing_id_,
                                       &modified_dispatch_type_event.event,
                                       latency, original_dispatch_type);
      }
    }
  } else if (blink::WebInputEvent::isTouchEventType(event->type)) {
    PendingTouchEvent modified_dispatch_type_event =
        PendingTouchEvent(*static_cast<const blink::WebTouchEvent*>(event),
                          latency, dispatch_type);
    modified_dispatch_type_event.event.dispatchedDuringFling = is_flinging_;

    // Adjust the |dispatchType| on the event since the compositor
    // determined all event listeners are passive.
    if (non_blocking) {
      modified_dispatch_type_event.event.dispatchType =
          blink::WebInputEvent::ListenersNonBlockingPassive;
    }

    if (touch_events_.state() == WebInputEventQueueState::ITEM_PENDING) {
      touch_events_.Queue(modified_dispatch_type_event);
    } else {
      if (non_blocking) {
        touch_events_.set_state(WebInputEventQueueState::ITEM_PENDING);
        client_->SendEventToMainThread(routing_id_,
                                       &modified_dispatch_type_event.event,
                                       latency, dispatch_type);
      } else {
        // If there is nothing in the event queue and the event is
        // blocking pass the |original_dispatch_type| to avoid
        // having the main thread call us back as an optimization.
        client_->SendEventToMainThread(routing_id_,
                                       &modified_dispatch_type_event.event,
                                       latency, original_dispatch_type);
      }
    }
  } else {
    client_->SendEventToMainThread(routing_id_, event, latency,
                                   original_dispatch_type);
  }

  // send an ack when we are non-blocking.
  return non_blocking;
}

void MainThreadEventQueue::EventHandled(blink::WebInputEvent::Type type,
                                        InputEventAckState ack_result) {
  if (type == blink::WebInputEvent::MouseWheel) {
    // There should not be two blocking wheel events in flight.
    DCHECK(!in_flight_wheel_event_ ||
           in_flight_wheel_event_->eventsToAck.size() == 0);

    if (!wheel_events_.empty()) {
      in_flight_wheel_event_ = wheel_events_.Pop();
      client_->SendEventToMainThread(
          routing_id_, &in_flight_wheel_event_->event,
          in_flight_wheel_event_->latency, in_flight_wheel_event_->type);
    } else {
      in_flight_wheel_event_.reset();
      wheel_events_.set_state(WebInputEventQueueState::ITEM_NOT_PENDING);
    }
  } else if (blink::WebInputEvent::isTouchEventType(type)) {
    if (in_flight_touch_event_) {
      // Send acks for blocking touch events.
      for (const auto id : in_flight_touch_event_->eventsToAck)
        client_->SendInputEventAck(routing_id_, type, ack_result, id);
    }

    if (!touch_events_.empty()) {
      in_flight_touch_event_ = touch_events_.Pop();
      client_->SendEventToMainThread(
          routing_id_, &in_flight_touch_event_->event,
          in_flight_touch_event_->latency, in_flight_touch_event_->type);
    } else {
      in_flight_touch_event_.reset();
      touch_events_.set_state(WebInputEventQueueState::ITEM_NOT_PENDING);
    }
  } else {
    NOTREACHED() << "Invalid passive event type";
  }
}

}  // namespace content
