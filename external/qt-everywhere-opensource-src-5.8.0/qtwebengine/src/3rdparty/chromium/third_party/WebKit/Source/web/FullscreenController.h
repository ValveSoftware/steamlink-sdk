/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef FullscreenController_h
#define FullscreenController_h

#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/IntSize.h"
#include "platform/heap/Handle.h"
#include "wtf/RefPtr.h"

namespace blink {

class Element;
class LocalFrame;
class WebViewImpl;

class FullscreenController final : public GarbageCollected<FullscreenController> {
public:
    static FullscreenController* create(WebViewImpl*);

    void didEnterFullScreen();
    void didExitFullScreen();

    void enterFullScreenForElement(Element*);
    void exitFullScreenForElement(Element*);

    bool isFullscreen() { return m_fullScreenFrame; }

    void updateSize();

    DECLARE_TRACE();

protected:
    explicit FullscreenController(WebViewImpl*);

private:
    void updatePageScaleConstraints(bool removeConstraints);

    WebViewImpl* m_webViewImpl;

    bool m_haveEnteredFullscreen;
    float m_exitFullscreenPageScaleFactor;
    IntSize m_exitFullscreenScrollOffset;
    FloatPoint m_exitFullscreenVisualViewportOffset;

    // If set, the WebView is transitioning to fullscreen for this element.
    Member<Element> m_provisionalFullScreenElement;

    // If set, the WebView is in fullscreen mode for an element in this frame.
    Member<LocalFrame> m_fullScreenFrame;

    bool m_isCancelingFullScreen;
};

} // namespace blink

#endif
