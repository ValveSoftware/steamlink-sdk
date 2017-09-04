// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/animation_player.h"

#include <algorithm>

#include "cc/animation/animation_delegate.h"
#include "cc/animation/animation_events.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/scroll_offset_animation_curve.h"
#include "cc/trees/property_animation_state.h"

namespace cc {

scoped_refptr<AnimationPlayer> AnimationPlayer::Create(int id) {
  return make_scoped_refptr(new AnimationPlayer(id));
}

AnimationPlayer::AnimationPlayer(int id)
    : animation_host_(),
      animation_timeline_(),
      element_animations_(),
      animation_delegate_(),
      id_(id),
      needs_push_properties_(false),
      needs_to_start_animations_(false) {
  DCHECK(id_);
}

AnimationPlayer::~AnimationPlayer() {
  DCHECK(!animation_timeline_);
  DCHECK(!element_animations_);
}

scoped_refptr<AnimationPlayer> AnimationPlayer::CreateImplInstance() const {
  scoped_refptr<AnimationPlayer> player = AnimationPlayer::Create(id());
  return player;
}

void AnimationPlayer::SetAnimationHost(AnimationHost* animation_host) {
  animation_host_ = animation_host;
}

void AnimationPlayer::SetAnimationTimeline(AnimationTimeline* timeline) {
  if (animation_timeline_ == timeline)
    return;

  // We need to unregister player to manage ElementAnimations and observers
  // properly.
  if (element_id_ && element_animations_)
    UnregisterPlayer();

  animation_timeline_ = timeline;

  // Register player only if layer AND host attached.
  if (element_id_ && animation_host_)
    RegisterPlayer();
}

void AnimationPlayer::AttachElement(ElementId element_id) {
  DCHECK(!element_id_);
  DCHECK(element_id);

  element_id_ = element_id;

  // Register player only if layer AND host attached.
  if (animation_host_)
    RegisterPlayer();
}

void AnimationPlayer::DetachElement() {
  DCHECK(element_id_);

  if (animation_host_)
    UnregisterPlayer();

  element_id_ = ElementId();
}

void AnimationPlayer::RegisterPlayer() {
  DCHECK(element_id_);
  DCHECK(animation_host_);
  DCHECK(!element_animations_);

  // Create ElementAnimations or re-use existing.
  animation_host_->RegisterPlayerForElement(element_id_, this);
  // Get local reference to shared ElementAnimations.
  BindElementAnimations();
}

void AnimationPlayer::UnregisterPlayer() {
  DCHECK(element_id_);
  DCHECK(animation_host_);
  DCHECK(element_animations_);

  UnbindElementAnimations();
  // Destroy ElementAnimations or release it if it's still needed.
  animation_host_->UnregisterPlayerForElement(element_id_, this);
}

void AnimationPlayer::BindElementAnimations() {
  DCHECK(!element_animations_);
  element_animations_ =
      animation_host_->GetElementAnimationsForElementId(element_id_);
  DCHECK(element_animations_);

  if (!animations_.empty())
    AnimationAdded();

  SetNeedsPushProperties();
}

void AnimationPlayer::UnbindElementAnimations() {
  SetNeedsPushProperties();
  element_animations_ = nullptr;
}

void AnimationPlayer::AddAnimation(std::unique_ptr<Animation> animation) {
  DCHECK(animation->target_property() != TargetProperty::SCROLL_OFFSET ||
         (animation_host_ && animation_host_->SupportsScrollAnimations()));
  DCHECK(!animation->is_impl_only() ||
         animation->target_property() == TargetProperty::SCROLL_OFFSET);

  animations_.push_back(std::move(animation));
  if (element_animations_) {
    AnimationAdded();
    SetNeedsPushProperties();
  }
}

void AnimationPlayer::AnimationAdded() {
  DCHECK(element_animations_);

  SetNeedsCommit();
  needs_to_start_animations_ = true;

  element_animations_->UpdateActivationNormal();
  element_animations_->UpdateClientAnimationState();
}

void AnimationPlayer::PauseAnimation(int animation_id, double time_offset) {
  const base::TimeDelta time_delta = base::TimeDelta::FromSecondsD(time_offset);

  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->id() == animation_id) {
      animations_[i]->SetRunState(Animation::PAUSED,
                                  time_delta + animations_[i]->start_time() +
                                      animations_[i]->time_offset());
    }
  }

  if (element_animations_) {
    SetNeedsCommit();
    SetNeedsPushProperties();
  }
}

void AnimationPlayer::RemoveAnimation(int animation_id) {
  bool animation_removed = false;

  // Since we want to use the animations that we're going to remove, we need to
  // use a stable_parition here instead of remove_if. Remove_if leaves the
  // removed items in an unspecified state.
  auto animations_to_remove = std::stable_partition(
      animations_.begin(), animations_.end(),
      [animation_id](const std::unique_ptr<Animation>& animation) {
        return animation->id() != animation_id;
      });
  for (auto it = animations_to_remove; it != animations_.end(); ++it) {
    if ((*it)->target_property() == TargetProperty::SCROLL_OFFSET) {
      if (element_animations_)
        element_animations_->SetScrollOffsetAnimationWasInterrupted();
    } else if (!(*it)->is_finished()) {
      animation_removed = true;
    }
  }

  animations_.erase(animations_to_remove, animations_.end());

  if (element_animations_) {
    element_animations_->UpdateActivationNormal();
    if (animation_removed)
      element_animations_->UpdateClientAnimationState();
    SetNeedsCommit();
    SetNeedsPushProperties();
  }
}

void AnimationPlayer::AbortAnimation(int animation_id) {
  if (Animation* animation = GetAnimationById(animation_id)) {
    if (!animation->is_finished()) {
      animation->SetRunState(Animation::ABORTED, last_tick_time_);
      if (element_animations_)
        element_animations_->UpdateClientAnimationState();
    }
  }

  if (element_animations_) {
    SetNeedsCommit();
    SetNeedsPushProperties();
  }
}

void AnimationPlayer::AbortAnimations(TargetProperty::Type target_property,
                                      bool needs_completion) {
  if (needs_completion)
    DCHECK(target_property == TargetProperty::SCROLL_OFFSET);

  bool aborted_animation = false;
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->target_property() == target_property &&
        !animations_[i]->is_finished()) {
      // Currently only impl-only scroll offset animations can be completed on
      // the main thread.
      if (needs_completion && animations_[i]->is_impl_only()) {
        animations_[i]->SetRunState(Animation::ABORTED_BUT_NEEDS_COMPLETION,
                                    last_tick_time_);
      } else {
        animations_[i]->SetRunState(Animation::ABORTED, last_tick_time_);
      }
      aborted_animation = true;
    }
  }

  if (element_animations_) {
    if (aborted_animation)
      element_animations_->UpdateClientAnimationState();
    SetNeedsCommit();
    SetNeedsPushProperties();
  }
}

void AnimationPlayer::PushPropertiesTo(AnimationPlayer* player_impl) {
  if (!needs_push_properties_)
    return;
  needs_push_properties_ = false;

  // Create or destroy ElementAnimations.
  if (element_id_ != player_impl->element_id()) {
    if (player_impl->element_id())
      player_impl->DetachElement();
    if (element_id_)
      player_impl->AttachElement(element_id_);
  }

  if (!has_any_animation() && !player_impl->has_any_animation())
    return;

  MarkAbortedAnimationsForDeletion(player_impl);
  PurgeAnimationsMarkedForDeletion();
  PushNewAnimationsToImplThread(player_impl);

  // Remove finished impl side animations only after pushing,
  // and only after the animations are deleted on the main thread
  // this insures we will never push an animation twice.
  RemoveAnimationsCompletedOnMainThread(player_impl);

  PushPropertiesToImplThread(player_impl);
}

bool AnimationPlayer::NotifyAnimationStarted(const AnimationEvent& event) {
  DCHECK(!event.is_impl_only);

  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->group() == event.group_id &&
        animations_[i]->target_property() == event.target_property &&
        animations_[i]->needs_synchronized_start_time()) {
      animations_[i]->set_needs_synchronized_start_time(false);
      if (!animations_[i]->has_set_start_time())
        animations_[i]->set_start_time(event.monotonic_time);

      if (animation_delegate_) {
        animation_delegate_->NotifyAnimationStarted(
            event.monotonic_time, event.target_property, event.group_id);
      }
      return true;
    }
  }

  return false;
}

bool AnimationPlayer::NotifyAnimationFinished(const AnimationEvent& event) {
  DCHECK(!event.is_impl_only);

  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->group() == event.group_id &&
        animations_[i]->target_property() == event.target_property) {
      animations_[i]->set_received_finished_event(true);

      if (animation_delegate_) {
        animation_delegate_->NotifyAnimationFinished(
            event.monotonic_time, event.target_property, event.group_id);
      }
      return true;
    }
  }

  return false;
}

bool AnimationPlayer::NotifyAnimationFinishedForTesting(
    TargetProperty::Type target_property,
    int group_id) {
  AnimationEvent event(AnimationEvent::FINISHED, element_id_, group_id,
                       target_property, base::TimeTicks());
  return NotifyAnimationFinished(event);
}

bool AnimationPlayer::NotifyAnimationAborted(const AnimationEvent& event) {
  DCHECK(!event.is_impl_only);

  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->group() == event.group_id &&
        animations_[i]->target_property() == event.target_property) {
      animations_[i]->SetRunState(Animation::ABORTED, event.monotonic_time);
      animations_[i]->set_received_finished_event(true);
      if (animation_delegate_) {
        animation_delegate_->NotifyAnimationAborted(
            event.monotonic_time, event.target_property, event.group_id);
      }
      return true;
    }
  }

  return false;
}

void AnimationPlayer::NotifyAnimationTakeover(const AnimationEvent& event) {
  DCHECK(!event.is_impl_only);
  DCHECK(event.target_property == TargetProperty::SCROLL_OFFSET);

  // We need to purge animations marked for deletion on CT.
  SetNeedsPushProperties();

  if (animation_delegate_) {
    DCHECK(event.curve);
    std::unique_ptr<AnimationCurve> animation_curve = event.curve->Clone();
    animation_delegate_->NotifyAnimationTakeover(
        event.monotonic_time, event.target_property, event.animation_start_time,
        std::move(animation_curve));
  }
}

void AnimationPlayer::SetNeedsCommit() {
  DCHECK(animation_host_);
  animation_host_->SetNeedsCommit();
}

void AnimationPlayer::SetNeedsPushProperties() {
  needs_push_properties_ = true;

  DCHECK(animation_timeline_);
  animation_timeline_->SetNeedsPushProperties();

  DCHECK(element_animations_);
  element_animations_->SetNeedsPushProperties();
}

bool AnimationPlayer::HasActiveAnimation() const {
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (!animations_[i]->is_finished())
      return true;
  }
  return false;
}

bool AnimationPlayer::HasNonDeletedAnimation() const {
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->run_state() != Animation::WAITING_FOR_DELETION)
      return true;
  }

  return false;
}

void AnimationPlayer::StartAnimations(base::TimeTicks monotonic_time) {
  DCHECK(needs_to_start_animations_);
  needs_to_start_animations_ = false;
  // First collect running properties affecting each type of element.
  TargetProperties blocked_properties_for_active_elements;
  TargetProperties blocked_properties_for_pending_elements;
  std::vector<size_t> animations_waiting_for_target;

  animations_waiting_for_target.reserve(animations_.size());
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->run_state() == Animation::STARTING ||
        animations_[i]->run_state() == Animation::RUNNING) {
      if (animations_[i]->affects_active_elements()) {
        blocked_properties_for_active_elements[animations_[i]
                                                   ->target_property()] = true;
      }
      if (animations_[i]->affects_pending_elements()) {
        blocked_properties_for_pending_elements[animations_[i]
                                                    ->target_property()] = true;
      }
    } else if (animations_[i]->run_state() ==
               Animation::WAITING_FOR_TARGET_AVAILABILITY) {
      animations_waiting_for_target.push_back(i);
    }
  }

  for (size_t i = 0; i < animations_waiting_for_target.size(); ++i) {
    // Collect all properties for animations with the same group id (they
    // should all also be in the list of animations).
    size_t animation_index = animations_waiting_for_target[i];
    Animation* animation_waiting_for_target =
        animations_[animation_index].get();
    // Check for the run state again even though the animation was waiting
    // for target because it might have changed the run state while handling
    // previous animation in this loop (if they belong to same group).
    if (animation_waiting_for_target->run_state() ==
        Animation::WAITING_FOR_TARGET_AVAILABILITY) {
      TargetProperties enqueued_properties;
      bool affects_active_elements =
          animation_waiting_for_target->affects_active_elements();
      bool affects_pending_elements =
          animation_waiting_for_target->affects_pending_elements();
      enqueued_properties[animation_waiting_for_target->target_property()] =
          true;
      for (size_t j = animation_index + 1; j < animations_.size(); ++j) {
        if (animation_waiting_for_target->group() == animations_[j]->group()) {
          enqueued_properties[animations_[j]->target_property()] = true;
          affects_active_elements |= animations_[j]->affects_active_elements();
          affects_pending_elements |=
              animations_[j]->affects_pending_elements();
        }
      }

      // Check to see if intersection of the list of properties affected by
      // the group and the list of currently blocked properties is null, taking
      // into account the type(s) of elements affected by the group. In any
      // case, the group's target properties need to be added to the lists of
      // blocked properties.
      bool null_intersection = true;
      for (int property = TargetProperty::FIRST_TARGET_PROPERTY;
           property <= TargetProperty::LAST_TARGET_PROPERTY; ++property) {
        if (enqueued_properties[property]) {
          if (affects_active_elements) {
            if (blocked_properties_for_active_elements[property])
              null_intersection = false;
            else
              blocked_properties_for_active_elements[property] = true;
          }
          if (affects_pending_elements) {
            if (blocked_properties_for_pending_elements[property])
              null_intersection = false;
            else
              blocked_properties_for_pending_elements[property] = true;
          }
        }
      }

      // If the intersection is null, then we are free to start the animations
      // in the group.
      if (null_intersection) {
        animation_waiting_for_target->SetRunState(Animation::STARTING,
                                                  monotonic_time);
        for (size_t j = animation_index + 1; j < animations_.size(); ++j) {
          if (animation_waiting_for_target->group() ==
              animations_[j]->group()) {
            animations_[j]->SetRunState(Animation::STARTING, monotonic_time);
          }
        }
      } else {
        needs_to_start_animations_ = true;
      }
    }
  }
}

void AnimationPlayer::PromoteStartedAnimations(base::TimeTicks monotonic_time,
                                               AnimationEvents* events) {
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->run_state() == Animation::STARTING &&
        animations_[i]->affects_active_elements()) {
      animations_[i]->SetRunState(Animation::RUNNING, monotonic_time);
      if (!animations_[i]->has_set_start_time() &&
          !animations_[i]->needs_synchronized_start_time())
        animations_[i]->set_start_time(monotonic_time);
      if (events) {
        base::TimeTicks start_time;
        if (animations_[i]->has_set_start_time())
          start_time = animations_[i]->start_time();
        else
          start_time = monotonic_time;
        AnimationEvent started_event(
            AnimationEvent::STARTED, element_id_, animations_[i]->group(),
            animations_[i]->target_property(), start_time);
        started_event.is_impl_only = animations_[i]->is_impl_only();
        if (started_event.is_impl_only) {
          // Notify delegate directly, do not record the event.
          if (animation_delegate_) {
            animation_delegate_->NotifyAnimationStarted(
                started_event.monotonic_time, started_event.target_property,
                started_event.group_id);
          }
        } else {
          events->events_.push_back(started_event);
        }
      }
    }
  }
}

void AnimationPlayer::MarkAnimationsForDeletion(base::TimeTicks monotonic_time,
                                                AnimationEvents* events) {
  bool marked_animations_for_deletions = false;
  std::vector<size_t> animations_with_same_group_id;

  animations_with_same_group_id.reserve(animations_.size());
  // Non-aborted animations are marked for deletion after a corresponding
  // AnimationEvent::FINISHED event is sent or received. This means that if
  // we don't have an events vector, we must ensure that non-aborted animations
  // have received a finished event before marking them for deletion.
  for (size_t i = 0; i < animations_.size(); i++) {
    int group_id = animations_[i]->group();
    if (animations_[i]->run_state() == Animation::ABORTED) {
      if (events && !animations_[i]->is_impl_only()) {
        AnimationEvent aborted_event(
            AnimationEvent::ABORTED, element_id_, group_id,
            animations_[i]->target_property(), monotonic_time);
        events->events_.push_back(aborted_event);
      }
      // If on the compositor or on the main thread and received finish event,
      // animation can be marked for deletion.
      if (events || animations_[i]->received_finished_event()) {
        animations_[i]->SetRunState(Animation::WAITING_FOR_DELETION,
                                    monotonic_time);
        marked_animations_for_deletions = true;
      }
      continue;
    }

    // If running on the compositor and need to complete an aborted animation
    // on the main thread.
    if (events &&
        animations_[i]->run_state() ==
            Animation::ABORTED_BUT_NEEDS_COMPLETION) {
      AnimationEvent aborted_event(AnimationEvent::TAKEOVER, element_id_,
                                   group_id, animations_[i]->target_property(),
                                   monotonic_time);
      aborted_event.animation_start_time =
          (animations_[i]->start_time() - base::TimeTicks()).InSecondsF();
      const ScrollOffsetAnimationCurve* scroll_offset_animation_curve =
          animations_[i]->curve()->ToScrollOffsetAnimationCurve();
      aborted_event.curve = scroll_offset_animation_curve->Clone();
      // Notify the compositor that the animation is finished.
      if (animation_delegate_) {
        animation_delegate_->NotifyAnimationFinished(
            aborted_event.monotonic_time, aborted_event.target_property,
            aborted_event.group_id);
      }
      // Notify main thread.
      events->events_.push_back(aborted_event);

      // Remove the animation from the compositor.
      animations_[i]->SetRunState(Animation::WAITING_FOR_DELETION,
                                  monotonic_time);
      marked_animations_for_deletions = true;
      continue;
    }

    bool all_anims_with_same_id_are_finished = false;

    // Since deleting an animation on the main thread leads to its deletion
    // on the impl thread, we only mark a FINISHED main thread animation for
    // deletion once it has received a FINISHED event from the impl thread.
    bool animation_i_will_send_or_has_received_finish_event =
        animations_[i]->is_controlling_instance() ||
        animations_[i]->is_impl_only() ||
        animations_[i]->received_finished_event();
    // If an animation is finished, and not already marked for deletion,
    // find out if all other animations in the same group are also finished.
    if (animations_[i]->run_state() == Animation::FINISHED &&
        animation_i_will_send_or_has_received_finish_event) {
      // Clear the animations_with_same_group_id if it was added for
      // the previous animation's iteration.
      if (animations_with_same_group_id.size() > 0)
        animations_with_same_group_id.clear();
      all_anims_with_same_id_are_finished = true;
      for (size_t j = 0; j < animations_.size(); ++j) {
        bool animation_j_will_send_or_has_received_finish_event =
            animations_[j]->is_controlling_instance() ||
            animations_[j]->is_impl_only() ||
            animations_[j]->received_finished_event();
        if (group_id == animations_[j]->group()) {
          if (!animations_[j]->is_finished() ||
              (animations_[j]->run_state() == Animation::FINISHED &&
               !animation_j_will_send_or_has_received_finish_event)) {
            all_anims_with_same_id_are_finished = false;
            break;
          } else if (j >= i &&
                     animations_[j]->run_state() != Animation::ABORTED) {
            // Mark down the animations which belong to the same group
            // and is not yet aborted. If this current iteration finds that all
            // animations with same ID are finished, then the marked
            // animations below will be set to WAITING_FOR_DELETION in next
            // iteration.
            animations_with_same_group_id.push_back(j);
          }
        }
      }
    }
    if (all_anims_with_same_id_are_finished) {
      // We now need to remove all animations with the same group id as
      // group_id (and send along animation finished notifications, if
      // necessary).
      for (size_t j = 0; j < animations_with_same_group_id.size(); j++) {
        size_t animation_index = animations_with_same_group_id[j];
        if (events) {
          AnimationEvent finished_event(
              AnimationEvent::FINISHED, element_id_,
              animations_[animation_index]->group(),
              animations_[animation_index]->target_property(), monotonic_time);
          finished_event.is_impl_only =
              animations_[animation_index]->is_impl_only();
          if (finished_event.is_impl_only) {
            // Notify delegate directly, do not record the event.
            if (animation_delegate_) {
              animation_delegate_->NotifyAnimationFinished(
                  finished_event.monotonic_time, finished_event.target_property,
                  finished_event.group_id);
            }
          } else {
            events->events_.push_back(finished_event);
          }
        }
        animations_[animation_index]->SetRunState(
            Animation::WAITING_FOR_DELETION, monotonic_time);
      }
      marked_animations_for_deletions = true;
    }
  }

  // Notify about animations waiting for deletion.
  // We need to purge animations marked for deletion, which happens in
  // PushProperties().
  if (marked_animations_for_deletions)
    SetNeedsPushProperties();
}

void AnimationPlayer::TickAnimations(base::TimeTicks monotonic_time) {
  DCHECK(element_animations_);

  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->run_state() == Animation::STARTING ||
        animations_[i]->run_state() == Animation::RUNNING ||
        animations_[i]->run_state() == Animation::PAUSED) {
      if (!animations_[i]->InEffect(monotonic_time))
        continue;

      base::TimeDelta trimmed =
          animations_[i]->TrimTimeToCurrentIteration(monotonic_time);

      switch (animations_[i]->target_property()) {
        case TargetProperty::TRANSFORM: {
          const TransformAnimationCurve* transform_animation_curve =
              animations_[i]->curve()->ToTransformAnimationCurve();
          const gfx::Transform transform =
              transform_animation_curve->GetValue(trimmed);
          element_animations_->NotifyClientTransformAnimated(
              transform, animations_[i]->affects_active_elements(),
              animations_[i]->affects_pending_elements());
          break;
        }

        case TargetProperty::OPACITY: {
          const FloatAnimationCurve* float_animation_curve =
              animations_[i]->curve()->ToFloatAnimationCurve();
          const float opacity = std::max(
              std::min(float_animation_curve->GetValue(trimmed), 1.0f), 0.f);
          element_animations_->NotifyClientOpacityAnimated(
              opacity, animations_[i]->affects_active_elements(),
              animations_[i]->affects_pending_elements());
          break;
        }

        case TargetProperty::FILTER: {
          const FilterAnimationCurve* filter_animation_curve =
              animations_[i]->curve()->ToFilterAnimationCurve();
          const FilterOperations filter =
              filter_animation_curve->GetValue(trimmed);
          element_animations_->NotifyClientFilterAnimated(
              filter, animations_[i]->affects_active_elements(),
              animations_[i]->affects_pending_elements());
          break;
        }

        case TargetProperty::BACKGROUND_COLOR: {
          // Not yet implemented.
          break;
        }

        case TargetProperty::SCROLL_OFFSET: {
          const ScrollOffsetAnimationCurve* scroll_offset_animation_curve =
              animations_[i]->curve()->ToScrollOffsetAnimationCurve();
          const gfx::ScrollOffset scroll_offset =
              scroll_offset_animation_curve->GetValue(trimmed);
          element_animations_->NotifyClientScrollOffsetAnimated(
              scroll_offset, animations_[i]->affects_active_elements(),
              animations_[i]->affects_pending_elements());
          break;
        }
      }
    }
  }

  last_tick_time_ = monotonic_time;
}

void AnimationPlayer::MarkFinishedAnimations(base::TimeTicks monotonic_time) {
  bool animation_finished = false;

  for (size_t i = 0; i < animations_.size(); ++i) {
    if (!animations_[i]->is_finished() &&
        animations_[i]->IsFinishedAt(monotonic_time)) {
      animations_[i]->SetRunState(Animation::FINISHED, monotonic_time);
      animation_finished = true;
    }
  }

  DCHECK(element_animations_);
  if (animation_finished)
    element_animations_->UpdateClientAnimationState();
}

void AnimationPlayer::ActivateAnimations() {
  bool animation_activated = false;

  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->affects_active_elements() !=
        animations_[i]->affects_pending_elements()) {
      animation_activated = true;
    }
    animations_[i]->set_affects_active_elements(
        animations_[i]->affects_pending_elements());
  }
  auto affects_no_elements = [](const std::unique_ptr<Animation>& animation) {
    return !animation->affects_active_elements() &&
           !animation->affects_pending_elements();
  };
  animations_.erase(std::remove_if(animations_.begin(), animations_.end(),
                                   affects_no_elements),
                    animations_.end());

  if (animation_activated)
    element_animations_->UpdateClientAnimationState();
}

bool AnimationPlayer::HasFilterAnimationThatInflatesBounds() const {
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (!animations_[i]->is_finished() &&
        animations_[i]->target_property() == TargetProperty::FILTER &&
        animations_[i]
            ->curve()
            ->ToFilterAnimationCurve()
            ->HasFilterThatMovesPixels())
      return true;
  }

  return false;
}

bool AnimationPlayer::HasTransformAnimationThatInflatesBounds() const {
  return IsCurrentlyAnimatingProperty(TargetProperty::TRANSFORM,
                                      ElementListType::ACTIVE) ||
         IsCurrentlyAnimatingProperty(TargetProperty::TRANSFORM,
                                      ElementListType::PENDING);
}

bool AnimationPlayer::TransformAnimationBoundsForBox(const gfx::BoxF& box,
                                                     gfx::BoxF* bounds) const {
  DCHECK(HasTransformAnimationThatInflatesBounds())
      << "TransformAnimationBoundsForBox will give incorrect results if there "
      << "are no transform animations affecting bounds, non-animated transform "
      << "is not known";

  // Compute bounds based on animations for which is_finished() is false.
  // Do nothing if there are no such animations; in this case, it is assumed
  // that callers will take care of computing bounds based on the owning layer's
  // actual transform.
  *bounds = gfx::BoxF();
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->is_finished() ||
        animations_[i]->target_property() != TargetProperty::TRANSFORM)
      continue;

    const TransformAnimationCurve* transform_animation_curve =
        animations_[i]->curve()->ToTransformAnimationCurve();
    gfx::BoxF animation_bounds;
    bool success =
        transform_animation_curve->AnimatedBoundsForBox(box, &animation_bounds);
    if (!success)
      return false;
    bounds->Union(animation_bounds);
  }

  return true;
}

bool AnimationPlayer::HasOnlyTranslationTransforms(
    ElementListType list_type) const {
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->is_finished() ||
        animations_[i]->target_property() != TargetProperty::TRANSFORM)
      continue;

    if ((list_type == ElementListType::ACTIVE &&
         !animations_[i]->affects_active_elements()) ||
        (list_type == ElementListType::PENDING &&
         !animations_[i]->affects_pending_elements()))
      continue;

    const TransformAnimationCurve* transform_animation_curve =
        animations_[i]->curve()->ToTransformAnimationCurve();
    if (!transform_animation_curve->IsTranslation())
      return false;
  }

  return true;
}

bool AnimationPlayer::AnimationsPreserveAxisAlignment() const {
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->is_finished() ||
        animations_[i]->target_property() != TargetProperty::TRANSFORM)
      continue;

    const TransformAnimationCurve* transform_animation_curve =
        animations_[i]->curve()->ToTransformAnimationCurve();
    if (!transform_animation_curve->PreservesAxisAlignment())
      return false;
  }

  return true;
}

bool AnimationPlayer::AnimationStartScale(ElementListType list_type,
                                          float* start_scale) const {
  *start_scale = 0.f;
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->is_finished() ||
        animations_[i]->target_property() != TargetProperty::TRANSFORM)
      continue;

    if ((list_type == ElementListType::ACTIVE &&
         !animations_[i]->affects_active_elements()) ||
        (list_type == ElementListType::PENDING &&
         !animations_[i]->affects_pending_elements()))
      continue;

    bool forward_direction = true;
    switch (animations_[i]->direction()) {
      case Animation::Direction::NORMAL:
      case Animation::Direction::ALTERNATE_NORMAL:
        forward_direction = animations_[i]->playback_rate() >= 0.0;
        break;
      case Animation::Direction::REVERSE:
      case Animation::Direction::ALTERNATE_REVERSE:
        forward_direction = animations_[i]->playback_rate() < 0.0;
        break;
    }

    const TransformAnimationCurve* transform_animation_curve =
        animations_[i]->curve()->ToTransformAnimationCurve();
    float animation_start_scale = 0.f;
    if (!transform_animation_curve->AnimationStartScale(forward_direction,
                                                        &animation_start_scale))
      return false;
    *start_scale = std::max(*start_scale, animation_start_scale);
  }
  return true;
}

bool AnimationPlayer::MaximumTargetScale(ElementListType list_type,
                                         float* max_scale) const {
  *max_scale = 0.f;
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->is_finished() ||
        animations_[i]->target_property() != TargetProperty::TRANSFORM)
      continue;

    if ((list_type == ElementListType::ACTIVE &&
         !animations_[i]->affects_active_elements()) ||
        (list_type == ElementListType::PENDING &&
         !animations_[i]->affects_pending_elements()))
      continue;

    bool forward_direction = true;
    switch (animations_[i]->direction()) {
      case Animation::Direction::NORMAL:
      case Animation::Direction::ALTERNATE_NORMAL:
        forward_direction = animations_[i]->playback_rate() >= 0.0;
        break;
      case Animation::Direction::REVERSE:
      case Animation::Direction::ALTERNATE_REVERSE:
        forward_direction = animations_[i]->playback_rate() < 0.0;
        break;
    }

    const TransformAnimationCurve* transform_animation_curve =
        animations_[i]->curve()->ToTransformAnimationCurve();
    float animation_scale = 0.f;
    if (!transform_animation_curve->MaximumTargetScale(forward_direction,
                                                       &animation_scale))
      return false;
    *max_scale = std::max(*max_scale, animation_scale);
  }

  return true;
}

bool AnimationPlayer::IsPotentiallyAnimatingProperty(
    TargetProperty::Type target_property,
    ElementListType list_type) const {
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (!animations_[i]->is_finished() &&
        animations_[i]->target_property() == target_property) {
      if ((list_type == ElementListType::ACTIVE &&
           animations_[i]->affects_active_elements()) ||
          (list_type == ElementListType::PENDING &&
           animations_[i]->affects_pending_elements()))
        return true;
    }
  }
  return false;
}

bool AnimationPlayer::IsCurrentlyAnimatingProperty(
    TargetProperty::Type target_property,
    ElementListType list_type) const {
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (!animations_[i]->is_finished() &&
        animations_[i]->InEffect(last_tick_time_) &&
        animations_[i]->target_property() == target_property) {
      if ((list_type == ElementListType::ACTIVE &&
           animations_[i]->affects_active_elements()) ||
          (list_type == ElementListType::PENDING &&
           animations_[i]->affects_pending_elements()))
        return true;
    }
  }
  return false;
}

bool AnimationPlayer::HasElementInActiveList() const {
  DCHECK(element_animations_);
  return element_animations_->has_element_in_active_list();
}

gfx::ScrollOffset AnimationPlayer::ScrollOffsetForAnimation() const {
  DCHECK(element_animations_);
  return element_animations_->ScrollOffsetForAnimation();
}

Animation* AnimationPlayer::GetAnimation(
    TargetProperty::Type target_property) const {
  for (size_t i = 0; i < animations_.size(); ++i) {
    size_t index = animations_.size() - i - 1;
    if (animations_[index]->target_property() == target_property)
      return animations_[index].get();
  }
  return nullptr;
}

Animation* AnimationPlayer::GetAnimationById(int animation_id) const {
  for (size_t i = 0; i < animations_.size(); ++i)
    if (animations_[i]->id() == animation_id)
      return animations_[i].get();
  return nullptr;
}

void AnimationPlayer::GetPropertyAnimationState(
    PropertyAnimationState* pending_state,
    PropertyAnimationState* active_state) const {
  pending_state->Clear();
  active_state->Clear();

  for (const auto& animation : animations_) {
    if (!animation->is_finished()) {
      bool in_effect = animation->InEffect(last_tick_time_);
      bool active = animation->affects_active_elements();
      bool pending = animation->affects_pending_elements();
      TargetProperty::Type property = animation->target_property();

      if (pending)
        pending_state->potentially_animating[property] = true;
      if (pending && in_effect)
        pending_state->currently_running[property] = true;

      if (active)
        active_state->potentially_animating[property] = true;
      if (active && in_effect)
        active_state->currently_running[property] = true;
    }
  }
}

void AnimationPlayer::MarkAbortedAnimationsForDeletion(
    AnimationPlayer* animation_player_impl) const {
  bool animation_aborted = false;

  auto& animations_impl = animation_player_impl->animations_;
  for (const auto& animation_impl : animations_impl) {
    // If the animation has been aborted on the main thread, mark it for
    // deletion.
    if (Animation* animation = GetAnimationById(animation_impl->id())) {
      if (animation->run_state() == Animation::ABORTED) {
        animation_impl->SetRunState(Animation::WAITING_FOR_DELETION,
                                    animation_player_impl->last_tick_time_);
        animation->SetRunState(Animation::WAITING_FOR_DELETION,
                               last_tick_time_);
        animation_aborted = true;
      }
    }
  }

  if (element_animations_ && animation_aborted)
    element_animations_->SetNeedsUpdateImplClientState();
}

void AnimationPlayer::PurgeAnimationsMarkedForDeletion() {
  animations_.erase(
      std::remove_if(animations_.begin(), animations_.end(),
                     [](const std::unique_ptr<Animation>& animation) {
                       return animation->run_state() ==
                              Animation::WAITING_FOR_DELETION;
                     }),
      animations_.end());
}

void AnimationPlayer::PushNewAnimationsToImplThread(
    AnimationPlayer* animation_player_impl) const {
  // Any new animations owned by the main thread's AnimationPlayer are cloned
  // and added to the impl thread's AnimationPlayer.
  for (size_t i = 0; i < animations_.size(); ++i) {
    // If the animation is already running on the impl thread, there is no
    // need to copy it over.
    if (animation_player_impl->GetAnimationById(animations_[i]->id()))
      continue;

    if (animations_[i]->target_property() == TargetProperty::SCROLL_OFFSET &&
        !animations_[i]
             ->curve()
             ->ToScrollOffsetAnimationCurve()
             ->HasSetInitialValue()) {
      gfx::ScrollOffset current_scroll_offset;
      if (animation_player_impl->HasElementInActiveList()) {
        current_scroll_offset =
            animation_player_impl->ScrollOffsetForAnimation();
      } else {
        // The owning layer isn't yet in the active tree, so the main thread
        // scroll offset will be up to date.
        current_scroll_offset = ScrollOffsetForAnimation();
      }
      animations_[i]->curve()->ToScrollOffsetAnimationCurve()->SetInitialValue(
          current_scroll_offset);
    }

    // The new animation should be set to run as soon as possible.
    Animation::RunState initial_run_state =
        Animation::WAITING_FOR_TARGET_AVAILABILITY;
    std::unique_ptr<Animation> to_add(
        animations_[i]->CloneAndInitialize(initial_run_state));
    DCHECK(!to_add->needs_synchronized_start_time());
    to_add->set_affects_active_elements(false);
    animation_player_impl->AddAnimation(std::move(to_add));
  }
}

static bool IsCompleted(Animation* animation,
                        const AnimationPlayer* main_thread_player) {
  if (animation->is_impl_only()) {
    return (animation->run_state() == Animation::WAITING_FOR_DELETION);
  } else {
    return !main_thread_player->GetAnimationById(animation->id());
  }
}

void AnimationPlayer::RemoveAnimationsCompletedOnMainThread(
    AnimationPlayer* animation_player_impl) const {
  bool animation_completed = false;

  // Animations removed on the main thread should no longer affect pending
  // elements, and should stop affecting active elements after the next call
  // to ActivateAnimations. If already WAITING_FOR_DELETION, they can be removed
  // immediately.
  auto& animations = animation_player_impl->animations_;
  for (const auto& animation : animations) {
    if (IsCompleted(animation.get(), this)) {
      animation->set_affects_pending_elements(false);
      animation_completed = true;
    }
  }
  auto affects_active_only_and_is_waiting_for_deletion =
      [](const std::unique_ptr<Animation>& animation) {
        return animation->run_state() == Animation::WAITING_FOR_DELETION &&
               !animation->affects_pending_elements();
      };
  animations.erase(
      std::remove_if(animations.begin(), animations.end(),
                     affects_active_only_and_is_waiting_for_deletion),
      animations.end());

  if (element_animations_ && animation_completed)
    element_animations_->SetNeedsUpdateImplClientState();
}

void AnimationPlayer::PushPropertiesToImplThread(
    AnimationPlayer* animation_player_impl) {
  for (size_t i = 0; i < animations_.size(); ++i) {
    Animation* current_impl =
        animation_player_impl->GetAnimationById(animations_[i]->id());
    if (current_impl)
      animations_[i]->PushPropertiesTo(current_impl);
  }
}

}  // namespace cc
