// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_SCROLLBAR_ANIMATION_CONTROLLER_H_
#define CC_INPUT_SCROLLBAR_ANIMATION_CONTROLLER_H_

#include "base/cancelable_callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "cc/base/cc_export.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/scrollbar_layer_impl_base.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

class ScrollbarAnimationController;

class CC_EXPORT ScrollbarAnimationControllerClient {
 public:
  virtual void PostDelayedScrollbarAnimationTask(const base::Closure& task,
                                                 base::TimeDelta delay) = 0;
  virtual void SetNeedsRedrawForScrollbarAnimation() = 0;
  virtual void SetNeedsAnimateForScrollbarAnimation() = 0;
  virtual ScrollbarSet ScrollbarsFor(int scroll_layer_id) const = 0;

 protected:
  virtual ~ScrollbarAnimationControllerClient() {}
};

// This abstract class represents the compositor-side analogy of
// ScrollbarAnimator.  Individual platforms should subclass it to provide
// specialized implementation.
class CC_EXPORT ScrollbarAnimationController {
 public:
  virtual ~ScrollbarAnimationController();

  bool Animate(base::TimeTicks now);

  virtual void DidScrollBegin();
  virtual void DidScrollUpdate(bool on_resize);
  virtual void DidScrollEnd();
  virtual void DidMouseMoveOffScrollbar() {}
  virtual void DidMouseMoveNear(float distance) {}

 protected:
  ScrollbarAnimationController(int scroll_layer_id,
                               ScrollbarAnimationControllerClient* client,
                               base::TimeDelta delay_before_starting,
                               base::TimeDelta resize_delay_before_starting,
                               base::TimeDelta duration);

  virtual void RunAnimationFrame(float progress) = 0;

  void StartAnimation();
  void StopAnimation();
  ScrollbarSet Scrollbars() const;

  ScrollbarAnimationControllerClient* client_;

 private:
  // Returns how far through the animation we are as a progress value from
  // 0 to 1.
  float AnimationProgressAtTime(base::TimeTicks now);

  void PostDelayedAnimationTask(bool on_resize);

  base::TimeTicks last_awaken_time_;
  base::TimeDelta delay_before_starting_;
  base::TimeDelta resize_delay_before_starting_;
  base::TimeDelta duration_;

  bool is_animating_;

  int scroll_layer_id_;
  bool currently_scrolling_;
  bool scroll_gesture_has_scrolled_;
  base::CancelableClosure delayed_scrollbar_fade_;

  base::WeakPtrFactory<ScrollbarAnimationController> weak_factory_;
};

}  // namespace cc

#endif  // CC_INPUT_SCROLLBAR_ANIMATION_CONTROLLER_H_
