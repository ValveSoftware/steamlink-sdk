// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/scrollbar_animation_controller_thinning.h"

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/scrollbar_layer_impl_base.h"
#include "cc/trees/layer_tree_impl.h"

namespace {
const float kIdleThicknessScale = 0.4f;
const float kDefaultMouseMoveDistanceToTriggerAnimation = 25.f;
}

namespace cc {

std::unique_ptr<ScrollbarAnimationControllerThinning>
ScrollbarAnimationControllerThinning::Create(
    int scroll_layer_id,
    ScrollbarAnimationControllerClient* client,
    base::TimeDelta delay_before_starting,
    base::TimeDelta resize_delay_before_starting,
    base::TimeDelta fade_duration,
    base::TimeDelta thinning_duration) {
  return base::WrapUnique(new ScrollbarAnimationControllerThinning(
      scroll_layer_id, client, delay_before_starting,
      resize_delay_before_starting, fade_duration, thinning_duration));
}

ScrollbarAnimationControllerThinning::ScrollbarAnimationControllerThinning(
    int scroll_layer_id,
    ScrollbarAnimationControllerClient* client,
    base::TimeDelta delay_before_starting,
    base::TimeDelta resize_delay_before_starting,
    base::TimeDelta fade_duration,
    base::TimeDelta thinning_duration)
    : ScrollbarAnimationController(scroll_layer_id,
                                   client,
                                   delay_before_starting,
                                   resize_delay_before_starting),
      opacity_(0.0f),
      captured_(false),
      mouse_is_over_scrollbar_(false),
      mouse_is_near_scrollbar_(false),
      thickness_change_(NONE),
      mouse_move_distance_to_trigger_animation_(
          kDefaultMouseMoveDistanceToTriggerAnimation),
      fade_duration_(fade_duration),
      thinning_duration_(thinning_duration),
      current_animating_property_(OPACITY) {
  ApplyOpacity(0.f);
  ApplyThumbThicknessScale(kIdleThicknessScale);
}

ScrollbarAnimationControllerThinning::~ScrollbarAnimationControllerThinning() {}

void ScrollbarAnimationControllerThinning::RunAnimationFrame(float progress) {
  if (captured_)
    return;

  if (current_animating_property_ == OPACITY)
    ApplyOpacity(1.f - progress);
  else
    ApplyThumbThicknessScale(ThumbThicknessScaleAt(progress));

  client_->SetNeedsRedrawForScrollbarAnimation();
  if (progress == 1.f) {
    StopAnimation();
    if (current_animating_property_ == THICKNESS) {
      thickness_change_ = NONE;
      SetCurrentAnimatingProperty(OPACITY);
      PostDelayedAnimationTask(false);
    }
  }
}

const base::TimeDelta& ScrollbarAnimationControllerThinning::Duration() {
  if (current_animating_property_ == OPACITY)
    return fade_duration_;
  else
    return thinning_duration_;
}

void ScrollbarAnimationControllerThinning::DidMouseDown() {
  if (!mouse_is_over_scrollbar_ || opacity_ == 0.0f)
    return;

  StopAnimation();
  captured_ = true;
  ApplyOpacity(1.f);
  ApplyThumbThicknessScale(1.f);
}

void ScrollbarAnimationControllerThinning::DidMouseUp() {
  if (!captured_ || opacity_ == 0.0f)
    return;

  captured_ = false;
  StopAnimation();

  if (!mouse_is_near_scrollbar_) {
    SetCurrentAnimatingProperty(THICKNESS);
    thickness_change_ = DECREASE;
    StartAnimation();
  } else {
    SetCurrentAnimatingProperty(OPACITY);
    PostDelayedAnimationTask(false);
  }
}

void ScrollbarAnimationControllerThinning::DidMouseLeave() {
  if (!mouse_is_over_scrollbar_ && !mouse_is_near_scrollbar_)
    return;

  mouse_is_over_scrollbar_ = false;
  mouse_is_near_scrollbar_ = false;

  if (captured_ || opacity_ == 0.0f)
    return;

  thickness_change_ = DECREASE;
  SetCurrentAnimatingProperty(THICKNESS);
  StartAnimation();
}

void ScrollbarAnimationControllerThinning::DidScrollUpdate(bool on_resize) {
  if (captured_)
    return;

  ScrollbarAnimationController::DidScrollUpdate(on_resize);
  ApplyOpacity(1.f);
  ApplyThumbThicknessScale(mouse_is_near_scrollbar_ ? 1.f
                                                    : kIdleThicknessScale);
  SetCurrentAnimatingProperty(OPACITY);
}

void ScrollbarAnimationControllerThinning::DidMouseMoveNear(float distance) {
  bool mouse_is_over_scrollbar = distance == 0.0f;
  bool mouse_is_near_scrollbar =
      distance < mouse_move_distance_to_trigger_animation_;

  if (captured_ || opacity_ == 0.0f) {
    mouse_is_near_scrollbar_ = mouse_is_near_scrollbar;
    mouse_is_over_scrollbar_ = mouse_is_over_scrollbar;
    return;
  }

  if (mouse_is_over_scrollbar == mouse_is_over_scrollbar_ &&
      mouse_is_near_scrollbar == mouse_is_near_scrollbar_)
    return;

  if (mouse_is_over_scrollbar_ != mouse_is_over_scrollbar)
    mouse_is_over_scrollbar_ = mouse_is_over_scrollbar;

  if (mouse_is_near_scrollbar_ != mouse_is_near_scrollbar) {
    mouse_is_near_scrollbar_ = mouse_is_near_scrollbar;
    thickness_change_ = mouse_is_near_scrollbar_ ? INCREASE : DECREASE;
  }

  SetCurrentAnimatingProperty(THICKNESS);
  StartAnimation();
}

bool ScrollbarAnimationControllerThinning::ScrollbarsHidden() const {
  return opacity_ == 0.0f;
}

float ScrollbarAnimationControllerThinning::ThumbThicknessScaleAt(
    float progress) {
  if (thickness_change_ == NONE)
    return mouse_is_near_scrollbar_ ? 1.f : kIdleThicknessScale;
  float factor = thickness_change_ == INCREASE ? progress : (1.f - progress);
  return ((1.f - kIdleThicknessScale) * factor) + kIdleThicknessScale;
}

float ScrollbarAnimationControllerThinning::AdjustScale(
    float new_value,
    float current_value,
    AnimationChange animation_change,
    float min_value,
    float max_value) {
  float result;
  if (animation_change == INCREASE && current_value > new_value)
    result = current_value;
  else if (animation_change == DECREASE && current_value < new_value)
    result = current_value;
  else
    result = new_value;
  if (result > max_value)
    return max_value;
  if (result < min_value)
    return min_value;
  return result;
}

void ScrollbarAnimationControllerThinning::ApplyOpacity(float opacity) {
  for (ScrollbarLayerImplBase* scrollbar : Scrollbars()) {
    if (!scrollbar->is_overlay_scrollbar())
      continue;
    float effective_opacity = scrollbar->CanScrollOrientation() ? opacity : 0;
    PropertyTrees* property_trees =
        scrollbar->layer_tree_impl()->property_trees();
    // If this method is called during LayerImpl::PushPropertiesTo, we may not
    // yet have valid effect_id_to_index_map entries as property trees are
    // pushed after layers during activation. We can skip updating opacity in
    // that case as we are only registering a scrollbar and because opacity will
    // be overwritten anyway when property trees are pushed.
    if (property_trees->IsInIdToIndexMap(PropertyTrees::TreeType::EFFECT,
                                         scrollbar->id())) {
      property_trees->effect_tree.OnOpacityAnimated(
          effective_opacity,
          property_trees->effect_id_to_index_map[scrollbar->id()],
          scrollbar->layer_tree_impl());
    }
  }

  bool previouslyVisible = opacity_ > 0.0f;
  bool currentlyVisible = opacity > 0.0f;

  opacity_ = opacity;

  if (previouslyVisible != currentlyVisible)
    client_->DidChangeScrollbarVisibility();
}

void ScrollbarAnimationControllerThinning::ApplyThumbThicknessScale(
    float thumb_thickness_scale) {
  for (ScrollbarLayerImplBase* scrollbar : Scrollbars()) {
    if (!scrollbar->is_overlay_scrollbar())
      continue;

    scrollbar->SetThumbThicknessScaleFactor(AdjustScale(
        thumb_thickness_scale, scrollbar->thumb_thickness_scale_factor(),
        thickness_change_, kIdleThicknessScale, 1));
  }
}

void ScrollbarAnimationControllerThinning::SetCurrentAnimatingProperty(
    AnimatingProperty property) {
  if (current_animating_property_ == property)
    return;

  StopAnimation();
  current_animating_property_ = property;
  if (current_animating_property_ == THICKNESS)
    ApplyOpacity(1.f);
}

}  // namespace cc
