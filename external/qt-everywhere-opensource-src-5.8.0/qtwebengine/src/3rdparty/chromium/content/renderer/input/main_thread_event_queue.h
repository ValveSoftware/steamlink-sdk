// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INPUT_MAIN_THREAD_EVENT_QUEUE_H_
#define CONTENT_RENDERER_INPUT_MAIN_THREAD_EVENT_QUEUE_H_

#include <deque>
#include "content/common/content_export.h"
#include "content/common/input/event_with_latency_info.h"
#include "content/common/input/input_event_ack_state.h"
#include "content/common/input/input_event_dispatch_type.h"
#include "content/common/input/web_input_event_queue.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/latency_info.h"

namespace content {

template <typename BaseClass, typename BaseType>
class EventWithDispatchType : public BaseClass {
 public:
  EventWithDispatchType(const BaseType& e,
                        const ui::LatencyInfo& l,
                        InputEventDispatchType t)
      : BaseClass(e, l), type(t) {}

  InputEventDispatchType type;
  std::deque<uint32_t> eventsToAck;

  bool CanCoalesceWith(const EventWithDispatchType& other) const
      WARN_UNUSED_RESULT {
    return other.type == type && BaseClass::CanCoalesceWith(other);
  }

  void CoalesceWith(const EventWithDispatchType& other) {
    // If we are blocking and are coalescing touch, make sure to keep
    // the touch ids that need to be acked.
    if (type == DISPATCH_TYPE_BLOCKING_NOTIFY_MAIN) {
      // We should only have blocking touch events that need coalescing.
      DCHECK(blink::WebInputEvent::isTouchEventType(other.event.type));
      eventsToAck.push_back(
          WebInputEventTraits::GetUniqueTouchEventId(other.event));
    }
    BaseClass::CoalesceWith(other);
  }
};

using PendingMouseWheelEvent =
    EventWithDispatchType<MouseWheelEventWithLatencyInfo,
                          blink::WebMouseWheelEvent>;

using PendingTouchEvent =
    EventWithDispatchType<TouchEventWithLatencyInfo, blink::WebTouchEvent>;

class CONTENT_EXPORT MainThreadEventQueueClient {
 public:
  // Send an |event| that was previously queued (possibly
  // coalesced with another event) to the |routing_id|'s
  // channel. Implementors must implement this callback.
  virtual void SendEventToMainThread(int routing_id,
                                     const blink::WebInputEvent* event,
                                     const ui::LatencyInfo& latency,
                                     InputEventDispatchType dispatch_type) = 0;

  virtual void SendInputEventAck(int routing_id,
                                 blink::WebInputEvent::Type type,
                                 InputEventAckState ack_result,
                                 uint32_t touch_event_id) = 0;
};

// MainThreadEventQueue implements a series of queues (one touch
// and one mouse wheel) for events that need to be queued between
// the compositor and main threads. When an event is sent
// from the compositor to main it can either be sent directly if no
// outstanding events of that type are in flight; or it needs to
// wait in a queue until the main thread has finished processing
// the in-flight event. This class tracks the state and queues
// for the event types. Methods on this class should only be called
// from the compositor thread.
//
// Below some example flows are how the code behaves.
// Legend: B=Browser, C=Compositor, M=Main Thread, NB=Non-blocking
//         BL=Blocking, PT=Post Task, ACK=Acknowledgement
//
// Normal blocking event sent to main thread.
//   B        C        M
//   ---(BL)-->
//            ---(PT)-->
//   <-------(ACK)------
//
// Non-blocking event sent to main thread.
//   B        C        M
//   ---(NB)-->
//            ---(PT)-->
//            <--(PT)---
//
// Non-blocking followed by blocking event sent to main thread.
//   B        C        M
//   ---(NB)-->
//            ---(PT)-->
//   ---(BL)-->
//            <--(PT)---     // Notify from NB event.
//            ---(PT)-->     // Release blocking event.
//            <--(PT)---     // Notify from BL event.
//   <-------(ACK)------
//
class CONTENT_EXPORT MainThreadEventQueue {
 public:
  MainThreadEventQueue(int routing_id, MainThreadEventQueueClient* client);
  ~MainThreadEventQueue();

  // Called once the compositor has handled |event| and indicated that it is
  // a non-blocking event to be queued to the main thread.
  bool HandleEvent(const blink::WebInputEvent* event,
                   const ui::LatencyInfo& latency,
                   InputEventDispatchType dispatch_type,
                   InputEventAckState ack_result);

  // Call once the main thread has handled an outstanding |type| event
  // in flight.
  void EventHandled(blink::WebInputEvent::Type type,
                    InputEventAckState ack_result);

  void set_is_flinging(bool is_flinging) { is_flinging_ = is_flinging; }

 private:
  friend class MainThreadEventQueueTest;
  int routing_id_;
  MainThreadEventQueueClient* client_;
  WebInputEventQueue<PendingMouseWheelEvent> wheel_events_;
  WebInputEventQueue<PendingTouchEvent> touch_events_;
  bool is_flinging_;

  // TODO(dtapuska): These can be removed when the queues are dequeued
  // on the main thread. See crbug.com/624021
  std::unique_ptr<PendingMouseWheelEvent> in_flight_wheel_event_;
  std::unique_ptr<PendingTouchEvent> in_flight_touch_event_;

  DISALLOW_COPY_AND_ASSIGN(MainThreadEventQueue);
};

}  // namespace content

#endif  // CONTENT_RENDERER_INPUT_MAIN_THREAD_EVENT_QUEUE_H_
