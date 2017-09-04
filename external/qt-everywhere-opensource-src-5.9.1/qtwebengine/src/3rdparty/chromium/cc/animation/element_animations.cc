// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/element_animations.h"

#include <stddef.h>

#include <algorithm>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "cc/animation/animation_delegate.h"
#include "cc/animation/animation_events.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_player.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "cc/output/filter_operations.h"
#include "cc/trees/mutator_host_client.h"
#include "ui/gfx/geometry/box_f.h"

namespace cc {

scoped_refptr<ElementAnimations> ElementAnimations::Create() {
  return make_scoped_refptr(new ElementAnimations());
}

ElementAnimations::ElementAnimations()
    : animation_host_(),
      element_id_(),
      is_active_(false),
      has_element_in_active_list_(false),
      has_element_in_pending_list_(false),
      scroll_offset_animation_was_interrupted_(false),
      needs_push_properties_(false),
      needs_update_impl_client_state_(false) {}

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

  UpdateActivation(ActivationType::FORCE);

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

TargetProperties ElementAnimations::GetPropertiesMaskForAnimationState() {
  TargetProperties properties;
  properties[TargetProperty::TRANSFORM] = true;
  properties[TargetProperty::OPACITY] = true;
  properties[TargetProperty::FILTER] = true;
  return properties;
}

void ElementAnimations::ClearAffectedElementTypes() {
  DCHECK(animation_host_);

  TargetProperties disable_properties = GetPropertiesMaskForAnimationState();
  PropertyAnimationState disabled_state_mask, disabled_state;
  disabled_state_mask.currently_running = disable_properties;
  disabled_state_mask.potentially_animating = disable_properties;

  if (has_element_in_active_list()) {
    animation_host()->mutator_host_client()->ElementIsAnimatingChanged(
        element_id(), ElementListType::ACTIVE, disabled_state_mask,
        disabled_state);
  }
  set_has_element_in_active_list(false);

  if (has_element_in_pending_list()) {
    animation_host()->mutator_host_client()->ElementIsAnimatingChanged(
        element_id(), ElementListType::PENDING, disabled_state_mask,
        disabled_state);
  }
  set_has_element_in_pending_list(false);

  animation_host_->DidDeactivateElementAnimations(this);
  UpdateActivation(ActivationType::FORCE);
}

void ElementAnimations::ElementRegistered(ElementId element_id,
                                          ElementListType list_type) {
  DCHECK_EQ(element_id_, element_id);

  if (!has_element_in_any_list())
    UpdateActivation(ActivationType::FORCE);

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
  players_list_.AddObserver(player);
}

void ElementAnimations::RemovePlayer(AnimationPlayer* player) {
  players_list_.RemoveObserver(player);
}

bool ElementAnimations::IsEmpty() const {
  return !players_list_.might_have_observers();
}

void ElementAnimations::SetNeedsPushProperties() {
  needs_push_properties_ = true;
}

void ElementAnimations::PushPropertiesTo(
    scoped_refptr<ElementAnimations> element_animations_impl) {
  DCHECK_NE(this, element_animations_impl);

  if (!needs_push_properties_)
    return;
  needs_push_properties_ = false;

  element_animations_impl->scroll_offset_animation_was_interrupted_ =
      scroll_offset_animation_was_interrupted_;
  scroll_offset_animation_was_interrupted_ = false;

  // Update impl client state.
  if (needs_update_impl_client_state_)
    element_animations_impl->UpdateClientAnimationState();
  needs_update_impl_client_state_ = false;

  element_animations_impl->UpdateActivation(ActivationType::NORMAL);
  UpdateActivation(ActivationType::NORMAL);
}

void ElementAnimations::Animate(base::TimeTicks monotonic_time) {
  DCHECK(!monotonic_time.is_null());
  if (!has_element_in_active_list() && !has_element_in_pending_list())
    return;

  for (auto& player : players_list_) {
    if (player.needs_to_start_animations())
      player.StartAnimations(monotonic_time);
  }

  for (auto& player : players_list_)
    player.TickAnimations(monotonic_time);

  last_tick_time_ = monotonic_time;
  UpdateClientAnimationState();
}

void ElementAnimations::UpdateState(bool start_ready_animations,
                                    AnimationEvents* events) {
  if (!has_element_in_active_list())
    return;

  // Animate hasn't been called, this happens if an element has been added
  // between the Commit and Draw phases.
  if (last_tick_time_ == base::TimeTicks())
    return;

  if (start_ready_animations) {
    for (auto& player : players_list_)
      player.PromoteStartedAnimations(last_tick_time_, events);
  }

  for (auto& player : players_list_)
    player.MarkFinishedAnimations(last_tick_time_);

  for (auto& player : players_list_)
    player.MarkAnimationsForDeletion(last_tick_time_, events);

  if (start_ready_animations) {
    for (auto& player : players_list_) {
      if (player.needs_to_start_animations()) {
        player.StartAnimations(last_tick_time_);
        player.PromoteStartedAnimations(last_tick_time_, events);
      }
    }
  }

  UpdateActivation(ActivationType::NORMAL);
}

void ElementAnimations::ActivateAnimations() {
  for (auto& player : players_list_)
    player.ActivateAnimations();

  scroll_offset_animation_was_interrupted_ = false;
  UpdateActivation(ActivationType::NORMAL);
}

void ElementAnimations::NotifyAnimationStarted(const AnimationEvent& event) {
  DCHECK(!event.is_impl_only);
  for (auto& player : players_list_) {
    if (player.NotifyAnimationStarted(event))
      break;
  }
}

void ElementAnimations::NotifyAnimationFinished(const AnimationEvent& event) {
  DCHECK(!event.is_impl_only);
  for (auto& player : players_list_) {
    if (player.NotifyAnimationFinished(event))
      break;
  }
}

void ElementAnimations::NotifyAnimationTakeover(const AnimationEvent& event) {
  DCHECK(!event.is_impl_only);
  DCHECK(event.target_property == TargetProperty::SCROLL_OFFSET);

  for (auto& player : players_list_)
    player.NotifyAnimationTakeover(event);
}

void ElementAnimations::NotifyAnimationAborted(const AnimationEvent& event) {
  DCHECK(!event.is_impl_only);

  for (auto& player : players_list_) {
    if (player.NotifyAnimationAborted(event))
      break;
  }

  UpdateClientAnimationState();
}

void ElementAnimations::NotifyAnimationPropertyUpdate(
    const AnimationEvent& event) {
  DCHECK(!event.is_impl_only);
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
  for (auto& player : players_list_) {
    if (player.HasFilterAnimationThatInflatesBounds())
      return true;
  }
  return false;
}

bool ElementAnimations::HasTransformAnimationThatInflatesBounds() const {
  for (auto& player : players_list_) {
    if (player.HasTransformAnimationThatInflatesBounds())
      return true;
  }
  return false;
}

bool ElementAnimations::FilterAnimationBoundsForBox(const gfx::BoxF& box,
                                                    gfx::BoxF* bounds) const {
  // TODO(avallee): Implement.
  return false;
}

bool ElementAnimations::TransformAnimationBoundsForBox(
    const gfx::BoxF& box,
    gfx::BoxF* bounds) const {
  *bounds = gfx::BoxF();

  for (auto& player : players_list_) {
    gfx::BoxF player_bounds;
    bool success = player.TransformAnimationBoundsForBox(box, &player_bounds);
    if (!success)
      return false;
    bounds->Union(player_bounds);
  }
  return true;
}

bool ElementAnimations::HasOnlyTranslationTransforms(
    ElementListType list_type) const {
  for (auto& player : players_list_) {
    if (!player.HasOnlyTranslationTransforms(list_type))
      return false;
  }
  return true;
}

bool ElementAnimations::AnimationsPreserveAxisAlignment() const {
  for (auto& player : players_list_) {
    if (!player.AnimationsPreserveAxisAlignment())
      return false;
  }
  return true;
}

bool ElementAnimations::AnimationStartScale(ElementListType list_type,
                                            float* start_scale) const {
  *start_scale = 0.f;

  for (auto& player : players_list_) {
    float player_start_scale = 0.f;
    bool success = player.AnimationStartScale(list_type, &player_start_scale);
    if (!success)
      return false;
    // Union: a maximum.
    *start_scale = std::max(*start_scale, player_start_scale);
  }

  return true;
}

bool ElementAnimations::MaximumTargetScale(ElementListType list_type,
                                           float* max_scale) const {
  *max_scale = 0.f;

  for (auto& player : players_list_) {
    float player_max_scale = 0.f;
    bool success = player.MaximumTargetScale(list_type, &player_max_scale);
    if (!success)
      return false;
    // Union: a maximum.
    *max_scale = std::max(*max_scale, player_max_scale);
  }

  return true;
}

void ElementAnimations::SetNeedsUpdateImplClientState() {
  needs_update_impl_client_state_ = true;
  SetNeedsPushProperties();
}

void ElementAnimations::UpdateActivation(ActivationType type) {
  bool force = type == ActivationType::FORCE;
  if (animation_host_) {
    bool was_active = is_active_;
    is_active_ = false;

    for (auto& player : players_list_) {
      if (player.HasNonDeletedAnimation()) {
        is_active_ = true;
        break;
      }
    }

    if (is_active_ && (!was_active || force)) {
      animation_host_->DidActivateElementAnimations(this);
    } else if (!is_active_ && (was_active || force)) {
      // Resetting last_tick_time_ here ensures that calling ::UpdateState
      // before ::Animate doesn't start an animation.
      last_tick_time_ = base::TimeTicks();
      animation_host_->DidDeactivateElementAnimations(this);
    }
  }
}

void ElementAnimations::UpdateActivationNormal() {
  UpdateActivation(ActivationType::NORMAL);
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

void ElementAnimations::UpdateClientAnimationState() {
  if (!element_id())
    return;
  DCHECK(animation_host());
  if (!animation_host()->mutator_host_client())
    return;

  PropertyAnimationState prev_pending = pending_state_;
  PropertyAnimationState prev_active = active_state_;

  pending_state_.Clear();
  active_state_.Clear();

  for (auto& player : players_list_) {
    PropertyAnimationState player_pending_state, player_active_state;
    player.GetPropertyAnimationState(&player_pending_state,
                                     &player_active_state);
    pending_state_ |= player_pending_state;
    active_state_ |= player_active_state;
  }

  TargetProperties allowed_properties = GetPropertiesMaskForAnimationState();
  PropertyAnimationState allowed_state;
  allowed_state.currently_running = allowed_properties;
  allowed_state.potentially_animating = allowed_properties;

  pending_state_ &= allowed_state;
  active_state_ &= allowed_state;

  DCHECK(pending_state_.IsValid());
  DCHECK(active_state_.IsValid());

  if (has_element_in_active_list() && prev_active != active_state_) {
    PropertyAnimationState diff_active = prev_active ^ active_state_;
    animation_host()->mutator_host_client()->ElementIsAnimatingChanged(
        element_id(), ElementListType::ACTIVE, diff_active, active_state_);
  }
  if (has_element_in_pending_list() && prev_pending != pending_state_) {
    PropertyAnimationState diff_pending = prev_pending ^ pending_state_;
    animation_host()->mutator_host_client()->ElementIsAnimatingChanged(
        element_id(), ElementListType::PENDING, diff_pending, pending_state_);
  }
}

bool ElementAnimations::HasActiveAnimation() const {
  for (auto& player : players_list_) {
    if (player.HasActiveAnimation())
      return true;
  }

  return false;
}

bool ElementAnimations::HasAnyAnimation() const {
  for (auto& player : players_list_) {
    if (player.has_any_animation())
      return true;
  }

  return false;
}

bool ElementAnimations::HasAnyAnimationTargetingProperty(
    TargetProperty::Type property) const {
  for (auto& player : players_list_) {
    if (player.GetAnimation(property))
      return true;
  }
  return false;
}

bool ElementAnimations::IsPotentiallyAnimatingProperty(
    TargetProperty::Type target_property,
    ElementListType list_type) const {
  for (auto& player : players_list_) {
    if (player.IsPotentiallyAnimatingProperty(target_property, list_type))
      return true;
  }

  return false;
}

bool ElementAnimations::IsCurrentlyAnimatingProperty(
    TargetProperty::Type target_property,
    ElementListType list_type) const {
  for (auto& player : players_list_) {
    if (player.IsCurrentlyAnimatingProperty(target_property, list_type))
      return true;
  }

  return false;
}

void ElementAnimations::SetScrollOffsetAnimationWasInterrupted() {
  scroll_offset_animation_was_interrupted_ = true;
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

gfx::ScrollOffset ElementAnimations::ScrollOffsetForAnimation() const {
  if (animation_host()) {
    DCHECK(animation_host()->mutator_host_client());
    return animation_host()->mutator_host_client()->GetScrollOffsetForAnimation(
        element_id());
  }

  return gfx::ScrollOffset();
}

}  // namespace cc
