// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/scrollbar_animation_controller_linear_fade.h"

#include "base/time/time.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/scrollbar_layer_impl_base.h"

namespace cc {

scoped_ptr<ScrollbarAnimationControllerLinearFade>
ScrollbarAnimationControllerLinearFade::Create(
    LayerImpl* scroll_layer,
    ScrollbarAnimationControllerClient* client,
    base::TimeDelta delay_before_starting,
    base::TimeDelta duration) {
  return make_scoped_ptr(new ScrollbarAnimationControllerLinearFade(
      scroll_layer, client, delay_before_starting, duration));
}

ScrollbarAnimationControllerLinearFade::ScrollbarAnimationControllerLinearFade(
    LayerImpl* scroll_layer,
    ScrollbarAnimationControllerClient* client,
    base::TimeDelta delay_before_starting,
    base::TimeDelta duration)
    : ScrollbarAnimationController(client, delay_before_starting, duration),
      scroll_layer_(scroll_layer) {
}

ScrollbarAnimationControllerLinearFade::
    ~ScrollbarAnimationControllerLinearFade() {}

void ScrollbarAnimationControllerLinearFade::RunAnimationFrame(float progress) {
  ApplyOpacityToScrollbars(1.f - progress);
  if (progress == 1.f)
    StopAnimation();
}

void ScrollbarAnimationControllerLinearFade::DidScrollUpdate() {
  ScrollbarAnimationController::DidScrollUpdate();
  ApplyOpacityToScrollbars(1.f);
}

void ScrollbarAnimationControllerLinearFade::ApplyOpacityToScrollbars(
    float opacity) {
  if (!scroll_layer_->scrollbars())
    return;

  LayerImpl::ScrollbarSet* scrollbars = scroll_layer_->scrollbars();
  for (LayerImpl::ScrollbarSet::iterator it = scrollbars->begin();
       it != scrollbars->end();
       ++it) {
    ScrollbarLayerImplBase* scrollbar = *it;
    if (scrollbar->is_overlay_scrollbar())
      scrollbar->SetOpacity(opacity);
  }
}

}  // namespace cc
