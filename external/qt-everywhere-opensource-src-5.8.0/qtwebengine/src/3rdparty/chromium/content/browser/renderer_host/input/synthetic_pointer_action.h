// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_POINTER_ACTION_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_POINTER_ACTION_H_

#include "base/macros.h"
#include "content/browser/renderer_host/input/synthetic_gesture.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target.h"
#include "content/browser/renderer_host/input/synthetic_pointer.h"
#include "content/common/content_export.h"
#include "content/common/input/synthetic_pointer_action_params.h"

namespace content {

class CONTENT_EXPORT SyntheticPointerAction : public SyntheticGesture {
 public:
  explicit SyntheticPointerAction(const SyntheticPointerActionParams& params);
  SyntheticPointerAction(const SyntheticPointerActionParams& params,
                         SyntheticPointer* synthetic_pointer);
  ~SyntheticPointerAction() override;

  SyntheticGesture::Result ForwardInputEvents(
      const base::TimeTicks& timestamp,
      SyntheticGestureTarget* target) override;

  void ForwardTouchOrMouseInputEvents(const base::TimeTicks& timestamp,
                                      SyntheticGestureTarget* target);

 private:
  SyntheticPointerActionParams params_;
  SyntheticPointer* synthetic_pointer_;

  DISALLOW_COPY_AND_ASSIGN(SyntheticPointerAction);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_POINTER_ACTION_H_
