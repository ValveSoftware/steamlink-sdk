/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/loader/NavigationAction.h"

#include "core/events/MouseEvent.h"
#include "core/loader/FrameLoader.h"

namespace WebCore {

static NavigationType navigationType(FrameLoadType frameLoadType, bool isFormSubmission, bool haveEvent)
{
    bool isReload = frameLoadType == FrameLoadTypeReload || frameLoadType == FrameLoadTypeReloadFromOrigin;
    bool isBackForward = isBackForwardLoadType(frameLoadType);
    if (isFormSubmission)
        return (isReload || isBackForward) ? NavigationTypeFormResubmitted : NavigationTypeFormSubmitted;
    if (haveEvent)
        return NavigationTypeLinkClicked;
    if (isReload)
        return NavigationTypeReload;
    if (isBackForward)
        return NavigationTypeBackForward;
    return NavigationTypeOther;
}

NavigationAction::NavigationAction()
    : m_type(NavigationTypeOther)
{
}

NavigationAction::NavigationAction(const ResourceRequest& resourceRequest, FrameLoadType frameLoadType,
    bool isFormSubmission, PassRefPtrWillBeRawPtr<Event> passEvent)
    : m_resourceRequest(resourceRequest)
{
    RefPtrWillBeRawPtr<Event> event = passEvent;
    m_type = navigationType(frameLoadType, isFormSubmission || resourceRequest.httpBody(), event);
    m_eventTimeStamp = event ? event->timeStamp() : 0;

    const MouseEvent* mouseEvent = 0;
    if (m_type == NavigationTypeLinkClicked) {
        ASSERT(event);
        if (event->isMouseEvent())
            mouseEvent = toMouseEvent(event.get());
    } else if (m_type == NavigationTypeFormSubmitted && event && event->underlyingEvent() && event->underlyingEvent()->isMouseEvent()) {
        mouseEvent = toMouseEvent(event->underlyingEvent());
    }

    if (!mouseEvent || !navigationPolicyFromMouseEvent(mouseEvent->button(), mouseEvent->ctrlKey(), mouseEvent->shiftKey(), mouseEvent->altKey(), mouseEvent->metaKey(), &m_policy))
        m_policy = NavigationPolicyCurrentTab;
}

}
