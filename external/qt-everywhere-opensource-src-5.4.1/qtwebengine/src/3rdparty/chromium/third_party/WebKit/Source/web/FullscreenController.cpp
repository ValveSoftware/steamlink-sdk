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

#include "config.h"
#include "web/FullscreenController.h"

#include "core/dom/Document.h"
#include "core/dom/FullscreenElementStack.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLMediaElement.h"
#include "platform/LayoutTestSupport.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "public/web/WebFrame.h"
#include "public/web/WebViewClient.h"
#include "web/WebViewImpl.h"

using namespace WebCore;

namespace blink {

PassOwnPtr<FullscreenController> FullscreenController::create(WebViewImpl* webViewImpl)
{
    return adoptPtr(new FullscreenController(webViewImpl));
}

FullscreenController::FullscreenController(WebViewImpl* webViewImpl)
    : m_webViewImpl(webViewImpl)
    , m_exitFullscreenPageScaleFactor(0)
    , m_isCancelingFullScreen(false)
{
}

void FullscreenController::willEnterFullScreen()
{
    if (!m_provisionalFullScreenElement)
        return;

    // Ensure that this element's document is still attached.
    Document& doc = m_provisionalFullScreenElement->document();
    if (doc.frame()) {
        FullscreenElementStack::from(doc).webkitWillEnterFullScreenForElement(m_provisionalFullScreenElement.get());
        m_fullScreenFrame = doc.frame();
    }
    m_provisionalFullScreenElement.clear();
}

void FullscreenController::didEnterFullScreen()
{
    if (!m_fullScreenFrame)
        return;

    if (Document* doc = m_fullScreenFrame->document()) {
        if (FullscreenElementStack::isFullScreen(*doc)) {
            if (!m_exitFullscreenPageScaleFactor) {
                m_exitFullscreenPageScaleFactor = m_webViewImpl->pageScaleFactor();
                m_exitFullscreenScrollOffset = m_webViewImpl->mainFrame()->scrollOffset();
                m_exitFullscreenPinchViewportOffset = m_webViewImpl->pinchViewportOffset();
                m_webViewImpl->setPageScaleFactor(1.0f);
                m_webViewImpl->setMainFrameScrollOffset(IntPoint());
                m_webViewImpl->setPinchViewportOffset(FloatPoint());
            }

            FullscreenElementStack::from(*doc).webkitDidEnterFullScreenForElement(0);
            if (RuntimeEnabledFeatures::overlayFullscreenVideoEnabled()) {
                Element* element = FullscreenElementStack::currentFullScreenElementFrom(*doc);
                ASSERT(element);
                if (isHTMLMediaElement(*element) && m_webViewImpl->layerTreeView())
                    m_webViewImpl->layerTreeView()->setHasTransparentBackground(true);
            }
        }
    }
}

void FullscreenController::willExitFullScreen()
{
    if (!m_fullScreenFrame)
        return;

    if (Document* doc = m_fullScreenFrame->document()) {
        FullscreenElementStack* fullscreen = FullscreenElementStack::fromIfExists(*doc);
        if (!fullscreen)
            return;
        if (fullscreen->isFullScreen(*doc)) {
            // When the client exits from full screen we have to call webkitCancelFullScreen to
            // notify the document. While doing that, suppress notifications back to the client.
            m_isCancelingFullScreen = true;
            fullscreen->webkitCancelFullScreen();
            m_isCancelingFullScreen = false;
            fullscreen->webkitWillExitFullScreenForElement(0);
            if (RuntimeEnabledFeatures::overlayFullscreenVideoEnabled() && m_webViewImpl->layerTreeView())
                m_webViewImpl->layerTreeView()->setHasTransparentBackground(m_webViewImpl->isTransparent());
        }
    }
}

void FullscreenController::didExitFullScreen()
{
    if (!m_fullScreenFrame)
        return;

    if (Document* doc = m_fullScreenFrame->document()) {
        if (FullscreenElementStack* fullscreen = FullscreenElementStack::fromIfExists(*doc)) {
            if (fullscreen->webkitIsFullScreen()) {
                if (m_exitFullscreenPageScaleFactor) {
                    m_webViewImpl->setPageScaleFactor(m_exitFullscreenPageScaleFactor);
                    m_webViewImpl->setMainFrameScrollOffset(IntPoint(m_exitFullscreenScrollOffset));
                    m_webViewImpl->setPinchViewportOffset(m_exitFullscreenPinchViewportOffset);
                    m_exitFullscreenPageScaleFactor = 0;
                    m_exitFullscreenScrollOffset = IntSize();
                }

                fullscreen->webkitDidExitFullScreenForElement(0);
            }
        }
    }

    m_fullScreenFrame.clear();
}

void FullscreenController::enterFullScreenForElement(WebCore::Element* element)
{
    // We are already transitioning to fullscreen for a different element.
    if (m_provisionalFullScreenElement) {
        m_provisionalFullScreenElement = element;
        return;
    }

    // We are already in fullscreen mode.
    if (m_fullScreenFrame) {
        m_provisionalFullScreenElement = element;
        willEnterFullScreen();
        didEnterFullScreen();
        return;
    }

    if (RuntimeEnabledFeatures::overlayFullscreenVideoEnabled()
        && isHTMLMediaElement(element)
        // FIXME: There is no embedder-side handling in layout test mode.
        && !isRunningLayoutTest()) {
        HTMLMediaElement* mediaElement = toHTMLMediaElement(element);
        if (mediaElement->webMediaPlayer() && mediaElement->webMediaPlayer()->canEnterFullscreen()) {
            mediaElement->webMediaPlayer()->enterFullscreen();
            m_provisionalFullScreenElement = element;
            return;
        }
    }

    // We need to transition to fullscreen mode.
    if (WebViewClient* client = m_webViewImpl->client()) {
        if (client->enterFullScreen())
            m_provisionalFullScreenElement = element;
    }
}

void FullscreenController::exitFullScreenForElement(WebCore::Element* element)
{
    // The client is exiting full screen, so don't send a notification.
    if (m_isCancelingFullScreen)
        return;
    if (RuntimeEnabledFeatures::overlayFullscreenVideoEnabled()
        && isHTMLMediaElement(element)
        // FIXME: There is no embedder-side handling in layout test mode.
        && !isRunningLayoutTest()) {
        HTMLMediaElement* mediaElement = toHTMLMediaElement(element);
        if (mediaElement->webMediaPlayer())
            mediaElement->webMediaPlayer()->exitFullscreen();
        return;
    }
    if (WebViewClient* client = m_webViewImpl->client())
        client->exitFullScreen();
}

}

