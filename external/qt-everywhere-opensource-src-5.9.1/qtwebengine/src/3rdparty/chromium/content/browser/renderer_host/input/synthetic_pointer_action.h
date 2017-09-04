// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_POINTER_ACTION_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_POINTER_ACTION_H_

#include <array>
#include "base/macros.h"
#include "content/browser/renderer_host/input/synthetic_gesture.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target.h"
#include "content/browser/renderer_host/input/synthetic_pointer.h"
#include "content/common/content_export.h"
#include "content/common/input/synthetic_pointer_action_params.h"

using blink::WebTouchEvent;

namespace content {

class CONTENT_EXPORT SyntheticPointerAction : public SyntheticGesture {
 public:
  using IndexMap = std::array<int, WebTouchEvent::kTouchesLengthCap>;

  SyntheticPointerAction(
      std::unique_ptr<std::vector<SyntheticPointerActionParams>> param_list,
      SyntheticPointer* synthetic_pointer,
      IndexMap* index_map);
  ~SyntheticPointerAction() override;

  SyntheticGesture::Result ForwardInputEvents(
      const base::TimeTicks& timestamp,
      SyntheticGestureTarget* target) override;

 private:
  explicit SyntheticPointerAction(const SyntheticPointerActionParams& params);
  SyntheticGesture::Result ForwardTouchOrMouseInputEvents(
      const base::TimeTicks& timestamp,
      SyntheticGestureTarget* target);
  int GetPointIndex(int index) const { return (*index_map_)[index]; }
  void SetPointIndex(int index, int point_index) {
    (*index_map_)[index] = point_index;
  }

  // Check if the user inputs in the SyntheticPointerActionParams can generate
  // a valid sequence of pointer actions.
  bool UserInputCheck(const SyntheticPointerActionParams& params);

  // SyntheticGestureController is responsible to create the
  // SyntheticPointerActions and control when to forward them.
  // This will be passed from SyntheticGestureController, which will reset its
  // value to push a new batch of action parameters.
  // This contains a list of pointer actions which will be dispatched together.
  const std::unique_ptr<std::vector<SyntheticPointerActionParams>> param_list_;
  // These two objects will be owned by SyntheticGestureController, which
  // will manage their lifetime by initiating them when it starts processing a
  // pointer action sequence and resetting them when it finishes.
  SyntheticPointer* synthetic_pointer_;
  IndexMap* index_map_;

  SyntheticPointerActionParams params_;

  DISALLOW_COPY_AND_ASSIGN(SyntheticPointerAction);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_POINTER_ACTION_H_
