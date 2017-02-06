/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebInputEventConversion_h
#define WebInputEventConversion_h

#include "platform/PlatformGestureEvent.h"
#include "platform/PlatformKeyboardEvent.h"
#include "platform/PlatformMouseEvent.h"
#include "platform/PlatformTouchEvent.h"
#include "platform/PlatformWheelEvent.h"
#include "public/web/WebInputEvent.h"
#include "web/WebExport.h"
#include "wtf/Compiler.h"

namespace blink {

class GestureEvent;
class KeyboardEvent;
class MouseEvent;
class LayoutItem;
class TouchEvent;
class WebMouseEvent;
class WebMouseWheelEvent;
class WebKeyboardEvent;
class WebTouchEvent;
class WebGestureEvent;
class WheelEvent;
class Widget;

// These classes are used to convert from WebInputEvent subclasses to
// corresponding WebCore events.

class WEB_EXPORT PlatformMouseEventBuilder : WTF_NON_EXPORTED_BASE(public PlatformMouseEvent) {
public:
    PlatformMouseEventBuilder(Widget*, const WebMouseEvent&);
};

class WEB_EXPORT PlatformWheelEventBuilder : WTF_NON_EXPORTED_BASE(public PlatformWheelEvent) {
public:
    PlatformWheelEventBuilder(Widget*, const WebMouseWheelEvent&);
};

class WEB_EXPORT PlatformGestureEventBuilder : WTF_NON_EXPORTED_BASE(public PlatformGestureEvent) {
public:
    PlatformGestureEventBuilder(Widget*, const WebGestureEvent&);
};

class WEB_EXPORT PlatformKeyboardEventBuilder : WTF_NON_EXPORTED_BASE(public PlatformKeyboardEvent) {
public:
    PlatformKeyboardEventBuilder(const WebKeyboardEvent&);
    void setKeyType(EventType);
    bool isCharacterKey() const;
};

// Converts a WebTouchPoint to a PlatformTouchPoint.
class WEB_EXPORT PlatformTouchPointBuilder : WTF_NON_EXPORTED_BASE(public PlatformTouchPoint) {
public:
    PlatformTouchPointBuilder(Widget*, const WebTouchPoint&);
};

// Converts a WebTouchEvent to a PlatformTouchEvent.
class WEB_EXPORT PlatformTouchEventBuilder : WTF_NON_EXPORTED_BASE(public PlatformTouchEvent) {
public:
    PlatformTouchEventBuilder(Widget*, const WebTouchEvent&);
};

class WEB_EXPORT WebMouseEventBuilder : WTF_NON_EXPORTED_BASE(public WebMouseEvent) {
public:
    // Converts a MouseEvent to a corresponding WebMouseEvent.
    // NOTE: This is only implemented for mousemove, mouseover, mouseout,
    // mousedown and mouseup. If the event mapping fails, the event type will
    // be set to Undefined.
    WebMouseEventBuilder(const Widget*, const LayoutItem, const MouseEvent&);
    WebMouseEventBuilder(const Widget*, const LayoutItem, const TouchEvent&);
};

// Converts a WheelEvent to a corresponding WebMouseWheelEvent.
// If the event mapping fails, the event type will be set to Undefined.
class WEB_EXPORT WebMouseWheelEventBuilder : WTF_NON_EXPORTED_BASE(public WebMouseWheelEvent) {
public:
    WebMouseWheelEventBuilder(const Widget*, const LayoutItem, const WheelEvent&);
};

// Converts a KeyboardEvent or PlatformKeyboardEvent to a
// corresponding WebKeyboardEvent.
// NOTE: For KeyboardEvent, this is only implemented for keydown,
// keyup, and keypress. If the event mapping fails, the event type will be set
// to Undefined.
class WEB_EXPORT WebKeyboardEventBuilder : WTF_NON_EXPORTED_BASE(public WebKeyboardEvent) {
public:
    WebKeyboardEventBuilder(const KeyboardEvent&);
};

// Converts a TouchEvent to a corresponding WebTouchEvent.
// NOTE: WebTouchEvents have a cap on the number of WebTouchPoints. Any points
// exceeding that cap will be dropped.
class WEB_EXPORT WebTouchEventBuilder : WTF_NON_EXPORTED_BASE(public WebTouchEvent) {
public:
    WebTouchEventBuilder(const LayoutItem, const TouchEvent&);
};

// Converts GestureEvent to a corresponding WebGestureEvent.
// NOTE: If event mapping fails, the type will be set to Undefined.
class WEB_EXPORT WebGestureEventBuilder : WTF_NON_EXPORTED_BASE(public WebGestureEvent) {
public:
    WebGestureEventBuilder(const LayoutItem, const GestureEvent&);
};

} // namespace blink

#endif
