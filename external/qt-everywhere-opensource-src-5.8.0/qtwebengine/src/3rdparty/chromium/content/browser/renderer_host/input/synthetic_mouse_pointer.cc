// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_mouse_pointer.h"

#include "content/browser/renderer_host/input/synthetic_gesture_target.h"

namespace content {

SyntheticMousePointer::SyntheticMousePointer() {}

SyntheticMousePointer::~SyntheticMousePointer() {}

void SyntheticMousePointer::DispatchEvent(SyntheticGestureTarget* target,
                                          const base::TimeTicks& timestamp) {
  mouse_event_.timeStampSeconds = ConvertTimestampToSeconds(timestamp);
  target->DispatchInputEventToPlatform(mouse_event_);
}

int SyntheticMousePointer::Press(float x,
                                 float y,
                                 SyntheticGestureTarget* target,
                                 const base::TimeTicks& timestamp) {
  mouse_event_ = SyntheticWebMouseEventBuilder::Build(
      blink::WebInputEvent::MouseDown, x, y, 0);
  mouse_event_.clickCount = 1;
  return 0;
}

void SyntheticMousePointer::Move(int index,
                                 float x,
                                 float y,
                                 SyntheticGestureTarget* target,
                                 const base::TimeTicks& timestamp) {
  mouse_event_ = SyntheticWebMouseEventBuilder::Build(
      blink::WebInputEvent::MouseMove, x, y, 0);
  mouse_event_.button = blink::WebMouseEvent::ButtonLeft;
}

void SyntheticMousePointer::Release(int index,
                                    SyntheticGestureTarget* target,
                                    const base::TimeTicks& timestamp) {
  mouse_event_ = SyntheticWebMouseEventBuilder::Build(
      blink::WebInputEvent::MouseUp, mouse_event_.x, mouse_event_.y, 0);
  mouse_event_.clickCount = 1;
}

}  // namespace content
