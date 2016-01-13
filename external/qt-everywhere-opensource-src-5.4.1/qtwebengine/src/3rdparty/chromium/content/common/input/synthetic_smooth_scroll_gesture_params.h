// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_SYNTHETIC_SMOOTH_SCROLL_GESTURE_PARAMS_H_
#define CONTENT_COMMON_INPUT_SYNTHETIC_SMOOTH_SCROLL_GESTURE_PARAMS_H_

#include <vector>

#include "content/common/content_export.h"
#include "content/common/input/synthetic_gesture_params.h"
#include "ui/gfx/point.h"
#include "ui/gfx/vector2d.h"

namespace content {

struct CONTENT_EXPORT SyntheticSmoothScrollGestureParams
    : public SyntheticGestureParams {
 public:
  SyntheticSmoothScrollGestureParams();
  SyntheticSmoothScrollGestureParams(
      const SyntheticSmoothScrollGestureParams& other);
  virtual ~SyntheticSmoothScrollGestureParams();

  virtual GestureType GetGestureType() const OVERRIDE;

  gfx::Point anchor;
  std::vector<gfx::Vector2d> distances;  // Positive X/Y to scroll left/up.
  bool prevent_fling;  // Defaults to true.
  int speed_in_pixels_s;

  static const SyntheticSmoothScrollGestureParams* Cast(
      const SyntheticGestureParams* gesture_params);
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_SYNTHETIC_SMOOTH_SCROLL_GESTURE_PARAMS_H_
