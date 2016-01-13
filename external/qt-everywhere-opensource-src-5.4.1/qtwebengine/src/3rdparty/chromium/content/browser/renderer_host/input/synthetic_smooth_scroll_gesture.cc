// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_smooth_scroll_gesture.h"

#include "base/logging.h"
#include "ui/gfx/point_f.h"

namespace content {
namespace {

gfx::Vector2d FloorTowardZero(const gfx::Vector2dF& vector) {
  int x = vector.x() > 0 ? floor(vector.x()) : ceil(vector.x());
  int y = vector.y() > 0 ? floor(vector.y()) : ceil(vector.y());
  return gfx::Vector2d(x, y);
}

gfx::Vector2d CeilFromZero(const gfx::Vector2dF& vector) {
  int x = vector.x() > 0 ? ceil(vector.x()) : floor(vector.x());
  int y = vector.y() > 0 ? ceil(vector.y()) : floor(vector.y());
  return gfx::Vector2d(x, y);
}

gfx::Vector2dF ProjectScalarOntoVector(
    float scalar, const gfx::Vector2d& vector) {
  return gfx::ScaleVector2d(vector, scalar / vector.Length());
}

}  // namespace

SyntheticSmoothScrollGesture::SyntheticSmoothScrollGesture(
    const SyntheticSmoothScrollGestureParams& params)
    : params_(params),
      gesture_source_type_(SyntheticGestureParams::DEFAULT_INPUT),
      state_(SETUP) {}

SyntheticSmoothScrollGesture::~SyntheticSmoothScrollGesture() {}

SyntheticGesture::Result SyntheticSmoothScrollGesture::ForwardInputEvents(
    const base::TimeTicks& timestamp, SyntheticGestureTarget* target) {
  if (state_ == SETUP) {
    gesture_source_type_ = params_.gesture_source_type;
    if (gesture_source_type_ == SyntheticGestureParams::DEFAULT_INPUT)
      gesture_source_type_ = target->GetDefaultSyntheticGestureSourceType();

    state_ = STARTED;
    current_scroll_segment_ = -1;
    current_scroll_segment_stop_time_ = timestamp;
  }

  DCHECK_NE(gesture_source_type_, SyntheticGestureParams::DEFAULT_INPUT);
  if (gesture_source_type_ == SyntheticGestureParams::TOUCH_INPUT)
    ForwardTouchInputEvents(timestamp, target);
  else if (gesture_source_type_ == SyntheticGestureParams::MOUSE_INPUT)
    ForwardMouseInputEvents(timestamp, target);
  else
    return SyntheticGesture::GESTURE_SOURCE_TYPE_NOT_IMPLEMENTED;

  return (state_ == DONE) ? SyntheticGesture::GESTURE_FINISHED
                          : SyntheticGesture::GESTURE_RUNNING;
}

void SyntheticSmoothScrollGesture::ForwardTouchInputEvents(
    const base::TimeTicks& timestamp, SyntheticGestureTarget* target) {
  base::TimeTicks event_timestamp = timestamp;
  switch (state_) {
    case STARTED:
      if (ScrollIsNoOp()) {
        state_ = DONE;
        break;
      }
      AddTouchSlopToFirstDistance(target);
      ComputeNextScrollSegment();
      current_scroll_segment_start_position_ = params_.anchor;
      PressTouchPoint(target, event_timestamp);
      state_ = MOVING;
      break;
    case MOVING: {
      event_timestamp = ClampTimestamp(timestamp);
      gfx::Vector2dF delta = GetPositionDeltaAtTime(event_timestamp);
      MoveTouchPoint(target, delta, event_timestamp);

      if (FinishedCurrentScrollSegment(event_timestamp)) {
        if (!IsLastScrollSegment()) {
          current_scroll_segment_start_position_ +=
              params_.distances[current_scroll_segment_];
          ComputeNextScrollSegment();
        } else if (params_.prevent_fling) {
          state_ = STOPPING;
        } else {
          ReleaseTouchPoint(target, event_timestamp);
          state_ = DONE;
        }
      }
    } break;
    case STOPPING:
      if (timestamp - current_scroll_segment_stop_time_ >=
          target->PointerAssumedStoppedTime()) {
        event_timestamp = current_scroll_segment_stop_time_ +
                          target->PointerAssumedStoppedTime();
        // Send one last move event, but don't change the location. Without this
        // we'd still sometimes cause a fling on Android.

        // Required to suppress flings on Aura, see
        // |UpdateWebTouchPointFromUIEvent|, remove when crbug.com/332418
        // is fixed.
        touch_event_.touches[0].position.y += 0.001f;

        ForwardTouchEvent(target, event_timestamp);
        ReleaseTouchPoint(target, event_timestamp);
        state_ = DONE;
      }
      break;
    case SETUP:
      NOTREACHED()
          << "State STARTED invalid for synthetic scroll using touch input.";
    case DONE:
      NOTREACHED()
          << "State DONE invalid for synthetic scroll using touch input.";
  }
}

void SyntheticSmoothScrollGesture::ForwardMouseInputEvents(
    const base::TimeTicks& timestamp, SyntheticGestureTarget* target) {
  switch (state_) {
    case STARTED:
      if (ScrollIsNoOp()) {
        state_ = DONE;
        break;
      }
      ComputeNextScrollSegment();
      state_ = MOVING;
      // Fall through to forward the first event.
    case MOVING: {
      // Even though WebMouseWheelEvents take floating point deltas,
      // internally the scroll position is stored as an integer. We therefore
      // keep track of the discrete delta which is consistent with the
      // internal scrolling state. This ensures that when the gesture has
      // finished we've scrolled exactly the specified distance.
      base::TimeTicks event_timestamp = ClampTimestamp(timestamp);
      gfx::Vector2dF current_scroll_segment_total_delta =
          GetPositionDeltaAtTime(event_timestamp);
      gfx::Vector2d delta_discrete =
          FloorTowardZero(current_scroll_segment_total_delta -
                          current_scroll_segment_total_delta_discrete_);
      ForwardMouseWheelEvent(target, delta_discrete, event_timestamp);
      current_scroll_segment_total_delta_discrete_ += delta_discrete;

      if (FinishedCurrentScrollSegment(event_timestamp)) {
        if (!IsLastScrollSegment()) {
          current_scroll_segment_total_delta_discrete_ = gfx::Vector2d();
          ComputeNextScrollSegment();
          ForwardMouseInputEvents(timestamp, target);
        } else {
          state_ = DONE;
        }
      }
    } break;
    case SETUP:
      NOTREACHED()
          << "State STARTED invalid for synthetic scroll using touch input.";
    case STOPPING:
      NOTREACHED()
          << "State STOPPING invalid for synthetic scroll using touch input.";
    case DONE:
      NOTREACHED()
          << "State DONE invalid for synthetic scroll using touch input.";
  }
}

void SyntheticSmoothScrollGesture::ForwardTouchEvent(
    SyntheticGestureTarget* target, const base::TimeTicks& timestamp) {
  touch_event_.timeStampSeconds = ConvertTimestampToSeconds(timestamp);

  target->DispatchInputEventToPlatform(touch_event_);
}

void SyntheticSmoothScrollGesture::ForwardMouseWheelEvent(
    SyntheticGestureTarget* target,
    const gfx::Vector2dF& delta,
    const base::TimeTicks& timestamp) const {
  blink::WebMouseWheelEvent mouse_wheel_event =
      SyntheticWebMouseWheelEventBuilder::Build(delta.x(), delta.y(), 0, false);

  mouse_wheel_event.x = params_.anchor.x();
  mouse_wheel_event.y = params_.anchor.y();

  mouse_wheel_event.timeStampSeconds = ConvertTimestampToSeconds(timestamp);

  target->DispatchInputEventToPlatform(mouse_wheel_event);
}

void SyntheticSmoothScrollGesture::PressTouchPoint(
    SyntheticGestureTarget* target, const base::TimeTicks& timestamp) {
  DCHECK_EQ(current_scroll_segment_, 0);
  touch_event_.PressPoint(params_.anchor.x(), params_.anchor.y());
  ForwardTouchEvent(target, timestamp);
}

void SyntheticSmoothScrollGesture::MoveTouchPoint(
    SyntheticGestureTarget* target,
    const gfx::Vector2dF& delta,
    const base::TimeTicks& timestamp) {
  DCHECK_GE(current_scroll_segment_, 0);
  DCHECK_LT(current_scroll_segment_,
            static_cast<int>(params_.distances.size()));
  gfx::PointF touch_position = current_scroll_segment_start_position_ + delta;
  touch_event_.MovePoint(0, touch_position.x(), touch_position.y());
  ForwardTouchEvent(target, timestamp);
}

void SyntheticSmoothScrollGesture::ReleaseTouchPoint(
    SyntheticGestureTarget* target, const base::TimeTicks& timestamp) {
  DCHECK_EQ(current_scroll_segment_,
            static_cast<int>(params_.distances.size()) - 1);
  touch_event_.ReleasePoint(0);
  ForwardTouchEvent(target, timestamp);
}

void SyntheticSmoothScrollGesture::AddTouchSlopToFirstDistance(
    SyntheticGestureTarget* target) {
  DCHECK_GE(params_.distances.size(), 1ul);
  gfx::Vector2d& first_scroll_distance = params_.distances[0];
  DCHECK_GT(first_scroll_distance.Length(), 0);
  first_scroll_distance += CeilFromZero(ProjectScalarOntoVector(
      target->GetTouchSlopInDips(), first_scroll_distance));
}

gfx::Vector2dF SyntheticSmoothScrollGesture::GetPositionDeltaAtTime(
    const base::TimeTicks& timestamp) const {
  // Make sure the final delta is correct. Using the computation below can lead
  // to issues with floating point precision.
  if (FinishedCurrentScrollSegment(timestamp))
    return params_.distances[current_scroll_segment_];

  float delta_length =
      params_.speed_in_pixels_s *
      (timestamp - current_scroll_segment_start_time_).InSecondsF();
  return ProjectScalarOntoVector(delta_length,
                                 params_.distances[current_scroll_segment_]);
}

void SyntheticSmoothScrollGesture::ComputeNextScrollSegment() {
  current_scroll_segment_++;
  DCHECK_LT(current_scroll_segment_,
            static_cast<int>(params_.distances.size()));
  int64 total_duration_in_us = static_cast<int64>(
      1e6 * (params_.distances[current_scroll_segment_].Length() /
             params_.speed_in_pixels_s));
  DCHECK_GT(total_duration_in_us, 0);
  current_scroll_segment_start_time_ = current_scroll_segment_stop_time_;
  current_scroll_segment_stop_time_ =
      current_scroll_segment_start_time_ +
      base::TimeDelta::FromMicroseconds(total_duration_in_us);
}

base::TimeTicks SyntheticSmoothScrollGesture::ClampTimestamp(
    const base::TimeTicks& timestamp) const {
  return std::min(timestamp, current_scroll_segment_stop_time_);
}

bool SyntheticSmoothScrollGesture::FinishedCurrentScrollSegment(
    const base::TimeTicks& timestamp) const {
  return timestamp >= current_scroll_segment_stop_time_;
}

bool SyntheticSmoothScrollGesture::IsLastScrollSegment() const {
  DCHECK_LT(current_scroll_segment_,
            static_cast<int>(params_.distances.size()));
  return current_scroll_segment_ ==
         static_cast<int>(params_.distances.size()) - 1;
}

bool SyntheticSmoothScrollGesture::ScrollIsNoOp() const {
  return params_.distances.size() == 0 || params_.distances[0].IsZero();
}

}  // namespace content
