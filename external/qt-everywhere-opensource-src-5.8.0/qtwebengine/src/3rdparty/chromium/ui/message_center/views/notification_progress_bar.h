// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_PROGRESS_BAR_H_
#define UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_PROGRESS_BAR_H_

#include "base/macros.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/message_center/message_center_export.h"
#include "ui/views/controls/progress_bar.h"

namespace message_center {

class MESSAGE_CENTER_EXPORT NotificationProgressBarBase
    : public views::ProgressBar {
 public:
  virtual bool is_indeterminate() = 0;

 private:
  // Overriden from views::ProgressBar (originally from views::View)
  gfx::Size GetPreferredSize() const override;
};

class MESSAGE_CENTER_EXPORT NotificationProgressBar
    : public NotificationProgressBarBase {
 public:
  NotificationProgressBar();
  ~NotificationProgressBar() override;

  bool is_indeterminate() override;

 private:
  // Overriden from views::ProgressBar (originally from views::View)
  void OnPaint(gfx::Canvas* canvas) override;

  DISALLOW_COPY_AND_ASSIGN(NotificationProgressBar);
};

class MESSAGE_CENTER_EXPORT NotificationIndeterminateProgressBar
    : public NotificationProgressBarBase, public gfx::AnimationDelegate {
 public:
  NotificationIndeterminateProgressBar();
  ~NotificationIndeterminateProgressBar() override;

  bool is_indeterminate() override;

 private:
  // Overriden from views::ProgressBar (originally from views::View)
  void OnPaint(gfx::Canvas* canvas) override;

  // Overriden from gfx::AnimationDelegate
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  std::unique_ptr<gfx::LinearAnimation> indeterminate_bar_animation_;

  DISALLOW_COPY_AND_ASSIGN(NotificationIndeterminateProgressBar);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_PROGRESS_BAR_H_
