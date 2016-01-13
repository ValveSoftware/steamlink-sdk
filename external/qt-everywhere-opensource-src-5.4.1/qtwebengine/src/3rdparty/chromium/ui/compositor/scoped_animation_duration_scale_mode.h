// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_SCOPED_ANIMATION_DURATION_SCALE_MODE_H_
#define UI_COMPOSITOR_SCOPED_ANIMATION_DURATION_SCALE_MODE_H_

#include "base/basictypes.h"
#include "ui/compositor/compositor_export.h"

namespace ui {

// Speed up or slow down animations for testing or debugging.
class COMPOSITOR_EXPORT ScopedAnimationDurationScaleMode {
 public:
  enum DurationScaleMode {
    NORMAL_DURATION,
    FAST_DURATION,
    SLOW_DURATION,
    ZERO_DURATION
  };

  explicit ScopedAnimationDurationScaleMode(
      DurationScaleMode scoped_duration_scale_mode)
      : old_duration_scale_mode_(duration_scale_mode_) {
    duration_scale_mode_ = scoped_duration_scale_mode;
  }

  ~ScopedAnimationDurationScaleMode() {
    duration_scale_mode_ = old_duration_scale_mode_;
  }

  static DurationScaleMode duration_scale_mode() {
    return duration_scale_mode_;
  }

 private:
  DurationScaleMode old_duration_scale_mode_;

  static DurationScaleMode duration_scale_mode_;

  DISALLOW_COPY_AND_ASSIGN(ScopedAnimationDurationScaleMode);
};

}  // namespace ui

#endif  // UI_COMPOSITOR_SCOPED_ANIMATION_DURATION_SCALE_MODE_H
