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

#include "config.h"
#include "web/WebPagePopupImpl.h"

#include "core/dom/ContextFeatures.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/loader/EmptyClients.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/page/Chrome.h"
#include "core/page/DOMWindowPagePopup.h"
#include "core/page/EventHandler.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "core/page/PagePopupClient.h"
#include "platform/TraceEvent.h"
#include "public/platform/WebCursorInfo.h"
#include "public/web/WebViewClient.h"
#include "public/web/WebWidgetClient.h"
#include "web/WebInputEventConversion.h"
#include "web/WebSettingsImpl.h"
#include "web/WebViewImpl.h"

using namespace WebCore;

namespace blink {

class PagePopupChromeClient : public EmptyChromeClient {
    WTF_MAKE_NONCOPYABLE(PagePopupChromeClient);
    WTF_MAKE_FAST_ALLOCATED;

public:
    explicit PagePopupChromeClient(WebPagePopupImpl* popup)
        : m_popup(popup)
    {
        ASSERT(m_popup->widgetClient());
    }

private:
    virtual void closeWindowSoon() OVERRIDE
    {
        m_popup->closePopup();
    }

    virtual FloatRect windowRect() OVERRIDE
    {
        return FloatRect(m_popup->m_windowRectInScreen.x, m_popup->m_windowRectInScreen.y, m_popup->m_windowRectInScreen.width, m_popup->m_windowRectInScreen.height);
    }

    virtual void setWindowRect(const FloatRect& rect) OVERRIDE
    {
        m_popup->m_windowRectInScreen = IntRect(rect);
        m_popup->widgetClient()->setWindowRect(m_popup->m_windowRectInScreen);
    }

    virtual void addMessageToConsole(LocalFrame*, MessageSource, MessageLevel, const String& message, unsigned lineNumber, const String&, const String&) OVERRIDE
    {
#ifndef NDEBUG
        fprintf(stderr, "CONSOLE MESSSAGE:%u: %s\n", lineNumber, message.utf8().data());
#endif
    }

    virtual void invalidateContentsAndRootView(const IntRect& paintRect) OVERRIDE
    {
        if (paintRect.isEmpty())
            return;
        m_popup->widgetClient()->didInvalidateRect(paintRect);
    }

    virtual void scroll(const IntSize& scrollDelta, const IntRect& scrollRect, const IntRect& clipRect) OVERRIDE
    {
        m_popup->widgetClient()->didScrollRect(scrollDelta.width(), scrollDelta.height(), intersection(scrollRect, clipRect));
    }

    virtual void invalidateContentsForSlowScroll(const IntRect& updateRect) OVERRIDE
    {
        invalidateContentsAndRootView(updateRect);
    }

    virtual void scheduleAnimation() OVERRIDE
    {
        if (m_popup->isAcceleratedCompositingActive()) {
            ASSERT(m_popup->m_layerTreeView);
            m_popup->m_layerTreeView->setNeedsAnimate();
            return;
        }
        m_popup->widgetClient()->scheduleAnimation();
    }

    virtual WebScreenInfo screenInfo() const OVERRIDE
    {
        return m_popup->m_webView->client() ? m_popup->m_webView->client()->screenInfo() : WebScreenInfo();
    }

    virtual void* webView() const OVERRIDE
    {
        return m_popup->m_webView;
    }

    virtual FloatSize minimumWindowSize() const OVERRIDE
    {
        return FloatSize(0, 0);
    }

    virtual void setCursor(const WebCore::Cursor& cursor) OVERRIDE
    {
        if (m_popup->m_webView->client())
            m_popup->m_webView->client()->didChangeCursor(WebCursorInfo(cursor));
    }

    virtual void needTouchEvents(bool needsTouchEvents) OVERRIDE
    {
        m_popup->widgetClient()->hasTouchEventHandlers(needsTouchEvents);
    }

    virtual GraphicsLayerFactory* graphicsLayerFactory() const OVERRIDE
    {
        return m_popup->m_webView->graphicsLayerFactory();
    }

    virtual void attachRootGraphicsLayer(GraphicsLayer* graphicsLayer) OVERRIDE
    {
        m_popup->setRootGraphicsLayer(graphicsLayer);
    }

    WebPagePopupImpl* m_popup;
};

class PagePopupFeaturesClient : public ContextFeaturesClient {
    virtual bool isEnabled(Document*, ContextFeatures::FeatureType, bool) OVERRIDE;
};

bool PagePopupFeaturesClient::isEnabled(Document*, ContextFeatures::FeatureType type, bool defaultValue)
{
    if (type == ContextFeatures::PagePopup)
        return true;
    return defaultValue;
}

// WebPagePopupImpl ----------------------------------------------------------------

WebPagePopupImpl::WebPagePopupImpl(WebWidgetClient* client)
    : m_widgetClient(client)
    , m_closing(false)
    , m_layerTreeView(0)
    , m_rootLayer(0)
    , m_rootGraphicsLayer(0)
    , m_isAcceleratedCompositingActive(false)
{
    ASSERT(client);
}

WebPagePopupImpl::~WebPagePopupImpl()
{
    ASSERT(!m_page);
}

bool WebPagePopupImpl::initialize(WebViewImpl* webView, PagePopupClient* popupClient, const IntRect&)
{
    ASSERT(webView);
    ASSERT(popupClient);
    m_webView = webView;
    m_popupClient = popupClient;

    resize(m_popupClient->contentSize());

    if (!initializePage())
        return false;
    m_widgetClient->show(WebNavigationPolicy());
    setFocus(true);

    return true;
}

bool WebPagePopupImpl::initializePage()
{
    Page::PageClients pageClients;
    fillWithEmptyClients(pageClients);
    m_chromeClient = adoptPtr(new PagePopupChromeClient(this));
    pageClients.chromeClient = m_chromeClient.get();

    m_page = adoptPtrWillBeNoop(new Page(pageClients));
    m_page->settings().setScriptEnabled(true);
    m_page->settings().setAllowScriptsToCloseWindows(true);
    m_page->setDeviceScaleFactor(m_webView->deviceScaleFactor());
    m_page->settings().setDeviceSupportsTouch(m_webView->page()->settings().deviceSupportsTouch());

    provideContextFeaturesTo(*m_page, adoptPtr(new PagePopupFeaturesClient()));
    static FrameLoaderClient* emptyFrameLoaderClient =  new EmptyFrameLoaderClient();
    RefPtr<LocalFrame> frame = LocalFrame::create(emptyFrameLoaderClient, &m_page->frameHost(), 0);
    frame->setView(FrameView::create(frame.get()));
    frame->init();
    frame->view()->resize(m_popupClient->contentSize());
    frame->view()->setTransparent(false);

    ASSERT(frame->domWindow());
    DOMWindowPagePopup::install(*frame->domWindow(), m_popupClient);

    RefPtr<SharedBuffer> data = SharedBuffer::create();
    m_popupClient->writeDocument(data.get());
    frame->loader().load(FrameLoadRequest(0, blankURL(), SubstituteData(data, "text/html", "UTF-8", KURL(), ForceSynchronousLoad)));
    return true;
}

void WebPagePopupImpl::destroyPage()
{
    if (!m_page)
        return;

    m_page->willBeDestroyed();
    m_page.clear();
}

void WebPagePopupImpl::setRootGraphicsLayer(GraphicsLayer* layer)
{
    m_rootGraphicsLayer = layer;
    m_rootLayer = layer ? layer->platformLayer() : 0;

    setIsAcceleratedCompositingActive(layer);
    if (m_layerTreeView) {
        if (m_rootLayer) {
            m_layerTreeView->setRootLayer(*m_rootLayer);
        } else {
            m_layerTreeView->clearRootLayer();
        }
    }
}

void WebPagePopupImpl::setIsAcceleratedCompositingActive(bool enter)
{
    if (m_isAcceleratedCompositingActive == enter)
        return;

    if (!enter) {
        m_isAcceleratedCompositingActive = false;
    } else if (m_layerTreeView) {
        m_isAcceleratedCompositingActive = true;
    } else {
        TRACE_EVENT0("webkit", "WebPagePopupImpl::setIsAcceleratedCompositingActive(true)");

        m_widgetClient->initializeLayerTreeView();
        m_layerTreeView = m_widgetClient->layerTreeView();
        if (m_layerTreeView) {
            m_layerTreeView->setVisible(true);
            m_isAcceleratedCompositingActive = true;
            m_layerTreeView->setDeviceScaleFactor(m_widgetClient->deviceScaleFactor());
        } else {
            m_isAcceleratedCompositingActive = false;
        }
    }
}

WebSize WebPagePopupImpl::size()
{
    return m_popupClient->contentSize();
}

void WebPagePopupImpl::animate(double)
{
    PageWidgetDelegate::animate(m_page.get(), monotonicallyIncreasingTime());
}

void WebPagePopupImpl::willCloseLayerTreeView()
{
    setIsAcceleratedCompositingActive(false);
    m_layerTreeView = 0;
}

void WebPagePopupImpl::layout()
{
    PageWidgetDelegate::layout(m_page.get());
}

void WebPagePopupImpl::paint(WebCanvas* canvas, const WebRect& rect)
{
    if (!m_closing)
        PageWidgetDelegate::paint(m_page.get(), 0, canvas, rect, PageWidgetDelegate::Opaque);
}

void WebPagePopupImpl::resize(const WebSize& newSize)
{
    m_windowRectInScreen = WebRect(m_windowRectInScreen.x, m_windowRectInScreen.y, newSize.width, newSize.height);
    m_widgetClient->setWindowRect(m_windowRectInScreen);

    if (m_page)
        toLocalFrame(m_page->mainFrame())->view()->resize(newSize);
    m_widgetClient->didInvalidateRect(WebRect(0, 0, newSize.width, newSize.height));
}

bool WebPagePopupImpl::handleKeyEvent(const WebKeyboardEvent&)
{
    // The main WebView receives key events and forward them to this via handleKeyEvent().
    ASSERT_NOT_REACHED();
    return false;
}

bool WebPagePopupImpl::handleCharEvent(const WebKeyboardEvent&)
{
    // The main WebView receives key events and forward them to this via handleKeyEvent().
    ASSERT_NOT_REACHED();
    return false;
}

bool WebPagePopupImpl::handleGestureEvent(const WebGestureEvent& event)
{
    if (m_closing || !m_page || !m_page->mainFrame() || !toLocalFrame(m_page->mainFrame())->view())
        return false;
    LocalFrame& frame = *toLocalFrame(m_page->mainFrame());
    return frame.eventHandler().handleGestureEvent(PlatformGestureEventBuilder(frame.view(), event));
}

bool WebPagePopupImpl::handleInputEvent(const WebInputEvent& event)
{
    if (m_closing)
        return false;
    return PageWidgetDelegate::handleInputEvent(m_page.get(), *this, event);
}

bool WebPagePopupImpl::handleKeyEvent(const PlatformKeyboardEvent& event)
{
    if (m_closing || !m_page->mainFrame() || !toLocalFrame(m_page->mainFrame())->view())
        return false;
    return toLocalFrame(m_page->mainFrame())->eventHandler().keyEvent(event);
}

void WebPagePopupImpl::setFocus(bool enable)
{
    if (!m_page)
        return;
    m_page->focusController().setFocused(enable);
    if (enable)
        m_page->focusController().setActive(true);
}

void WebPagePopupImpl::close()
{
    m_closing = true;
    destroyPage(); // In case closePopup() was not called.
    m_widgetClient = 0;
    deref();
}

void WebPagePopupImpl::closePopup()
{
    if (m_page) {
        toLocalFrame(m_page->mainFrame())->loader().stopAllLoaders();
        ASSERT(m_page->mainFrame()->domWindow());
        DOMWindowPagePopup::uninstall(*m_page->mainFrame()->domWindow());
    }
    m_closing = true;

    destroyPage();

    // m_widgetClient might be 0 because this widget might be already closed.
    if (m_widgetClient) {
        // closeWidgetSoon() will call this->close() later.
        m_widgetClient->closeWidgetSoon();
    }

    m_popupClient->didClosePopup();
}

// WebPagePopup ----------------------------------------------------------------

WebPagePopup* WebPagePopup::create(WebWidgetClient* client)
{
    if (!client)
        CRASH();
    // A WebPagePopupImpl instance usually has two references.
    //  - One owned by the instance itself. It represents the visible widget.
    //  - One owned by a WebViewImpl. It's released when the WebViewImpl ask the
    //    WebPagePopupImpl to close.
    // We need them because the closing operation is asynchronous and the widget
    // can be closed while the WebViewImpl is unaware of it.
    return adoptRef(new WebPagePopupImpl(client)).leakRef();
}

} // namespace blink
