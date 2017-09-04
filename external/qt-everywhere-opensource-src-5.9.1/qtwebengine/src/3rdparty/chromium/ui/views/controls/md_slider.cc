// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/md_slider.h"

#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/slider.h"

namespace views {

// Color of slider at the active and the disabled state, respectively.
const SkColor kActiveColor = SkColorSetARGB(0xFF, 0x42, 0x85, 0xF4);
const SkColor kDisabledColor = SkColorSetARGB(0xFF, 0xBD, 0xBD, 0xBD);
const uint8_t kHighlightColorAlpha = 0x4D;

// The thickness of the slider.
const int kLineThickness = 2;

// The radius used to draw rounded slider ends.
const float kSliderRoundedRadius = 2.f;

// The radius of the thumb and the highlighted thumb of the slider,
// respectively.
const float kThumbRadius = 6.f;
const float kThumbHighlightRadius = 10.f;

// The stroke of the thumb when the slider is disabled.
const int kThumbStroke = 2;

// Duration of the thumb highlight growing effect animation.
const int kSlideHighlightChangeDurationMs = 150;

MdSlider::MdSlider(SliderListener* listener)
    : Slider(listener), is_active_(true), thumb_highlight_radius_(0.f) {
  SchedulePaint();
}

MdSlider::~MdSlider() {}

void MdSlider::OnPaint(gfx::Canvas* canvas) {
  Slider::OnPaint(canvas);

  // Paint the slider.
  const gfx::Rect content = GetContentsBounds();
  const int width = content.width() - kThumbRadius * 2;
  const int full = GetAnimatingValue() * width;
  const int empty = width - full;
  const int y = content.height() / 2 - kLineThickness / 2;
  const int x = content.x() + full + kThumbRadius;
  const SkColor current_thumb_color =
      is_active_ ? kActiveColor : kDisabledColor;

  // Extra space used to hide slider ends behind the thumb.
  const int extra_padding = 1;

  SkPaint slider_paint;
  slider_paint.setFlags(SkPaint::kAntiAlias_Flag);
  slider_paint.setColor(current_thumb_color);
  canvas->DrawRoundRect(
      gfx::Rect(content.x(), y, full + extra_padding, kLineThickness),
      kSliderRoundedRadius, slider_paint);
  slider_paint.setColor(kDisabledColor);
  canvas->DrawRoundRect(gfx::Rect(x + kThumbRadius - extra_padding, y,
                                  empty + extra_padding, kLineThickness),
                        kSliderRoundedRadius, slider_paint);

  gfx::Point thumb_center(x, content.height() / 2);

  // Paint the thumb highlight if it exists.
  if (is_active_ && thumb_highlight_radius_ > kThumbRadius) {
    SkPaint highlight;
    SkColor kHighlightColor = SkColorSetA(kActiveColor, kHighlightColorAlpha);
    highlight.setColor(kHighlightColor);
    highlight.setFlags(SkPaint::kAntiAlias_Flag);
    canvas->DrawCircle(thumb_center, thumb_highlight_radius_, highlight);
  }

  // Paint the thumb of the slider.
  SkPaint paint;
  paint.setColor(current_thumb_color);
  paint.setFlags(SkPaint::kAntiAlias_Flag);

  if (!is_active_) {
    paint.setStrokeWidth(kThumbStroke);
    paint.setStyle(SkPaint::kStroke_Style);
  }
  canvas->DrawCircle(
      thumb_center,
      is_active_ ? kThumbRadius : (kThumbRadius - kThumbStroke / 2), paint);
}

const char* MdSlider::GetClassName() const {
  return "MdSlider";
}

void MdSlider::UpdateState(bool control_on) {
  is_active_ = control_on;
  SchedulePaint();
}

int MdSlider::GetThumbWidth() {
  return kThumbRadius * 2;
}

void MdSlider::SetHighlighted(bool is_highlighted) {
  if (!highlight_animation_) {
    if (!is_highlighted)
      return;

    highlight_animation_.reset(new gfx::SlideAnimation(this));
    highlight_animation_->SetSlideDuration(kSlideHighlightChangeDurationMs);
  }
  if (is_highlighted)
    highlight_animation_->Show();
  else
    highlight_animation_->Hide();
}

void MdSlider::AnimationProgressed(const gfx::Animation* animation) {
  if (animation != highlight_animation_.get()) {
    Slider::AnimationProgressed(animation);
    return;
  }
  thumb_highlight_radius_ =
      animation->CurrentValueBetween(kThumbRadius, kThumbHighlightRadius);
  SchedulePaint();
}

void MdSlider::AnimationEnded(const gfx::Animation* animation) {
  if (animation != highlight_animation_.get()) {
    Slider::AnimationEnded(animation);
    return;
  }
  if (animation == highlight_animation_.get() &&
      !highlight_animation_->IsShowing()) {
    highlight_animation_.reset();
  }
}

}  // namespace views
