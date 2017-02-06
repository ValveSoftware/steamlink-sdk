// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_pointer_action.h"

#include "base/logging.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/latency_info.h"

namespace content {

SyntheticPointerAction::SyntheticPointerAction(
    const SyntheticPointerActionParams& params)
    : params_(params) {}

SyntheticPointerAction::SyntheticPointerAction(
    const SyntheticPointerActionParams& params,
    SyntheticPointer* synthetic_pointer)
    : params_(params), synthetic_pointer_(synthetic_pointer) {}

SyntheticPointerAction::~SyntheticPointerAction() {}

SyntheticGesture::Result SyntheticPointerAction::ForwardInputEvents(
    const base::TimeTicks& timestamp,
    SyntheticGestureTarget* target) {
  if (params_.gesture_source_type == SyntheticGestureParams::DEFAULT_INPUT)
    params_.gesture_source_type =
        target->GetDefaultSyntheticGestureSourceType();

  DCHECK_NE(params_.gesture_source_type, SyntheticGestureParams::DEFAULT_INPUT);

  ForwardTouchOrMouseInputEvents(timestamp, target);
  return SyntheticGesture::GESTURE_FINISHED;
}

void SyntheticPointerAction::ForwardTouchOrMouseInputEvents(
    const base::TimeTicks& timestamp,
    SyntheticGestureTarget* target) {
  switch (params_.pointer_action_type()) {
    case SyntheticPointerActionParams::PointerActionType::PRESS:
      synthetic_pointer_->Press(params_.position().x(), params_.position().y(),
                                target, timestamp);
      break;
    case SyntheticPointerActionParams::PointerActionType::MOVE:
      synthetic_pointer_->Move(params_.index(), params_.position().x(),
                               params_.position().y(), target, timestamp);
      break;
    case SyntheticPointerActionParams::PointerActionType::RELEASE:
      synthetic_pointer_->Release(params_.index(), target, timestamp);
      break;
    default:
      NOTREACHED();
      break;
  }
  synthetic_pointer_->DispatchEvent(target, timestamp);
}

}  // namespace content
