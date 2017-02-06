// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/overscroll_controller.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "content/browser/renderer_host/overscroll_controller_delegate.h"
#include "content/public/browser/overscroll_configuration.h"
#include "content/public/common/content_switches.h"

using blink::WebInputEvent;

namespace {

bool IsScrollEndEffectEnabled() {
  return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kScrollEndEffect) == "1";
}

bool IsGestureEventFromTouchpad(const blink::WebInputEvent& event) {
  DCHECK(blink::WebInputEvent::isGestureEventType(event.type));
  const blink::WebGestureEvent& gesture =
      static_cast<const blink::WebGestureEvent&>(event);
  return gesture.sourceDevice == blink::WebGestureDeviceTouchpad;
}

}  // namespace

namespace content {

OverscrollController::OverscrollController()
    : overscroll_mode_(OVERSCROLL_NONE),
      scroll_state_(STATE_UNKNOWN),
      overscroll_delta_x_(0.f),
      overscroll_delta_y_(0.f),
      delegate_(NULL) {}

OverscrollController::~OverscrollController() {
}

bool OverscrollController::ShouldProcessEvent(
    const blink::WebInputEvent& event) {
    switch (event.type) {
      case blink::WebInputEvent::MouseWheel:
        return false;
      case blink::WebInputEvent::GestureScrollBegin:
      case blink::WebInputEvent::GestureScrollUpdate:
      case blink::WebInputEvent::GestureScrollEnd: {
        const blink::WebGestureEvent& gesture =
            static_cast<const blink::WebGestureEvent&>(event);
        if (gesture.sourceDevice == blink::WebGestureDeviceTouchpad)
          return true;
        blink::WebGestureEvent::ScrollUnits scrollUnits;
        switch (event.type) {
          case blink::WebInputEvent::GestureScrollBegin:
            scrollUnits = gesture.data.scrollBegin.deltaHintUnits;
            break;
          case blink::WebInputEvent::GestureScrollUpdate:
            scrollUnits = gesture.data.scrollUpdate.deltaUnits;
            break;
          case blink::WebInputEvent::GestureScrollEnd:
            scrollUnits = gesture.data.scrollEnd.deltaUnits;
            break;
          default:
            scrollUnits = blink::WebGestureEvent::Pixels;
            break;
        }

        return scrollUnits == blink::WebGestureEvent::PrecisePixels;
      }
      default:
        break;
    }
  return true;
}

bool OverscrollController::WillHandleEvent(const blink::WebInputEvent& event) {
  if (!ShouldProcessEvent(event))
    return false;

  bool reset_scroll_state = false;
  if (scroll_state_ != STATE_UNKNOWN ||
      overscroll_delta_x_ || overscroll_delta_y_) {
    switch (event.type) {
      case blink::WebInputEvent::GestureScrollEnd:
        // Avoid resetting the state on GestureScrollEnd generated
        // from the touchpad since it is sent based on a timeout.
        reset_scroll_state = !IsGestureEventFromTouchpad(event);
        break;

      case blink::WebInputEvent::GestureFlingStart:
        reset_scroll_state = true;
        break;

      case blink::WebInputEvent::MouseWheel: {
        const blink::WebMouseWheelEvent& wheel =
            static_cast<const blink::WebMouseWheelEvent&>(event);
        if (!wheel.hasPreciseScrollingDeltas ||
            wheel.phase == blink::WebMouseWheelEvent::PhaseEnded ||
            wheel.phase == blink::WebMouseWheelEvent::PhaseCancelled) {
          reset_scroll_state = true;
        }
        break;
      }

      default:
        if (blink::WebInputEvent::isMouseEventType(event.type) ||
            blink::WebInputEvent::isKeyboardEventType(event.type)) {
          reset_scroll_state = true;
        }
        break;
    }
  }

  if (reset_scroll_state)
    scroll_state_ = STATE_UNKNOWN;

  if (DispatchEventCompletesAction(event)) {
    CompleteAction();

    // Let the event be dispatched to the renderer.
    return false;
  }

  if (overscroll_mode_ != OVERSCROLL_NONE && DispatchEventResetsState(event)) {
    SetOverscrollMode(OVERSCROLL_NONE);

    // Let the event be dispatched to the renderer.
    return false;
  }

  if (overscroll_mode_ != OVERSCROLL_NONE) {
    // Consume the event only if it updates the overscroll state.
    if (ProcessEventForOverscroll(event))
      return true;
  } else if (reset_scroll_state) {
    overscroll_delta_x_ = overscroll_delta_y_ = 0.f;
  }


  return false;
}

void OverscrollController::ReceivedEventACK(const blink::WebInputEvent& event,
                                            bool processed) {
  if (!ShouldProcessEvent(event))
    return;

  if (processed) {
    // If a scroll event is consumed by the page, i.e. some content on the page
    // has been scrolled, then there is not going to be an overscroll gesture,
    // until the current scroll ends, and a new scroll gesture starts.
    if (scroll_state_ == STATE_UNKNOWN &&
        (event.type == blink::WebInputEvent::MouseWheel ||
         event.type == blink::WebInputEvent::GestureScrollUpdate)) {
      scroll_state_ = STATE_CONTENT_SCROLLING;
    }
    return;
  }
  ProcessEventForOverscroll(event);
}

void OverscrollController::DiscardingGestureEvent(
    const blink::WebGestureEvent& gesture) {
  if (scroll_state_ != STATE_UNKNOWN &&
      (gesture.type == blink::WebInputEvent::GestureScrollEnd ||
       gesture.type == blink::WebInputEvent::GestureFlingStart)) {
    scroll_state_ = STATE_UNKNOWN;
  }
}

void OverscrollController::Reset() {
  overscroll_mode_ = OVERSCROLL_NONE;
  overscroll_delta_x_ = overscroll_delta_y_ = 0.f;
  scroll_state_ = STATE_UNKNOWN;
}

void OverscrollController::Cancel() {
  SetOverscrollMode(OVERSCROLL_NONE);
  overscroll_delta_x_ = overscroll_delta_y_ = 0.f;
  scroll_state_ = STATE_UNKNOWN;
}

bool OverscrollController::DispatchEventCompletesAction (
    const blink::WebInputEvent& event) const {
  if (overscroll_mode_ == OVERSCROLL_NONE)
    return false;

  // Complete the overscroll gesture if there was a mouse move or a scroll-end
  // after the threshold.
  if (event.type != blink::WebInputEvent::MouseMove &&
      event.type != blink::WebInputEvent::GestureScrollEnd &&
      event.type != blink::WebInputEvent::GestureFlingStart)
    return false;

  // Avoid completing the action on GestureScrollEnd generated
  // from the touchpad since it is sent based on a timeout not
  // when the user has stopped interacting.
  if (event.type == blink::WebInputEvent::GestureScrollEnd &&
      IsGestureEventFromTouchpad(event))
    return false;

  if (!delegate_)
    return false;

  gfx::Rect bounds = delegate_->GetVisibleBounds();
  if (bounds.IsEmpty())
    return false;

  if (event.type == blink::WebInputEvent::GestureFlingStart) {
    // Check to see if the fling is in the same direction of the overscroll.
    const blink::WebGestureEvent gesture =
        static_cast<const blink::WebGestureEvent&>(event);
    switch (overscroll_mode_) {
      case OVERSCROLL_EAST:
        if (gesture.data.flingStart.velocityX < 0)
          return false;
        break;
      case OVERSCROLL_WEST:
        if (gesture.data.flingStart.velocityX > 0)
          return false;
        break;
      case OVERSCROLL_NORTH:
        if (gesture.data.flingStart.velocityY > 0)
          return false;
        break;
      case OVERSCROLL_SOUTH:
        if (gesture.data.flingStart.velocityY < 0)
          return false;
        break;
      case OVERSCROLL_NONE:
        NOTREACHED();
    }
  }

  float ratio, threshold;
  if (overscroll_mode_ == OVERSCROLL_WEST ||
      overscroll_mode_ == OVERSCROLL_EAST) {
    ratio = fabs(overscroll_delta_x_) / bounds.width();
    threshold = GetOverscrollConfig(OVERSCROLL_CONFIG_HORIZ_THRESHOLD_COMPLETE);
  } else {
    ratio = fabs(overscroll_delta_y_) / bounds.height();
    threshold = GetOverscrollConfig(OVERSCROLL_CONFIG_VERT_THRESHOLD_COMPLETE);
  }

  return ratio >= threshold;
}

bool OverscrollController::DispatchEventResetsState(
    const blink::WebInputEvent& event) const {
  switch (event.type) {
    case blink::WebInputEvent::MouseWheel: {
      // Only wheel events with precise deltas (i.e. from trackpad) contribute
      // to the overscroll gesture.
      const blink::WebMouseWheelEvent& wheel =
          static_cast<const blink::WebMouseWheelEvent&>(event);
      return !wheel.hasPreciseScrollingDeltas;
    }

    // Avoid resetting overscroll on GestureScrollBegin/End generated
    // from the touchpad since it is sent based on a timeout.
    case blink::WebInputEvent::GestureScrollBegin:
    case blink::WebInputEvent::GestureScrollEnd:
      return !IsGestureEventFromTouchpad(event);

    case blink::WebInputEvent::GestureScrollUpdate:
    case blink::WebInputEvent::GestureFlingCancel:
      return false;

    default:
      // Touch events can arrive during an overscroll gesture initiated by
      // touch-scrolling. These events should not reset the overscroll state.
      return !blink::WebInputEvent::isTouchEventType(event.type);
  }
}

bool OverscrollController::ProcessEventForOverscroll(
    const blink::WebInputEvent& event) {
  bool event_processed = false;
  switch (event.type) {
    case blink::WebInputEvent::MouseWheel: {
      const blink::WebMouseWheelEvent& wheel =
          static_cast<const blink::WebMouseWheelEvent&>(event);
      if (!wheel.hasPreciseScrollingDeltas)
        break;
      event_processed =
          ProcessOverscroll(wheel.deltaX * wheel.accelerationRatioX,
                            wheel.deltaY * wheel.accelerationRatioY, true);
      break;
    }
    case blink::WebInputEvent::GestureScrollUpdate: {
      const blink::WebGestureEvent& gesture =
          static_cast<const blink::WebGestureEvent&>(event);
      event_processed = ProcessOverscroll(
          gesture.data.scrollUpdate.deltaX, gesture.data.scrollUpdate.deltaY,
          gesture.sourceDevice == blink::WebGestureDeviceTouchpad);
      break;
    }
    case blink::WebInputEvent::GestureFlingStart: {
      const float kFlingVelocityThreshold = 1100.f;
      const blink::WebGestureEvent& gesture =
          static_cast<const blink::WebGestureEvent&>(event);
      float velocity_x = gesture.data.flingStart.velocityX;
      float velocity_y = gesture.data.flingStart.velocityY;
      if (fabs(velocity_x) > kFlingVelocityThreshold) {
        if ((overscroll_mode_ == OVERSCROLL_WEST && velocity_x < 0) ||
            (overscroll_mode_ == OVERSCROLL_EAST && velocity_x > 0)) {
          CompleteAction();
          event_processed = true;
          break;
        }
      } else if (fabs(velocity_y) > kFlingVelocityThreshold) {
        if ((overscroll_mode_ == OVERSCROLL_NORTH && velocity_y < 0) ||
            (overscroll_mode_ == OVERSCROLL_SOUTH && velocity_y > 0)) {
          CompleteAction();
          event_processed = true;
          break;
        }
      }

      // Reset overscroll state if fling didn't complete the overscroll gesture.
      SetOverscrollMode(OVERSCROLL_NONE);
      break;
    }

    default:
      DCHECK(blink::WebInputEvent::isGestureEventType(event.type) ||
             blink::WebInputEvent::isTouchEventType(event.type))
          << "Received unexpected event: " << event.type;
  }
  return event_processed;
}

bool OverscrollController::ProcessOverscroll(float delta_x,
                                             float delta_y,
                                             bool is_touchpad) {
  if (scroll_state_ != STATE_CONTENT_SCROLLING)
    overscroll_delta_x_ += delta_x;
  overscroll_delta_y_ += delta_y;

  float horiz_threshold = GetOverscrollConfig(
      is_touchpad ? OVERSCROLL_CONFIG_HORIZ_THRESHOLD_START_TOUCHPAD
                  : OVERSCROLL_CONFIG_HORIZ_THRESHOLD_START_TOUCHSCREEN);
  float vert_threshold = GetOverscrollConfig(
      OVERSCROLL_CONFIG_VERT_THRESHOLD_START);
  if (fabs(overscroll_delta_x_) <= horiz_threshold &&
      fabs(overscroll_delta_y_) <= vert_threshold) {
    SetOverscrollMode(OVERSCROLL_NONE);
    return true;
  }

  // Compute the current overscroll direction. If the direction is different
  // from the current direction, then always switch to no-overscroll mode first
  // to make sure that subsequent scroll events go through to the page first.
  OverscrollMode new_mode = OVERSCROLL_NONE;
  const float kMinRatio = 2.5;
  if (fabs(overscroll_delta_x_) > horiz_threshold &&
      fabs(overscroll_delta_x_) > fabs(overscroll_delta_y_) * kMinRatio)
    new_mode = overscroll_delta_x_ > 0.f ? OVERSCROLL_EAST : OVERSCROLL_WEST;
  else if (fabs(overscroll_delta_y_) > vert_threshold &&
           fabs(overscroll_delta_y_) > fabs(overscroll_delta_x_) * kMinRatio)
    new_mode = overscroll_delta_y_ > 0.f ? OVERSCROLL_SOUTH : OVERSCROLL_NORTH;

  // The vertical oversrcoll currently does not have any UX effects other then
  // for the scroll end effect, so testing if it is enabled.
  if ((new_mode == OVERSCROLL_SOUTH || new_mode == OVERSCROLL_NORTH) &&
      !IsScrollEndEffectEnabled())
    new_mode = OVERSCROLL_NONE;

  if (overscroll_mode_ == OVERSCROLL_NONE)
    SetOverscrollMode(new_mode);
  else if (new_mode != overscroll_mode_)
    SetOverscrollMode(OVERSCROLL_NONE);

  if (overscroll_mode_ == OVERSCROLL_NONE)
    return false;

  // Tell the delegate about the overscroll update so that it can update
  // the display accordingly (e.g. show history preview etc.).
  if (delegate_) {
    // Do not include the threshold amount when sending the deltas to the
    // delegate.
    float delegate_delta_x = overscroll_delta_x_;
    if (fabs(delegate_delta_x) > horiz_threshold) {
      if (delegate_delta_x < 0)
        delegate_delta_x += horiz_threshold;
      else
        delegate_delta_x -= horiz_threshold;
    } else {
      delegate_delta_x = 0.f;
    }

    float delegate_delta_y = overscroll_delta_y_;
    if (fabs(delegate_delta_y) > vert_threshold) {
      if (delegate_delta_y < 0)
        delegate_delta_y += vert_threshold;
      else
        delegate_delta_y -= vert_threshold;
    } else {
      delegate_delta_y = 0.f;
    }
    return delegate_->OnOverscrollUpdate(delegate_delta_x, delegate_delta_y);
  }
  return false;
}

void OverscrollController::CompleteAction() {
  if (delegate_)
    delegate_->OnOverscrollComplete(overscroll_mode_);
  overscroll_mode_ = OVERSCROLL_NONE;
  overscroll_delta_x_ = overscroll_delta_y_ = 0.f;
}

void OverscrollController::SetOverscrollMode(OverscrollMode mode) {
  if (overscroll_mode_ == mode)
    return;
  OverscrollMode old_mode = overscroll_mode_;
  overscroll_mode_ = mode;
  if (overscroll_mode_ == OVERSCROLL_NONE)
    overscroll_delta_x_ = overscroll_delta_y_ = 0.f;
  else
    scroll_state_ = STATE_OVERSCROLLING;
  if (delegate_)
    delegate_->OnOverscrollModeChange(old_mode, overscroll_mode_);
}

}  // namespace content
