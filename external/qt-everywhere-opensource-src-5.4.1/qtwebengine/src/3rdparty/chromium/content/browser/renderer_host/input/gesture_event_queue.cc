// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/gesture_event_queue.h"

#include "base/debug/trace_event.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/renderer_host/input/input_router.h"
#include "content/browser/renderer_host/input/touchpad_tap_suppression_controller.h"
#include "content/browser/renderer_host/input/touchscreen_tap_suppression_controller.h"
#include "content/public/common/content_switches.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;

namespace content {

GestureEventQueue::Config::Config() {
}

GestureEventQueue::GestureEventQueue(
    GestureEventQueueClient* client,
    TouchpadTapSuppressionControllerClient* touchpad_client,
    const Config& config)
    : client_(client),
      fling_in_progress_(false),
      scrolling_in_progress_(false),
      ignore_next_ack_(false),
      touchpad_tap_suppression_controller_(
          touchpad_client,
          config.touchpad_tap_suppression_config),
      touchscreen_tap_suppression_controller_(
          this,
          config.touchscreen_tap_suppression_config),
      debounce_interval_(config.debounce_interval) {
  DCHECK(client);
  DCHECK(touchpad_client);
}

GestureEventQueue::~GestureEventQueue() { }

bool GestureEventQueue::ShouldDiscardFlingCancelEvent(
    const GestureEventWithLatencyInfo& gesture_event) const {
  if (coalesced_gesture_events_.empty() && fling_in_progress_)
    return false;
  GestureQueue::const_reverse_iterator it =
      coalesced_gesture_events_.rbegin();
  while (it != coalesced_gesture_events_.rend()) {
    if (it->event.type == WebInputEvent::GestureFlingStart)
      return false;
    if (it->event.type == WebInputEvent::GestureFlingCancel)
      return true;
    it++;
  }
  return true;
}

bool GestureEventQueue::ShouldForwardForBounceReduction(
    const GestureEventWithLatencyInfo& gesture_event) {
  if (debounce_interval_ <= base::TimeDelta())
    return true;
  switch (gesture_event.event.type) {
    case WebInputEvent::GestureScrollUpdate:
      if (!scrolling_in_progress_) {
        debounce_deferring_timer_.Start(
            FROM_HERE,
            debounce_interval_,
            this,
            &GestureEventQueue::SendScrollEndingEventsNow);
      } else {
        // Extend the bounce interval.
        debounce_deferring_timer_.Reset();
      }
      scrolling_in_progress_ = true;
      debouncing_deferral_queue_.clear();
      return true;
    case WebInputEvent::GesturePinchBegin:
    case WebInputEvent::GesturePinchEnd:
    case WebInputEvent::GesturePinchUpdate:
      // TODO(rjkroege): Debounce pinch (http://crbug.com/147647)
      return true;
    default:
      if (scrolling_in_progress_) {
        debouncing_deferral_queue_.push_back(gesture_event);
        return false;
      }
      return true;
  }
}

// NOTE: The filters are applied successively. This simplifies the change.
bool GestureEventQueue::ShouldForward(
    const GestureEventWithLatencyInfo& gesture_event) {
  TRACE_EVENT0("input", "GestureEventQueue::ShouldForward");
  return ShouldForwardForBounceReduction(gesture_event) &&
         ShouldForwardForGFCFiltering(gesture_event) &&
         ShouldForwardForTapSuppression(gesture_event) &&
         ShouldForwardForCoalescing(gesture_event);
}

bool GestureEventQueue::ShouldForwardForGFCFiltering(
    const GestureEventWithLatencyInfo& gesture_event) const {
  return gesture_event.event.type != WebInputEvent::GestureFlingCancel ||
      !ShouldDiscardFlingCancelEvent(gesture_event);
}

bool GestureEventQueue::ShouldForwardForTapSuppression(
    const GestureEventWithLatencyInfo& gesture_event) {
  switch (gesture_event.event.type) {
    case WebInputEvent::GestureFlingCancel:
      if (gesture_event.event.sourceDevice ==
          blink::WebGestureDeviceTouchscreen)
        touchscreen_tap_suppression_controller_.GestureFlingCancel();
      else
        touchpad_tap_suppression_controller_.GestureFlingCancel();
      return true;
    case WebInputEvent::GestureTapDown:
    case WebInputEvent::GestureShowPress:
    case WebInputEvent::GestureTapUnconfirmed:
    case WebInputEvent::GestureTapCancel:
    case WebInputEvent::GestureTap:
    case WebInputEvent::GestureDoubleTap:
      if (gesture_event.event.sourceDevice ==
          blink::WebGestureDeviceTouchscreen) {
        return !touchscreen_tap_suppression_controller_.FilterTapEvent(
            gesture_event);
      }
      return true;
    default:
      return true;
  }
}

bool GestureEventQueue::ShouldForwardForCoalescing(
    const GestureEventWithLatencyInfo& gesture_event) {
  switch (gesture_event.event.type) {
    case WebInputEvent::GestureFlingCancel:
      fling_in_progress_ = false;
      break;
    case WebInputEvent::GestureFlingStart:
      fling_in_progress_ = true;
      break;
    case WebInputEvent::GesturePinchUpdate:
    case WebInputEvent::GestureScrollUpdate:
      MergeOrInsertScrollAndPinchEvent(gesture_event);
      return ShouldHandleEventNow();
    default:
      break;
  }
  coalesced_gesture_events_.push_back(gesture_event);
  return ShouldHandleEventNow();
}

void GestureEventQueue::ProcessGestureAck(InputEventAckState ack_result,
                                           WebInputEvent::Type type,
                                           const ui::LatencyInfo& latency) {
  TRACE_EVENT0("input", "GestureEventQueue::ProcessGestureAck");

  if (coalesced_gesture_events_.empty()) {
    DLOG(ERROR) << "Received unexpected ACK for event type " << type;
    return;
  }

  // It's possible that the ack for the second event in an in-flight, coalesced
  // Gesture{Scroll,Pinch}Update pair is received prior to the first event ack.
  // TODO(jdduke): Unify GSU/GPU pairs into a single event, crbug.com/359115.
  size_t event_index = 0;
  if (ignore_next_ack_ &&
      coalesced_gesture_events_.size() > 1 &&
      coalesced_gesture_events_[0].event.type != type &&
      coalesced_gesture_events_[1].event.type == type) {
    event_index = 1;
  }
  GestureEventWithLatencyInfo event_with_latency =
      coalesced_gesture_events_[event_index];
  DCHECK_EQ(event_with_latency.event.type, type);
  event_with_latency.latency.AddNewLatencyFrom(latency);

  // Ack'ing an event may enqueue additional gesture events.  By ack'ing the
  // event before the forwarding of queued events below, such additional events
  // can be coalesced with existing queued events prior to dispatch.
  client_->OnGestureEventAck(event_with_latency, ack_result);

  const bool processed = (INPUT_EVENT_ACK_STATE_CONSUMED == ack_result);
  if (type == WebInputEvent::GestureFlingCancel) {
    if (event_with_latency.event.sourceDevice ==
        blink::WebGestureDeviceTouchscreen)
      touchscreen_tap_suppression_controller_.GestureFlingCancelAck(processed);
    else
      touchpad_tap_suppression_controller_.GestureFlingCancelAck(processed);
  }
  DCHECK_LT(event_index, coalesced_gesture_events_.size());
  coalesced_gesture_events_.erase(coalesced_gesture_events_.begin() +
                                  event_index);

  if (ignore_next_ack_) {
    ignore_next_ack_ = false;
    return;
  }

  if (coalesced_gesture_events_.empty())
    return;

  const GestureEventWithLatencyInfo& first_gesture_event =
      coalesced_gesture_events_.front();

  // TODO(jdduke): Unify GSU/GPU pairs into a single event, crbug.com/359115.
  // Check for the coupled GesturePinchUpdate before sending either event,
  // handling the case where the first GestureScrollUpdate ack is synchronous.
  GestureEventWithLatencyInfo second_gesture_event;
  if (first_gesture_event.event.type == WebInputEvent::GestureScrollUpdate &&
      coalesced_gesture_events_.size() > 1 &&
      coalesced_gesture_events_[1].event.type ==
          WebInputEvent::GesturePinchUpdate) {
    second_gesture_event = coalesced_gesture_events_[1];
    ignore_next_ack_ = true;
  }

  client_->SendGestureEventImmediately(first_gesture_event);
  if (second_gesture_event.event.type != WebInputEvent::Undefined)
    client_->SendGestureEventImmediately(second_gesture_event);
}

TouchpadTapSuppressionController*
    GestureEventQueue::GetTouchpadTapSuppressionController() {
  return &touchpad_tap_suppression_controller_;
}

bool GestureEventQueue::ExpectingGestureAck() const {
  return !coalesced_gesture_events_.empty();
}

void GestureEventQueue::FlingHasBeenHalted() {
  fling_in_progress_ = false;
}

bool GestureEventQueue::ShouldHandleEventNow() const {
  return coalesced_gesture_events_.size() == 1;
}

void GestureEventQueue::ForwardGestureEvent(
    const GestureEventWithLatencyInfo& gesture_event) {
  if (ShouldForwardForCoalescing(gesture_event))
    client_->SendGestureEventImmediately(gesture_event);
}

void GestureEventQueue::SendScrollEndingEventsNow() {
  scrolling_in_progress_ = false;
  if (debouncing_deferral_queue_.empty())
    return;
  GestureQueue debouncing_deferral_queue;
  debouncing_deferral_queue.swap(debouncing_deferral_queue_);
  for (GestureQueue::const_iterator it = debouncing_deferral_queue.begin();
       it != debouncing_deferral_queue.end(); it++) {
    if (ShouldForwardForGFCFiltering(*it) &&
        ShouldForwardForTapSuppression(*it) &&
        ShouldForwardForCoalescing(*it)) {
      client_->SendGestureEventImmediately(*it);
    }
  }
}

void GestureEventQueue::MergeOrInsertScrollAndPinchEvent(
    const GestureEventWithLatencyInfo& gesture_event) {
  const size_t unsent_events_count =
      coalesced_gesture_events_.size() - EventsInFlightCount();
  if (!unsent_events_count) {
    coalesced_gesture_events_.push_back(gesture_event);
    return;
  }

  GestureEventWithLatencyInfo* last_event = &coalesced_gesture_events_.back();
  if (last_event->CanCoalesceWith(gesture_event)) {
    last_event->CoalesceWith(gesture_event);
    return;
  }

  if (!ShouldTryMerging(gesture_event, *last_event)) {
    coalesced_gesture_events_.push_back(gesture_event);
    return;
  }

  GestureEventWithLatencyInfo scroll_event;
  GestureEventWithLatencyInfo pinch_event;
  scroll_event.event.modifiers |= gesture_event.event.modifiers;
  scroll_event.event.sourceDevice = gesture_event.event.sourceDevice;
  scroll_event.event.timeStampSeconds = gesture_event.event.timeStampSeconds;
  // Keep the oldest LatencyInfo.
  DCHECK_LE(last_event->latency.trace_id, gesture_event.latency.trace_id);
  scroll_event.latency = last_event->latency;
  pinch_event = scroll_event;
  scroll_event.event.type = WebInputEvent::GestureScrollUpdate;
  pinch_event.event.type = WebInputEvent::GesturePinchUpdate;
  pinch_event.event.x = gesture_event.event.type ==
      WebInputEvent::GesturePinchUpdate ?
          gesture_event.event.x : last_event->event.x;
  pinch_event.event.y = gesture_event.event.type ==
      WebInputEvent::GesturePinchUpdate ?
          gesture_event.event.y : last_event->event.y;

  gfx::Transform combined_scroll_pinch = GetTransformForEvent(*last_event);
  // Only include the second-to-last event in the coalesced pair if it exists
  // and can be combined with the new event.
  if (unsent_events_count > 1) {
    const GestureEventWithLatencyInfo& second_last_event =
        coalesced_gesture_events_[coalesced_gesture_events_.size() - 2];
    if (ShouldTryMerging(gesture_event, second_last_event)) {
      // Keep the oldest LatencyInfo.
      DCHECK_LE(second_last_event.latency.trace_id,
                scroll_event.latency.trace_id);
      scroll_event.latency = second_last_event.latency;
      pinch_event.latency = second_last_event.latency;
      combined_scroll_pinch.PreconcatTransform(
          GetTransformForEvent(second_last_event));
      coalesced_gesture_events_.pop_back();
    }
  }
  combined_scroll_pinch.ConcatTransform(GetTransformForEvent(gesture_event));
  coalesced_gesture_events_.pop_back();

  float combined_scale =
      SkMScalarToFloat(combined_scroll_pinch.matrix().get(0, 0));
  float combined_scroll_pinch_x =
      SkMScalarToFloat(combined_scroll_pinch.matrix().get(0, 3));
  float combined_scroll_pinch_y =
      SkMScalarToFloat(combined_scroll_pinch.matrix().get(1, 3));
  scroll_event.event.data.scrollUpdate.deltaX =
      (combined_scroll_pinch_x + pinch_event.event.x) / combined_scale -
      pinch_event.event.x;
  scroll_event.event.data.scrollUpdate.deltaY =
      (combined_scroll_pinch_y + pinch_event.event.y) / combined_scale -
      pinch_event.event.y;
  coalesced_gesture_events_.push_back(scroll_event);
  pinch_event.event.data.pinchUpdate.scale = combined_scale;
  coalesced_gesture_events_.push_back(pinch_event);
}

bool GestureEventQueue::ShouldTryMerging(
    const GestureEventWithLatencyInfo& new_event,
    const GestureEventWithLatencyInfo& event_in_queue) const {
  DLOG_IF(WARNING,
          new_event.event.timeStampSeconds <
          event_in_queue.event.timeStampSeconds)
          << "Event time not monotonic?\n";
  return (event_in_queue.event.type == WebInputEvent::GestureScrollUpdate ||
      event_in_queue.event.type == WebInputEvent::GesturePinchUpdate) &&
      event_in_queue.event.modifiers == new_event.event.modifiers &&
      event_in_queue.event.sourceDevice == new_event.event.sourceDevice;
}

gfx::Transform GestureEventQueue::GetTransformForEvent(
    const GestureEventWithLatencyInfo& gesture_event) const {
  gfx::Transform gesture_transform;
  if (gesture_event.event.type == WebInputEvent::GestureScrollUpdate) {
    gesture_transform.Translate(gesture_event.event.data.scrollUpdate.deltaX,
                                gesture_event.event.data.scrollUpdate.deltaY);
  } else if (gesture_event.event.type == WebInputEvent::GesturePinchUpdate) {
    float scale = gesture_event.event.data.pinchUpdate.scale;
    gesture_transform.Translate(-gesture_event.event.x, -gesture_event.event.y);
    gesture_transform.Scale(scale,scale);
    gesture_transform.Translate(gesture_event.event.x, gesture_event.event.y);
  }
  return gesture_transform;
}

size_t GestureEventQueue::EventsInFlightCount() const {
  if (coalesced_gesture_events_.empty())
    return 0;

  if (!ignore_next_ack_)
    return 1;

  DCHECK_GT(coalesced_gesture_events_.size(), 1U);
  return 2;
}

}  // namespace content
