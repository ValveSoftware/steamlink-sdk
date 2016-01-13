// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_SYNTHETIC_WEB_INPUT_EVENT_BUILDERS_H_
#define CONTENT_COMMON_INPUT_SYNTHETIC_WEB_INPUT_EVENT_BUILDERS_H_

#include "base/time/time.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

// Provides sensible creation of default WebInputEvents for testing purposes.

namespace content {

class CONTENT_EXPORT SyntheticWebMouseEventBuilder {
 public:
  static blink::WebMouseEvent Build(blink::WebInputEvent::Type type);
  static blink::WebMouseEvent Build(blink::WebInputEvent::Type type,
                                    int window_x,
                                    int window_y,
                                    int modifiers);
};

class CONTENT_EXPORT SyntheticWebMouseWheelEventBuilder {
 public:
  static blink::WebMouseWheelEvent Build(
      blink::WebMouseWheelEvent::Phase phase);
  static blink::WebMouseWheelEvent Build(float dx,
                                         float dy,
                                         int modifiers,
                                         bool precise);
};

class CONTENT_EXPORT SyntheticWebKeyboardEventBuilder {
 public:
  static blink::WebKeyboardEvent Build(blink::WebInputEvent::Type type);
};

class CONTENT_EXPORT SyntheticWebGestureEventBuilder {
 public:
  static blink::WebGestureEvent Build(blink::WebInputEvent::Type type,
                                      blink::WebGestureDevice source_device);
  static blink::WebGestureEvent BuildScrollBegin(float dx_hint,
                                                 float dy_hint);
  static blink::WebGestureEvent BuildScrollUpdate(float dx,
                                                  float dy,
                                                  int modifiers);
  static blink::WebGestureEvent BuildPinchUpdate(
      float scale,
      float anchor_x,
      float anchor_y,
      int modifiers,
      blink::WebGestureDevice source_device);
  static blink::WebGestureEvent BuildFling(
      float velocity_x,
      float velocity_y,
      blink::WebGestureDevice source_device);
};

class CONTENT_EXPORT SyntheticWebTouchEvent
    : public NON_EXPORTED_BASE(blink::WebTouchEvent) {
 public:
  SyntheticWebTouchEvent();

  // Mark all the points as stationary, and remove any released points.
  void ResetPoints();

  // Adds an additional point to the touch list, returning the point's index.
  int PressPoint(float x, float y);
  void MovePoint(int index, float x, float y);
  void ReleasePoint(int index);
  void CancelPoint(int index);

  void SetTimestamp(base::TimeDelta timestamp);
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_SYNTHETIC_WEB_INPUT_EVENT_BUILDERS_H_
