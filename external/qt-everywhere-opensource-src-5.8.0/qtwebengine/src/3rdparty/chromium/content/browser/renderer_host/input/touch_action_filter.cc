// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touch_action_filter.h"

#include <math.h>

#include "base/logging.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

using blink::WebInputEvent;
using blink::WebGestureEvent;

namespace content {
namespace {

// Actions on an axis are disallowed if the perpendicular axis has a filter set
// and no filter is set for the queried axis.
bool IsYAxisActionDisallowed(TouchAction action) {
  return ((action & TOUCH_ACTION_PAN_X) && !(action & TOUCH_ACTION_PAN_Y));
}

bool IsXAxisActionDisallowed(TouchAction action) {
  return ((action & TOUCH_ACTION_PAN_Y) && !(action & TOUCH_ACTION_PAN_X));
}

}  // namespace

TouchActionFilter::TouchActionFilter() :
  drop_scroll_gesture_events_(false),
  drop_pinch_gesture_events_(false),
  drop_current_tap_ending_event_(false),
  allow_current_double_tap_event_(true),
  allowed_touch_action_(TOUCH_ACTION_AUTO) {
}

bool TouchActionFilter::FilterGestureEvent(WebGestureEvent* gesture_event) {
  if (gesture_event->sourceDevice != blink::WebGestureDeviceTouchscreen)
    return false;

  // Filter for allowable touch actions first (eg. before the TouchEventQueue
  // can decide to send a touch cancel event).
  switch (gesture_event->type) {
    case WebInputEvent::GestureScrollBegin:
      DCHECK(!drop_scroll_gesture_events_);
      drop_scroll_gesture_events_ = ShouldSuppressScroll(*gesture_event);
      return drop_scroll_gesture_events_;

    case WebInputEvent::GestureScrollUpdate:
      if (drop_scroll_gesture_events_) {
        return true;
      } else {
        // Scrolls restricted to a specific axis shouldn't permit movement
        // in the perpendicular axis.
        if (IsYAxisActionDisallowed(allowed_touch_action_)) {
          gesture_event->data.scrollUpdate.deltaY = 0;
          gesture_event->data.scrollUpdate.velocityY = 0;
        } else if (IsXAxisActionDisallowed(allowed_touch_action_)) {
          gesture_event->data.scrollUpdate.deltaX = 0;
          gesture_event->data.scrollUpdate.velocityX = 0;
        }
      }
      break;

    case WebInputEvent::GestureFlingStart:
      // Touchscreen flings should always have non-zero velocity.
      DCHECK(gesture_event->data.flingStart.velocityX ||
             gesture_event->data.flingStart.velocityY);
      if (!drop_scroll_gesture_events_) {
        // Flings restricted to a specific axis shouldn't permit velocity
        // in the perpendicular axis.
        if (IsYAxisActionDisallowed(allowed_touch_action_))
          gesture_event->data.flingStart.velocityY = 0;
        else if (IsXAxisActionDisallowed(allowed_touch_action_))
          gesture_event->data.flingStart.velocityX = 0;
        // As the renderer expects a scroll-ending event, but does not expect a
        // zero-velocity fling, convert the now zero-velocity fling accordingly.
        if (!gesture_event->data.flingStart.velocityX &&
            !gesture_event->data.flingStart.velocityY) {
          gesture_event->type = WebInputEvent::GestureScrollEnd;
        }
      }
      return FilterScrollEndingGesture();

    case WebInputEvent::GestureScrollEnd:
      return FilterScrollEndingGesture();

    case WebInputEvent::GesturePinchBegin:
      DCHECK(!drop_pinch_gesture_events_);
      if (allowed_touch_action_ & TOUCH_ACTION_PINCH_ZOOM) {
        // Pinch events are always bracketed by scroll events, and the W3C
        // standard touch-action provides no way to disable scrolling without
        // also disabling pinching (validated by the IPC ENUM traits).
        DCHECK(allowed_touch_action_ == TOUCH_ACTION_AUTO ||
            allowed_touch_action_ == TOUCH_ACTION_MANIPULATION);
        DCHECK(!drop_scroll_gesture_events_);
      } else {
        drop_pinch_gesture_events_ = true;
      }
      return drop_pinch_gesture_events_;

    case WebInputEvent::GesturePinchUpdate:
      return drop_pinch_gesture_events_;

    case WebInputEvent::GesturePinchEnd:
      if (drop_pinch_gesture_events_) {
        drop_pinch_gesture_events_ = false;
        return true;
      }
      DCHECK(!drop_scroll_gesture_events_);
      break;

    // The double tap gesture is a tap ending event. If a double tap gesture is
    // filtered out, replace it with a tap event.
    case WebInputEvent::GestureDoubleTap:
      DCHECK_EQ(1, gesture_event->data.tap.tapCount);
      if (!allow_current_double_tap_event_)
        gesture_event->type = WebInputEvent::GestureTap;
      allow_current_double_tap_event_ = true;
      break;

    // If double tap is disabled, there's no reason for the tap delay.
    case WebInputEvent::GestureTapUnconfirmed:
      DCHECK_EQ(1, gesture_event->data.tap.tapCount);
      allow_current_double_tap_event_ =
          (allowed_touch_action_ & TOUCH_ACTION_DOUBLE_TAP_ZOOM) != 0;
      if (!allow_current_double_tap_event_) {
        gesture_event->type = WebInputEvent::GestureTap;
        drop_current_tap_ending_event_ = true;
      }
      break;

    case WebInputEvent::GestureTap:
      allow_current_double_tap_event_ =
          (allowed_touch_action_ & TOUCH_ACTION_DOUBLE_TAP_ZOOM) != 0;
      // Fall through.
    case WebInputEvent::GestureTapCancel:
      if (drop_current_tap_ending_event_) {
        drop_current_tap_ending_event_ = false;
        return true;
      }
      break;

    case WebInputEvent::GestureTapDown:
      DCHECK(!drop_current_tap_ending_event_);
      break;

    default:
      // Gesture events unrelated to touch actions (panning/zooming) are left
      // alone.
      break;
  }

  return false;
}

bool TouchActionFilter::FilterScrollEndingGesture() {
  DCHECK(!drop_pinch_gesture_events_);
  if (drop_scroll_gesture_events_) {
    drop_scroll_gesture_events_ = false;
    return true;
  }
  return false;
}

void TouchActionFilter::OnSetTouchAction(TouchAction touch_action) {
  // For multiple fingers, we take the intersection of the touch actions for
  // all fingers that have gone down during this action.  In the majority of
  // real-world scenarios the touch action for all fingers will be the same.
  // This is left as implementation-defined in the pointer events
  // specification because of the relationship to gestures (which are off
  // limits for the spec).  I believe the following are desirable properties
  // of this choice:
  // 1. Not sensitive to finger touch order.  Behavior of putting two fingers
  //    down "at once" will be deterministic.
  // 2. Only subtractive - eg. can't trigger scrolling on a element that
  //    otherwise has scrolling disabling by the addition of a finger.
  allowed_touch_action_ &= touch_action;
}

void TouchActionFilter::ResetTouchAction() {
  // Note that resetting the action mid-sequence is tolerated. Gestures that had
  // their begin event(s) suppressed will be suppressed until the next sequence.
  allowed_touch_action_ = TOUCH_ACTION_AUTO;
}

bool TouchActionFilter::ShouldSuppressScroll(
    const blink::WebGestureEvent& gesture_event) {
  DCHECK_EQ(gesture_event.type, WebInputEvent::GestureScrollBegin);
  if ((allowed_touch_action_ & TOUCH_ACTION_PAN) == TOUCH_ACTION_PAN)
    // All possible panning is enabled.
    return false;
  if (!(allowed_touch_action_ & TOUCH_ACTION_PAN))
    // No panning is enabled.
    return true;

  // If there's no hint or it's perfectly diagonal, then allow the scroll.
  if (fabs(gesture_event.data.scrollBegin.deltaXHint) ==
      fabs(gesture_event.data.scrollBegin.deltaYHint))
    return false;

  // Determine the primary initial axis of the scroll, and check whether
  // panning along that axis is permitted.
  if (fabs(gesture_event.data.scrollBegin.deltaXHint) >
      fabs(gesture_event.data.scrollBegin.deltaYHint)) {
    if (gesture_event.data.scrollBegin.deltaXHint > 0 &&
        allowed_touch_action_ & TOUCH_ACTION_PAN_LEFT) {
      return false;
    } else if (gesture_event.data.scrollBegin.deltaXHint < 0 &&
               allowed_touch_action_ & TOUCH_ACTION_PAN_RIGHT) {
      return false;
    } else {
      return true;
    }
  } else {
    if (gesture_event.data.scrollBegin.deltaYHint > 0 &&
        allowed_touch_action_ & TOUCH_ACTION_PAN_UP) {
      return false;
    } else if (gesture_event.data.scrollBegin.deltaYHint < 0 &&
               allowed_touch_action_ & TOUCH_ACTION_PAN_DOWN) {
      return false;
    } else {
      return true;
    }
  }
}

}  // namespace content
