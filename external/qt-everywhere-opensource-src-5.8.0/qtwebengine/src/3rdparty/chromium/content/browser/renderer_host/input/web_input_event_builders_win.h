// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_WEB_INPUT_EVENT_BUILDERS_WIN_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_WEB_INPUT_EVENT_BUILDERS_WIN_H_

#include <windows.h>

#include "content/common/content_export.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace content {

class CONTENT_EXPORT WebKeyboardEventBuilder {
 public:
  static blink::WebKeyboardEvent Build(HWND hwnd,
                                       UINT message,
                                       WPARAM wparam,
                                       LPARAM lparam,
                                       double time_stamp);
};

class CONTENT_EXPORT WebMouseEventBuilder {
 public:
  static blink::WebMouseEvent Build(
      HWND hwnd,
      UINT message,
      WPARAM wparam,
      LPARAM lparam,
      double time_stamp,
      blink::WebPointerProperties::PointerType pointer_type);
};

class CONTENT_EXPORT WebMouseWheelEventBuilder {
 public:
  static blink::WebMouseWheelEvent Build(
      HWND hwnd,
      UINT message,
      WPARAM wparam,
      LPARAM lparam,
      double time_stamp,
      blink::WebPointerProperties::PointerType pointer_type);
};

} // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_WEB_INPUT_EVENT_BUILDERS_WIN_H_
