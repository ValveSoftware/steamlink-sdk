// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/element_animations.h"

#include <stddef.h>

#include <algorithm>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "cc/animation/animation_delegate.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_player.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "cc/animation/scroll_offset_animation_curve.h"
#include "cc/output/filter_operations.h"
#include "cc/trees/mutator_host_client.h"
#include "ui/gfx/geometry/box_f.h"

namespace cc {

scoped_refptr<ElementAnimations> ElementAnimations::Create() {
  return make_scoped_refptr(new ElementAnimations());
}

ElementAnimations::ElementAnimations()
    : players_list_(new PlayersList()),
      animation_host_(),
      element_id_(),
      is_active_(false),
      has_element_in_active_list_(false),
      has_element_in_pending_list_(false),
      needs_to_start_animations_(false),
      scroll_offset_animation_was_interrupted_(false) {}

ElementAnimations::~ElementAnimations() {}

void ElementAnimations::SetAnimationHost(AnimationHost* host) {
  animation_host_ = host;
}

void ElementAnimations::SetElementId(ElementId element_id) {
  element_id_ = element_id;
}

void ElementAnimations::InitAffectedElementTypes() {
  DCHECK(element_id_);
  DCHECK(animation_host_);

  UpdateActivation(FORCE_ACTIVATION);

  DCHECK(animation_host_->mutator_host_client());
  if (animation_host_->mutator_host_client()->IsElementInList(
          element_id_, ElementListType::ACTIVE)) {
    set_has_element_in_active_list(true);
  }
  if (animation_host_->mutator_host_client()->IsElementInList(
          element_id_, ElementListType::PENDING)) {
    set_has_element_in_pending_list(true);
  }
}

void ElementAnimations::ClearAffectedElementTypes() {
  DCHECK(animation_host_);

  if (has_element_in_active_list()) {
    IsAnimatingChanged(ElementListType::ACTIVE, TargetProperty::TRANSFORM,
                       AnimationChangeType::BOTH, false);
    IsAnimatingChanged(ElementListType::ACTIVE, TargetProperty::OPACITY,
                       AnimationChangeType::BOTH, false);
  }
  set_has_element_in_active_list(false);

  if (has_element_in_pending_list()) {
    IsAnimatingChanged(ElementListType::PENDING, TargetProperty::TRANSFORM,
                       AnimationChangeType::BOTH, false);
    IsAnimatingChanged(ElementListType::PENDING, TargetProperty::OPACITY,
                       AnimationChangeType::BOTH, false);
  }
  set_has_element_in_pending_list(false);

  animation_host_->DidDeactivateElementAnimations(this);
  UpdateActivation(FORCE_ACTIVATION);
}

void ElementAnimations::ElementRegistered(ElementId element_id,
                                          ElementListType list_type) {
  DCHECK_EQ(element_id_, element_id);

  if (!has_element_in_any_list())
    UpdateActivation(FORCE_ACTIVATION);

  if (list_type == ElementListType::ACTIVE)
    set_has_element_in_active_list(true);
  else
    set_has_element_in_pending_list(true);
}

void ElementAnimations::ElementUnregistered(ElementId element_id,
                                            ElementListType list_type) {
  DCHECK_EQ(this->element_id(), element_id);
  if (list_type == ElementListType::ACTIVE)
    set_has_element_in_active_list(false);
  else
    set_has_element_in_pending_list(false);

  if (!has_element_in_any_list())
    animation_host_->DidDeactivateElementAnimations(this);
}

void ElementAnimations::AddPlayer(AnimationPlayer* player) {
  players_list_->Append(player);
}

void ElementAnimations::RemovePlayer(AnimationPlayer* player) {
  for (PlayersListNode* node = players_list_->head();
       node != players_list_->end(); node = node->next()) {
    if (node->value() == player) {
      node->RemoveFromList();
      return;
    }
  }
}

bool ElementAnimations::IsEmpty() const {
  return players_list_->empty();
}

void ElementAnimations::PushPropertiesTo(
    scoped_refptr<ElementAnimations> element_animations_impl) {
  DCHECK_NE(this, element_animations_impl);
  if (!has_any_animation() && !element_animations_impl->has_any_animation())
    return;
  MarkAbortedAnimationsForDeletion(element_animations_impl.get());
  PurgeAnimationsMarkedForDeletion();
  PushNewAnimationsToImplThread(element_animations_impl.get());

  // Remove finished impl side animations only after pushing,
  // and only after the animations are deleted on the main thread
  // this insures we will never push an animation twice.
  RemoveAnimationsCompletedOnMainThread(element_animations_impl.get());

  PushPropertiesToImplThread(element_animations_impl.get());
  element_animations_impl->UpdateActivation(NORMAL_ACTIVATION);
  UpdateActivation(NORMAL_ACTIVATION);
}

void ElementAnimations::AddAnimation(std::unique_ptr<Animation> animation) {
  DCHECK(!animation->is_impl_only() ||
         animation->target_property() == TargetProperty::SCROLL_OFFSET);
  bool added_transform_animation =
      animation->target_property() == TargetProperty::TRANSFORM;
  bool added_opacity_animation =
      animation->target_property() == TargetProperty::OPACITY;
  animations_.push_back(std::move(animation));
  needs_to_start_animations_ = true;
  UpdateActivation(NORMAL_ACTIVATION);
  if (added_transform_animation)
    UpdateClientAnimationState(TargetProperty::TRANSFORM);
  if (added_opacity_animation)
    UpdateClientAnimationState(TargetProperty::OPACITY);
}

void ElementAnimations::Animate(base::TimeTicks monotonic_time) {
  DCHECK(!monotonic_time.is_null());
  if (!has_element_in_active_list() && !has_element_in_pending_list())
    return;

  if (needs_to_start_animations_)
    StartAnimations(monotonic_time);
  TickAnimations(monotonic_time);
  last_tick_time_ = monotonic_time;
  UpdateClientAnimationState(TargetProperty::OPACITY);
  UpdateClientAnimationState(TargetProperty::TRANSFORM);
}

void ElementAnimations::UpdateState(bool start_ready_animations,
                                    AnimationEvents* events) {
  if (!has_element_in_active_list())
    return;

  // Animate hasn't been called, this happens if an element has been added
  // between the Commit and Draw phases.
  if (last_tick_time_ == base::TimeTicks())
    return;

  if (start_ready_animations)
    PromoteStartedAnimations(last_tick_time_, events);

  MarkFinishedAnimations(last_tick_time_);
  MarkAnimationsForDeletion(last_tick_time_, events);

  if (needs_to_start_animations_ && start_ready_animations) {
    StartAnimations(last_tick_time_);
    PromoteStartedAnimations(last_tick_time_, events);
  }

  UpdateActivation(NORMAL_ACTIVATION);
}

void ElementAnimations::ActivateAnimations() {
  bool changed_transform_animation = false;
  bool changed_opacity_animation = false;
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->affects_active_elements() !=
        animations_[i]->affects_pending_elements()) {
      if (animations_[i]->target_property() == TargetProperty::TRANSFORM)
        changed_transform_animation = true;
      else if (animations_[i]->target_property() == TargetProperty::OPACITY)
        changed_opacity_animation = true;
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
  scroll_offset_animation_was_interrupted_ = false;
  UpdateActivation(NORMAL_ACTIVATION);
  if (changed_transform_animation)
    UpdateClientAnimationState(TargetProperty::TRANSFORM);
  if (changed_opacity_animation)
    UpdateClientAnimationState(TargetProperty::OPACITY);
}

void ElementAnimations::NotifyAnimationStarted(const AnimationEvent& event) {
  if (event.is_impl_only) {
    NotifyPlayersAnimationStarted(event.monotonic_time, event.target_property,
                                  event.group_id);
    return;
  }

  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->group() == event.group_id &&
        animations_[i]->target_property() == event.target_property &&
        animations_[i]->needs_synchronized_start_time()) {
      animations_[i]->set_needs_synchronized_start_time(false);
      if (!animations_[i]->has_set_start_time())
        animations_[i]->set_start_time(event.monotonic_time);

      NotifyPlayersAnimationStarted(event.monotonic_time, event.target_property,
                                    event.group_id);

      return;
    }
  }
}

void ElementAnimations::NotifyAnimationFinished(const AnimationEvent& event) {
  if (event.is_impl_only) {
    NotifyPlayersAnimationFinished(event.monotonic_time, event.target_property,
                                   event.group_id);
    return;
  }

  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->group() == event.group_id &&
        animations_[i]->target_property() == event.target_property) {
      animations_[i]->set_received_finished_event(true);
      NotifyPlayersAnimationFinished(event.monotonic_time,
                                     event.target_property, event.group_id);

      return;
    }
  }
}

void ElementAnimations::NotifyAnimationTakeover(const AnimationEvent& event) {
  DCHECK(event.target_property == TargetProperty::SCROLL_OFFSET);
  if (!players_list_->empty()) {
    std::unique_ptr<AnimationCurve> animation_curve = event.curve->Clone();
    NotifyPlayersAnimationTakeover(event.monotonic_time, event.target_property,
                                   event.animation_start_time,
                                   std::move(animation_curve));
  }
}

void ElementAnimations::NotifyAnimationAborted(const AnimationEvent& event) {
  bool aborted_transform_animation = false;
  bool aborted_opacity_animation = false;
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->group() == event.group_id &&
        animations_[i]->target_property() == event.target_property) {
      animations_[i]->SetRunState(Animation::ABORTED, event.monotonic_time);
      animations_[i]->set_received_finished_event(true);
      NotifyPlayersAnimationAborted(event.monotonic_time, event.target_property,
                                    event.group_id);
      if (event.target_property == TargetProperty::TRANSFORM)
        aborted_transform_animation = true;
      else if (event.target_property == TargetProperty::OPACITY)
        aborted_opacity_animation = true;
      break;
    }
  }
  if (aborted_transform_animation)
    UpdateClientAnimationState(TargetProperty::TRANSFORM);
  if (aborted_opacity_animation)
    UpdateClientAnimationState(TargetProperty::OPACITY);
}

void ElementAnimations::NotifyAnimationPropertyUpdate(
    const AnimationEvent& event) {
  bool notify_active_elements = true;
  bool notify_pending_elements = true;
  switch (event.target_property) {
    case TargetProperty::OPACITY:
      NotifyClientOpacityAnimated(event.opacity, notify_active_elements,
                                  notify_pending_elements);
      break;
    case TargetProperty::TRANSFORM:
      NotifyClientTransformAnimated(event.transform, notify_active_elements,
                                    notify_pending_elements);
      break;
    default:
      NOTREACHED();
  }
}

bool ElementAnimations::HasFilterAnimationThatInflatesBounds() const {
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

bool ElementAnimations::HasTransformAnimationThatInflatesBounds() const {
  return IsCurrentlyAnimatingProperty(TargetProperty::TRANSFORM,
                                      ElementListType::ACTIVE) ||
         IsCurrentlyAnimatingProperty(TargetProperty::TRANSFORM,
                                      ElementListType::PENDING);
}

bool ElementAnimations::FilterAnimationBoundsForBox(const gfx::BoxF& box,
                                                    gfx::BoxF* bounds) const {
  // TODO(avallee): Implement.
  return false;
}

bool ElementAnimations::TransformAnimationBoundsForBox(
    const gfx::BoxF& box,
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

bool ElementAnimations::HasAnimationThatAffectsScale() const {
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->is_finished() ||
        animations_[i]->target_property() != TargetProperty::TRANSFORM)
      continue;

    const TransformAnimationCurve* transform_animation_curve =
        animations_[i]->curve()->ToTransformAnimationCurve();
    if (transform_animation_curve->AffectsScale())
      return true;
  }

  return false;
}

bool ElementAnimations::HasOnlyTranslationTransforms(
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

bool ElementAnimations::AnimationsPreserveAxisAlignment() const {
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

bool ElementAnimations::AnimationStartScale(ElementListType list_type,
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

bool ElementAnimations::MaximumTargetScale(ElementListType list_type,
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

void ElementAnimations::PushNewAnimationsToImplThread(
    ElementAnimations* element_animations_impl) const {
  // Any new animations owned by the main thread's ElementAnimations are cloned
  // and added to the impl thread's ElementAnimations.
  for (size_t i = 0; i < animations_.size(); ++i) {
    // If the animation is already running on the impl thread, there is no
    // need to copy it over.
    if (element_animations_impl->GetAnimationById(animations_[i]->id()))
      continue;

    if (animations_[i]->target_property() == TargetProperty::SCROLL_OFFSET &&
        !animations_[i]
             ->curve()
             ->ToScrollOffsetAnimationCurve()
             ->HasSetInitialValue()) {
      gfx::ScrollOffset current_scroll_offset;
      if (element_animations_impl->has_element_in_active_list()) {
        current_scroll_offset =
            element_animations_impl->ScrollOffsetForAnimation();
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
    element_animations_impl->AddAnimation(std::move(to_add));
  }
}

static bool IsCompleted(
    Animation* animation,
    const ElementAnimations* main_thread_element_animations) {
  if (animation->is_impl_only()) {
    return (animation->run_state() == Animation::WAITING_FOR_DELETION);
  } else {
    return !main_thread_element_animations->GetAnimationById(animation->id());
  }
}

void ElementAnimations::RemoveAnimationsCompletedOnMainThread(
    ElementAnimations* element_animations_impl) const {
  bool removed_transform_animation = false;
  bool removed_opacity_animation = false;
  // Animations removed on the main thread should no longer affect pending
  // elements, and should stop affecting active elements after the next call
  // to ActivateAnimations. If already WAITING_FOR_DELETION, they can be removed
  // immediately.
  auto& animations = element_animations_impl->animations_;
  for (const auto& animation : animations) {
    if (IsCompleted(animation.get(), this)) {
      animation->set_affects_pending_elements(false);
      if (animation->target_property() == TargetProperty::TRANSFORM)
        removed_transform_animation = true;
      else if (animation->target_property() == TargetProperty::OPACITY)
        removed_opacity_animation = true;
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

  if (removed_transform_animation)
    element_animations_impl->UpdateClientAnimationState(
        TargetProperty::TRANSFORM);
  if (removed_opacity_animation)
    element_animations_impl->UpdateClientAnimationState(
        TargetProperty::OPACITY);
}

void ElementAnimations::PushPropertiesToImplThread(
    ElementAnimations* element_animations_impl) {
  for (size_t i = 0; i < animations_.size(); ++i) {
    Animation* current_impl =
        element_animations_impl->GetAnimationById(animations_[i]->id());
    if (current_impl)
      animations_[i]->PushPropertiesTo(current_impl);
  }
  element_animations_impl->scroll_offset_animation_was_interrupted_ =
      scroll_offset_animation_was_interrupted_;
  scroll_offset_animation_was_interrupted_ = false;
}

void ElementAnimations::StartAnimations(base::TimeTicks monotonic_time) {
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
      static_assert(TargetProperty::FIRST_TARGET_PROPERTY == 0,
                    "TargetProperty must be 0-based enum");
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

void ElementAnimations::PromoteStartedAnimations(base::TimeTicks monotonic_time,
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
        if (started_event.is_impl_only)
          NotifyAnimationStarted(started_event);
        else
          events->events_.push_back(started_event);
      }
    }
  }
}

void ElementAnimations::MarkFinishedAnimations(base::TimeTicks monotonic_time) {
  bool finished_transform_animation = false;
  bool finished_opacity_animation = false;
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (!animations_[i]->is_finished() &&
        animations_[i]->IsFinishedAt(monotonic_time)) {
      animations_[i]->SetRunState(Animation::FINISHED, monotonic_time);
      if (animations_[i]->target_property() == TargetProperty::TRANSFORM)
        finished_transform_animation = true;
      else if (animations_[i]->target_property() == TargetProperty::OPACITY)
        finished_opacity_animation = true;
    }
  }
  if (finished_transform_animation)
    UpdateClientAnimationState(TargetProperty::TRANSFORM);
  if (finished_opacity_animation)
    UpdateClientAnimationState(TargetProperty::OPACITY);
}

void ElementAnimations::MarkAnimationsForDeletion(
    base::TimeTicks monotonic_time,
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
      NotifyPlayersAnimationFinished(aborted_event.monotonic_time,
                                     aborted_event.target_property,
                                     aborted_event.group_id);
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
          if (finished_event.is_impl_only)
            NotifyAnimationFinished(finished_event);
          else
            events->events_.push_back(finished_event);
        }
        animations_[animation_index]->SetRunState(
            Animation::WAITING_FOR_DELETION, monotonic_time);
      }
      marked_animations_for_deletions = true;
    }
  }
  if (marked_animations_for_deletions)
    NotifyClientAnimationWaitingForDeletion();
}

void ElementAnimations::MarkAbortedAnimationsForDeletion(
    ElementAnimations* element_animations_impl) const {
  bool aborted_transform_animation = false;
  bool aborted_opacity_animation = false;
  auto& animations_impl = element_animations_impl->animations_;
  for (const auto& animation_impl : animations_impl) {
    // If the animation has been aborted on the main thread, mark it for
    // deletion.
    if (Animation* animation = GetAnimationById(animation_impl->id())) {
      if (animation->run_state() == Animation::ABORTED) {
        animation_impl->SetRunState(Animation::WAITING_FOR_DELETION,
                                    element_animations_impl->last_tick_time_);
        animation->SetRunState(Animation::WAITING_FOR_DELETION,
                               last_tick_time_);
        if (animation_impl->target_property() == TargetProperty::TRANSFORM)
          aborted_transform_animation = true;
        else if (animation_impl->target_property() == TargetProperty::OPACITY)
          aborted_opacity_animation = true;
      }
    }
  }

  if (aborted_transform_animation)
    element_animations_impl->UpdateClientAnimationState(
        TargetProperty::TRANSFORM);
  if (aborted_opacity_animation)
    element_animations_impl->UpdateClientAnimationState(
        TargetProperty::OPACITY);
}

void ElementAnimations::PurgeAnimationsMarkedForDeletion() {
  animations_.erase(
      std::remove_if(animations_.begin(), animations_.end(),
                     [](const std::unique_ptr<Animation>& animation) {
                       return animation->run_state() ==
                              Animation::WAITING_FOR_DELETION;
                     }),
      animations_.end());
}

void ElementAnimations::TickAnimations(base::TimeTicks monotonic_time) {
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
          NotifyClientTransformAnimated(
              transform, animations_[i]->affects_active_elements(),
              animations_[i]->affects_pending_elements());
          break;
        }

        case TargetProperty::OPACITY: {
          const FloatAnimationCurve* float_animation_curve =
              animations_[i]->curve()->ToFloatAnimationCurve();
          const float opacity = std::max(
              std::min(float_animation_curve->GetValue(trimmed), 1.0f), 0.f);
          NotifyClientOpacityAnimated(
              opacity, animations_[i]->affects_active_elements(),
              animations_[i]->affects_pending_elements());
          break;
        }

        case TargetProperty::FILTER: {
          const FilterAnimationCurve* filter_animation_curve =
              animations_[i]->curve()->ToFilterAnimationCurve();
          const FilterOperations filter =
              filter_animation_curve->GetValue(trimmed);
          NotifyClientFilterAnimated(
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
          NotifyClientScrollOffsetAnimated(
              scroll_offset, animations_[i]->affects_active_elements(),
              animations_[i]->affects_pending_elements());
          break;
        }
      }
    }
  }
}

void ElementAnimations::UpdateActivation(UpdateActivationType type) {
  bool force = type == FORCE_ACTIVATION;
  if (animation_host_) {
    bool was_active = is_active_;
    is_active_ = false;
    for (size_t i = 0; i < animations_.size(); ++i) {
      if (animations_[i]->run_state() != Animation::WAITING_FOR_DELETION) {
        is_active_ = true;
        break;
      }
    }

    if (is_active_ && (!was_active || force))
      animation_host_->DidActivateElementAnimations(this);
    else if (!is_active_ && (was_active || force))
      animation_host_->DidDeactivateElementAnimations(this);
  }
}

void ElementAnimations::NotifyClientOpacityAnimated(
    float opacity,
    bool notify_active_elements,
    bool notify_pending_elements) {
  if (notify_active_elements && has_element_in_active_list())
    OnOpacityAnimated(ElementListType::ACTIVE, opacity);
  if (notify_pending_elements && has_element_in_pending_list())
    OnOpacityAnimated(ElementListType::PENDING, opacity);
}

void ElementAnimations::NotifyClientTransformAnimated(
    const gfx::Transform& transform,
    bool notify_active_elements,
    bool notify_pending_elements) {
  if (notify_active_elements && has_element_in_active_list())
    OnTransformAnimated(ElementListType::ACTIVE, transform);
  if (notify_pending_elements && has_element_in_pending_list())
    OnTransformAnimated(ElementListType::PENDING, transform);
}

void ElementAnimations::NotifyClientFilterAnimated(
    const FilterOperations& filters,
    bool notify_active_elements,
    bool notify_pending_elements) {
  if (notify_active_elements && has_element_in_active_list())
    OnFilterAnimated(ElementListType::ACTIVE, filters);
  if (notify_pending_elements && has_element_in_pending_list())
    OnFilterAnimated(ElementListType::PENDING, filters);
}

void ElementAnimations::NotifyClientScrollOffsetAnimated(
    const gfx::ScrollOffset& scroll_offset,
    bool notify_active_elements,
    bool notify_pending_elements) {
  if (notify_active_elements && has_element_in_active_list())
    OnScrollOffsetAnimated(ElementListType::ACTIVE, scroll_offset);
  if (notify_pending_elements && has_element_in_pending_list())
    OnScrollOffsetAnimated(ElementListType::PENDING, scroll_offset);
}

void ElementAnimations::NotifyClientAnimationWaitingForDeletion() {
  OnAnimationWaitingForDeletion();
}

void ElementAnimations::NotifyClientAnimationChanged(
    TargetProperty::Type property,
    ElementListType list_type,
    bool notify_elements_about_potential_animation,
    bool notify_elements_about_running_animation) {
  struct PropertyAnimationState* animation_state = nullptr;
  switch (property) {
    case TargetProperty::OPACITY:
      animation_state = &opacity_animation_state_;
      break;
    case TargetProperty::TRANSFORM:
      animation_state = &transform_animation_state_;
      break;
    default:
      NOTREACHED();
      break;
  }

  bool notify_elements_about_potential_and_running_animation =
      notify_elements_about_potential_animation &&
      notify_elements_about_running_animation;
  bool active = list_type == ElementListType::ACTIVE;
  if (notify_elements_about_potential_and_running_animation) {
    bool potentially_animating =
        active ? animation_state->potentially_animating_for_active_elements
               : animation_state->potentially_animating_for_pending_elements;
    bool currently_animating =
        active ? animation_state->currently_running_for_active_elements
               : animation_state->currently_running_for_pending_elements;
    DCHECK_EQ(potentially_animating, currently_animating);
    IsAnimatingChanged(list_type, property, AnimationChangeType::BOTH,
                       potentially_animating);
  } else if (notify_elements_about_potential_animation) {
    bool potentially_animating =
        active ? animation_state->potentially_animating_for_active_elements
               : animation_state->potentially_animating_for_pending_elements;
    IsAnimatingChanged(list_type, property, AnimationChangeType::POTENTIAL,
                       potentially_animating);
  } else if (notify_elements_about_running_animation) {
    bool currently_animating =
        active ? animation_state->currently_running_for_active_elements
               : animation_state->currently_running_for_pending_elements;
    IsAnimatingChanged(list_type, property, AnimationChangeType::RUNNING,
                       currently_animating);
  }
}

void ElementAnimations::UpdateClientAnimationState(
    TargetProperty::Type property) {
  struct PropertyAnimationState* animation_state = nullptr;
  switch (property) {
    case TargetProperty::OPACITY:
      animation_state = &opacity_animation_state_;
      break;
    case TargetProperty::TRANSFORM:
      animation_state = &transform_animation_state_;
      break;
    default:
      NOTREACHED();
      break;
  }
  bool was_currently_running_animation_for_active_elements =
      animation_state->currently_running_for_active_elements;
  bool was_currently_running_animation_for_pending_elements =
      animation_state->currently_running_for_pending_elements;
  bool was_potentially_animating_for_active_elements =
      animation_state->potentially_animating_for_active_elements;
  bool was_potentially_animating_for_pending_elements =
      animation_state->potentially_animating_for_pending_elements;

  animation_state->Clear();
  DCHECK(was_potentially_animating_for_active_elements ||
         !was_currently_running_animation_for_active_elements);
  DCHECK(was_potentially_animating_for_pending_elements ||
         !was_currently_running_animation_for_pending_elements);

  for (const auto& animation : animations_) {
    if (!animation->is_finished() && animation->target_property() == property) {
      animation_state->potentially_animating_for_active_elements |=
          animation->affects_active_elements();
      animation_state->potentially_animating_for_pending_elements |=
          animation->affects_pending_elements();
      animation_state->currently_running_for_active_elements =
          animation_state->potentially_animating_for_active_elements &&
          animation->InEffect(last_tick_time_);
      animation_state->currently_running_for_pending_elements =
          animation_state->potentially_animating_for_pending_elements &&
          animation->InEffect(last_tick_time_);
    }
  }

  bool potentially_animating_changed_for_active_elements =
      was_potentially_animating_for_active_elements !=
      animation_state->potentially_animating_for_active_elements;
  bool potentially_animating_changed_for_pending_elements =
      was_potentially_animating_for_pending_elements !=
      animation_state->potentially_animating_for_pending_elements;
  bool currently_running_animation_changed_for_active_elements =
      was_currently_running_animation_for_active_elements !=
      animation_state->currently_running_for_active_elements;
  bool currently_running_animation_changed_for_pending_elements =
      was_currently_running_animation_for_pending_elements !=
      animation_state->currently_running_for_pending_elements;
  if (!potentially_animating_changed_for_active_elements &&
      !potentially_animating_changed_for_pending_elements &&
      !currently_running_animation_changed_for_active_elements &&
      !currently_running_animation_changed_for_pending_elements)
    return;
  if (has_element_in_active_list())
    NotifyClientAnimationChanged(
        property, ElementListType::ACTIVE,
        potentially_animating_changed_for_active_elements,
        currently_running_animation_changed_for_active_elements);
  if (has_element_in_pending_list())
    NotifyClientAnimationChanged(
        property, ElementListType::PENDING,
        potentially_animating_changed_for_pending_elements,
        currently_running_animation_changed_for_pending_elements);
}

bool ElementAnimations::HasActiveAnimation() const {
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (!animations_[i]->is_finished())
      return true;
  }
  return false;
}

bool ElementAnimations::IsPotentiallyAnimatingProperty(
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

bool ElementAnimations::IsCurrentlyAnimatingProperty(
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

void ElementAnimations::PauseAnimation(int animation_id,
                                       base::TimeDelta time_offset) {
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->id() == animation_id) {
      animations_[i]->SetRunState(Animation::PAUSED,
                                  time_offset + animations_[i]->start_time() +
                                      animations_[i]->time_offset());
    }
  }
}

void ElementAnimations::RemoveAnimation(int animation_id) {
  bool removed_transform_animation = false;
  bool removed_opacity_animation = false;
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
      scroll_offset_animation_was_interrupted_ = true;
    } else if ((*it)->target_property() == TargetProperty::TRANSFORM &&
               !(*it)->is_finished()) {
      removed_transform_animation = true;
    } else if ((*it)->target_property() == TargetProperty::OPACITY &&
               !(*it)->is_finished()) {
      removed_opacity_animation = true;
    }
  }

  animations_.erase(animations_to_remove, animations_.end());
  UpdateActivation(NORMAL_ACTIVATION);
  if (removed_transform_animation)
    UpdateClientAnimationState(TargetProperty::TRANSFORM);
  if (removed_opacity_animation)
    UpdateClientAnimationState(TargetProperty::OPACITY);
}

void ElementAnimations::AbortAnimation(int animation_id) {
  bool aborted_transform_animation = false;
  bool aborted_opacity_animation = false;
  if (Animation* animation = GetAnimationById(animation_id)) {
    if (!animation->is_finished()) {
      animation->SetRunState(Animation::ABORTED, last_tick_time_);
      if (animation->target_property() == TargetProperty::TRANSFORM)
        aborted_transform_animation = true;
      else if (animation->target_property() == TargetProperty::OPACITY)
        aborted_opacity_animation = true;
    }
  }
  if (aborted_transform_animation)
    UpdateClientAnimationState(TargetProperty::TRANSFORM);
  if (aborted_opacity_animation)
    UpdateClientAnimationState(TargetProperty::OPACITY);
}

void ElementAnimations::AbortAnimations(TargetProperty::Type target_property,
                                        bool needs_completion) {
  if (needs_completion)
    DCHECK(target_property == TargetProperty::SCROLL_OFFSET);

  bool aborted_transform_animation = false;
  bool aborted_opacity_animation = false;
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
      if (target_property == TargetProperty::TRANSFORM)
        aborted_transform_animation = true;
      else if (target_property == TargetProperty::OPACITY)
        aborted_opacity_animation = true;
    }
  }
  if (aborted_transform_animation)
    UpdateClientAnimationState(TargetProperty::TRANSFORM);
  if (aborted_opacity_animation)
    UpdateClientAnimationState(TargetProperty::OPACITY);
}

Animation* ElementAnimations::GetAnimation(
    TargetProperty::Type target_property) const {
  for (size_t i = 0; i < animations_.size(); ++i) {
    size_t index = animations_.size() - i - 1;
    if (animations_[index]->target_property() == target_property)
      return animations_[index].get();
  }
  return nullptr;
}

Animation* ElementAnimations::GetAnimationById(int animation_id) const {
  for (size_t i = 0; i < animations_.size(); ++i)
    if (animations_[i]->id() == animation_id)
      return animations_[i].get();
  return nullptr;
}

void ElementAnimations::OnFilterAnimated(ElementListType list_type,
                                         const FilterOperations& filters) {
  DCHECK(element_id());
  DCHECK(animation_host());
  DCHECK(animation_host()->mutator_host_client());
  animation_host()->mutator_host_client()->SetElementFilterMutated(
      element_id(), list_type, filters);
}

void ElementAnimations::OnOpacityAnimated(ElementListType list_type,
                                          float opacity) {
  DCHECK(element_id());
  DCHECK(animation_host());
  DCHECK(animation_host()->mutator_host_client());
  animation_host()->mutator_host_client()->SetElementOpacityMutated(
      element_id(), list_type, opacity);
}

void ElementAnimations::OnTransformAnimated(ElementListType list_type,
                                            const gfx::Transform& transform) {
  DCHECK(element_id());
  DCHECK(animation_host());
  DCHECK(animation_host()->mutator_host_client());
  animation_host()->mutator_host_client()->SetElementTransformMutated(
      element_id(), list_type, transform);
}

void ElementAnimations::OnScrollOffsetAnimated(
    ElementListType list_type,
    const gfx::ScrollOffset& scroll_offset) {
  DCHECK(element_id());
  DCHECK(animation_host());
  DCHECK(animation_host()->mutator_host_client());
  animation_host()->mutator_host_client()->SetElementScrollOffsetMutated(
      element_id(), list_type, scroll_offset);
}

void ElementAnimations::OnAnimationWaitingForDeletion() {
  // TODO(loyso): Invalidate AnimationHost::SetNeedsPushProperties here.
  // But we always do PushProperties in AnimationHost for now. crbug.com/604280
  DCHECK(animation_host());
  animation_host()->OnAnimationWaitingForDeletion();
}

void ElementAnimations::IsAnimatingChanged(ElementListType list_type,
                                           TargetProperty::Type property,
                                           AnimationChangeType change_type,
                                           bool is_animating) {
  if (!element_id())
    return;
  DCHECK(animation_host());
  if (animation_host()->mutator_host_client()) {
    switch (property) {
      case TargetProperty::OPACITY:
        animation_host()
            ->mutator_host_client()
            ->ElementOpacityIsAnimatingChanged(element_id(), list_type,
                                               change_type, is_animating);
        break;
      case TargetProperty::TRANSFORM:
        animation_host()
            ->mutator_host_client()
            ->ElementTransformIsAnimatingChanged(element_id(), list_type,
                                                 change_type, is_animating);
        break;
      default:
        NOTREACHED();
        break;
    }
  }
}

void ElementAnimations::NotifyPlayersAnimationStarted(
    base::TimeTicks monotonic_time,
    TargetProperty::Type target_property,
    int group) {
  for (PlayersListNode* node = players_list_->head();
       node != players_list_->end(); node = node->next()) {
    AnimationPlayer* player = node->value();
    player->NotifyAnimationStarted(monotonic_time, target_property, group);
  }
}

void ElementAnimations::NotifyPlayersAnimationFinished(
    base::TimeTicks monotonic_time,
    TargetProperty::Type target_property,
    int group) {
  for (PlayersListNode* node = players_list_->head();
       node != players_list_->end(); node = node->next()) {
    AnimationPlayer* player = node->value();
    player->NotifyAnimationFinished(monotonic_time, target_property, group);
  }
}

void ElementAnimations::NotifyPlayersAnimationAborted(
    base::TimeTicks monotonic_time,
    TargetProperty::Type target_property,
    int group) {
  for (PlayersListNode* node = players_list_->head();
       node != players_list_->end(); node = node->next()) {
    AnimationPlayer* player = node->value();
    player->NotifyAnimationAborted(monotonic_time, target_property, group);
  }
}

void ElementAnimations::NotifyPlayersAnimationTakeover(
    base::TimeTicks monotonic_time,
    TargetProperty::Type target_property,
    double animation_start_time,
    std::unique_ptr<AnimationCurve> curve) {
  DCHECK(curve);
  for (PlayersListNode* node = players_list_->head();
       node != players_list_->end(); node = node->next()) {
    std::unique_ptr<AnimationCurve> animation_curve = curve->Clone();
    AnimationPlayer* player = node->value();
    player->NotifyAnimationTakeover(monotonic_time, target_property,
                                    animation_start_time,
                                    std::move(animation_curve));
  }
}

gfx::ScrollOffset ElementAnimations::ScrollOffsetForAnimation() const {
  if (animation_host()) {
    DCHECK(animation_host()->mutator_host_client());
    return animation_host()->mutator_host_client()->GetScrollOffsetForAnimation(
        element_id());
  }

  return gfx::ScrollOffset();
}

}  // namespace cc
