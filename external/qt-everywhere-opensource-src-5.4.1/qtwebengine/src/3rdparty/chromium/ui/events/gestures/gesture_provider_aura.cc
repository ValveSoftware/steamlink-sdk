// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gestures/gesture_provider_aura.h"

#include "base/auto_reset.h"
#include "base/logging.h"
#include "ui/events/event.h"
#include "ui/events/gesture_detection/gesture_config_helper.h"
#include "ui/events/gesture_detection/gesture_event_data.h"
#include "ui/events/gestures/gesture_configuration.h"

namespace ui {

GestureProviderAura::GestureProviderAura(GestureProviderAuraClient* client)
    : client_(client),
      filtered_gesture_provider_(ui::DefaultGestureProviderConfig(), this),
      handling_event_(false) {
  filtered_gesture_provider_.SetDoubleTapSupportForPlatformEnabled(false);
}

GestureProviderAura::~GestureProviderAura() {}

bool GestureProviderAura::OnTouchEvent(const TouchEvent& event) {
  bool pointer_id_is_active = false;
  for (size_t i = 0; i < pointer_state_.GetPointerCount(); ++i) {
    if (event.touch_id() != pointer_state_.GetPointerId(i))
      continue;
    pointer_id_is_active = true;
    break;
  }

  if (event.type() == ET_TOUCH_PRESSED && pointer_id_is_active) {
    // Ignore touch press events if we already believe the pointer is down.
    return false;
  } else if (event.type() != ET_TOUCH_PRESSED && !pointer_id_is_active) {
    // We could have an active touch stream transfered to us, resulting in touch
    // move or touch up events without associated touch down events. Ignore
    // them.
    return false;
  }

  last_touch_event_flags_ = event.flags();
  last_touch_event_latency_info_ = *event.latency();
  pointer_state_.OnTouch(event);

  bool result = filtered_gesture_provider_.OnTouchEvent(pointer_state_);
  pointer_state_.CleanupRemovedTouchPoints(event);
  return result;
}

void GestureProviderAura::OnTouchEventAck(bool event_consumed) {
  DCHECK(pending_gestures_.empty());
  DCHECK(!handling_event_);
  base::AutoReset<bool> handling_event(&handling_event_, true);
  filtered_gesture_provider_.OnTouchEventAck(event_consumed);
  last_touch_event_latency_info_.Clear();
}

void GestureProviderAura::OnGestureEvent(
    const GestureEventData& gesture) {
  GestureEventDetails details = gesture.details;

  if (gesture.type() == ET_GESTURE_TAP) {
    int tap_count = 1;
    if (previous_tap_ && IsConsideredDoubleTap(*previous_tap_, gesture))
      tap_count = 1 + (previous_tap_->details.tap_count() % 3);
    details.set_tap_count(tap_count);
    if (!previous_tap_)
      previous_tap_.reset(new GestureEventData(gesture));
    else
      *previous_tap_ = gesture;
    previous_tap_->details = details;
  } else if (gesture.type() == ET_GESTURE_TAP_CANCEL) {
    previous_tap_.reset();
  }

  scoped_ptr<ui::GestureEvent> event(
      new ui::GestureEvent(gesture.type(),
                           gesture.x,
                           gesture.y,
                           last_touch_event_flags_,
                           gesture.time - base::TimeTicks(),
                           details,
                           // ui::GestureEvent stores a bitfield indicating the
                           // ids of active touch points. This is currently only
                           // used when one finger is down, and will eventually
                           // be cleaned up. See crbug.com/366707.
                           1 << gesture.motion_event_id));


  ui::LatencyInfo* gesture_latency = event->latency();

  gesture_latency->CopyLatencyFrom(
      last_touch_event_latency_info_,
      ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT);
  gesture_latency->CopyLatencyFrom(
      last_touch_event_latency_info_,
      ui::INPUT_EVENT_LATENCY_UI_COMPONENT);
  gesture_latency->CopyLatencyFrom(
      last_touch_event_latency_info_,
      ui::INPUT_EVENT_LATENCY_ACKED_TOUCH_COMPONENT);

  if (!handling_event_) {
    // Dispatching event caused by timer.
    client_->OnGestureEvent(event.get());
  } else {
    // Memory managed by ScopedVector pending_gestures_.
    pending_gestures_.push_back(event.release());
  }
}

ScopedVector<GestureEvent>* GestureProviderAura::GetAndResetPendingGestures() {
  if (pending_gestures_.empty())
    return NULL;
  // Caller is responsible for deleting old_pending_gestures.
  ScopedVector<GestureEvent>* old_pending_gestures =
      new ScopedVector<GestureEvent>();
  old_pending_gestures->swap(pending_gestures_);
  return old_pending_gestures;
}

bool GestureProviderAura::IsConsideredDoubleTap(
    const GestureEventData& previous_tap,
    const GestureEventData& current_tap) const {
  if (current_tap.time - previous_tap.time >
      base::TimeDelta::FromMilliseconds(
          ui::GestureConfiguration::max_seconds_between_double_click() *
          1000)) {
    return false;
  }

  double double_tap_slop_square =
      GestureConfiguration::max_distance_between_taps_for_double_tap();
  double_tap_slop_square *= double_tap_slop_square;
  const float delta_x = previous_tap.x - current_tap.x;
  const float delta_y = previous_tap.y - current_tap.y;
  return (delta_x * delta_x + delta_y * delta_y < double_tap_slop_square);
}

}  // namespace content
