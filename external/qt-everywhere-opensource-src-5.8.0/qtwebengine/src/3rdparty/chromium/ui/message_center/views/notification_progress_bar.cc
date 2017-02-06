// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_progress_bar.h"

#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/canvas.h"
#include "ui/message_center/message_center_style.h"

namespace {

// Dimensions.
const int kProgressBarWidth = message_center::kNotificationWidth -
    message_center::kTextLeftPadding - message_center::kTextRightPadding;

const int kAnimationFrameRateHz = 60;

const int kAnimationDuration = 2000;  // In millisecond

} // namespace

namespace message_center {

// NotificationProgressBarBase /////////////////////////////////////////////////

gfx::Size NotificationProgressBarBase::GetPreferredSize() const {
  gfx::Size pref_size(kProgressBarWidth, message_center::kProgressBarThickness);
  gfx::Insets insets = GetInsets();
  pref_size.Enlarge(insets.width(), insets.height());
  return pref_size;
}

// NotificationProgressBar /////////////////////////////////////////////////////

NotificationProgressBar::NotificationProgressBar() {
}

NotificationProgressBar::~NotificationProgressBar() {
}

bool NotificationProgressBar::is_indeterminate() {
  return false;
}

void NotificationProgressBar::OnPaint(gfx::Canvas* canvas) {
  gfx::Rect content_bounds = GetContentsBounds();

  // Draw background.
  SkPath background_path;
  background_path.addRoundRect(gfx::RectToSkRect(content_bounds),
                               message_center::kProgressBarCornerRadius,
                               message_center::kProgressBarCornerRadius);
  SkPaint background_paint;
  background_paint.setStyle(SkPaint::kFill_Style);
  background_paint.setFlags(SkPaint::kAntiAlias_Flag);
  background_paint.setColor(message_center::kProgressBarBackgroundColor);
  canvas->DrawPath(background_path, background_paint);

  // Draw slice.
  SkPath slice_path;
  const int slice_width =
      static_cast<int>(content_bounds.width() * GetNormalizedValue() + 0.5);
  if (slice_width < 1)
    return;

  gfx::Rect slice_bounds = content_bounds;
  slice_bounds.set_width(slice_width);
  slice_path.addRoundRect(gfx::RectToSkRect(slice_bounds),
                          message_center::kProgressBarCornerRadius,
                          message_center::kProgressBarCornerRadius);

  SkPaint slice_paint;
  slice_paint.setStyle(SkPaint::kFill_Style);
  slice_paint.setFlags(SkPaint::kAntiAlias_Flag);
  slice_paint.setColor(message_center::kProgressBarSliceColor);
  canvas->DrawPath(slice_path, slice_paint);
}

// NotificationIndeteminateProgressBar /////////////////////////////////////////

NotificationIndeterminateProgressBar::NotificationIndeterminateProgressBar() {
  indeterminate_bar_animation_.reset(
      new gfx::LinearAnimation(kAnimationFrameRateHz, this));
  indeterminate_bar_animation_->SetDuration(kAnimationDuration);
  indeterminate_bar_animation_->Start();
}

NotificationIndeterminateProgressBar::~NotificationIndeterminateProgressBar() {
  indeterminate_bar_animation_->Stop();  // Just in case
}

bool NotificationIndeterminateProgressBar::is_indeterminate() {
  return true;
}

void NotificationIndeterminateProgressBar::OnPaint(gfx::Canvas* canvas) {
  gfx::Rect content_bounds = GetContentsBounds();

  // Draw background.
  SkPath background_path;
  background_path.addRoundRect(gfx::RectToSkRect(content_bounds),
                               message_center::kProgressBarCornerRadius,
                               message_center::kProgressBarCornerRadius);
  SkPaint background_paint;
  background_paint.setStyle(SkPaint::kFill_Style);
  background_paint.setFlags(SkPaint::kAntiAlias_Flag);
  background_paint.setColor(message_center::kProgressBarBackgroundColor);
  canvas->DrawPath(background_path, background_paint);

  // Draw slice.
  SkPath slice_path;
  double time = indeterminate_bar_animation_->GetCurrentValue();

  // The animation spec corresponds to the material design lite's parameter.
  // (cf. https://github.com/google/material-design-lite/)
  double bar1_left;
  double bar1_width;
  double bar2_left;
  double bar2_width;
  if (time < 0.50) {
    bar1_left = time / 2;
    bar1_width = time * 1.5;
    bar2_left = 0;
    bar2_width = 0;
  } else if (time < 0.75) {
    bar1_left = time * 3 - 1.25;
    bar1_width = 0.75 - (time - 0.5)  * 3;
    bar2_left = 0;
    bar2_width = time - 0.5;
  } else {
    bar1_left = 1;
    bar1_width = 0;
    bar2_left = (time - 0.75) * 4;
    bar2_width = 0.25 - (time - 0.75);
  }

  int bar1_x = static_cast<int>(content_bounds.width() * bar1_left);
  int bar1_w =
      std::min(static_cast<int>(content_bounds.width() * bar1_width + 0.5),
               content_bounds.width() - bar1_x);
  int bar2_x = static_cast<int>(content_bounds.width() * bar2_left);
  int bar2_w =
      std::min(static_cast<int>(content_bounds.width() * bar2_width + 0.5),
               content_bounds.width() - bar2_x);


  gfx::Rect slice_bounds = content_bounds;
  slice_bounds.set_x(content_bounds.x() + bar1_x);
  slice_bounds.set_width(bar1_w);
  slice_path.addRoundRect(gfx::RectToSkRect(slice_bounds),
                          message_center::kProgressBarCornerRadius,
                          message_center::kProgressBarCornerRadius);
  slice_bounds.set_x(content_bounds.x() + bar2_x);
  slice_bounds.set_width(bar2_w);
  slice_path.addRoundRect(gfx::RectToSkRect(slice_bounds),
                          message_center::kProgressBarCornerRadius,
                          message_center::kProgressBarCornerRadius);

  SkPaint slice_paint;
  slice_paint.setStyle(SkPaint::kFill_Style);
  slice_paint.setFlags(SkPaint::kAntiAlias_Flag);
  slice_paint.setColor(message_center::kProgressBarSliceColor);
  canvas->DrawPath(slice_path, slice_paint);
}

void NotificationIndeterminateProgressBar::AnimationProgressed(
    const gfx::Animation* animation) {
  if (animation == indeterminate_bar_animation_.get()) {
    DCHECK(indeterminate_bar_animation_);
    SchedulePaint();
  }
}

void NotificationIndeterminateProgressBar::AnimationEnded(
    const gfx::Animation* animation) {
  // Restarts animation.
  if (animation == indeterminate_bar_animation_.get()) {
    DCHECK(indeterminate_bar_animation_);
    indeterminate_bar_animation_->Start();
  }
}

}  // namespace message_center
