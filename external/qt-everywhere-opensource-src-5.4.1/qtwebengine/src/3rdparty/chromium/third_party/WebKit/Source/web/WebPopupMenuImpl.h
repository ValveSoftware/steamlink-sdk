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

#ifndef WebPopupMenuImpl_h
#define WebPopupMenuImpl_h

#include "platform/scroll/FramelessScrollViewClient.h"
#include "public/platform/WebContentLayerClient.h"
#include "public/platform/WebPoint.h"
#include "public/platform/WebSize.h"
#include "public/web/WebPopupMenu.h"
#include "wtf/OwnPtr.h"
#include "wtf/RefCounted.h"

namespace WebCore {
class LocalFrame;
class FramelessScrollView;
class KeyboardEvent;
class Page;
class PlatformKeyboardEvent;
class Range;
class Widget;
}

namespace blink {
class WebContentLayer;
class WebGestureEvent;
class WebKeyboardEvent;
class WebLayerTreeView;
class WebMouseEvent;
class WebMouseWheelEvent;
class WebRange;
struct WebRect;
class WebTouchEvent;

class WebPopupMenuImpl : public WebPopupMenu, public WebCore::FramelessScrollViewClient, public WebContentLayerClient, public RefCounted<WebPopupMenuImpl> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    // WebWidget functions:
    virtual void close() OVERRIDE FINAL;
    virtual WebSize size() OVERRIDE FINAL { return m_size; }
    virtual void willStartLiveResize() OVERRIDE FINAL;
    virtual void resize(const WebSize&) OVERRIDE FINAL;
    virtual void willEndLiveResize() OVERRIDE FINAL;
    virtual void animate(double frameBeginTime) OVERRIDE FINAL;
    virtual void layout() OVERRIDE FINAL;
    virtual void paint(WebCanvas*, const WebRect&) OVERRIDE FINAL;
    virtual void themeChanged() OVERRIDE FINAL;
    virtual bool handleInputEvent(const WebInputEvent&) OVERRIDE FINAL;
    virtual void mouseCaptureLost() OVERRIDE FINAL;
    virtual void setFocus(bool enable) OVERRIDE FINAL;
    virtual bool setComposition(
        const WebString& text,
        const WebVector<WebCompositionUnderline>& underlines,
        int selectionStart, int selectionEnd) OVERRIDE FINAL;
    virtual bool confirmComposition() OVERRIDE FINAL;
    virtual bool confirmComposition(ConfirmCompositionBehavior selectionBehavior) OVERRIDE FINAL;
    virtual bool confirmComposition(const WebString& text) OVERRIDE FINAL;
    virtual bool compositionRange(size_t* location, size_t* length) OVERRIDE FINAL;
    virtual bool caretOrSelectionRange(size_t* location, size_t* length) OVERRIDE FINAL;
    virtual void setTextDirection(WebTextDirection) OVERRIDE FINAL;
    virtual bool isAcceleratedCompositingActive() const OVERRIDE FINAL { return false; }
    virtual bool isPopupMenu() const OVERRIDE FINAL { return true; }
    virtual void willCloseLayerTreeView() OVERRIDE FINAL;

    // WebContentLayerClient
    virtual void paintContents(WebCanvas*, const WebRect& clip, bool canPaintLCDTest, WebFloatRect& opaque,
        WebContentLayerClient::GraphicsContextStatus = GraphicsContextEnabled) OVERRIDE FINAL;

    // WebPopupMenuImpl
    void initialize(WebCore::FramelessScrollView* widget, const WebRect& bounds);

    WebWidgetClient* client() { return m_client; }

    void handleMouseMove(const WebMouseEvent&);
    void handleMouseLeave(const WebMouseEvent&);
    void handleMouseDown(const WebMouseEvent&);
    void handleMouseUp(const WebMouseEvent&);
    void handleMouseDoubleClick(const WebMouseEvent&);
    void handleMouseWheel(const WebMouseWheelEvent&);
    bool handleGestureEvent(const WebGestureEvent&);
    bool handleTouchEvent(const WebTouchEvent&);
    bool handleKeyEvent(const WebKeyboardEvent&);

   protected:
    friend class WebPopupMenu; // For WebPopupMenu::create.
    friend class WTF::RefCounted<WebPopupMenuImpl>;

    WebPopupMenuImpl(WebWidgetClient*);
    ~WebPopupMenuImpl();

    // WebCore::HostWindow methods:
    virtual void invalidateContentsAndRootView(const WebCore::IntRect&) OVERRIDE FINAL;
    virtual void invalidateContentsForSlowScroll(const WebCore::IntRect&) OVERRIDE FINAL;
    virtual void scheduleAnimation() OVERRIDE FINAL;
    virtual void scroll(
        const WebCore::IntSize& scrollDelta, const WebCore::IntRect& scrollRect,
        const WebCore::IntRect& clipRect) OVERRIDE FINAL;
    virtual WebCore::IntRect rootViewToScreen(const WebCore::IntRect&) const OVERRIDE FINAL;
    virtual WebScreenInfo screenInfo() const OVERRIDE FINAL;

    // WebCore::FramelessScrollViewClient methods:
    virtual void popupClosed(WebCore::FramelessScrollView*) OVERRIDE FINAL;

    WebWidgetClient* m_client;
    WebSize m_size;

    WebLayerTreeView* m_layerTreeView;
    OwnPtr<WebContentLayer> m_rootLayer;

    WebPoint m_lastMousePosition;

    // This is a non-owning ref. The popup will notify us via popupClosed()
    // before it is destroyed.
    WebCore::FramelessScrollView* m_widget;
};

DEFINE_TYPE_CASTS(WebPopupMenuImpl, WebWidget, widget, widget->isPopupMenu(), widget.isPopupMenu());
// WebPopupMenuImpl is the only implementation of FramelessScrollViewClient, so
// no need for further checking.
DEFINE_TYPE_CASTS(WebPopupMenuImpl, WebCore::FramelessScrollViewClient, client, true, true);

} // namespace blink

#endif
