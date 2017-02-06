// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_ANIMATION_HOST_H_
#define CC_ANIMATION_ANIMATION_HOST_H_

#include <memory>
#include <unordered_map>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "cc/animation/animation.h"
#include "cc/base/cc_export.h"
#include "cc/trees/mutator_host_client.h"
#include "ui/gfx/geometry/box_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace gfx {
class ScrollOffset;
}

namespace cc {

class AnimationEvents;
class AnimationPlayer;
class AnimationTimeline;
class ElementAnimations;
class LayerTreeHost;
class ScrollOffsetAnimations;
class ScrollOffsetAnimationsImpl;

enum class ThreadInstance { MAIN, IMPL };

// An AnimationHost contains all the state required to play animations.
// Specifically, it owns all the AnimationTimelines objects.
// There is just one AnimationHost for LayerTreeHost on main renderer thread
// and just one AnimationHost for LayerTreeHostImpl on impl thread.
// We synchronize them during the commit process in a one-way data flow process
// (PushPropertiesTo).
// An AnimationHost talks to its correspondent LayerTreeHost via
// LayerTreeMutatorsClient interface.
class CC_EXPORT AnimationHost {
 public:
  using ElementToAnimationsMap =
      std::unordered_map<ElementId,
                         scoped_refptr<ElementAnimations>,
                         ElementIdHash>;

  static std::unique_ptr<AnimationHost> CreateMainInstance();
  static std::unique_ptr<AnimationHost> CreateForTesting(
      ThreadInstance thread_instance);
  std::unique_ptr<AnimationHost> CreateImplInstance(
      bool supports_impl_scrolling) const;
  ~AnimationHost();

  void AddAnimationTimeline(scoped_refptr<AnimationTimeline> timeline);
  void RemoveAnimationTimeline(scoped_refptr<AnimationTimeline> timeline);
  AnimationTimeline* GetTimelineById(int timeline_id) const;

  void ClearTimelines();

  void RegisterElement(ElementId element_id, ElementListType list_type);
  void UnregisterElement(ElementId element_id, ElementListType list_type);

  void RegisterPlayerForElement(ElementId element_id, AnimationPlayer* player);
  void UnregisterPlayerForElement(ElementId element_id,
                                  AnimationPlayer* player);

  scoped_refptr<ElementAnimations> GetElementAnimationsForElementId(
      ElementId element_id) const;

  // Parent LayerTreeHost or LayerTreeHostImpl.
  MutatorHostClient* mutator_host_client() { return mutator_host_client_; }
  const MutatorHostClient* mutator_host_client() const {
    return mutator_host_client_;
  }
  void SetMutatorHostClient(MutatorHostClient* client);

  void SetNeedsCommit();
  void SetNeedsRebuildPropertyTrees();

  void PushPropertiesTo(AnimationHost* host_impl);

  void SetSupportsScrollAnimations(bool supports_scroll_animations);
  bool SupportsScrollAnimations() const;
  bool NeedsAnimateLayers() const;

  bool ActivateAnimations();
  bool AnimateLayers(base::TimeTicks monotonic_time);
  bool UpdateAnimationState(bool start_ready_animations,
                            AnimationEvents* events);

  std::unique_ptr<AnimationEvents> CreateEvents();
  void SetAnimationEvents(std::unique_ptr<AnimationEvents> events);

  bool ScrollOffsetAnimationWasInterrupted(ElementId element_id) const;

  bool IsAnimatingFilterProperty(ElementId element_id,
                                 ElementListType list_type) const;
  bool IsAnimatingOpacityProperty(ElementId element_id,
                                  ElementListType list_type) const;
  bool IsAnimatingTransformProperty(ElementId element_id,
                                    ElementListType list_type) const;

  bool HasPotentiallyRunningFilterAnimation(ElementId element_id,
                                            ElementListType list_type) const;
  bool HasPotentiallyRunningOpacityAnimation(ElementId element_id,
                                             ElementListType list_type) const;
  bool HasPotentiallyRunningTransformAnimation(ElementId element_id,
                                               ElementListType list_type) const;

  bool HasAnyAnimationTargetingProperty(ElementId element_id,
                                        TargetProperty::Type property) const;

  bool HasFilterAnimationThatInflatesBounds(ElementId element_id) const;
  bool HasTransformAnimationThatInflatesBounds(ElementId element_id) const;
  bool HasAnimationThatInflatesBounds(ElementId element_id) const;

  bool FilterAnimationBoundsForBox(ElementId element_id,
                                   const gfx::BoxF& box,
                                   gfx::BoxF* bounds) const;
  bool TransformAnimationBoundsForBox(ElementId element_id,
                                      const gfx::BoxF& box,
                                      gfx::BoxF* bounds) const;

  bool HasOnlyTranslationTransforms(ElementId element_id,
                                    ElementListType list_type) const;
  bool AnimationsPreserveAxisAlignment(ElementId element_id) const;

  bool MaximumTargetScale(ElementId element_id,
                          ElementListType list_type,
                          float* max_scale) const;
  bool AnimationStartScale(ElementId element_id,
                           ElementListType list_type,
                           float* start_scale) const;

  bool HasAnyAnimation(ElementId element_id) const;
  bool HasActiveAnimationForTesting(ElementId element_id) const;

  void ImplOnlyScrollAnimationCreate(ElementId element_id,
                                     const gfx::ScrollOffset& target_offset,
                                     const gfx::ScrollOffset& current_offset);
  bool ImplOnlyScrollAnimationUpdateTarget(
      ElementId element_id,
      const gfx::Vector2dF& scroll_delta,
      const gfx::ScrollOffset& max_scroll_offset,
      base::TimeTicks frame_monotonic_time);

  void ScrollAnimationAbort(bool needs_completion);

  // This should only be called from the main thread.
  ScrollOffsetAnimations& scroll_offset_animations() const;

  // Registers the given element animations as active. An active element
  // animations is one that has a running animation that needs to be ticked.
  void DidActivateElementAnimations(ElementAnimations* element_animations);

  // Unregisters the given element animations. When this happens, the
  // element animations will no longer be ticked (since it's not active).
  void DidDeactivateElementAnimations(ElementAnimations* element_animations);

  // Registers the given ElementAnimations as alive.
  void RegisterElementAnimations(ElementAnimations* element_animations);
  // Unregisters the given ElementAnimations as alive.
  void UnregisterElementAnimations(ElementAnimations* element_animations);

  const ElementToAnimationsMap& active_element_animations_for_testing() const;
  const ElementToAnimationsMap& all_element_animations_for_testing() const;

  bool animation_waiting_for_deletion() const {
    return animation_waiting_for_deletion_;
  }
  void OnAnimationWaitingForDeletion();

 private:
  explicit AnimationHost(ThreadInstance thread_instance);

  void PushTimelinesToImplThread(AnimationHost* host_impl) const;
  void RemoveTimelinesFromImplThread(AnimationHost* host_impl) const;
  void PushPropertiesToImplThread(AnimationHost* host_impl);

  void EraseTimeline(scoped_refptr<AnimationTimeline> timeline);

  ElementToAnimationsMap element_to_animations_map_;
  ElementToAnimationsMap active_element_to_animations_map_;

  // A list of all timelines which this host owns.
  using IdToTimelineMap =
      std::unordered_map<int, scoped_refptr<AnimationTimeline>>;
  IdToTimelineMap id_to_timeline_map_;

  MutatorHostClient* mutator_host_client_;

  std::unique_ptr<ScrollOffsetAnimations> scroll_offset_animations_;
  std::unique_ptr<ScrollOffsetAnimationsImpl> scroll_offset_animations_impl_;

  const ThreadInstance thread_instance_;

  bool supports_scroll_animations_;
  bool animation_waiting_for_deletion_;

  DISALLOW_COPY_AND_ASSIGN(AnimationHost);
};

}  // namespace cc

#endif  // CC_ANIMATION_ANIMATION_HOST_H_
