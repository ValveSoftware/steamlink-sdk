// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/scrollbar_animation_controller_linear_fade.h"

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/scrollbar_layer_impl_base.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {

std::unique_ptr<ScrollbarAnimationControllerLinearFade>
ScrollbarAnimationControllerLinearFade::Create(
    int scroll_layer_id,
    ScrollbarAnimationControllerClient* client,
    base::TimeDelta delay_before_starting,
    base::TimeDelta resize_delay_before_starting,
    base::TimeDelta duration) {
  return base::WrapUnique(new ScrollbarAnimationControllerLinearFade(
      scroll_layer_id, client, delay_before_starting,
      resize_delay_before_starting, duration));
}

ScrollbarAnimationControllerLinearFade::ScrollbarAnimationControllerLinearFade(
    int scroll_layer_id,
    ScrollbarAnimationControllerClient* client,
    base::TimeDelta delay_before_starting,
    base::TimeDelta resize_delay_before_starting,
    base::TimeDelta duration)
    : ScrollbarAnimationController(scroll_layer_id,
                                   client,
                                   delay_before_starting,
                                   resize_delay_before_starting),
      duration_(duration) {}

ScrollbarAnimationControllerLinearFade::
    ~ScrollbarAnimationControllerLinearFade() {}

void ScrollbarAnimationControllerLinearFade::RunAnimationFrame(float progress) {
  ApplyOpacityToScrollbars(1.f - progress);
  client_->SetNeedsRedrawForScrollbarAnimation();
  if (progress == 1.f)
    StopAnimation();
}

const base::TimeDelta& ScrollbarAnimationControllerLinearFade::Duration() {
  return duration_;
}

void ScrollbarAnimationControllerLinearFade::DidScrollUpdate(bool on_resize) {
  ScrollbarAnimationController::DidScrollUpdate(on_resize);
  ApplyOpacityToScrollbars(1.f);
}

void ScrollbarAnimationControllerLinearFade::ApplyOpacityToScrollbars(
    float opacity) {
  for (ScrollbarLayerImplBase* scrollbar : Scrollbars()) {
    if (!scrollbar->is_overlay_scrollbar())
      continue;
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
          scrollbar->CanScrollOrientation() ? opacity : 0,
          property_trees->effect_id_to_index_map[scrollbar->id()],
          scrollbar->layer_tree_impl());
    }
  }
}

}  // namespace cc
