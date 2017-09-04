// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_pointer_action.h"

#include "base/logging.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "ui/events/latency_info.h"

namespace content {

SyntheticPointerAction::SyntheticPointerAction(
    const SyntheticPointerActionParams& params)
    : params_(params) {}

SyntheticPointerAction::SyntheticPointerAction(
    std::unique_ptr<std::vector<SyntheticPointerActionParams>> param_list,
    SyntheticPointer* synthetic_pointer,
    IndexMap* index_map)
    : param_list_(std::move(param_list)),
      synthetic_pointer_(synthetic_pointer),
      index_map_(index_map) {}

SyntheticPointerAction::~SyntheticPointerAction() {}

SyntheticGesture::Result SyntheticPointerAction::ForwardInputEvents(
    const base::TimeTicks& timestamp,
    SyntheticGestureTarget* target) {
  DCHECK(synthetic_pointer_);
  return ForwardTouchOrMouseInputEvents(timestamp, target);
}

SyntheticGesture::Result SyntheticPointerAction::ForwardTouchOrMouseInputEvents(
    const base::TimeTicks& timestamp,
    SyntheticGestureTarget* target) {
  int point_index;
  for (const SyntheticPointerActionParams& params : *param_list_) {
    if (!UserInputCheck(params))
      return POINTER_ACTION_INPUT_INVALID;

    switch (params.pointer_action_type()) {
      case SyntheticPointerActionParams::PointerActionType::PRESS:
        point_index = synthetic_pointer_->Press(
            params.position().x(), params.position().y(), target, timestamp);
        SetPointIndex(params.index(), point_index);
        break;
      case SyntheticPointerActionParams::PointerActionType::MOVE:
        point_index = GetPointIndex(params.index());
        synthetic_pointer_->Move(point_index, params.position().x(),
                                 params.position().y(), target, timestamp);
        break;
      case SyntheticPointerActionParams::PointerActionType::RELEASE:
        point_index = GetPointIndex(params.index());
        synthetic_pointer_->Release(point_index, target, timestamp);
        SetPointIndex(params.index(), -1);
        break;
      default:
        return POINTER_ACTION_INPUT_INVALID;
    }
  }
  synthetic_pointer_->DispatchEvent(target, timestamp);
  return GESTURE_FINISHED;
}

bool SyntheticPointerAction::UserInputCheck(
    const SyntheticPointerActionParams& params) {
  if (params.index() < 0 || params.index() >= WebTouchEvent::kTouchesLengthCap)
    return false;

  if (synthetic_pointer_->SourceType() != params.gesture_source_type)
    return false;

  if (params.pointer_action_type() ==
          SyntheticPointerActionParams::PointerActionType::PRESS &&
      GetPointIndex(params.index()) >= 0) {
    return false;
  }

  if (synthetic_pointer_->SourceType() == SyntheticGestureParams::TOUCH_INPUT &&
      params.pointer_action_type() ==
          SyntheticPointerActionParams::PointerActionType::MOVE &&
      GetPointIndex(params.index()) < 0) {
    return false;
  }

  if (params.pointer_action_type() ==
          SyntheticPointerActionParams::PointerActionType::RELEASE &&
      GetPointIndex(params.index()) < 0) {
    return false;
  }
  return true;
}

}  // namespace content
