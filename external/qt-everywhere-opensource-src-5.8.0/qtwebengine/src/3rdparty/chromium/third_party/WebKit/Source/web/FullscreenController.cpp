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

#include "web/FullscreenController.h"

#include "core/dom/Document.h"
#include "core/dom/Fullscreen.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/PageScaleConstraintsSet.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/layout/LayoutFullScreen.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "public/platform/WebLayerTreeView.h"
#include "public/web/WebFrameClient.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebSettingsImpl.h"
#include "web/WebViewImpl.h"

namespace blink {

FullscreenController* FullscreenController::create(WebViewImpl* webViewImpl)
{
    return new FullscreenController(webViewImpl);
}

FullscreenController::FullscreenController(WebViewImpl* webViewImpl)
    : m_webViewImpl(webViewImpl)
    , m_haveEnteredFullscreen(false)
    , m_exitFullscreenPageScaleFactor(0)
    , m_isCancelingFullScreen(false)
{
}

void FullscreenController::didEnterFullScreen()
{
    if (!m_provisionalFullScreenElement)
        return;

    Element* element = m_provisionalFullScreenElement.release();
    Document& document = element->document();
    m_fullScreenFrame = document.frame();

    if (!m_fullScreenFrame)
        return;

    if (!m_haveEnteredFullscreen) {
        updatePageScaleConstraints(false);
        m_webViewImpl->setPageScaleFactor(1.0f);
        if (m_webViewImpl->mainFrame()->isWebLocalFrame())
            m_webViewImpl->mainFrame()->setScrollOffset(WebSize());
        m_webViewImpl->setVisualViewportOffset(FloatPoint());
        m_haveEnteredFullscreen = true;
    }

    Fullscreen::from(document).didEnterFullScreenForElement(element);
    DCHECK_EQ(Fullscreen::currentFullScreenElementFrom(document), element);

    if (isHTMLVideoElement(element)) {
        HTMLVideoElement* videoElement = toHTMLVideoElement(element);
        if (videoElement->usesOverlayFullscreenVideo() && m_webViewImpl->layerTreeView())
            m_webViewImpl->layerTreeView()->setHasTransparentBackground(true);
    }
}

void FullscreenController::didExitFullScreen()
{
    if (!m_fullScreenFrame)
        return;

    if (m_haveEnteredFullscreen)
        updatePageScaleConstraints(true);

    if (Document* document = m_fullScreenFrame->document()) {
        if (Fullscreen* fullscreen = Fullscreen::fromIfExists(*document)) {
            Element* element = fullscreen->webkitCurrentFullScreenElement();
            if (element) {
                // When the client exits from full screen we have to call fullyExitFullscreen to notify
                // the document. While doing that, suppress notifications back to the client.
                m_isCancelingFullScreen = true;
                Fullscreen::fullyExitFullscreen(*document);
                m_isCancelingFullScreen = false;

                // If the video used overlay fullscreen mode, the background was made transparent. Restore the transparency.
                if (isHTMLVideoElement(element) && m_webViewImpl->layerTreeView())
                    m_webViewImpl->layerTreeView()->setHasTransparentBackground(m_webViewImpl->isTransparent());

                if (m_haveEnteredFullscreen) {
                    m_webViewImpl->setPageScaleFactor(m_exitFullscreenPageScaleFactor);
                    if (m_webViewImpl->mainFrame()->isWebLocalFrame())
                        m_webViewImpl->mainFrame()->setScrollOffset(WebSize(m_exitFullscreenScrollOffset));
                    m_webViewImpl->setVisualViewportOffset(m_exitFullscreenVisualViewportOffset);
                }

                fullscreen->didExitFullScreenForElement();
            }
        }
    }

    m_haveEnteredFullscreen = false;
    m_fullScreenFrame.clear();
}

void FullscreenController::enterFullScreenForElement(Element* element)
{
    // We are already transitioning to fullscreen for a different element.
    if (m_provisionalFullScreenElement) {
        m_provisionalFullScreenElement = element;
        return;
    }

    // We are already in fullscreen mode.
    if (m_fullScreenFrame) {
        m_provisionalFullScreenElement = element;
        didEnterFullScreen();
        return;
    }

    // We need to store these values here rather than didEnterFullScreen since
    // by the time the latter is called, a Resize has already occured, clamping
    // the scroll offset.
    if (!m_haveEnteredFullscreen) {
        m_exitFullscreenPageScaleFactor = m_webViewImpl->pageScaleFactor();
        m_exitFullscreenScrollOffset = m_webViewImpl->mainFrame()->isWebLocalFrame() ? m_webViewImpl->mainFrame()->scrollOffset() : WebSize();
        m_exitFullscreenVisualViewportOffset = m_webViewImpl->visualViewportOffset();
    }

    // We need to transition to fullscreen mode.
    WebLocalFrameImpl* frame = WebLocalFrameImpl::fromFrame(element->document().frame());
    if (frame && frame->client()) {
        if (!Fullscreen::from(element->document()).forCrossProcessAncestor())
            frame->client()->enterFullscreen();
        m_provisionalFullScreenElement = element;
    }
}

void FullscreenController::exitFullScreenForElement(Element* element)
{
    DCHECK(element);

    // The client is exiting full screen, so don't send a notification.
    if (m_isCancelingFullScreen)
        return;

    WebLocalFrameImpl* frame = WebLocalFrameImpl::fromFrame(element->document().frame());
    if (frame && frame->client())
        frame->client()->exitFullscreen();
}

void FullscreenController::updateSize()
{
    if (!isFullscreen())
        return;

    updatePageScaleConstraints(false);

    LayoutFullScreen* layoutObject = Fullscreen::from(*m_fullScreenFrame->document()).fullScreenLayoutObject();
    if (layoutObject)
        layoutObject->updateStyle();
}

void FullscreenController::updatePageScaleConstraints(bool removeConstraints)
{
    PageScaleConstraints fullscreenConstraints;
    if (!removeConstraints) {
        fullscreenConstraints = PageScaleConstraints(1.0, 1.0, 1.0);
        fullscreenConstraints.layoutSize = FloatSize(m_webViewImpl->size());
    }
    m_webViewImpl->pageScaleConstraintsSet().setFullscreenConstraints(fullscreenConstraints);
    m_webViewImpl->pageScaleConstraintsSet().computeFinalConstraints();

    // Although we called computedFinalConstraints() above, the "final" constraints are not
    // actually final. They are still subject to scale factor clamping by contents size.
    // Normally they should be dirtied due to contents size mutation after layout, however the
    // contents size is not guaranteed to mutate, and the scale factor may remain unclamped.
    // Just fire the event again to ensure the final constraints pick up the latest contents size.
    m_webViewImpl->didChangeContentsSize();
    if (m_webViewImpl->mainFrameImpl() && m_webViewImpl->mainFrameImpl()->frameView())
        m_webViewImpl->mainFrameImpl()->frameView()->setNeedsLayout();

    m_webViewImpl->updateMainFrameLayoutSize();
}

DEFINE_TRACE(FullscreenController)
{
    visitor->trace(m_provisionalFullScreenElement);
    visitor->trace(m_fullScreenFrame);
}

} // namespace blink

