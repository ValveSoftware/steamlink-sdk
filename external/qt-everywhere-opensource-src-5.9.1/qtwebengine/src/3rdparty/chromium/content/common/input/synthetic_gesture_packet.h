// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_SYNTHETIC_GESTURE_PACKET_H_
#define CONTENT_COMMON_INPUT_SYNTHETIC_GESTURE_PACKET_H_

#include <memory>
#include <utility>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/common/input/synthetic_gesture_params.h"

namespace content {

// Wraps an object of type SyntheticGestureParams (or one of its subclasses) for
// sending it over IPC.
class CONTENT_EXPORT SyntheticGesturePacket {
 public:
  SyntheticGesturePacket();
  ~SyntheticGesturePacket();

  void set_gesture_params(
      std::unique_ptr<SyntheticGestureParams> gesture_params) {
    gesture_params_ = std::move(gesture_params);
  }
  const SyntheticGestureParams* gesture_params() const {
    return gesture_params_.get();
  }
  std::unique_ptr<SyntheticGestureParams> pass_gesture_params() {
    return std::move(gesture_params_);
  }

 private:
  std::unique_ptr<SyntheticGestureParams> gesture_params_;

  DISALLOW_COPY_AND_ASSIGN(SyntheticGesturePacket);
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_SYNTHETIC_GESTURE_PACKET_H_
