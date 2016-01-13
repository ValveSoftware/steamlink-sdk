// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_SMOOTH_SCROLL_GESTURE_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_SMOOTH_SCROLL_GESTURE_H_

#include "base/time/time.h"
#include "content/browser/renderer_host/input/synthetic_gesture.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target.h"
#include "content/common/content_export.h"
#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/gfx/vector2d.h"
#include "ui/gfx/vector2d_f.h"

namespace content {

// Simulates scrolling given a sequence of scroll distances as a continuous
// gestures (i.e. when synthesizing touch events, the touch pointer is not
// lifted when changing scroll direction).
// If no distance is provided or the first one is 0, no touch events are
// generated.
// When synthesizing touch events, the first distance is extended to compensate
// for the touch slop.
class CONTENT_EXPORT SyntheticSmoothScrollGesture : public SyntheticGesture {
 public:
  explicit SyntheticSmoothScrollGesture(
      const SyntheticSmoothScrollGestureParams& params);
  virtual ~SyntheticSmoothScrollGesture();

  virtual SyntheticGesture::Result ForwardInputEvents(
      const base::TimeTicks& timestamp,
      SyntheticGestureTarget* target) OVERRIDE;

 private:
  enum GestureState {
    SETUP,
    STARTED,
    MOVING,
    STOPPING,
    DONE
  };

  void ForwardTouchInputEvents(
      const base::TimeTicks& timestamp, SyntheticGestureTarget* target);
  void ForwardMouseInputEvents(
      const base::TimeTicks& timestamp, SyntheticGestureTarget* target);

  void ForwardTouchEvent(SyntheticGestureTarget* target,
                         const base::TimeTicks& timestamp);
  void ForwardMouseWheelEvent(SyntheticGestureTarget* target,
                              const gfx::Vector2dF& delta,
                              const base::TimeTicks& timestamp) const;

  void PressTouchPoint(SyntheticGestureTarget* target,
                       const base::TimeTicks& timestamp);
  void MoveTouchPoint(SyntheticGestureTarget* target,
                      const gfx::Vector2dF& delta,
                      const base::TimeTicks& timestamp);
  void ReleaseTouchPoint(SyntheticGestureTarget* target,
                         const base::TimeTicks& timestamp);

  void AddTouchSlopToFirstDistance(SyntheticGestureTarget* target);
  gfx::Vector2dF GetPositionDeltaAtTime(const base::TimeTicks& timestamp)
      const;
  void ComputeNextScrollSegment();
  base::TimeTicks ClampTimestamp(const base::TimeTicks& timestamp) const;
  bool FinishedCurrentScrollSegment(const base::TimeTicks& timestamp) const;
  bool IsLastScrollSegment() const;
  bool ScrollIsNoOp() const;

  SyntheticSmoothScrollGestureParams params_;
  // Used for mouse input.
  gfx::Vector2d current_scroll_segment_total_delta_discrete_;
  // Used for touch input.
  gfx::Point current_scroll_segment_start_position_;
  SyntheticWebTouchEvent touch_event_;
  SyntheticGestureParams::GestureSourceType gesture_source_type_;
  GestureState state_;
  int current_scroll_segment_;
  base::TimeTicks current_scroll_segment_start_time_;
  base::TimeTicks current_scroll_segment_stop_time_;

  DISALLOW_COPY_AND_ASSIGN(SyntheticSmoothScrollGesture);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_SMOOTH_SCROLL_GESTURE_H_
