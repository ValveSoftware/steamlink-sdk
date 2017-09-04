// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/aura/overscroll_window_delegate.h"

#include "base/i18n/rtl.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/overscroll_controller_delegate.h"
#include "content/public/browser/overscroll_configuration.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/gfx/image/image_png_rep.h"

namespace content {

OverscrollWindowDelegate::OverscrollWindowDelegate(
    OverscrollControllerDelegate* delegate,
    const gfx::Image& image)
    : delegate_(delegate),
      overscroll_mode_(OVERSCROLL_NONE),
      delta_x_(0.f),
      complete_threshold_ratio_(content::GetOverscrollConfig(
          content::OVERSCROLL_CONFIG_HORIZ_THRESHOLD_COMPLETE)),
      start_threshold_touchscreen_(content::GetOverscrollConfig(
          content::OVERSCROLL_CONFIG_HORIZ_THRESHOLD_START_TOUCHSCREEN)),
      start_threshold_touchpad_(content::GetOverscrollConfig(
          content::OVERSCROLL_CONFIG_HORIZ_THRESHOLD_START_TOUCHPAD)),
      active_start_threshold_(0.f) {
  SetImage(image);
}

OverscrollWindowDelegate::~OverscrollWindowDelegate() {
}

void OverscrollWindowDelegate::StartOverscroll() {
  OverscrollMode old_mode = overscroll_mode_;
  if (delta_x_ > 0)
    overscroll_mode_ = OVERSCROLL_EAST;
  else
    overscroll_mode_ = OVERSCROLL_WEST;
  delegate_->OnOverscrollModeChange(old_mode, overscroll_mode_);
}

void OverscrollWindowDelegate::ResetOverscroll() {
  if (overscroll_mode_ == OVERSCROLL_NONE)
    return;
  delegate_->OnOverscrollModeChange(overscroll_mode_, OVERSCROLL_NONE);
  overscroll_mode_ = OVERSCROLL_NONE;
  delta_x_ = 0;
}

void OverscrollWindowDelegate::CompleteOrResetOverscroll() {
  if (overscroll_mode_ == OVERSCROLL_NONE)
    return;
  int width = delegate_->GetVisibleBounds().width();
  float ratio = (fabs(delta_x_)) / width;
  if (ratio < complete_threshold_ratio_) {
    ResetOverscroll();
    return;
  }
  delegate_->OnOverscrollComplete(overscroll_mode_);
  overscroll_mode_ = OVERSCROLL_NONE;
  delta_x_ = 0;
}

void OverscrollWindowDelegate::UpdateOverscroll(float delta_x) {
  float old_delta_x = delta_x_;
  delta_x_ += delta_x;
  if (overscroll_mode_ == OVERSCROLL_NONE) {
    if (fabs(delta_x_) > active_start_threshold_)
      StartOverscroll();
    return;
  }
  if ((old_delta_x < 0 && delta_x_ > 0) || (old_delta_x > 0 && delta_x_ < 0)) {
    ResetOverscroll();
    return;
  }
  delegate_->OnOverscrollUpdate(delta_x_, 0.f);
}

void OverscrollWindowDelegate::OnKeyEvent(ui::KeyEvent* event) {
  ResetOverscroll();
}

void OverscrollWindowDelegate::OnMouseEvent(ui::MouseEvent* event) {
  if (!(event->flags() & ui::EF_IS_SYNTHESIZED) &&
      event->type() != ui::ET_MOUSE_CAPTURE_CHANGED) {
    ResetOverscroll();
  }
}

void OverscrollWindowDelegate::OnScrollEvent(ui::ScrollEvent* event) {
  active_start_threshold_ = start_threshold_touchpad_;
  if (event->type() == ui::ET_SCROLL)
    UpdateOverscroll(event->x_offset_ordinal());
  else if (event->type() == ui::ET_SCROLL_FLING_START)
    CompleteOrResetOverscroll();
  else
    ResetOverscroll();
  event->SetHandled();
}

void OverscrollWindowDelegate::OnGestureEvent(ui::GestureEvent* event) {
  active_start_threshold_ = start_threshold_touchscreen_;
  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_UPDATE:
      UpdateOverscroll(event->details().scroll_x());
      break;

    case ui::ET_GESTURE_SCROLL_END:
      CompleteOrResetOverscroll();
      break;

    case ui::ET_SCROLL_FLING_START:
      CompleteOrResetOverscroll();
      break;

    case ui::ET_GESTURE_PINCH_BEGIN:
    case ui::ET_GESTURE_PINCH_UPDATE:
    case ui::ET_GESTURE_PINCH_END:
      ResetOverscroll();
      break;

    default:
      break;
  }
  event->SetHandled();
}

}  // namespace content
