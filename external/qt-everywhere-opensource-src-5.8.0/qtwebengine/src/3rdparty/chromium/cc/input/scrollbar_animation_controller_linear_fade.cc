// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/scrollbar_animation_controller_linear_fade.h"

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/scrollbar_layer_impl_base.h"

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
                                   resize_delay_before_starting,
                                   duration) {}

ScrollbarAnimationControllerLinearFade::
    ~ScrollbarAnimationControllerLinearFade() {}

void ScrollbarAnimationControllerLinearFade::RunAnimationFrame(float progress) {
  ApplyOpacityToScrollbars(1.f - progress);
  client_->SetNeedsRedrawForScrollbarAnimation();
  if (progress == 1.f)
    StopAnimation();
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
    scrollbar->OnOpacityAnimated(scrollbar->CanScrollOrientation() ? opacity
                                                                   : 0);
  }
}

}  // namespace cc
