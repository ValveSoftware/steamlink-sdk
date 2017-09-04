// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/non_md_slider.h"

#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/controls/slider.h"
#include "ui/views/resources/grit/views_resources.h"

namespace {
const int kBarImagesActive[] = {
    IDR_SLIDER_ACTIVE_LEFT, IDR_SLIDER_ACTIVE_CENTER, IDR_SLIDER_PRESSED_CENTER,
    IDR_SLIDER_PRESSED_RIGHT,
};

const int kBarImagesDisabled[] = {
    IDR_SLIDER_DISABLED_LEFT, IDR_SLIDER_DISABLED_CENTER,
    IDR_SLIDER_DISABLED_CENTER, IDR_SLIDER_DISABLED_RIGHT,
};

// The image chunks.
enum BorderElements {
  LEFT,
  CENTER_LEFT,
  CENTER_RIGHT,
  RIGHT,
};
}  // namespace

namespace views {

NonMdSlider::NonMdSlider(SliderListener* listener)
    : Slider(listener),
      bar_active_images_(kBarImagesActive),
      bar_disabled_images_(kBarImagesDisabled) {
  UpdateSliderAppearance(true);
}

NonMdSlider::~NonMdSlider() {}

void NonMdSlider::OnPaint(gfx::Canvas* canvas) {
  Slider::OnPaint(canvas);
  gfx::Rect content = GetContentsBounds();
  float value = GetAnimatingValue();

  // Inset the slider bar a little bit, so that the left or the right end of
  // the slider bar will not be exposed under the thumb button when the thumb
  // button slides to the left most or right most position.
  const int kBarInsetX = 2;
  int bar_width = content.width() - kBarInsetX * 2;
  int bar_cy = content.height() / 2 - bar_height_ / 2;

  int w = content.width() - thumb_->width();
  int full = value * w;
  int middle = std::max(full, images_[LEFT]->width());

  canvas->Save();
  canvas->Translate(gfx::Vector2d(kBarInsetX, bar_cy));
  canvas->DrawImageInt(*images_[LEFT], 0, 0);
  canvas->DrawImageInt(*images_[RIGHT], bar_width - images_[RIGHT]->width(), 0);
  canvas->TileImageInt(*images_[CENTER_LEFT], images_[LEFT]->width(), 0,
                       middle - images_[LEFT]->width(), bar_height_);
  canvas->TileImageInt(*images_[CENTER_RIGHT], middle, 0,
                       bar_width - middle - images_[RIGHT]->width(),
                       bar_height_);
  canvas->Restore();

  // Paint slider thumb.
  int button_cx = content.x() + full;
  int thumb_y = content.height() / 2 - thumb_->height() / 2;
  canvas->DrawImageInt(*thumb_, button_cx, thumb_y);
}

const char* NonMdSlider::GetClassName() const {
  return "NonMdSlider";
}

void NonMdSlider::UpdateState(bool control_on) {
  UpdateSliderAppearance(control_on);
}

void NonMdSlider::UpdateSliderAppearance(bool control_on) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  if (control_on) {
    thumb_ = rb.GetImageNamed(IDR_SLIDER_ACTIVE_THUMB).ToImageSkia();
    for (int i = 0; i < 4; ++i)
      images_[i] = rb.GetImageNamed(bar_active_images_[i]).ToImageSkia();
  } else {
    thumb_ = rb.GetImageNamed(IDR_SLIDER_DISABLED_THUMB).ToImageSkia();
    for (int i = 0; i < 4; ++i)
      images_[i] = rb.GetImageNamed(bar_disabled_images_[i]).ToImageSkia();
  }
  bar_height_ = images_[LEFT]->height();
  SchedulePaint();
}

int NonMdSlider::GetThumbWidth() {
  return thumb_->width();
}

}  // namespace views
