// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_IMPL_H_
#define UI_VIEWS_ANIMATION_INK_DROP_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight_observer.h"
#include "ui/views/animation/ink_drop_ripple_observer.h"
#include "ui/views/views_export.h"

namespace base {
class Timer;
}  // namespace base

namespace views {
namespace test {
class InkDropImplTestApi;
}  // namespace test

class InkDropRipple;
class InkDropHost;
class InkDropHighlight;
class InkDropFactoryTest;

// A functional implementation of an InkDrop.
class VIEWS_EXPORT InkDropImpl : public InkDrop,
                                 public InkDropRippleObserver,
                                 public InkDropHighlightObserver {
 public:
  // Constructs an ink drop that will attach the ink drop to the given
  // |ink_drop_host|.
  explicit InkDropImpl(InkDropHost* ink_drop_host);
  ~InkDropImpl() override;

  // InkDrop:
  InkDropState GetTargetInkDropState() const override;
  void AnimateToState(InkDropState ink_drop_state) override;
  void SnapToActivated() override;
  void SetHovered(bool is_hovered) override;
  void SetFocused(bool is_focused) override;

 private:
  friend class test::InkDropImplTestApi;

  // Destroys |ink_drop_ripple_| if it's targeted to the HIDDEN state.
  void DestroyHiddenTargetedAnimations();

  // Creates a new InkDropRipple and sets it to |ink_drop_ripple_|. If
  // |ink_drop_ripple_| wasn't null then it will be destroyed using
  // DestroyInkDropRipple().
  void CreateInkDropRipple();

  // Destroys the current |ink_drop_ripple_|.
  void DestroyInkDropRipple();

  // Creates a new InkDropHighlight and assigns it to |highlight_|. If
  // |highlight_| wasn't null then it will be destroyed using
  // DestroyInkDropHighlight().
  void CreateInkDropHighlight();

  // Destroys the current |highlight_|.
  void DestroyInkDropHighlight();

  // Adds the |root_layer_| to the |ink_drop_host_| if it hasn't already been
  // added.
  void AddRootLayerToHostIfNeeded();

  // Removes the |root_layer_| from the |ink_drop_host_| if no ink drop ripple
  // or highlight is active.
  void RemoveRootLayerFromHostIfNeeded();

  // Returns true if the highlight animation is in the process of fading in or
  // is visible.
  bool IsHighlightFadingInOrVisible() const;

  // views::InkDropRippleObserver:
  void AnimationStarted(InkDropState ink_drop_state) override;
  void AnimationEnded(InkDropState ink_drop_state,
                      InkDropAnimationEndedReason reason) override;

  // views::InkDropHighlightObserver:
  void AnimationStarted(
      InkDropHighlight::AnimationType animation_type) override;
  void AnimationEnded(InkDropHighlight::AnimationType animation_type,
                      InkDropAnimationEndedReason reason) override;

  // Enables or disables the highlight state based on |should_highlight| and if
  // an animation is triggered it will be scheduled to have the given
  // |animation_duration|. If |explode| is true the highlight will expand as it
  // fades out. |explode| is ignored when |should_higlight| is true.
  void SetHighlight(bool should_highlight,
                    base::TimeDelta animation_duration,
                    bool explode);

  // Returns true if this ink drop is hovered or focused.
  bool ShouldHighlight() const;

  // Starts the |highlight_after_ripple_timer_| timer. This will stop the
  // current
  // |highlight_after_ripple_timer_| instance if it exists.
  void StartHighlightAfterRippleTimer();

  // Stops and destroys the current |highlight_after_ripple_timer_| instance.
  void StopHighlightAfterRippleTimer();

  // Callback for when the |highlight_after_ripple_timer_| fires.
  void HighlightAfterRippleTimerFired();

  // The host of the ink drop. Used to poll for information such as whether the
  // highlight should be shown or not.
  InkDropHost* ink_drop_host_;

  // The root Layer that parents the InkDropRipple layers and the
  // InkDropHighlight layers. The |root_layer_| is the one that is added and
  // removed from the InkDropHost.
  std::unique_ptr<ui::Layer> root_layer_;

  // True when the |root_layer_| has been added to the |ink_drop_host_|.
  bool root_layer_added_to_host_;

  // The current InkDropHighlight. Lazily created using
  // CreateInkDropHighlight();
  std::unique_ptr<InkDropHighlight> highlight_;

  // Tracks the logical hovered state of |this| as manipulated by the public
  // SetHovered() function.
  bool is_hovered_;

  // Tracks the logical focused state of |this| as manipulated by the public
  // SetFocused() function.
  bool is_focused_;

  // The current InkDropRipple. Created on demand using CreateInkDropRipple().
  std::unique_ptr<InkDropRipple> ink_drop_ripple_;

  // The timer used to delay the highlight fade in after an ink drop ripple
  // animation.
  std::unique_ptr<base::Timer> highlight_after_ripple_timer_;

  DISALLOW_COPY_AND_ASSIGN(InkDropImpl);
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_IMPL_H_
