// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_BLINK_WEB_INPUT_EVENT_BUILDERS_WIN_H_
#define UI_EVENTS_BLINK_WEB_INPUT_EVENT_BUILDERS_WIN_H_

#include <windows.h>

#include "third_party/WebKit/public/platform/WebInputEvent.h"

namespace ui {

class WebKeyboardEventBuilder {
 public:
  static blink::WebKeyboardEvent Build(HWND hwnd,
                                       UINT message,
                                       WPARAM wparam,
                                       LPARAM lparam,
                                       double time_stamp);
};

class WebMouseEventBuilder {
 public:
  static blink::WebMouseEvent Build(
      HWND hwnd,
      UINT message,
      WPARAM wparam,
      LPARAM lparam,
      double time_stamp,
      blink::WebPointerProperties::PointerType pointer_type);
};

class WebMouseWheelEventBuilder {
 public:
  static blink::WebMouseWheelEvent Build(
      HWND hwnd,
      UINT message,
      WPARAM wparam,
      LPARAM lparam,
      double time_stamp,
      blink::WebPointerProperties::PointerType pointer_type);
};

}  // namespace ui

#endif  // UI_EVENTS_BLINK_WEB_INPUT_EVENT_BUILDERS_WIN_H_
