/*
 * Copyright (C) 2016 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/frame/DOMVisualViewport.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/frame/FrameHost.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/VisualViewport.h"
#include "core/page/Page.h"
#include "core/style/ComputedStyle.h"

namespace blink {

DOMVisualViewport::DOMVisualViewport(LocalDOMWindow* window)
    : m_window(window)
{
}

DOMVisualViewport::~DOMVisualViewport()
{
}

DEFINE_TRACE(DOMVisualViewport)
{
    visitor->trace(m_window);
    EventTargetWithInlineData::trace(visitor);
}

const AtomicString& DOMVisualViewport::interfaceName() const
{
    return EventTargetNames::DOMVisualViewport;
}

ExecutionContext* DOMVisualViewport::getExecutionContext() const
{
    return m_window->getExecutionContext();
}

double DOMVisualViewport::scrollLeft()
{
    LocalFrame* frame = m_window->frame();
    if (!frame || !frame->isMainFrame())
        return 0;

    if (FrameHost* host = frame->host())
        return host->visualViewport().scrollLeft();

    return 0;
}

double DOMVisualViewport::scrollTop()
{
    LocalFrame* frame = m_window->frame();
    if (!frame || !frame->isMainFrame())
        return 0;

    if (FrameHost* host = frame->host())
        return host->visualViewport().scrollTop();

    return 0;
}

double DOMVisualViewport::clientWidth()
{
    LocalFrame* frame = m_window->frame();
    if (!frame)
        return 0;

    if (!frame->isMainFrame()) {
        FloatSize viewportSize = m_window->getViewportSize(ExcludeScrollbars);
        return adjustForAbsoluteZoom(expandedIntSize(viewportSize).width(), frame->pageZoomFactor());
    }

    if (FrameHost* host = frame->host())
        return host->visualViewport().clientWidth();

    return 0;
}

double DOMVisualViewport::clientHeight()
{
    LocalFrame* frame = m_window->frame();
    if (!frame)
        return 0;

    if (!frame->isMainFrame()) {
        FloatSize viewportSize = m_window->getViewportSize(ExcludeScrollbars);
        return adjustForAbsoluteZoom(expandedIntSize(viewportSize).height(), frame->pageZoomFactor());
    }

    if (FrameHost* host = frame->host())
        return host->visualViewport().clientHeight();

    return 0;
}

double DOMVisualViewport::scale()
{
    LocalFrame* frame = m_window->frame();
    if (!frame)
        return 0;

    if (!frame->isMainFrame())
        return 1;

    if (FrameHost* host = m_window->frame()->host())
        return host->visualViewport().pageScale();

    return 0;
}

} // namespace blink
