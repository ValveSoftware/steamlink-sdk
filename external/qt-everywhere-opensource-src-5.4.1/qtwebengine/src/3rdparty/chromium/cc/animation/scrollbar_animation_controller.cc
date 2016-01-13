// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/scrollbar_animation_controller.h"

#include <algorithm>

#include "base/time/time.h"

namespace cc {

ScrollbarAnimationController::ScrollbarAnimationController(
    ScrollbarAnimationControllerClient* client,
    base::TimeDelta delay_before_starting,
    base::TimeDelta duration)
    : client_(client),
      delay_before_starting_(delay_before_starting),
      duration_(duration),
      is_animating_(false),
      currently_scrolling_(false),
      scroll_gesture_has_scrolled_(false),
      weak_factory_(this) {
}

ScrollbarAnimationController::~ScrollbarAnimationController() {
}

void ScrollbarAnimationController::Animate(base::TimeTicks now) {
  if (!is_animating_)
    return;

  if (last_awaken_time_.is_null())
    last_awaken_time_ = now;

  float progress = AnimationProgressAtTime(now);
  RunAnimationFrame(progress);

  if (is_animating_) {
    delayed_scrollbar_fade_.Cancel();
    client_->SetNeedsScrollbarAnimationFrame();
  }
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

void ScrollbarAnimationController::DidScrollUpdate() {
  StopAnimation();
  delayed_scrollbar_fade_.Cancel();

  // As an optimization, we avoid spamming fade delay tasks during active fast
  // scrolls.  But if we're not within one, we need to post every scroll update.
  if (!currently_scrolling_)
    PostDelayedFade();
  else
    scroll_gesture_has_scrolled_ = true;
}

void ScrollbarAnimationController::DidScrollEnd() {
  if (scroll_gesture_has_scrolled_) {
    PostDelayedFade();
    scroll_gesture_has_scrolled_ = false;
  }

  currently_scrolling_ = false;
}

void ScrollbarAnimationController::PostDelayedFade() {
  delayed_scrollbar_fade_.Reset(
      base::Bind(&ScrollbarAnimationController::StartAnimation,
                 weak_factory_.GetWeakPtr()));
  client_->PostDelayedScrollbarFade(delayed_scrollbar_fade_.callback(),
                                    delay_before_starting_);
}

void ScrollbarAnimationController::StartAnimation() {
  delayed_scrollbar_fade_.Cancel();
  is_animating_ = true;
  last_awaken_time_ = base::TimeTicks();
  client_->SetNeedsScrollbarAnimationFrame();
}

void ScrollbarAnimationController::StopAnimation() {
  is_animating_ = false;
}

}  // namespace cc
