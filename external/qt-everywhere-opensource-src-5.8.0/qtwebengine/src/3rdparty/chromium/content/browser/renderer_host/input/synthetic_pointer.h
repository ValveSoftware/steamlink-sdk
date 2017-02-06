// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_POINTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_POINTER_H_

#include <memory>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/common/input/synthetic_gesture_params.h"
#include "content/common/input/synthetic_web_input_event_builders.h"

namespace content {

class SyntheticGestureTarget;

class CONTENT_EXPORT SyntheticPointer {
 public:
  SyntheticPointer();
  virtual ~SyntheticPointer();

  static std::unique_ptr<SyntheticPointer> Create(
      SyntheticGestureParams::GestureSourceType gesture_source_type);

  virtual void DispatchEvent(SyntheticGestureTarget* target,
                             const base::TimeTicks& timestamp) = 0;

  virtual int Press(float x,
                    float y,
                    SyntheticGestureTarget* target,
                    const base::TimeTicks& timestamp) = 0;
  virtual void Move(int index,
                    float x,
                    float y,
                    SyntheticGestureTarget* target,
                    const base::TimeTicks& timestamp) = 0;
  virtual void Release(int index,
                       SyntheticGestureTarget* target,
                       const base::TimeTicks& timestamp) = 0;

 protected:
  static double ConvertTimestampToSeconds(const base::TimeTicks& timestamp);

 private:
  DISALLOW_COPY_AND_ASSIGN(SyntheticPointer);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_POINTER_H_
