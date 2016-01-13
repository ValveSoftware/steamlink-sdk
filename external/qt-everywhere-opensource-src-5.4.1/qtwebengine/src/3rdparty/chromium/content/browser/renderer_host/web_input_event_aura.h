// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_WEB_INPUT_EVENT_AURA_H_
#define CONTENT_BROWSER_RENDERER_HOST_WEB_INPUT_EVENT_AURA_H_

#include "content/common/content_export.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace ui {
class GestureEvent;
class KeyEvent;
class MouseEvent;
class MouseWheelEvent;
class ScrollEvent;
class TouchEvent;
}

namespace content {

// Used for scrolling. This matches Firefox behavior.
const int kPixelsPerTick = 53;

#if defined(USE_X11) || defined(USE_OZONE)
CONTENT_EXPORT blink::WebUChar GetControlCharacter(
    int windows_key_code, bool shift);
#endif
CONTENT_EXPORT blink::WebMouseEvent MakeWebMouseEvent(
    ui::MouseEvent* event);
CONTENT_EXPORT blink::WebMouseWheelEvent MakeWebMouseWheelEvent(
    ui::MouseWheelEvent* event);
CONTENT_EXPORT blink::WebMouseWheelEvent MakeWebMouseWheelEvent(
    ui::ScrollEvent* event);
CONTENT_EXPORT blink::WebKeyboardEvent MakeWebKeyboardEvent(
    ui::KeyEvent* event);
CONTENT_EXPORT blink::WebGestureEvent MakeWebGestureEvent(
    ui::GestureEvent* event);
CONTENT_EXPORT blink::WebGestureEvent MakeWebGestureEvent(
    ui::ScrollEvent* event);
CONTENT_EXPORT blink::WebGestureEvent MakeWebGestureEventFlingCancel();

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_WEB_INPUT_EVENT_AURA_H_
