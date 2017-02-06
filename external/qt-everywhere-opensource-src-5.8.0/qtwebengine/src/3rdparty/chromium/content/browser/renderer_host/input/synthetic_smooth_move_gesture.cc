// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_smooth_move_gesture.h"

#include <stdint.h>

#include "base/logging.h"
#include "ui/gfx/geometry/point_f.h"

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

gfx::Vector2dF ProjectScalarOntoVector(float scalar,
                                       const gfx::Vector2dF& vector) {
  return gfx::ScaleVector2d(vector, scalar / vector.Length());
}

double ConvertTimestampToSeconds(const base::TimeTicks& timestamp) {
  return (timestamp - base::TimeTicks()).InSecondsF();
}

const int kDefaultSpeedInPixelsPerSec = 800;

}  // namespace

SyntheticSmoothMoveGestureParams::SyntheticSmoothMoveGestureParams()
    : speed_in_pixels_s(kDefaultSpeedInPixelsPerSec),
      prevent_fling(true),
      add_slop(true) {}

SyntheticSmoothMoveGestureParams::SyntheticSmoothMoveGestureParams(
    const SyntheticSmoothMoveGestureParams& other) = default;

SyntheticSmoothMoveGestureParams::~SyntheticSmoothMoveGestureParams() {}

SyntheticSmoothMoveGesture::SyntheticSmoothMoveGesture(
    SyntheticSmoothMoveGestureParams params)
    : params_(params),
      current_move_segment_start_position_(params.start_point),
      state_(SETUP) {
}

SyntheticSmoothMoveGesture::~SyntheticSmoothMoveGesture() {}

SyntheticGesture::Result SyntheticSmoothMoveGesture::ForwardInputEvents(
    const base::TimeTicks& timestamp,
    SyntheticGestureTarget* target) {
  if (state_ == SETUP) {
    state_ = STARTED;
    current_move_segment_ = -1;
    current_move_segment_stop_time_ = timestamp;
  }

  switch (params_.input_type) {
    case SyntheticSmoothMoveGestureParams::TOUCH_INPUT:
      if (!synthetic_pointer_)
        synthetic_pointer_ =
            SyntheticPointer::Create(SyntheticGestureParams::TOUCH_INPUT);
      ForwardTouchInputEvents(timestamp, target);
      break;
    case SyntheticSmoothMoveGestureParams::MOUSE_DRAG_INPUT:
      if (!synthetic_pointer_)
        synthetic_pointer_ =
            SyntheticPointer::Create(SyntheticGestureParams::MOUSE_INPUT);
      ForwardMouseClickInputEvents(timestamp, target);
      break;
    case SyntheticSmoothMoveGestureParams::MOUSE_WHEEL_INPUT:
      ForwardMouseWheelInputEvents(timestamp, target);
      break;
    default:
      return SyntheticGesture::GESTURE_SOURCE_TYPE_NOT_IMPLEMENTED;
  }
  return (state_ == DONE) ? SyntheticGesture::GESTURE_FINISHED
                          : SyntheticGesture::GESTURE_RUNNING;
}

// TODO(ssid): Clean up the switch statements by adding functions instead of
// large code, in the Forward*Events functions. Move the actions for all input
// types to different class (SyntheticInputDevice) which generates input events
// for all input types. The gesture class can use instance of device actions.
// Refer: crbug.com/461825

void SyntheticSmoothMoveGesture::ForwardTouchInputEvents(
    const base::TimeTicks& timestamp,
    SyntheticGestureTarget* target) {
  base::TimeTicks event_timestamp = timestamp;
  switch (state_) {
    case STARTED:
      if (MoveIsNoOp()) {
        state_ = DONE;
        break;
      }
      if (params_.add_slop)
        AddTouchSlopToFirstDistance(target);
      ComputeNextMoveSegment();
      PressPoint(target, event_timestamp);
      state_ = MOVING;
      break;
    case MOVING: {
      event_timestamp = ClampTimestamp(timestamp);
      gfx::Vector2dF delta = GetPositionDeltaAtTime(event_timestamp);
      MovePoint(target, delta, event_timestamp);

      if (FinishedCurrentMoveSegment(event_timestamp)) {
        if (!IsLastMoveSegment()) {
          current_move_segment_start_position_ +=
              params_.distances[current_move_segment_];
          ComputeNextMoveSegment();
        } else if (params_.prevent_fling) {
          state_ = STOPPING;
        } else {
          ReleasePoint(target, event_timestamp);
          state_ = DONE;
        }
      }
    } break;
    case STOPPING:
      if (timestamp - current_move_segment_stop_time_ >=
          target->PointerAssumedStoppedTime()) {
        event_timestamp = current_move_segment_stop_time_ +
                          target->PointerAssumedStoppedTime();
        ReleasePoint(target, event_timestamp);
        state_ = DONE;
      }
      break;
    case SETUP:
      NOTREACHED()
          << "State SETUP invalid for synthetic scroll using touch input.";
    case DONE:
      NOTREACHED()
          << "State DONE invalid for synthetic scroll using touch input.";
  }
}

void SyntheticSmoothMoveGesture::ForwardMouseWheelInputEvents(
    const base::TimeTicks& timestamp,
    SyntheticGestureTarget* target) {
  switch (state_) {
    case STARTED:
      if (MoveIsNoOp()) {
        state_ = DONE;
        break;
      }
      ComputeNextMoveSegment();
      state_ = MOVING;
      // Fall through to forward the first event.
    case MOVING: {
      // Even though WebMouseWheelEvents take floating point deltas,
      // internally the scroll position is stored as an integer. We therefore
      // keep track of the discrete delta which is consistent with the
      // internal scrolling state. This ensures that when the gesture has
      // finished we've scrolled exactly the specified distance.
      base::TimeTicks event_timestamp = ClampTimestamp(timestamp);
      gfx::Vector2dF current_move_segment_total_delta =
          GetPositionDeltaAtTime(event_timestamp);
      gfx::Vector2d delta_discrete =
          FloorTowardZero(current_move_segment_total_delta -
                          current_move_segment_total_delta_discrete_);
      ForwardMouseWheelEvent(target, delta_discrete, event_timestamp);
      current_move_segment_total_delta_discrete_ += delta_discrete;

      if (FinishedCurrentMoveSegment(event_timestamp)) {
        if (!IsLastMoveSegment()) {
          current_move_segment_total_delta_discrete_ = gfx::Vector2d();
          ComputeNextMoveSegment();
          ForwardMouseWheelInputEvents(timestamp, target);
        } else {
          state_ = DONE;
        }
      }
    } break;
    case SETUP:
      NOTREACHED() << "State SETUP invalid for synthetic scroll using mouse "
                      "wheel input.";
    case STOPPING:
      NOTREACHED() << "State STOPPING invalid for synthetic scroll using mouse "
                      "wheel input.";
    case DONE:
      NOTREACHED()
          << "State DONE invalid for synthetic scroll using mouse wheel input.";
  }
}

void SyntheticSmoothMoveGesture::ForwardMouseClickInputEvents(
    const base::TimeTicks& timestamp,
    SyntheticGestureTarget* target) {
  base::TimeTicks event_timestamp = timestamp;
  switch (state_) {
    case STARTED:
      if (MoveIsNoOp()) {
        state_ = DONE;
        break;
      }
      ComputeNextMoveSegment();
      PressPoint(target, event_timestamp);
      state_ = MOVING;
      break;
    case MOVING: {
      base::TimeTicks event_timestamp = ClampTimestamp(timestamp);
      gfx::Vector2dF delta = GetPositionDeltaAtTime(event_timestamp);
      MovePoint(target, delta, event_timestamp);

      if (FinishedCurrentMoveSegment(event_timestamp)) {
        if (!IsLastMoveSegment()) {
          current_move_segment_start_position_ +=
              params_.distances[current_move_segment_];
          ComputeNextMoveSegment();
        } else {
          ReleasePoint(target, event_timestamp);
          state_ = DONE;
        }
      }
    } break;
    case STOPPING:
      NOTREACHED()
          << "State STOPPING invalid for synthetic drag using mouse input.";
    case SETUP:
      NOTREACHED()
          << "State SETUP invalid for synthetic drag using mouse input.";
    case DONE:
      NOTREACHED()
          << "State DONE invalid for synthetic drag using mouse input.";
  }
}

void SyntheticSmoothMoveGesture::ForwardMouseWheelEvent(
    SyntheticGestureTarget* target,
    const gfx::Vector2dF& delta,
    const base::TimeTicks& timestamp) const {
  blink::WebMouseWheelEvent mouse_wheel_event =
      SyntheticWebMouseWheelEventBuilder::Build(0, 0, delta.x(), delta.y(), 0,
                                                false);

  mouse_wheel_event.x = current_move_segment_start_position_.x();
  mouse_wheel_event.y = current_move_segment_start_position_.y();

  mouse_wheel_event.timeStampSeconds = ConvertTimestampToSeconds(timestamp);

  target->DispatchInputEventToPlatform(mouse_wheel_event);
}

void SyntheticSmoothMoveGesture::PressPoint(SyntheticGestureTarget* target,
                                            const base::TimeTicks& timestamp) {
  DCHECK_EQ(current_move_segment_, 0);
  synthetic_pointer_->Press(current_move_segment_start_position_.x(),
                            current_move_segment_start_position_.y(), target,
                            timestamp);
  synthetic_pointer_->DispatchEvent(target, timestamp);
}

void SyntheticSmoothMoveGesture::MovePoint(SyntheticGestureTarget* target,
                                           const gfx::Vector2dF& delta,
                                           const base::TimeTicks& timestamp) {
  DCHECK_GE(current_move_segment_, 0);
  DCHECK_LT(current_move_segment_, static_cast<int>(params_.distances.size()));
  gfx::PointF new_position = current_move_segment_start_position_ + delta;
  synthetic_pointer_->Move(0, new_position.x(), new_position.y(), target,
                           timestamp);
  synthetic_pointer_->DispatchEvent(target, timestamp);
}

void SyntheticSmoothMoveGesture::ReleasePoint(
    SyntheticGestureTarget* target,
    const base::TimeTicks& timestamp) {
  DCHECK_EQ(current_move_segment_,
            static_cast<int>(params_.distances.size()) - 1);
  gfx::PointF position;
  if (params_.input_type ==
      SyntheticSmoothMoveGestureParams::MOUSE_DRAG_INPUT) {
    position = current_move_segment_start_position_ +
               GetPositionDeltaAtTime(timestamp);
  }
  synthetic_pointer_->Release(0, target, timestamp);
  synthetic_pointer_->DispatchEvent(target, timestamp);
}

void SyntheticSmoothMoveGesture::AddTouchSlopToFirstDistance(
    SyntheticGestureTarget* target) {
  DCHECK_GE(params_.distances.size(), 1ul);
  gfx::Vector2dF& first_move_distance = params_.distances[0];
  DCHECK_GT(first_move_distance.Length(), 0);
  first_move_distance += CeilFromZero(ProjectScalarOntoVector(
      target->GetTouchSlopInDips(), first_move_distance));
}

gfx::Vector2dF SyntheticSmoothMoveGesture::GetPositionDeltaAtTime(
    const base::TimeTicks& timestamp) const {
  // Make sure the final delta is correct. Using the computation below can lead
  // to issues with floating point precision.
  if (FinishedCurrentMoveSegment(timestamp))
    return params_.distances[current_move_segment_];

  float delta_length =
      params_.speed_in_pixels_s *
      (timestamp - current_move_segment_start_time_).InSecondsF();
  return ProjectScalarOntoVector(delta_length,
                                 params_.distances[current_move_segment_]);
}

void SyntheticSmoothMoveGesture::ComputeNextMoveSegment() {
  current_move_segment_++;
  DCHECK_LT(current_move_segment_, static_cast<int>(params_.distances.size()));
  int64_t total_duration_in_us = static_cast<int64_t>(
      1e6 * (params_.distances[current_move_segment_].Length() /
             params_.speed_in_pixels_s));
  DCHECK_GT(total_duration_in_us, 0);
  current_move_segment_start_time_ = current_move_segment_stop_time_;
  current_move_segment_stop_time_ =
      current_move_segment_start_time_ +
      base::TimeDelta::FromMicroseconds(total_duration_in_us);
}

base::TimeTicks SyntheticSmoothMoveGesture::ClampTimestamp(
    const base::TimeTicks& timestamp) const {
  return std::min(timestamp, current_move_segment_stop_time_);
}

bool SyntheticSmoothMoveGesture::FinishedCurrentMoveSegment(
    const base::TimeTicks& timestamp) const {
  return timestamp >= current_move_segment_stop_time_;
}

bool SyntheticSmoothMoveGesture::IsLastMoveSegment() const {
  DCHECK_LT(current_move_segment_, static_cast<int>(params_.distances.size()));
  return current_move_segment_ ==
         static_cast<int>(params_.distances.size()) - 1;
}

bool SyntheticSmoothMoveGesture::MoveIsNoOp() const {
  return params_.distances.size() == 0 || params_.distances[0].IsZero();
}

}  // namespace content
