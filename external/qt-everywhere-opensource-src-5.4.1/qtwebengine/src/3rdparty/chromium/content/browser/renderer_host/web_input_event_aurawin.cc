// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/web_input_event_aura.h"

#include "base/event_types.h"
#include "base/logging.h"
#include "content/browser/renderer_host/input/web_input_event_builders_win.h"

namespace content {

// On Windows, we can just use the builtin WebKit factory methods to fully
// construct our pre-translated events.

blink::WebMouseEvent MakeUntranslatedWebMouseEventFromNativeEvent(
    const base::NativeEvent& native_event) {
  return WebMouseEventBuilder::Build(native_event.hwnd,
                                     native_event.message,
                                     native_event.wParam,
                                     native_event.lParam,
                                     native_event.time);
}

blink::WebMouseWheelEvent MakeUntranslatedWebMouseWheelEventFromNativeEvent(
    const base::NativeEvent& native_event) {
  return WebMouseWheelEventBuilder::Build(native_event.hwnd,
                                          native_event.message,
                                          native_event.wParam,
                                          native_event.lParam,
                                          native_event.time);
}

blink::WebKeyboardEvent MakeWebKeyboardEventFromNativeEvent(
    const base::NativeEvent& native_event) {
  return WebKeyboardEventBuilder::Build(native_event.hwnd,
                                        native_event.message,
                                        native_event.wParam,
                                        native_event.lParam,
                                        native_event.time);
}

blink::WebGestureEvent MakeWebGestureEventFromNativeEvent(
    const base::NativeEvent& native_event) {
  // TODO: Create gestures from native event.
  NOTIMPLEMENTED();
  return  blink::WebGestureEvent();
}

}  // namespace content
