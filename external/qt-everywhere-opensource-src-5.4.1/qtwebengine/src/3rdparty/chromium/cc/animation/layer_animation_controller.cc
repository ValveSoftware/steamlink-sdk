// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/layer_animation_controller.h"

#include <algorithm>

#include "cc/animation/animation.h"
#include "cc/animation/animation_delegate.h"
#include "cc/animation/animation_registrar.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "cc/animation/layer_animation_value_observer.h"
#include "cc/animation/layer_animation_value_provider.h"
#include "cc/animation/scroll_offset_animation_curve.h"
#include "cc/base/scoped_ptr_algorithm.h"
#include "cc/output/filter_operations.h"
#include "ui/gfx/box_f.h"
#include "ui/gfx/transform.h"

namespace cc {

LayerAnimationController::LayerAnimationController(int id)
    : registrar_(0),
      id_(id),
      is_active_(false),
      value_provider_(NULL),
      layer_animation_delegate_(NULL),
      needs_to_start_animations_(false) {
}

LayerAnimationController::~LayerAnimationController() {
  if (registrar_)
    registrar_->UnregisterAnimationController(this);
}

scoped_refptr<LayerAnimationController> LayerAnimationController::Create(
    int id) {
  return make_scoped_refptr(new LayerAnimationController(id));
}

void LayerAnimationController::PauseAnimation(int animation_id,
                                              base::TimeDelta time_offset) {
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->id() == animation_id) {
      animations_[i]->SetRunState(Animation::Paused,
                                  time_offset + animations_[i]->start_time());
    }
  }
}

struct HasAnimationId {
  explicit HasAnimationId(int id) : id_(id) {}
  bool operator()(Animation* animation) const {
    return animation->id() == id_;
  }

 private:
  int id_;
};

void LayerAnimationController::RemoveAnimation(int animation_id) {
  animations_.erase(cc::remove_if(&animations_,
                                  animations_.begin(),
                                  animations_.end(),
                                  HasAnimationId(animation_id)),
                    animations_.end());
  UpdateActivation(NormalActivation);
}

struct HasAnimationIdAndProperty {
  HasAnimationIdAndProperty(int id, Animation::TargetProperty target_property)
      : id_(id), target_property_(target_property) {}
  bool operator()(Animation* animation) const {
    return animation->id() == id_ &&
        animation->target_property() == target_property_;
  }

 private:
  int id_;
  Animation::TargetProperty target_property_;
};

void LayerAnimationController::RemoveAnimation(
    int animation_id,
    Animation::TargetProperty target_property) {
  animations_.erase(
      cc::remove_if(&animations_,
                    animations_.begin(),
                    animations_.end(),
                    HasAnimationIdAndProperty(animation_id, target_property)),
      animations_.end());
  UpdateActivation(NormalActivation);
}

void LayerAnimationController::AbortAnimations(
    Animation::TargetProperty target_property) {
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->target_property() == target_property &&
        !animations_[i]->is_finished())
      animations_[i]->SetRunState(Animation::Aborted, last_tick_time_);
  }
}

// Ensures that the list of active animations on the main thread and the impl
// thread are kept in sync.
void LayerAnimationController::PushAnimationUpdatesTo(
    LayerAnimationController* controller_impl) {
  DCHECK(this != controller_impl);
  if (!has_any_animation() && !controller_impl->has_any_animation())
    return;
  PurgeAnimationsMarkedForDeletion();
  PushNewAnimationsToImplThread(controller_impl);

  // Remove finished impl side animations only after pushing,
  // and only after the animations are deleted on the main thread
  // this insures we will never push an animation twice.
  RemoveAnimationsCompletedOnMainThread(controller_impl);

  PushPropertiesToImplThread(controller_impl);
  controller_impl->UpdateActivation(NormalActivation);
  UpdateActivation(NormalActivation);
}

void LayerAnimationController::Animate(base::TimeTicks monotonic_time) {
  DCHECK(!monotonic_time.is_null());
  if (!HasValueObserver())
    return;

  if (needs_to_start_animations_)
    StartAnimations(monotonic_time);
  TickAnimations(monotonic_time);
  last_tick_time_ = monotonic_time;
}

void LayerAnimationController::AccumulatePropertyUpdates(
    base::TimeTicks monotonic_time,
    AnimationEventsVector* events) {
  if (!events)
    return;

  for (size_t i = 0; i < animations_.size(); ++i) {
    Animation* animation = animations_[i];
    if (!animation->is_impl_only())
      continue;

    double trimmed = animation->TrimTimeToCurrentIteration(monotonic_time);
    switch (animation->target_property()) {
      case Animation::Opacity: {
        AnimationEvent event(AnimationEvent::PropertyUpdate,
                             id_,
                             animation->group(),
                             Animation::Opacity,
                             monotonic_time);
        event.opacity = animation->curve()->ToFloatAnimationCurve()->GetValue(
            trimmed);
        event.is_impl_only = true;
        events->push_back(event);
        break;
      }

      case Animation::Transform: {
        AnimationEvent event(AnimationEvent::PropertyUpdate,
                             id_,
                             animation->group(),
                             Animation::Transform,
                             monotonic_time);
        event.transform =
            animation->curve()->ToTransformAnimationCurve()->GetValue(trimmed);
        event.is_impl_only = true;
        events->push_back(event);
        break;
      }

      case Animation::Filter: {
        AnimationEvent event(AnimationEvent::PropertyUpdate,
                             id_,
                             animation->group(),
                             Animation::Filter,
                             monotonic_time);
        event.filters = animation->curve()->ToFilterAnimationCurve()->GetValue(
            trimmed);
        event.is_impl_only = true;
        events->push_back(event);
        break;
      }

      case Animation::BackgroundColor: { break; }

      case Animation::ScrollOffset: {
        // Impl-side changes to scroll offset are already sent back to the
        // main thread (e.g. for user-driven scrolling), so a PropertyUpdate
        // isn't needed.
        break;
      }

      case Animation::TargetPropertyEnumSize:
        NOTREACHED();
    }
  }
}

void LayerAnimationController::UpdateState(bool start_ready_animations,
                                           AnimationEventsVector* events) {
  if (!HasActiveValueObserver())
    return;

  DCHECK(last_tick_time_ != base::TimeTicks());
  if (start_ready_animations)
    PromoteStartedAnimations(last_tick_time_, events);

  MarkFinishedAnimations(last_tick_time_);
  MarkAnimationsForDeletion(last_tick_time_, events);

  if (needs_to_start_animations_ && start_ready_animations) {
    StartAnimations(last_tick_time_);
    PromoteStartedAnimations(last_tick_time_, events);
  }

  AccumulatePropertyUpdates(last_tick_time_, events);

  UpdateActivation(NormalActivation);
}

struct AffectsNoObservers {
  bool operator()(Animation* animation) const {
    return !animation->affects_active_observers() &&
           !animation->affects_pending_observers();
  }
};

void LayerAnimationController::ActivateAnimations() {
  for (size_t i = 0; i < animations_.size(); ++i) {
    animations_[i]->set_affects_active_observers(
        animations_[i]->affects_pending_observers());
  }
  animations_.erase(cc::remove_if(&animations_,
                                  animations_.begin(),
                                  animations_.end(),
                                  AffectsNoObservers()),
                    animations_.end());
  UpdateActivation(NormalActivation);
}

void LayerAnimationController::AddAnimation(scoped_ptr<Animation> animation) {
  animations_.push_back(animation.Pass());
  needs_to_start_animations_ = true;
  UpdateActivation(NormalActivation);
}

Animation* LayerAnimationController::GetAnimation(
    int group_id,
    Animation::TargetProperty target_property) const {
  for (size_t i = 0; i < animations_.size(); ++i)
    if (animations_[i]->group() == group_id &&
        animations_[i]->target_property() == target_property)
      return animations_[i];
  return 0;
}

Animation* LayerAnimationController::GetAnimation(
    Animation::TargetProperty target_property) const {
  for (size_t i = 0; i < animations_.size(); ++i) {
    size_t index = animations_.size() - i - 1;
    if (animations_[index]->target_property() == target_property)
      return animations_[index];
  }
  return 0;
}

bool LayerAnimationController::HasActiveAnimation() const {
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (!animations_[i]->is_finished())
      return true;
  }
  return false;
}

bool LayerAnimationController::IsAnimatingProperty(
    Animation::TargetProperty target_property) const {
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (!animations_[i]->is_finished() &&
        animations_[i]->target_property() == target_property)
      return true;
  }
  return false;
}

void LayerAnimationController::SetAnimationRegistrar(
    AnimationRegistrar* registrar) {
  if (registrar_ == registrar)
    return;

  if (registrar_)
    registrar_->UnregisterAnimationController(this);

  registrar_ = registrar;
  if (registrar_)
    registrar_->RegisterAnimationController(this);

  UpdateActivation(ForceActivation);
}

void LayerAnimationController::NotifyAnimationStarted(
    const AnimationEvent& event) {
  if (event.is_impl_only) {
    FOR_EACH_OBSERVER(LayerAnimationEventObserver, event_observers_,
                      OnAnimationStarted(event));
    if (layer_animation_delegate_)
      layer_animation_delegate_->NotifyAnimationStarted(event.monotonic_time,
                                                        event.target_property);
    return;
  }

  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->group() == event.group_id &&
        animations_[i]->target_property() == event.target_property &&
        animations_[i]->needs_synchronized_start_time()) {
      animations_[i]->set_needs_synchronized_start_time(false);
      if (!animations_[i]->has_set_start_time())
        animations_[i]->set_start_time(event.monotonic_time);

      FOR_EACH_OBSERVER(LayerAnimationEventObserver, event_observers_,
                        OnAnimationStarted(event));
      if (layer_animation_delegate_)
        layer_animation_delegate_->NotifyAnimationStarted(
            event.monotonic_time, event.target_property);

      return;
    }
  }
}

void LayerAnimationController::NotifyAnimationFinished(
    const AnimationEvent& event) {
  if (event.is_impl_only) {
    if (layer_animation_delegate_)
      layer_animation_delegate_->NotifyAnimationFinished(event.monotonic_time,
                                                         event.target_property);
    return;
  }

  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->group() == event.group_id &&
        animations_[i]->target_property() == event.target_property) {
      animations_[i]->set_received_finished_event(true);
      if (layer_animation_delegate_)
        layer_animation_delegate_->NotifyAnimationFinished(
            event.monotonic_time, event.target_property);

      return;
    }
  }
}

void LayerAnimationController::NotifyAnimationAborted(
    const AnimationEvent& event) {
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->group() == event.group_id &&
        animations_[i]->target_property() == event.target_property) {
      animations_[i]->SetRunState(Animation::Aborted, event.monotonic_time);
    }
  }
}

void LayerAnimationController::NotifyAnimationPropertyUpdate(
    const AnimationEvent& event) {
  bool notify_active_observers = true;
  bool notify_pending_observers = true;
  switch (event.target_property) {
    case Animation::Opacity:
      NotifyObserversOpacityAnimated(
          event.opacity, notify_active_observers, notify_pending_observers);
      break;
    case Animation::Transform:
      NotifyObserversTransformAnimated(
          event.transform, notify_active_observers, notify_pending_observers);
      break;
    default:
      NOTREACHED();
  }
}

void LayerAnimationController::AddValueObserver(
    LayerAnimationValueObserver* observer) {
  if (!value_observers_.HasObserver(observer))
    value_observers_.AddObserver(observer);
}

void LayerAnimationController::RemoveValueObserver(
    LayerAnimationValueObserver* observer) {
  value_observers_.RemoveObserver(observer);
}

void LayerAnimationController::AddEventObserver(
    LayerAnimationEventObserver* observer) {
  if (!event_observers_.HasObserver(observer))
    event_observers_.AddObserver(observer);
}

void LayerAnimationController::RemoveEventObserver(
    LayerAnimationEventObserver* observer) {
  event_observers_.RemoveObserver(observer);
}

bool LayerAnimationController::HasFilterAnimationThatInflatesBounds() const {
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (!animations_[i]->is_finished() &&
        animations_[i]->target_property() == Animation::Filter &&
        animations_[i]
            ->curve()
            ->ToFilterAnimationCurve()
            ->HasFilterThatMovesPixels())
      return true;
  }

  return false;
}

bool LayerAnimationController::HasTransformAnimationThatInflatesBounds() const {
  return IsAnimatingProperty(Animation::Transform);
}

bool LayerAnimationController::FilterAnimationBoundsForBox(
    const gfx::BoxF& box, gfx::BoxF* bounds) const {
  // TODO(avallee): Implement.
  return false;
}

bool LayerAnimationController::TransformAnimationBoundsForBox(
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
        animations_[i]->target_property() != Animation::Transform)
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

bool LayerAnimationController::HasAnimationThatAffectsScale() const {
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->is_finished() ||
        animations_[i]->target_property() != Animation::Transform)
      continue;

    const TransformAnimationCurve* transform_animation_curve =
        animations_[i]->curve()->ToTransformAnimationCurve();
    if (transform_animation_curve->AffectsScale())
      return true;
  }

  return false;
}

bool LayerAnimationController::HasOnlyTranslationTransforms() const {
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->is_finished() ||
        animations_[i]->target_property() != Animation::Transform)
      continue;

    const TransformAnimationCurve* transform_animation_curve =
        animations_[i]->curve()->ToTransformAnimationCurve();
    if (!transform_animation_curve->IsTranslation())
      return false;
  }

  return true;
}

bool LayerAnimationController::MaximumScale(float* max_scale) const {
  *max_scale = 0.f;
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->is_finished() ||
        animations_[i]->target_property() != Animation::Transform)
      continue;

    const TransformAnimationCurve* transform_animation_curve =
        animations_[i]->curve()->ToTransformAnimationCurve();
    float animation_scale = 0.f;
    if (!transform_animation_curve->MaximumScale(&animation_scale))
      return false;
    *max_scale = std::max(*max_scale, animation_scale);
  }

  return true;
}

void LayerAnimationController::PushNewAnimationsToImplThread(
    LayerAnimationController* controller_impl) const {
  // Any new animations owned by the main thread's controller are cloned and
  // add to the impl thread's controller.
  for (size_t i = 0; i < animations_.size(); ++i) {
    // If the animation is already running on the impl thread, there is no
    // need to copy it over.
    if (controller_impl->GetAnimation(animations_[i]->group(),
                                      animations_[i]->target_property()))
      continue;

    // If the animation is not running on the impl thread, it does not
    // necessarily mean that it needs to be copied over and started; it may
    // have already finished. In this case, the impl thread animation will
    // have already notified that it has started and the main thread animation
    // will no longer need
    // a synchronized start time.
    if (!animations_[i]->needs_synchronized_start_time())
      continue;

    // Scroll animations always start at the current scroll offset.
    if (animations_[i]->target_property() == Animation::ScrollOffset) {
      gfx::Vector2dF current_scroll_offset;
      if (controller_impl->value_provider_) {
        current_scroll_offset =
            controller_impl->value_provider_->ScrollOffsetForAnimation();
      } else {
        // The owning layer isn't yet in the active tree, so the main thread
        // scroll offset will be up-to-date.
        current_scroll_offset = value_provider_->ScrollOffsetForAnimation();
      }
      animations_[i]->curve()->ToScrollOffsetAnimationCurve()->SetInitialValue(
          current_scroll_offset);
    }

    // The new animation should be set to run as soon as possible.
    Animation::RunState initial_run_state =
        Animation::WaitingForTargetAvailability;
    scoped_ptr<Animation> to_add(
        animations_[i]->CloneAndInitialize(initial_run_state));
    DCHECK(!to_add->needs_synchronized_start_time());
    to_add->set_affects_active_observers(false);
    controller_impl->AddAnimation(to_add.Pass());
  }
}

static bool IsCompleted(
    Animation* animation,
    const LayerAnimationController* main_thread_controller) {
  if (animation->is_impl_only()) {
    return (animation->run_state() == Animation::WaitingForDeletion);
  } else {
    return !main_thread_controller->GetAnimation(animation->group(),
                                                 animation->target_property());
  }
}

static bool AffectsActiveOnlyAndIsWaitingForDeletion(Animation* animation) {
  return animation->run_state() == Animation::WaitingForDeletion &&
         !animation->affects_pending_observers();
}

void LayerAnimationController::RemoveAnimationsCompletedOnMainThread(
    LayerAnimationController* controller_impl) const {
  // Animations removed on the main thread should no longer affect pending
  // observers, and should stop affecting active observers after the next call
  // to ActivateAnimations. If already WaitingForDeletion, they can be removed
  // immediately.
  ScopedPtrVector<Animation>& animations = controller_impl->animations_;
  for (size_t i = 0; i < animations.size(); ++i) {
    if (IsCompleted(animations[i], this))
      animations[i]->set_affects_pending_observers(false);
  }
  animations.erase(cc::remove_if(&animations,
                                 animations.begin(),
                                 animations.end(),
                                 AffectsActiveOnlyAndIsWaitingForDeletion),
                   animations.end());
}

void LayerAnimationController::PushPropertiesToImplThread(
    LayerAnimationController* controller_impl) const {
  for (size_t i = 0; i < animations_.size(); ++i) {
    Animation* current_impl = controller_impl->GetAnimation(
        animations_[i]->group(), animations_[i]->target_property());
    if (current_impl)
      animations_[i]->PushPropertiesTo(current_impl);
  }
}

void LayerAnimationController::StartAnimations(base::TimeTicks monotonic_time) {
  DCHECK(needs_to_start_animations_);
  needs_to_start_animations_ = false;
  // First collect running properties affecting each type of observer.
  TargetProperties blocked_properties_for_active_observers;
  TargetProperties blocked_properties_for_pending_observers;
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->run_state() == Animation::Starting ||
        animations_[i]->run_state() == Animation::Running) {
      if (animations_[i]->affects_active_observers()) {
        blocked_properties_for_active_observers.insert(
            animations_[i]->target_property());
      }
      if (animations_[i]->affects_pending_observers()) {
        blocked_properties_for_pending_observers.insert(
            animations_[i]->target_property());
      }
    }
  }

  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->run_state() ==
        Animation::WaitingForTargetAvailability) {
      // Collect all properties for animations with the same group id (they
      // should all also be in the list of animations).
      TargetProperties enqueued_properties;
      bool affects_active_observers =
          animations_[i]->affects_active_observers();
      bool affects_pending_observers =
          animations_[i]->affects_pending_observers();
      enqueued_properties.insert(animations_[i]->target_property());
      for (size_t j = i + 1; j < animations_.size(); ++j) {
        if (animations_[i]->group() == animations_[j]->group()) {
          enqueued_properties.insert(animations_[j]->target_property());
          affects_active_observers |=
              animations_[j]->affects_active_observers();
          affects_pending_observers |=
              animations_[j]->affects_pending_observers();
        }
      }

      // Check to see if intersection of the list of properties affected by
      // the group and the list of currently blocked properties is null, taking
      // into account the type(s) of observers affected by the group. In any
      // case, the group's target properties need to be added to the lists of
      // blocked properties.
      bool null_intersection = true;
      for (TargetProperties::iterator p_iter = enqueued_properties.begin();
           p_iter != enqueued_properties.end();
           ++p_iter) {
        if (affects_active_observers &&
            !blocked_properties_for_active_observers.insert(*p_iter).second)
          null_intersection = false;
        if (affects_pending_observers &&
            !blocked_properties_for_pending_observers.insert(*p_iter).second)
          null_intersection = false;
      }

      // If the intersection is null, then we are free to start the animations
      // in the group.
      if (null_intersection) {
        animations_[i]->SetRunState(Animation::Starting, monotonic_time);
        for (size_t j = i + 1; j < animations_.size(); ++j) {
          if (animations_[i]->group() == animations_[j]->group()) {
            animations_[j]->SetRunState(Animation::Starting, monotonic_time);
          }
        }
      } else {
        needs_to_start_animations_ = true;
      }
    }
  }
}

void LayerAnimationController::PromoteStartedAnimations(
    base::TimeTicks monotonic_time,
    AnimationEventsVector* events) {
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->run_state() == Animation::Starting &&
        animations_[i]->affects_active_observers()) {
      animations_[i]->SetRunState(Animation::Running, monotonic_time);
      if (!animations_[i]->has_set_start_time() &&
          !animations_[i]->needs_synchronized_start_time())
        animations_[i]->set_start_time(monotonic_time);
      if (events) {
        AnimationEvent started_event(AnimationEvent::Started,
                                     id_,
                                     animations_[i]->group(),
                                     animations_[i]->target_property(),
                                     monotonic_time);
        started_event.is_impl_only = animations_[i]->is_impl_only();
        events->push_back(started_event);
      }
    }
  }
}

void LayerAnimationController::MarkFinishedAnimations(
    base::TimeTicks monotonic_time) {
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->IsFinishedAt(monotonic_time) &&
        animations_[i]->run_state() != Animation::Aborted &&
        animations_[i]->run_state() != Animation::WaitingForDeletion)
      animations_[i]->SetRunState(Animation::Finished, monotonic_time);
  }
}

void LayerAnimationController::MarkAnimationsForDeletion(
    base::TimeTicks monotonic_time,
    AnimationEventsVector* events) {
  bool marked_animations_for_deletions = false;

  // Non-aborted animations are marked for deletion after a corresponding
  // AnimationEvent::Finished event is sent or received. This means that if
  // we don't have an events vector, we must ensure that non-aborted animations
  // have received a finished event before marking them for deletion.
  for (size_t i = 0; i < animations_.size(); i++) {
    int group_id = animations_[i]->group();
    if (animations_[i]->run_state() == Animation::Aborted) {
      if (events && !animations_[i]->is_impl_only()) {
        AnimationEvent aborted_event(AnimationEvent::Aborted,
                                     id_,
                                     group_id,
                                     animations_[i]->target_property(),
                                     monotonic_time);
        events->push_back(aborted_event);
      }
      animations_[i]->SetRunState(Animation::WaitingForDeletion,
                                  monotonic_time);
      marked_animations_for_deletions = true;
      continue;
    }

    bool all_anims_with_same_id_are_finished = false;

    // Since deleting an animation on the main thread leads to its deletion
    // on the impl thread, we only mark a Finished main thread animation for
    // deletion once it has received a Finished event from the impl thread.
    bool animation_i_will_send_or_has_received_finish_event =
        events || animations_[i]->received_finished_event();
    // If an animation is finished, and not already marked for deletion,
    // find out if all other animations in the same group are also finished.
    if (animations_[i]->run_state() == Animation::Finished &&
        animation_i_will_send_or_has_received_finish_event) {
      all_anims_with_same_id_are_finished = true;
      for (size_t j = 0; j < animations_.size(); ++j) {
        bool animation_j_will_send_or_has_received_finish_event =
            events || animations_[j]->received_finished_event();
        if (group_id == animations_[j]->group() &&
            (!animations_[j]->is_finished() ||
             (animations_[j]->run_state() == Animation::Finished &&
              !animation_j_will_send_or_has_received_finish_event))) {
          all_anims_with_same_id_are_finished = false;
          break;
        }
      }
    }
    if (all_anims_with_same_id_are_finished) {
      // We now need to remove all animations with the same group id as
      // group_id (and send along animation finished notifications, if
      // necessary).
      for (size_t j = i; j < animations_.size(); j++) {
        if (animations_[j]->group() == group_id &&
            animations_[j]->run_state() != Animation::Aborted) {
          if (events) {
            AnimationEvent finished_event(AnimationEvent::Finished,
                                          id_,
                                          animations_[j]->group(),
                                          animations_[j]->target_property(),
                                          monotonic_time);
            finished_event.is_impl_only = animations_[j]->is_impl_only();
            events->push_back(finished_event);
          }
          animations_[j]->SetRunState(Animation::WaitingForDeletion,
                                      monotonic_time);
        }
      }
      marked_animations_for_deletions = true;
    }
  }
  if (marked_animations_for_deletions)
    NotifyObserversAnimationWaitingForDeletion();
}

static bool IsWaitingForDeletion(Animation* animation) {
  return animation->run_state() == Animation::WaitingForDeletion;
}

void LayerAnimationController::PurgeAnimationsMarkedForDeletion() {
  animations_.erase(cc::remove_if(&animations_,
                                  animations_.begin(),
                                  animations_.end(),
                                  IsWaitingForDeletion),
                    animations_.end());
}

void LayerAnimationController::TickAnimations(base::TimeTicks monotonic_time) {
  for (size_t i = 0; i < animations_.size(); ++i) {
    if (animations_[i]->run_state() == Animation::Starting ||
        animations_[i]->run_state() == Animation::Running ||
        animations_[i]->run_state() == Animation::Paused) {
      double trimmed =
          animations_[i]->TrimTimeToCurrentIteration(monotonic_time);

      switch (animations_[i]->target_property()) {
        case Animation::Transform: {
          const TransformAnimationCurve* transform_animation_curve =
              animations_[i]->curve()->ToTransformAnimationCurve();
          const gfx::Transform transform =
              transform_animation_curve->GetValue(trimmed);
          NotifyObserversTransformAnimated(
              transform,
              animations_[i]->affects_active_observers(),
              animations_[i]->affects_pending_observers());
          break;
        }

        case Animation::Opacity: {
          const FloatAnimationCurve* float_animation_curve =
              animations_[i]->curve()->ToFloatAnimationCurve();
          const float opacity = float_animation_curve->GetValue(trimmed);
          NotifyObserversOpacityAnimated(
              opacity,
              animations_[i]->affects_active_observers(),
              animations_[i]->affects_pending_observers());
          break;
        }

        case Animation::Filter: {
          const FilterAnimationCurve* filter_animation_curve =
              animations_[i]->curve()->ToFilterAnimationCurve();
          const FilterOperations filter =
              filter_animation_curve->GetValue(trimmed);
          NotifyObserversFilterAnimated(
              filter,
              animations_[i]->affects_active_observers(),
              animations_[i]->affects_pending_observers());
          break;
        }

        case Animation::BackgroundColor: {
          // Not yet implemented.
          break;
        }

        case Animation::ScrollOffset: {
          const ScrollOffsetAnimationCurve* scroll_offset_animation_curve =
              animations_[i]->curve()->ToScrollOffsetAnimationCurve();
          const gfx::Vector2dF scroll_offset =
              scroll_offset_animation_curve->GetValue(trimmed);
          NotifyObserversScrollOffsetAnimated(
              scroll_offset,
              animations_[i]->affects_active_observers(),
              animations_[i]->affects_pending_observers());
          break;
        }

        // Do nothing for sentinel value.
        case Animation::TargetPropertyEnumSize:
          NOTREACHED();
      }
    }
  }
}

void LayerAnimationController::UpdateActivation(UpdateActivationType type) {
  bool force = type == ForceActivation;
  if (registrar_) {
    bool was_active = is_active_;
    is_active_ = false;
    for (size_t i = 0; i < animations_.size(); ++i) {
      if (animations_[i]->run_state() != Animation::WaitingForDeletion) {
        is_active_ = true;
        break;
      }
    }

    if (is_active_ && (!was_active || force))
      registrar_->DidActivateAnimationController(this);
    else if (!is_active_ && (was_active || force))
      registrar_->DidDeactivateAnimationController(this);
  }
}

void LayerAnimationController::NotifyObserversOpacityAnimated(
    float opacity,
    bool notify_active_observers,
    bool notify_pending_observers) {
  if (value_observers_.might_have_observers()) {
    ObserverListBase<LayerAnimationValueObserver>::Iterator it(
        value_observers_);
    LayerAnimationValueObserver* obs;
    while ((obs = it.GetNext()) != NULL) {
      if ((notify_active_observers && obs->IsActive()) ||
          (notify_pending_observers && !obs->IsActive()))
        obs->OnOpacityAnimated(opacity);
    }
  }
}

void LayerAnimationController::NotifyObserversTransformAnimated(
    const gfx::Transform& transform,
    bool notify_active_observers,
    bool notify_pending_observers) {
  if (value_observers_.might_have_observers()) {
    ObserverListBase<LayerAnimationValueObserver>::Iterator it(
        value_observers_);
    LayerAnimationValueObserver* obs;
    while ((obs = it.GetNext()) != NULL) {
      if ((notify_active_observers && obs->IsActive()) ||
          (notify_pending_observers && !obs->IsActive()))
        obs->OnTransformAnimated(transform);
    }
  }
}

void LayerAnimationController::NotifyObserversFilterAnimated(
    const FilterOperations& filters,
    bool notify_active_observers,
    bool notify_pending_observers) {
  if (value_observers_.might_have_observers()) {
    ObserverListBase<LayerAnimationValueObserver>::Iterator it(
        value_observers_);
    LayerAnimationValueObserver* obs;
    while ((obs = it.GetNext()) != NULL) {
      if ((notify_active_observers && obs->IsActive()) ||
          (notify_pending_observers && !obs->IsActive()))
        obs->OnFilterAnimated(filters);
    }
  }
}

void LayerAnimationController::NotifyObserversScrollOffsetAnimated(
    const gfx::Vector2dF& scroll_offset,
    bool notify_active_observers,
    bool notify_pending_observers) {
  if (value_observers_.might_have_observers()) {
    ObserverListBase<LayerAnimationValueObserver>::Iterator it(
        value_observers_);
    LayerAnimationValueObserver* obs;
    while ((obs = it.GetNext()) != NULL) {
      if ((notify_active_observers && obs->IsActive()) ||
          (notify_pending_observers && !obs->IsActive()))
        obs->OnScrollOffsetAnimated(scroll_offset);
    }
  }
}

void LayerAnimationController::NotifyObserversAnimationWaitingForDeletion() {
  FOR_EACH_OBSERVER(LayerAnimationValueObserver,
                    value_observers_,
                    OnAnimationWaitingForDeletion());
}

bool LayerAnimationController::HasValueObserver() {
  if (value_observers_.might_have_observers()) {
    ObserverListBase<LayerAnimationValueObserver>::Iterator it(
        value_observers_);
    return it.GetNext() != NULL;
  }
  return false;
}

bool LayerAnimationController::HasActiveValueObserver() {
  if (value_observers_.might_have_observers()) {
    ObserverListBase<LayerAnimationValueObserver>::Iterator it(
        value_observers_);
    LayerAnimationValueObserver* obs;
    while ((obs = it.GetNext()) != NULL)
      if (obs->IsActive())
        return true;
  }
  return false;
}

}  // namespace cc
