// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/slide_out_view.h"

#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/transform.h"

namespace views {

SlideOutView::SlideOutView() {
  // If accelerated compositing is not available, this widget tracks the
  // OnSlideOut event but does not render any visible changes.
  SetPaintToLayer(true);
  layer()->SetFillsBoundsOpaquely(false);
}

SlideOutView::~SlideOutView() {
}

void SlideOutView::OnGestureEvent(ui::GestureEvent* event) {
  const float kScrollRatioForClosingNotification = 0.5f;

  if (event->type() == ui::ET_SCROLL_FLING_START) {
    // The threshold for the fling velocity is computed empirically.
    // The unit is in pixels/second.
    const float kFlingThresholdForClose = 800.f;
    if (is_slide_out_enabled_ &&
        fabsf(event->details().velocity_x()) > kFlingThresholdForClose) {
      SlideOutAndClose(event->details().velocity_x() < 0 ? SLIDE_LEFT :
                       SLIDE_RIGHT);
      event->StopPropagation();
      return;
    }
    RestoreVisualState();
    return;
  }

  if (!event->IsScrollGestureEvent())
    return;

  if (event->type() == ui::ET_GESTURE_SCROLL_BEGIN) {
    gesture_amount_ = 0.f;
  } else if (event->type() == ui::ET_GESTURE_SCROLL_UPDATE) {
    // The scroll-update events include the incremental scroll amount.
    gesture_amount_ += event->details().scroll_x();

    float scroll_amount;
    if (is_slide_out_enabled_) {
      scroll_amount = gesture_amount_;
      layer()->SetOpacity(1.f - std::min(fabsf(scroll_amount) / width(), 1.f));
    } else {
      if (gesture_amount_ >= 0) {
        scroll_amount = std::min(0.5f * gesture_amount_,
                                 width() * kScrollRatioForClosingNotification);
      } else {
        scroll_amount =
            std::max(0.5f * gesture_amount_,
                     -1.f * width() * kScrollRatioForClosingNotification);
      }
    }

    gfx::Transform transform;
    transform.Translate(scroll_amount, 0.0);
    layer()->SetTransform(transform);

  } else if (event->type() == ui::ET_GESTURE_SCROLL_END) {
    float scrolled_ratio = fabsf(gesture_amount_) / width();
    if (is_slide_out_enabled_ &&
        scrolled_ratio >= kScrollRatioForClosingNotification) {
      SlideOutAndClose(gesture_amount_ < 0 ? SLIDE_LEFT : SLIDE_RIGHT);
      event->StopPropagation();
      return;
    }
    RestoreVisualState();
  }

  event->SetHandled();
}

void SlideOutView::RestoreVisualState() {
  // Restore the layer state.
  const int kSwipeRestoreDurationMS = 150;
  ui::ScopedLayerAnimationSettings settings(layer()->GetAnimator());
  settings.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kSwipeRestoreDurationMS));
  layer()->SetTransform(gfx::Transform());
  layer()->SetOpacity(1.f);
}

void SlideOutView::SlideOutAndClose(SlideDirection direction) {
  const int kSwipeOutTotalDurationMS = 150;
  int swipe_out_duration = kSwipeOutTotalDurationMS * layer()->opacity();
  ui::ScopedLayerAnimationSettings settings(layer()->GetAnimator());
  settings.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(swipe_out_duration));
  settings.AddObserver(this);

  gfx::Transform transform;
  transform.Translate(direction == SLIDE_LEFT ? -width() : width(), 0.0);
  layer()->SetTransform(transform);
  layer()->SetOpacity(0.f);
}

void SlideOutView::OnImplicitAnimationsCompleted() {
  OnSlideOut();
}

}  // namespace views
