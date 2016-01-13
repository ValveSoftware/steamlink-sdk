// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/top_controls_manager.h"

#include <algorithm>

#include "base/logging.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "cc/animation/timing_function.h"
#include "cc/input/top_controls_manager_client.h"
#include "cc/output/begin_frame_args.h"
#include "cc/trees/layer_tree_impl.h"
#include "ui/gfx/frame_time.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/vector2d_f.h"

namespace cc {
namespace {
// These constants were chosen empirically for their visually pleasant behavior.
// Contact tedchoc@chromium.org for questions about changing these values.
const int64 kShowHideMaxDurationMs = 200;
}

// static
scoped_ptr<TopControlsManager> TopControlsManager::Create(
    TopControlsManagerClient* client,
    float top_controls_height,
    float top_controls_show_threshold,
    float top_controls_hide_threshold) {
  return make_scoped_ptr(new TopControlsManager(client,
                                                top_controls_height,
                                                top_controls_show_threshold,
                                                top_controls_hide_threshold));
}

TopControlsManager::TopControlsManager(TopControlsManagerClient* client,
                                       float top_controls_height,
                                       float top_controls_show_threshold,
                                       float top_controls_hide_threshold)
    : client_(client),
      animation_direction_(NO_ANIMATION),
      permitted_state_(BOTH),
      controls_top_offset_(0.f),
      top_controls_height_(top_controls_height),
      current_scroll_delta_(0.f),
      controls_scroll_begin_offset_(0.f),
      top_controls_show_height_(
          top_controls_height * top_controls_hide_threshold),
      top_controls_hide_height_(
          top_controls_height * (1.f - top_controls_show_threshold)),
      pinch_gesture_active_(false) {
  CHECK(client_);
}

TopControlsManager::~TopControlsManager() {
}

void TopControlsManager::UpdateTopControlsState(TopControlsState constraints,
                                                TopControlsState current,
                                                bool animate) {
  DCHECK(!(constraints == SHOWN && current == HIDDEN));
  DCHECK(!(constraints == HIDDEN && current == SHOWN));

  permitted_state_ = constraints;

  // Don't do anything if it doesn't matter which state the controls are in.
  if (constraints == BOTH && current == BOTH)
    return;

  // Don't do anything if there is no change in offset.
  float final_controls_position = 0.f;
  if (constraints == HIDDEN || current == HIDDEN) {
    final_controls_position = -top_controls_height_;
  }
  if (final_controls_position == controls_top_offset_) {
    return;
  }

  AnimationDirection animation_direction = SHOWING_CONTROLS;
  if (constraints == HIDDEN || current == HIDDEN)
    animation_direction = HIDING_CONTROLS;
  ResetAnimations();
  if (animate) {
    SetupAnimation(animation_direction);
  } else {
    controls_top_offset_ = final_controls_position;
  }
  client_->DidChangeTopControlsPosition();
}

void TopControlsManager::ScrollBegin() {
  DCHECK(!pinch_gesture_active_);
  ResetAnimations();
  current_scroll_delta_ = 0.f;
  controls_scroll_begin_offset_ = controls_top_offset_;
}

gfx::Vector2dF TopControlsManager::ScrollBy(
    const gfx::Vector2dF& pending_delta) {
  if (pinch_gesture_active_)
    return pending_delta;

  if (permitted_state_ == SHOWN && pending_delta.y() > 0)
    return pending_delta;
  else if (permitted_state_ == HIDDEN && pending_delta.y() < 0)
    return pending_delta;

  current_scroll_delta_ += pending_delta.y();

  float old_offset = controls_top_offset_;
  SetControlsTopOffset(controls_scroll_begin_offset_ - current_scroll_delta_);

  // If the controls are fully visible, treat the current position as the
  // new baseline even if the gesture didn't end.
  if (controls_top_offset_ == 0.f) {
    current_scroll_delta_ = 0.f;
    controls_scroll_begin_offset_ = 0.f;
  }

  ResetAnimations();

  gfx::Vector2dF applied_delta(0.f, old_offset - controls_top_offset_);
  return pending_delta - applied_delta;
}

void TopControlsManager::ScrollEnd() {
  DCHECK(!pinch_gesture_active_);
  StartAnimationIfNecessary();
}

void TopControlsManager::PinchBegin() {
  DCHECK(!pinch_gesture_active_);
  pinch_gesture_active_ = true;
  StartAnimationIfNecessary();
}

void TopControlsManager::PinchEnd() {
  DCHECK(pinch_gesture_active_);
  // Pinch{Begin,End} will always occur within the scope of Scroll{Begin,End},
  // so return to a state expected by the remaining scroll sequence.
  pinch_gesture_active_ = false;
  ScrollBegin();
}

void TopControlsManager::SetControlsTopOffset(float controls_top_offset) {
  controls_top_offset = std::max(controls_top_offset, -top_controls_height_);
  controls_top_offset = std::min(controls_top_offset, 0.f);

  if (controls_top_offset_ == controls_top_offset)
    return;

  controls_top_offset_ = controls_top_offset;

  client_->DidChangeTopControlsPosition();
}

gfx::Vector2dF TopControlsManager::Animate(base::TimeTicks monotonic_time) {
  if (!top_controls_animation_ || !client_->HaveRootScrollLayer())
    return gfx::Vector2dF();

  double time = (monotonic_time - base::TimeTicks()).InMillisecondsF();

  float old_offset = controls_top_offset_;
  SetControlsTopOffset(top_controls_animation_->GetValue(time));

  if (IsAnimationCompleteAtTime(monotonic_time))
    ResetAnimations();

  gfx::Vector2dF scroll_delta(0.f, controls_top_offset_ - old_offset);
  return scroll_delta;
}

void TopControlsManager::ResetAnimations() {
  if (top_controls_animation_)
    top_controls_animation_.reset();

  animation_direction_ = NO_ANIMATION;
}

void TopControlsManager::SetupAnimation(AnimationDirection direction) {
  DCHECK(direction != NO_ANIMATION);

  if (direction == SHOWING_CONTROLS && controls_top_offset_ == 0)
    return;

  if (direction == HIDING_CONTROLS &&
      controls_top_offset_ == -top_controls_height_) {
    return;
  }

  if (top_controls_animation_ && animation_direction_ == direction)
    return;

  top_controls_animation_ = KeyframedFloatAnimationCurve::Create();
  double start_time =
      (gfx::FrameTime::Now() - base::TimeTicks()).InMillisecondsF();
  top_controls_animation_->AddKeyframe(
      FloatKeyframe::Create(start_time, controls_top_offset_,
                            scoped_ptr<TimingFunction>()));
  float max_ending_offset =
      (direction == SHOWING_CONTROLS ? 1 : -1) * top_controls_height_;
  top_controls_animation_->AddKeyframe(
      FloatKeyframe::Create(start_time + kShowHideMaxDurationMs,
                            controls_top_offset_ + max_ending_offset,
                            EaseTimingFunction::Create()));
  animation_direction_ = direction;
  client_->DidChangeTopControlsPosition();
}

void TopControlsManager::StartAnimationIfNecessary() {
  if (controls_top_offset_ != 0
      && controls_top_offset_ != -top_controls_height_) {
    AnimationDirection show_controls = NO_ANIMATION;

    if (controls_top_offset_ >= -top_controls_show_height_) {
      // If we're showing so much that the hide threshold won't trigger, show.
      show_controls = SHOWING_CONTROLS;
    } else if (controls_top_offset_ <= -top_controls_hide_height_) {
      // If we're showing so little that the show threshold won't trigger, hide.
      show_controls = HIDING_CONTROLS;
    } else {
      // If we could be either showing or hiding, we determine which one to
      // do based on whether or not the total scroll delta was moving up or
      // down.
      show_controls = current_scroll_delta_ <= 0.f ?
          SHOWING_CONTROLS : HIDING_CONTROLS;
    }

    if (show_controls != NO_ANIMATION)
      SetupAnimation(show_controls);
  }
}

bool TopControlsManager::IsAnimationCompleteAtTime(base::TimeTicks time) {
  if (!top_controls_animation_)
    return true;

  double time_ms = (time - base::TimeTicks()).InMillisecondsF();
  float new_offset = top_controls_animation_->GetValue(time_ms);

  if ((animation_direction_ == SHOWING_CONTROLS && new_offset >= 0) ||
      (animation_direction_ == HIDING_CONTROLS
          && new_offset <= -top_controls_height_)) {
    return true;
  }
  return false;
}

}  // namespace cc
