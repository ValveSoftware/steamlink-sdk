// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_tap_gesture.h"

#include "base/logging.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/latency_info.h"

namespace content {

SyntheticTapGesture::SyntheticTapGesture(
    const SyntheticTapGestureParams& params)
    : params_(params),
      gesture_source_type_(SyntheticGestureParams::DEFAULT_INPUT),
      state_(SETUP) {
  DCHECK_GE(params_.duration_ms, 0);
}

SyntheticTapGesture::~SyntheticTapGesture() {}

SyntheticGesture::Result SyntheticTapGesture::ForwardInputEvents(
    const base::TimeTicks& timestamp, SyntheticGestureTarget* target) {
  if (state_ == SETUP) {
    gesture_source_type_ = params_.gesture_source_type;
    if (gesture_source_type_ == SyntheticGestureParams::DEFAULT_INPUT)
      gesture_source_type_ = target->GetDefaultSyntheticGestureSourceType();

    state_ = PRESS;
  }

  DCHECK_NE(gesture_source_type_, SyntheticGestureParams::DEFAULT_INPUT);

  if (!synthetic_pointer_)
    synthetic_pointer_ = SyntheticPointer::Create(gesture_source_type_);

  if (gesture_source_type_ == SyntheticGestureParams::TOUCH_INPUT ||
      gesture_source_type_ == SyntheticGestureParams::MOUSE_INPUT)
    ForwardTouchOrMouseInputEvents(timestamp, target);
  else
    return SyntheticGesture::GESTURE_SOURCE_TYPE_NOT_IMPLEMENTED;

  return (state_ == DONE) ? SyntheticGesture::GESTURE_FINISHED
                          : SyntheticGesture::GESTURE_RUNNING;
}

void SyntheticTapGesture::ForwardTouchOrMouseInputEvents(
    const base::TimeTicks& timestamp, SyntheticGestureTarget* target) {
  switch (state_) {
    case PRESS:
      synthetic_pointer_->Press(params_.position.x(), params_.position.y(),
                                target, timestamp);
      synthetic_pointer_->DispatchEvent(target, timestamp);
      // Release immediately if duration is 0.
      if (params_.duration_ms == 0) {
        synthetic_pointer_->Release(0, target, timestamp);
        synthetic_pointer_->DispatchEvent(target, timestamp);
        state_ = DONE;
      } else {
        start_time_ = timestamp;
        state_ = WAITING_TO_RELEASE;
      }
      break;
    case WAITING_TO_RELEASE:
      if (timestamp - start_time_ >= GetDuration()) {
        synthetic_pointer_->Release(0, target, start_time_ + GetDuration());
        synthetic_pointer_->DispatchEvent(target, start_time_ + GetDuration());
        state_ = DONE;
      }
      break;
    case SETUP:
      NOTREACHED() << "State SETUP invalid for synthetic tap gesture.";
    case DONE:
      NOTREACHED() << "State DONE invalid for synthetic tap gesture.";
  }
}

base::TimeDelta SyntheticTapGesture::GetDuration() const {
  return base::TimeDelta::FromMilliseconds(params_.duration_ms);
}

}  // namespace content
