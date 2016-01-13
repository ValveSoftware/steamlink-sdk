/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "core/html/shadow/ClearButtonElement.h"

#include "core/events/MouseEvent.h"
#include "core/frame/LocalFrame.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "core/page/EventHandler.h"
#include "core/rendering/RenderView.h"

namespace WebCore {

using namespace HTMLNames;

inline ClearButtonElement::ClearButtonElement(Document& document, ClearButtonOwner& clearButtonOwner)
    : HTMLDivElement(document)
    , m_clearButtonOwner(&clearButtonOwner)
    , m_capturing(false)
{
}

PassRefPtrWillBeRawPtr<ClearButtonElement> ClearButtonElement::create(Document& document, ClearButtonOwner& clearButtonOwner)
{
    RefPtrWillBeRawPtr<ClearButtonElement> element = adoptRefWillBeNoop(new ClearButtonElement(document, clearButtonOwner));
    element->setShadowPseudoId(AtomicString("-webkit-clear-button", AtomicString::ConstructFromLiteral));
    element->setAttribute(idAttr, ShadowElementNames::clearButton());
    return element.release();
}

void ClearButtonElement::detach(const AttachContext& context)
{
    if (m_capturing) {
        if (LocalFrame* frame = document().frame())
            frame->eventHandler().setCapturingMouseEventsNode(nullptr);
    }
    HTMLDivElement::detach(context);
}

void ClearButtonElement::releaseCapture()
{
    if (!m_capturing)
        return;

    if (LocalFrame* frame = document().frame()) {
        frame->eventHandler().setCapturingMouseEventsNode(nullptr);
        m_capturing = false;
    }
}

void ClearButtonElement::defaultEventHandler(Event* event)
{
    if (!m_clearButtonOwner) {
        if (!event->defaultHandled())
            HTMLDivElement::defaultEventHandler(event);
        return;
    }

    if (!m_clearButtonOwner->shouldClearButtonRespondToMouseEvents()) {
        if (!event->defaultHandled())
            HTMLDivElement::defaultEventHandler(event);
        return;
    }

    if (event->type() == EventTypeNames::mousedown && event->isMouseEvent() && toMouseEvent(event)->button() == LeftButton) {
        if (renderer() && renderer()->visibleToHitTesting()) {
            if (LocalFrame* frame = document().frame()) {
                frame->eventHandler().setCapturingMouseEventsNode(this);
                m_capturing = true;
            }
        }
        m_clearButtonOwner->focusAndSelectClearButtonOwner();
        event->setDefaultHandled();
    }
    if (event->type() == EventTypeNames::mouseup && event->isMouseEvent() && toMouseEvent(event)->button() == LeftButton) {
        if (m_capturing) {
            if (LocalFrame* frame = document().frame()) {
                frame->eventHandler().setCapturingMouseEventsNode(nullptr);
                m_capturing = false;
            }
            if (hovered()) {
                m_clearButtonOwner->clearValue();
                event->setDefaultHandled();
            }
        }
    }

    if (!event->defaultHandled())
        HTMLDivElement::defaultEventHandler(event);
}

bool ClearButtonElement::isClearButtonElement() const
{
    return true;
}

void ClearButtonElement::trace(Visitor* visitor)
{
    visitor->trace(m_clearButtonOwner);
    HTMLDivElement::trace(visitor);
}

}
