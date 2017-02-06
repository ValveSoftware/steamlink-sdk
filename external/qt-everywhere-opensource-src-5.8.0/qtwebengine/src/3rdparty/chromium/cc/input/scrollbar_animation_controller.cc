// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/scrollbar_animation_controller.h"

#include <algorithm>

#include "base/time/time.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {

ScrollbarAnimationController::ScrollbarAnimationController(
    int scroll_layer_id,
    ScrollbarAnimationControllerClient* client,
    base::TimeDelta delay_before_starting,
    base::TimeDelta resize_delay_before_starting,
    base::TimeDelta duration)
    : client_(client),
      delay_before_starting_(delay_before_starting),
      resize_delay_before_starting_(resize_delay_before_starting),
      duration_(duration),
      is_animating_(false),
      scroll_layer_id_(scroll_layer_id),
      currently_scrolling_(false),
      scroll_gesture_has_scrolled_(false),
      weak_factory_(this) {}

ScrollbarAnimationController::~ScrollbarAnimationController() {}

bool ScrollbarAnimationController::Animate(base::TimeTicks now) {
  if (!is_animating_)
    return false;

  if (last_awaken_time_.is_null())
    last_awaken_time_ = now;

  float progress = AnimationProgressAtTime(now);
  RunAnimationFrame(progress);

  if (is_animating_)
    client_->SetNeedsAnimateForScrollbarAnimation();
  return true;
}

float ScrollbarAnimationController::AnimationProgressAtTime(
    base::TimeTicks now) {
  base::TimeDelta delta = now - last_awaken_time_;
  float progress = delta.InSecondsF() / duration_.InSecondsF();
  return std::max(std::min(progress, 1.f), 0.f);
}

void ScrollbarAnimationController::DidScrollBegin() {
  currently_scrolling_ = true;
}

void ScrollbarAnimationController::DidScrollUpdate(bool on_resize) {
  StopAnimation();
  delayed_scrollbar_fade_.Cancel();

  // As an optimization, we avoid spamming fade delay tasks during active fast
  // scrolls.  But if we're not within one, we need to post every scroll update.
  if (!currently_scrolling_)
    PostDelayedAnimationTask(on_resize);
  else
    scroll_gesture_has_scrolled_ = true;
}

void ScrollbarAnimationController::DidScrollEnd() {
  if (scroll_gesture_has_scrolled_) {
    PostDelayedAnimationTask(false);
    scroll_gesture_has_scrolled_ = false;
  }

  currently_scrolling_ = false;
}

void ScrollbarAnimationController::PostDelayedAnimationTask(bool on_resize) {
  base::TimeDelta delay =
      on_resize ? resize_delay_before_starting_ : delay_before_starting_;
  delayed_scrollbar_fade_.Reset(
      base::Bind(&ScrollbarAnimationController::StartAnimation,
                 weak_factory_.GetWeakPtr()));
  client_->PostDelayedScrollbarAnimationTask(delayed_scrollbar_fade_.callback(),
                                             delay);
}

void ScrollbarAnimationController::StartAnimation() {
  delayed_scrollbar_fade_.Cancel();
  is_animating_ = true;
  last_awaken_time_ = base::TimeTicks();
  client_->SetNeedsAnimateForScrollbarAnimation();
}

void ScrollbarAnimationController::StopAnimation() {
  is_animating_ = false;
}

ScrollbarSet ScrollbarAnimationController::Scrollbars() const {
  return client_->ScrollbarsFor(scroll_layer_id_);
}

}  // namespace cc
