/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef PageWidgetDelegate_h
#define PageWidgetDelegate_h

#include "public/platform/WebCanvas.h"
#include "public/web/WebInputEvent.h"
#include "public/web/WebWidget.h"
#include "web/WebExport.h"

namespace blink {

class LocalFrame;
class Page;
class WebGestureEvent;
class WebInputEvent;
class WebKeyboardEvent;
class WebMouseEvent;
class WebMouseWheelEvent;
class WebTouchEvent;

class WEB_EXPORT PageWidgetEventHandler {
public:
    virtual void handleMouseMove(LocalFrame& mainFrame, const WebMouseEvent&);
    virtual void handleMouseLeave(LocalFrame& mainFrame, const WebMouseEvent&);
    virtual void handleMouseDown(LocalFrame& mainFrame, const WebMouseEvent&);
    virtual void handleMouseUp(LocalFrame& mainFrame, const WebMouseEvent&);
    virtual WebInputEventResult handleMouseWheel(LocalFrame& mainFrame, const WebMouseWheelEvent&);
    virtual WebInputEventResult handleKeyEvent(const WebKeyboardEvent&) = 0;
    virtual WebInputEventResult handleCharEvent(const WebKeyboardEvent&) = 0;
    virtual WebInputEventResult handleGestureEvent(const WebGestureEvent&) = 0;
    virtual WebInputEventResult handleTouchEvent(LocalFrame& mainFrame, const WebTouchEvent&);
    virtual ~PageWidgetEventHandler() { }
protected:
    const char* inputTypeToName(WebInputEvent::Type);
};


// Common implementation of WebViewImpl and WebPagePopupImpl.
class PageWidgetDelegate {
public:
    static void animate(Page&, double monotonicFrameBeginTime);

    // For the following methods, the |root| argument indicates a root localFrame from which
    // to start performing the specified operation.

    // See documents of methods with the same names in FrameView class.
    static void updateAllLifecyclePhases(Page&, LocalFrame& root);

    static void paint(Page&, WebCanvas*, const WebRect&, LocalFrame& root);
    static void paintIgnoringCompositing(Page&, WebCanvas*, const WebRect&, LocalFrame& root);

    // See FIXME in the function body about nullptr |root|.
    static WebInputEventResult handleInputEvent(PageWidgetEventHandler&, const WebInputEvent&, LocalFrame* root);

private:
    PageWidgetDelegate() { }
};

} // namespace blink
#endif
