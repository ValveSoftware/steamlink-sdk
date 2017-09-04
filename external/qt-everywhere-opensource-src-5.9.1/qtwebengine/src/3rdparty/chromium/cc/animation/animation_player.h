// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_ANIMATION_PLAYER_H_
#define CC_ANIMATION_ANIMATION_PLAYER_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "cc/animation/animation.h"
#include "cc/animation/animation_curve.h"
#include "cc/animation/element_animations.h"
#include "cc/base/cc_export.h"
#include "cc/trees/element_id.h"

namespace cc {

class AnimationDelegate;
class AnimationHost;
class AnimationTimeline;
struct AnimationEvent;
struct PropertyAnimationState;

// An AnimationPlayer owns all animations to be run on particular CC Layer.
// Multiple AnimationPlayers can be attached to one layer. In this case,
// they share common ElementAnimations so the
// ElementAnimations-to-Layer relationship is 1:1.
// For now, the blink logic is responsible for handling of conflicting
// same-property animations.
// Each AnimationPlayer has its copy on the impl thread.
// This is a CC counterpart for blink::AnimationPlayer (in 1:1 relationship).
class CC_EXPORT AnimationPlayer : public base::RefCounted<AnimationPlayer> {
 public:
  static scoped_refptr<AnimationPlayer> Create(int id);
  scoped_refptr<AnimationPlayer> CreateImplInstance() const;

  int id() const { return id_; }
  ElementId element_id() const { return element_id_; }

  // Parent AnimationHost. AnimationPlayer can be detached from
  // AnimationTimeline.
  AnimationHost* animation_host() { return animation_host_; }
  const AnimationHost* animation_host() const { return animation_host_; }
  void SetAnimationHost(AnimationHost* animation_host);

  // Parent AnimationTimeline.
  AnimationTimeline* animation_timeline() { return animation_timeline_; }
  const AnimationTimeline* animation_timeline() const {
    return animation_timeline_;
  }
  void SetAnimationTimeline(AnimationTimeline* timeline);

  // ElementAnimations object where this player is listed.
  scoped_refptr<ElementAnimations> element_animations() const {
    return element_animations_;
  }

  void set_animation_delegate(AnimationDelegate* delegate) {
    animation_delegate_ = delegate;
  }

  void AttachElement(ElementId element_id);
  void DetachElement();

  void AddAnimation(std::unique_ptr<Animation> animation);
  void PauseAnimation(int animation_id, double time_offset);
  void RemoveAnimation(int animation_id);
  void AbortAnimation(int animation_id);
  void AbortAnimations(TargetProperty::Type target_property,
                       bool needs_completion);

  void PushPropertiesTo(AnimationPlayer* player_impl);

  // AnimationDelegate routing.
  bool NotifyAnimationStarted(const AnimationEvent& event);
  bool NotifyAnimationFinished(const AnimationEvent& event);
  bool NotifyAnimationAborted(const AnimationEvent& event);
  void NotifyAnimationTakeover(const AnimationEvent& event);
  bool NotifyAnimationFinishedForTesting(TargetProperty::Type target_property,
                                         int group_id);

  // Returns true if there are any animations that have neither finished nor
  // aborted.
  bool HasActiveAnimation() const;

  // Returns true if there are any animations at all to process.
  bool has_any_animation() const { return !animations_.empty(); }

  bool needs_push_properties() const { return needs_push_properties_; }
  void SetNeedsPushProperties();

  bool HasNonDeletedAnimation() const;

  bool needs_to_start_animations() const { return needs_to_start_animations_; }

  void StartAnimations(base::TimeTicks monotonic_time);
  void PromoteStartedAnimations(base::TimeTicks monotonic_time,
                                AnimationEvents* events);
  void MarkAnimationsForDeletion(base::TimeTicks monotonic_time,
                                 AnimationEvents* events);
  void TickAnimations(base::TimeTicks monotonic_time);
  void MarkFinishedAnimations(base::TimeTicks monotonic_time);

  // Make animations affect active elements if and only if they affect
  // pending elements. Any animations that no longer affect any elements
  // are deleted.
  void ActivateAnimations();

  bool HasFilterAnimationThatInflatesBounds() const;
  bool HasTransformAnimationThatInflatesBounds() const;

  bool TransformAnimationBoundsForBox(const gfx::BoxF& box,
                                      gfx::BoxF* bounds) const;
  bool HasOnlyTranslationTransforms(ElementListType list_type) const;
  bool AnimationsPreserveAxisAlignment() const;

  // Sets |start_scale| to the maximum of starting animation scale along any
  // dimension at any destination in active animations. Returns false if the
  // starting scale cannot be computed.
  bool AnimationStartScale(ElementListType list_type, float* start_scale) const;

  // Sets |max_scale| to the maximum scale along any dimension at any
  // destination in active animations. Returns false if the maximum scale cannot
  // be computed.
  bool MaximumTargetScale(ElementListType list_type, float* max_scale) const;

  // Returns true if there is an animation that is either currently animating
  // the given property or scheduled to animate this property in the future, and
  // that affects the given tree type.
  bool IsPotentiallyAnimatingProperty(TargetProperty::Type target_property,
                                      ElementListType list_type) const;

  // Returns true if there is an animation that is currently animating the given
  // property and that affects the given tree type.
  bool IsCurrentlyAnimatingProperty(TargetProperty::Type target_property,
                                    ElementListType list_type) const;

  bool HasElementInActiveList() const;
  gfx::ScrollOffset ScrollOffsetForAnimation() const;

  // Returns the active animation animating the given property that is either
  // running, or is next to run, if such an animation exists.
  Animation* GetAnimation(TargetProperty::Type target_property) const;

  // Returns the active animation for the given unique animation id.
  Animation* GetAnimationById(int animation_id) const;

  void GetPropertyAnimationState(PropertyAnimationState* pending_state,
                                 PropertyAnimationState* active_state) const;

 private:
  friend class base::RefCounted<AnimationPlayer>;

  explicit AnimationPlayer(int id);
  ~AnimationPlayer();

  void SetNeedsCommit();

  void RegisterPlayer();
  void UnregisterPlayer();

  void BindElementAnimations();
  void UnbindElementAnimations();

  void AnimationAdded();

  void MarkAbortedAnimationsForDeletion(
      AnimationPlayer* animation_player_impl) const;
  void PurgeAnimationsMarkedForDeletion();
  void PushNewAnimationsToImplThread(
      AnimationPlayer* animation_player_impl) const;
  void RemoveAnimationsCompletedOnMainThread(
      AnimationPlayer* animation_player_impl) const;
  void PushPropertiesToImplThread(AnimationPlayer* animation_player_impl);

  using Animations = std::vector<std::unique_ptr<Animation>>;
  Animations animations_;

  AnimationHost* animation_host_;
  AnimationTimeline* animation_timeline_;
  // element_animations isn't null if player attached to an element (layer).
  scoped_refptr<ElementAnimations> element_animations_;
  AnimationDelegate* animation_delegate_;

  int id_;
  ElementId element_id_;
  bool needs_push_properties_;
  base::TimeTicks last_tick_time_;

  // Only try to start animations when new animations are added or when the
  // previous attempt at starting animations failed to start all animations.
  bool needs_to_start_animations_;

  DISALLOW_COPY_AND_ASSIGN(AnimationPlayer);
};

}  // namespace cc

#endif  // CC_ANIMATION_ANIMATION_PLAYER_H_
