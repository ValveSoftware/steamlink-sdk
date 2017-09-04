// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/scroll_offset_animations_impl.h"

#include "cc/animation/animation_host.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/animation_player.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/element_animations.h"
#include "cc/animation/timing_function.h"

namespace cc {

ScrollOffsetAnimationsImpl::ScrollOffsetAnimationsImpl(
    AnimationHost* animation_host)
    : animation_host_(animation_host),
      scroll_offset_timeline_(
          AnimationTimeline::Create(AnimationIdProvider::NextTimelineId())),
      scroll_offset_animation_player_(
          AnimationPlayer::Create(AnimationIdProvider::NextPlayerId())) {
  scroll_offset_timeline_->set_is_impl_only(true);
  scroll_offset_animation_player_->set_animation_delegate(this);

  animation_host_->AddAnimationTimeline(scroll_offset_timeline_.get());
  scroll_offset_timeline_->AttachPlayer(scroll_offset_animation_player_.get());
}

ScrollOffsetAnimationsImpl::~ScrollOffsetAnimationsImpl() {
  scroll_offset_timeline_->DetachPlayer(scroll_offset_animation_player_.get());
  animation_host_->RemoveAnimationTimeline(scroll_offset_timeline_.get());
}

void ScrollOffsetAnimationsImpl::ScrollAnimationCreate(
    ElementId element_id,
    const gfx::ScrollOffset& target_offset,
    const gfx::ScrollOffset& current_offset,
    base::TimeDelta delayed_by) {
  std::unique_ptr<ScrollOffsetAnimationCurve> curve =
      ScrollOffsetAnimationCurve::Create(
          target_offset, CubicBezierTimingFunction::CreatePreset(
                             CubicBezierTimingFunction::EaseType::EASE_IN_OUT),
          ScrollOffsetAnimationCurve::DurationBehavior::INVERSE_DELTA);
  curve->SetInitialValue(current_offset, delayed_by);

  std::unique_ptr<Animation> animation = Animation::Create(
      std::move(curve), AnimationIdProvider::NextAnimationId(),
      AnimationIdProvider::NextGroupId(), TargetProperty::SCROLL_OFFSET);
  animation->set_is_impl_only(true);

  DCHECK(scroll_offset_animation_player_);
  DCHECK(scroll_offset_animation_player_->animation_timeline());

  ReattachScrollOffsetPlayerIfNeeded(element_id);

  scroll_offset_animation_player_->AddAnimation(std::move(animation));
}

bool ScrollOffsetAnimationsImpl::ScrollAnimationUpdateTarget(
    ElementId element_id,
    const gfx::Vector2dF& scroll_delta,
    const gfx::ScrollOffset& max_scroll_offset,
    base::TimeTicks frame_monotonic_time,
    base::TimeDelta delayed_by) {
  DCHECK(scroll_offset_animation_player_);
  if (!scroll_offset_animation_player_->element_animations())
    return false;

  DCHECK_EQ(element_id, scroll_offset_animation_player_->element_id());

  Animation* animation = scroll_offset_animation_player_->GetAnimation(
      TargetProperty::SCROLL_OFFSET);
  if (!animation) {
    scroll_offset_animation_player_->DetachElement();
    return false;
  }
  if (scroll_delta.IsZero())
    return true;

  ScrollOffsetAnimationCurve* curve =
      animation->curve()->ToScrollOffsetAnimationCurve();

  gfx::ScrollOffset new_target =
      gfx::ScrollOffsetWithDelta(curve->target_value(), scroll_delta);
  new_target.SetToMax(gfx::ScrollOffset());
  new_target.SetToMin(max_scroll_offset);

  // TODO(ymalik): Animation::TrimTimeToCurrentIteration should probably check
  // for run_state == Animation::WAITING_FOR_TARGET_AVAILABILITY.
  base::TimeDelta trimmed =
      animation->run_state() == Animation::WAITING_FOR_TARGET_AVAILABILITY
          ? base::TimeDelta()
          : animation->TrimTimeToCurrentIteration(frame_monotonic_time);

  // Re-target taking the delay into account. Note that if the duration of the
  // animation is 0, trimmed will be 0 and UpdateTarget will be called with
  // t = -delayed_by.
  trimmed -= delayed_by;

  curve->UpdateTarget(trimmed.InSecondsF(), new_target);

  return true;
}

void ScrollOffsetAnimationsImpl::ScrollAnimationApplyAdjustment(
    ElementId element_id,
    const gfx::Vector2dF& adjustment) {
  DCHECK(scroll_offset_animation_player_);
  if (element_id != scroll_offset_animation_player_->element_id())
    return;

  if (!scroll_offset_animation_player_->element_animations())
    return;

  Animation* animation = scroll_offset_animation_player_->GetAnimation(
      TargetProperty::SCROLL_OFFSET);
  if (!animation)
    return;

  std::unique_ptr<ScrollOffsetAnimationCurve> new_curve =
      animation->curve()
          ->ToScrollOffsetAnimationCurve()
          ->CloneToScrollOffsetAnimationCurve();
  new_curve->ApplyAdjustment(adjustment);

  std::unique_ptr<Animation> new_animation = Animation::Create(
      std::move(new_curve), AnimationIdProvider::NextAnimationId(),
      AnimationIdProvider::NextGroupId(), TargetProperty::SCROLL_OFFSET);
  new_animation->set_start_time(animation->start_time());
  new_animation->set_is_impl_only(true);
  new_animation->set_affects_active_elements(false);

  // Abort the old animation.
  ScrollAnimationAbort(/* needs_completion */ false);

  // Start a new one with the adjusment.
  scroll_offset_animation_player_->AddAnimation(std::move(new_animation));
}

void ScrollOffsetAnimationsImpl::ScrollAnimationAbort(bool needs_completion) {
  DCHECK(scroll_offset_animation_player_);
  scroll_offset_animation_player_->AbortAnimations(
      TargetProperty::SCROLL_OFFSET, needs_completion);
}

void ScrollOffsetAnimationsImpl::NotifyAnimationFinished(
    base::TimeTicks monotonic_time,
    TargetProperty::Type target_property,
    int group) {
  DCHECK_EQ(target_property, TargetProperty::SCROLL_OFFSET);
  DCHECK(animation_host_->mutator_host_client());
  animation_host_->mutator_host_client()->ScrollOffsetAnimationFinished();
}

void ScrollOffsetAnimationsImpl::ReattachScrollOffsetPlayerIfNeeded(
    ElementId element_id) {
  if (scroll_offset_animation_player_->element_id() != element_id) {
    if (scroll_offset_animation_player_->element_id())
      scroll_offset_animation_player_->DetachElement();
    if (element_id)
      scroll_offset_animation_player_->AttachElement(element_id);
  }
}

}  // namespace cc
