// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_TAP_GESTURE_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_TAP_GESTURE_H_

#include "content/browser/renderer_host/input/synthetic_gesture.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target.h"
#include "content/common/content_export.h"
#include "content/common/input/synthetic_tap_gesture_params.h"
#include "content/common/input/synthetic_web_input_event_builders.h"

namespace content {

class CONTENT_EXPORT SyntheticTapGesture : public SyntheticGesture {
 public:
  explicit SyntheticTapGesture(const SyntheticTapGestureParams& params);
  virtual ~SyntheticTapGesture();

  virtual SyntheticGesture::Result ForwardInputEvents(
      const base::TimeTicks& timestamp,
      SyntheticGestureTarget* target) OVERRIDE;

 private:
  enum GestureState {
    SETUP,
    PRESS,
    WAITING_TO_RELEASE,
    DONE
  };

  void ForwardTouchOrMouseInputEvents(const base::TimeTicks& timestamp,
                                      SyntheticGestureTarget* target);

  void Press(SyntheticGestureTarget* target, const base::TimeTicks& timestamp);
  void Release(SyntheticGestureTarget* target,
               const base::TimeTicks& timestamp);

  base::TimeDelta GetDuration() const;

  SyntheticTapGestureParams params_;
  base::TimeTicks start_time_;
  SyntheticWebTouchEvent touch_event_;
  SyntheticGestureParams::GestureSourceType gesture_source_type_;
  GestureState state_;

  DISALLOW_COPY_AND_ASSIGN(SyntheticTapGesture);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_TAP_GESTURE_H_
