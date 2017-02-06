// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_SCROLL_OFFSET_ANIMATIONS_IMPL_H_
#define CC_ANIMATION_SCROLL_OFFSET_ANIMATIONS_IMPL_H_

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "cc/animation/animation_delegate.h"
#include "cc/animation/scroll_offset_animation_curve.h"
#include "cc/trees/mutator_host_client.h"
#include "ui/gfx/geometry/scroll_offset.h"

namespace cc {

class AnimationPlayer;
class AnimationHost;
class AnimationTimeline;
class ElementAnimations;

// Contains an AnimationTimeline and its AnimationPlayer that owns the impl
// only scroll offset animations running on a particular CC Layer.
// We have just one player for impl-only scroll offset animations. I.e. only
// one element can have an impl-only scroll offset animation at any given time.
// Note that this class only exists on the compositor thread.
class CC_EXPORT ScrollOffsetAnimationsImpl : public AnimationDelegate {
 public:
  explicit ScrollOffsetAnimationsImpl(AnimationHost* animation_host);

  ~ScrollOffsetAnimationsImpl() override;

  void ScrollAnimationCreate(ElementId element_id,
                             const gfx::ScrollOffset& target_offset,
                             const gfx::ScrollOffset& current_offset);

  bool ScrollAnimationUpdateTarget(ElementId element_id,
                                   const gfx::Vector2dF& scroll_delta,
                                   const gfx::ScrollOffset& max_scroll_offset,
                                   base::TimeTicks frame_monotonic_time);

  // Aborts the currently running scroll offset animation on an element and
  // starts a new one offsetted by adjustment.
  void ScrollAnimationApplyAdjustment(ElementId element_id,
                                      const gfx::Vector2dF& adjustment);

  void ScrollAnimationAbort(bool needs_completion);

  // AnimationDelegate implementation.
  void NotifyAnimationStarted(base::TimeTicks monotonic_time,
                              TargetProperty::Type target_property,
                              int group) override {}
  void NotifyAnimationFinished(base::TimeTicks monotonic_time,
                               TargetProperty::Type target_property,
                               int group) override;
  void NotifyAnimationAborted(base::TimeTicks monotonic_time,
                              TargetProperty::Type target_property,
                              int group) override {}
  void NotifyAnimationTakeover(base::TimeTicks monotonic_time,
                               TargetProperty::Type target_property,
                               double animation_start_time,
                               std::unique_ptr<AnimationCurve> curve) override {
  }

 private:
  void ReattachScrollOffsetPlayerIfNeeded(ElementId element_id);

  AnimationHost* animation_host_;
  scoped_refptr<AnimationTimeline> scroll_offset_timeline_;

  // We have just one player for impl-only scroll offset animations.
  // I.e. only one element can have an impl-only scroll offset animation at
  // any given time.
  scoped_refptr<AnimationPlayer> scroll_offset_animation_player_;

  DISALLOW_COPY_AND_ASSIGN(ScrollOffsetAnimationsImpl);
};

}  // namespace cc

#endif  // CC_ANIMATION_SCROLL_OFFSET_ANIMATIONS_IMPL_H_
