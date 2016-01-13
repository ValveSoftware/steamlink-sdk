// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_H_

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/common/input/synthetic_gesture_params.h"

namespace content {

class SyntheticGestureTarget;

// Base class for synthetic gesture implementations. A synthetic gesture class
// is responsible for forwaring InputEvents, simulating the gesture, to a
// SyntheticGestureTarget.
//
// Adding new gesture types involved the following steps:
//   1) Create a sub-type of SyntheticGesture that implements the gesture.
//   2) Extend SyntheticGesture::Create with the new class.
//   3) Add at least one unit test per supported input source type (touch,
//      mouse, etc) to SyntheticGestureController unit tests. The unit tests
//      only checks basic functionality and termination. If the gesture is
//      hooked up to Telemetry its correctness can additionally be tested there.
class CONTENT_EXPORT SyntheticGesture {
 public:
  SyntheticGesture();
  virtual ~SyntheticGesture();

  static scoped_ptr<SyntheticGesture> Create(
      const SyntheticGestureParams& gesture_params);

  enum Result {
    GESTURE_RUNNING,
    GESTURE_FINISHED,
    GESTURE_SOURCE_TYPE_NOT_IMPLEMENTED,
    GESTURE_RESULT_MAX = GESTURE_SOURCE_TYPE_NOT_IMPLEMENTED
  };

  // Update the state of the gesture and forward the appropriate events to the
  // platform. This function is called repeatedly by the synthetic gesture
  // controller until it stops returning GESTURE_RUNNING.
  virtual Result ForwardInputEvents(
      const base::TimeTicks& timestamp, SyntheticGestureTarget* target) = 0;

 protected:
  static double ConvertTimestampToSeconds(const base::TimeTicks& timestamp);

  DISALLOW_COPY_AND_ASSIGN(SyntheticGesture);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_H_
