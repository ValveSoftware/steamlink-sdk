// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_WEB_INPUT_EVENT_BUILDERS_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_WEB_INPUT_EVENT_BUILDERS_ANDROID_H_

#include <jni.h>

#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace content {

class MotionEventAndroid;

class WebMouseEventBuilder {
 public:
  static blink::WebMouseEvent Build(blink::WebInputEvent::Type type,
                                    blink::WebMouseEvent::Button button,
                                    double time_sec,
                                    int window_x,
                                    int window_y,
                                    int modifiers,
                                    int click_count);
};

class WebMouseWheelEventBuilder {
 public:
  enum Direction {
    DIRECTION_UP,
    DIRECTION_DOWN,
    DIRECTION_LEFT,
    DIRECTION_RIGHT,
  };

  static blink::WebMouseWheelEvent Build(Direction direction,
                                         double time_sec,
                                         int window_x,
                                         int window_y);
};

class WebKeyboardEventBuilder {
 public:
  static blink::WebKeyboardEvent Build(blink::WebInputEvent::Type type,
                                       int modifiers,
                                       double time_sec,
                                       int keycode,
                                       int unicode_character,
                                       bool is_system_key);
};

class WebGestureEventBuilder {
 public:
  static blink::WebGestureEvent Build(blink::WebInputEvent::Type type,
                                      double time_sec,
                                      int x,
                                      int y);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_WEB_INPUT_EVENT_BUILDERS_ANDROID_H_
