// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_touch_pointer.h"

#include "content/browser/renderer_host/input/synthetic_gesture_target.h"

namespace content {

SyntheticTouchPointer::SyntheticTouchPointer() {}

SyntheticTouchPointer::SyntheticTouchPointer(SyntheticWebTouchEvent touch_event)
    : touch_event_(touch_event) {}

SyntheticTouchPointer::~SyntheticTouchPointer() {}

void SyntheticTouchPointer::DispatchEvent(SyntheticGestureTarget* target,
                                          const base::TimeTicks& timestamp) {
  touch_event_.timeStampSeconds = ConvertTimestampToSeconds(timestamp);
  target->DispatchInputEventToPlatform(touch_event_);
}

int SyntheticTouchPointer::Press(float x,
                                 float y,
                                 SyntheticGestureTarget* target,
                                 const base::TimeTicks& timestamp) {
  int index = touch_event_.PressPoint(x, y);
  return index;
}

void SyntheticTouchPointer::Move(int index,
                                 float x,
                                 float y,
                                 SyntheticGestureTarget* target,
                                 const base::TimeTicks& timestamp) {
  touch_event_.MovePoint(index, x, y);
}

void SyntheticTouchPointer::Release(int index,
                                    SyntheticGestureTarget* target,
                                    const base::TimeTicks& timestamp) {
  touch_event_.ReleasePoint(index);
}

}  // namespace content
