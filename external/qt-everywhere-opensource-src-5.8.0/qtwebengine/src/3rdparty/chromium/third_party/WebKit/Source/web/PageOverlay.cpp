/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. AND ITS CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE INC.
 * OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "web/PageOverlay.h"

#include "core/frame/FrameHost.h"
#include "core/frame/VisualViewport.h"
#include "core/page/Page.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/GraphicsLayerClient.h"
#include "platform/scroll/MainThreadScrollingReason.h"
#include "public/platform/WebLayer.h"
#include "public/web/WebViewClient.h"
#include "web/WebDevToolsAgentImpl.h"
#include "web/WebViewImpl.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

std::unique_ptr<PageOverlay> PageOverlay::create(WebViewImpl* viewImpl, PageOverlay::Delegate* delegate)
{
    return wrapUnique(new PageOverlay(viewImpl, delegate));
}

PageOverlay::PageOverlay(WebViewImpl* viewImpl, PageOverlay::Delegate* delegate)
    : m_viewImpl(viewImpl)
    , m_delegate(delegate)
{
}

PageOverlay::~PageOverlay()
{
    if (!m_layer)
        return;

    m_layer->removeFromParent();
    if (WebDevToolsAgentImpl* devTools = m_viewImpl->mainFrameDevToolsAgentImpl())
        devTools->didRemovePageOverlay(m_layer.get());
    m_layer = nullptr;
}

void PageOverlay::update()
{
    if (!m_viewImpl->isAcceleratedCompositingActive())
        return;

    Page* page = m_viewImpl->page();
    if (!page)
        return;

    if (!page->mainFrame()->isLocalFrame())
        return;

    if (!m_layer) {
        m_layer = GraphicsLayer::create(this);
        m_layer->setDrawsContent(true);

        if (WebDevToolsAgentImpl* devTools = m_viewImpl->mainFrameDevToolsAgentImpl())
            devTools->willAddPageOverlay(m_layer.get());

        // This is required for contents of overlay to stay in sync with the page while scrolling.
        WebLayer* platformLayer = m_layer->platformLayer();
        platformLayer->addMainThreadScrollingReasons(MainThreadScrollingReason::kPageOverlay);
        page->frameHost().visualViewport().containerLayer()->addChild(m_layer.get());
    }

    FloatSize size(page->frameHost().visualViewport().size());
    if (size != m_layer->size())
        m_layer->setSize(size);

    m_layer->setNeedsDisplay();
}

LayoutRect PageOverlay::visualRect() const
{
    DCHECK(m_layer.get());
    return LayoutRect(FloatPoint(), m_layer->size());
}

IntRect PageOverlay::computeInterestRect(const GraphicsLayer* graphicsLayer, const IntRect&) const
{
    return IntRect(IntPoint(), expandedIntSize(m_layer->size()));
}

void PageOverlay::paintContents(const GraphicsLayer* graphicsLayer, GraphicsContext& gc, GraphicsLayerPaintingPhase phase, const IntRect& interestRect) const
{
    DCHECK(m_layer);
    m_delegate->paintPageOverlay(*this, gc, interestRect.size());
}

String PageOverlay::debugName(const GraphicsLayer*) const
{
    return "WebViewImpl Page Overlay Content Layer";
}

} // namespace blink
