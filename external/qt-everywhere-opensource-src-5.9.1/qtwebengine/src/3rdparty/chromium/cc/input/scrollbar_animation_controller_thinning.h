// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_SCROLLBAR_ANIMATION_CONTROLLER_THINNING_H_
#define CC_INPUT_SCROLLBAR_ANIMATION_CONTROLLER_THINNING_H_

#include <memory>

#include "base/macros.h"
#include "cc/base/cc_export.h"
#include "cc/input/scrollbar_animation_controller.h"

namespace cc {

// Scrollbar animation that partially fades and thins after an idle delay,
// and reacts to mouse movements.
class CC_EXPORT ScrollbarAnimationControllerThinning
    : public ScrollbarAnimationController {
 public:
  static std::unique_ptr<ScrollbarAnimationControllerThinning> Create(
      int scroll_layer_id,
      ScrollbarAnimationControllerClient* client,
      base::TimeDelta delay_before_starting,
      base::TimeDelta resize_delay_before_starting,
      base::TimeDelta fade_duration,
      base::TimeDelta thinning_duration);

  ~ScrollbarAnimationControllerThinning() override;

  void set_mouse_move_distance_for_test(float distance) {
    mouse_move_distance_to_trigger_animation_ = distance;
  }
  bool mouse_is_over_scrollbar() const { return mouse_is_over_scrollbar_; }
  bool mouse_is_near_scrollbar() const { return mouse_is_near_scrollbar_; }

  void DidScrollUpdate(bool on_resize) override;

  void DidMouseDown() override;
  void DidMouseUp() override;
  void DidMouseLeave() override;
  void DidMouseMoveNear(float distance) override;
  bool ScrollbarsHidden() const override;

 protected:
  ScrollbarAnimationControllerThinning(
      int scroll_layer_id,
      ScrollbarAnimationControllerClient* client,
      base::TimeDelta delay_before_starting,
      base::TimeDelta resize_delay_before_starting,
      base::TimeDelta fade_duration,
      base::TimeDelta thinning_duration);

  void RunAnimationFrame(float progress) override;
  const base::TimeDelta& Duration() override;

 private:
  // Describes whether the current animation should INCREASE (thicken)
  // a bar or DECREASE it (thin).
  enum AnimationChange { NONE, INCREASE, DECREASE };
  enum AnimatingProperty { OPACITY, THICKNESS };
  float ThumbThicknessScaleAt(float progress);
  float AdjustScale(float new_value,
                    float current_value,
                    AnimationChange animation_change,
                    float min_value,
                    float max_value);
  void ApplyOpacity(float opacity);
  void ApplyThumbThicknessScale(float thumb_thickness_scale);

  void SetCurrentAnimatingProperty(AnimatingProperty property);

  float opacity_;
  bool captured_;
  bool mouse_is_over_scrollbar_;
  bool mouse_is_near_scrollbar_;
  // Are we narrowing or thickening the bars.
  AnimationChange thickness_change_;
  // How close should the mouse be to the scrollbar before we thicken it.
  float mouse_move_distance_to_trigger_animation_;

  base::TimeDelta fade_duration_;
  base::TimeDelta thinning_duration_;

  AnimatingProperty current_animating_property_;

  DISALLOW_COPY_AND_ASSIGN(ScrollbarAnimationControllerThinning);
};

}  // namespace cc

#endif  // CC_INPUT_SCROLLBAR_ANIMATION_CONTROLLER_THINNING_H_
