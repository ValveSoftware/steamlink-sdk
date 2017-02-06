// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_pointer.h"

#include "base/memory/ptr_util.h"
#include "content/browser/renderer_host/input/synthetic_mouse_pointer.h"
#include "content/browser/renderer_host/input/synthetic_touch_pointer.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace content {

SyntheticPointer::SyntheticPointer() {}
SyntheticPointer::~SyntheticPointer() {}

// static
std::unique_ptr<SyntheticPointer> SyntheticPointer::Create(
    SyntheticGestureParams::GestureSourceType gesture_source_type) {
  if (gesture_source_type == SyntheticGestureParams::TOUCH_INPUT) {
    return base::WrapUnique(new SyntheticTouchPointer());
  } else if (gesture_source_type == SyntheticGestureParams::MOUSE_INPUT) {
    return base::WrapUnique(new SyntheticMousePointer());
  } else {
    NOTREACHED() << "Invalid gesture source type";
    return std::unique_ptr<SyntheticPointer>();
  }
}

// static
double SyntheticPointer::ConvertTimestampToSeconds(
    const base::TimeTicks& timestamp) {
  return (timestamp - base::TimeTicks()).InSecondsF();
}

}  // namespace content
