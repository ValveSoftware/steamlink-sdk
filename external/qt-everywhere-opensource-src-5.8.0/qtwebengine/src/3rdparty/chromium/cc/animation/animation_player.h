// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_ANIMATION_PLAYER_H_
#define CC_ANIMATION_ANIMATION_PLAYER_H_

#include <vector>

#include "base/containers/linked_list.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "cc/animation/animation.h"
#include "cc/animation/animation_curve.h"
#include "cc/animation/element_animations.h"
#include "cc/base/cc_export.h"

namespace cc {

class AnimationDelegate;
class AnimationHost;
class AnimationTimeline;
class ElementAnimations;

// An AnimationPlayer owns all animations to be run on particular CC Layer.
// Multiple AnimationPlayers can be attached to one layer. In this case,
// they share common ElementAnimations so the
// ElementAnimations-to-Layer relationship is 1:1.
// For now, the blink logic is responsible for handling of conflicting
// same-property animations.
// Each AnimationPlayer has its copy on the impl thread.
// This is a CC counterpart for blink::AnimationPlayer (in 1:1 relationship).
class CC_EXPORT AnimationPlayer : public base::RefCounted<AnimationPlayer>,
                                  public base::LinkNode<AnimationPlayer> {
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
  void NotifyAnimationStarted(base::TimeTicks monotonic_time,
                              TargetProperty::Type target_property,
                              int group);
  void NotifyAnimationFinished(base::TimeTicks monotonic_time,
                               TargetProperty::Type target_property,
                               int group);
  void NotifyAnimationAborted(base::TimeTicks monotonic_time,
                              TargetProperty::Type target_property,
                              int group);
  void NotifyAnimationTakeover(base::TimeTicks monotonic_time,
                               TargetProperty::Type target_property,
                               double animation_start_time,
                               std::unique_ptr<AnimationCurve> curve);

  // Whether this player has animations waiting to get sent to ElementAnimations
  bool has_pending_animations_for_testing() const {
    return !animations_.empty();
  }

 private:
  friend class base::RefCounted<AnimationPlayer>;

  explicit AnimationPlayer(int id);
  ~AnimationPlayer();

  void SetNeedsCommit();

  void RegisterPlayer();
  void UnregisterPlayer();

  void BindElementAnimations();
  void UnbindElementAnimations();

  // We accumulate added animations in animations_ container
  // if element_animations_ is a nullptr. It allows us to add/remove animations
  // to non-attached AnimationPlayers.
  std::vector<std::unique_ptr<Animation>> animations_;

  AnimationHost* animation_host_;
  AnimationTimeline* animation_timeline_;
  // element_animations isn't null if player attached to an element (layer).
  scoped_refptr<ElementAnimations> element_animations_;
  AnimationDelegate* animation_delegate_;

  int id_;
  ElementId element_id_;

  DISALLOW_COPY_AND_ASSIGN(AnimationPlayer);
};

}  // namespace cc

#endif  // CC_ANIMATION_ANIMATION_PLAYER_H_
