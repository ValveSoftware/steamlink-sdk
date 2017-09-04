// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_SYNTHETIC_GESTURE_PARAMS_H_
#define CONTENT_COMMON_INPUT_SYNTHETIC_GESTURE_PARAMS_H_

#include <memory>

#include "content/common/content_export.h"

namespace content {

// Base class for storing parameters of synthetic gestures. Sending an object
// over IPC is handled by encapsulating it in a SyntheticGesturePacket object.
//
// The subclasses of this class only store data on synthetic gestures.
// The logic for dispatching input events that implement the gesture lives
// in separate classes in content/browser/renderer_host/input/.
//
// Adding new gesture types involves the following steps:
//   1) Create a new sub-type of SyntheticGestureParams with the parameters
//      needed for the new gesture.
//   2) Use IPC macros to create serialization methods for the new type in
//      content/common/input_messages.h.
//   3) Extend ParamTraits<content::SyntheticGesturePacket>::Write/Read/Log in
//      content/common/input/input_param_traits.cc.
//   4) Add a new unit test to make sure that sending the type over IPC works
//      correctly.
// The details of each step should become clear when looking at other types.
struct CONTENT_EXPORT SyntheticGestureParams {
  SyntheticGestureParams();
  SyntheticGestureParams(const SyntheticGestureParams& other);
  virtual ~SyntheticGestureParams();

  // Describes which type of input events synthetic gesture objects should
  // generate. When specifying DEFAULT_INPUT the platform will be queried for
  // the preferred input event type.
  enum GestureSourceType {
    DEFAULT_INPUT,
    TOUCH_INPUT,
    MOUSE_INPUT,
    GESTURE_SOURCE_TYPE_MAX = MOUSE_INPUT
  };
  GestureSourceType gesture_source_type;

  enum GestureType {
    SMOOTH_SCROLL_GESTURE,
    SMOOTH_DRAG_GESTURE,
    PINCH_GESTURE,
    TAP_GESTURE,
    POINTER_ACTION,
    SYNTHETIC_GESTURE_TYPE_MAX = POINTER_ACTION
  };

  virtual GestureType GetGestureType() const = 0;

  // Returns true if the specific gesture source type is supported on this
  // platform.
  static bool IsGestureSourceTypeSupported(
      GestureSourceType gesture_source_type);
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_SYNTHETIC_GESTURE_PARAMS_H_
