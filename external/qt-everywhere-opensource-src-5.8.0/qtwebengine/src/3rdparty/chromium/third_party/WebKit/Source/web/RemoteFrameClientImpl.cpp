// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/RemoteFrameClientImpl.h"

#include "core/events/KeyboardEvent.h"
#include "core/events/MouseEvent.h"
#include "core/events/WheelEvent.h"
#include "core/frame/RemoteFrame.h"
#include "core/frame/RemoteFrameView.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/api/LayoutItem.h"
#include "platform/exported/WrappedResourceRequest.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "public/web/WebRemoteFrameClient.h"
#include "web/WebInputEventConversion.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebRemoteFrameImpl.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

namespace {

// Convenience helper for frame tree helpers in FrameClient to reduce the amount
// of null-checking boilerplate code. Since the frame tree is maintained in the
// web/ layer, the frame tree helpers often have to deal with null WebFrames:
// for example, a frame with no parent will return null for WebFrame::parent().
// TODO(dcheng): Remove duplication between FrameLoaderClientImpl and
// RemoteFrameClientImpl somehow...
Frame* toCoreFrame(WebFrame* frame)
{
    return frame ? frame->toImplBase()->frame() : nullptr;
}

} // namespace

RemoteFrameClientImpl::RemoteFrameClientImpl(WebRemoteFrameImpl* webFrame)
    : m_webFrame(webFrame)
{
}

RemoteFrameClientImpl* RemoteFrameClientImpl::create(WebRemoteFrameImpl* webFrame)
{
    return new RemoteFrameClientImpl(webFrame);
}

DEFINE_TRACE(RemoteFrameClientImpl)
{
    visitor->trace(m_webFrame);
    RemoteFrameClient::trace(visitor);
}

bool RemoteFrameClientImpl::inShadowTree() const
{
    return m_webFrame->inShadowTree();
}

void RemoteFrameClientImpl::willBeDetached()
{
}

void RemoteFrameClientImpl::detached(FrameDetachType type)
{
    // Alert the client that the frame is being detached.
    WebRemoteFrameClient* client = m_webFrame->client();
    if (!client)
        return;

    client->frameDetached(static_cast<WebRemoteFrameClient::DetachType>(type));
    // Clear our reference to RemoteFrame at the very end, in case the client
    // refers to it.
    m_webFrame->setCoreFrame(nullptr);
}

Frame* RemoteFrameClientImpl::opener() const
{
    return toCoreFrame(m_webFrame->opener());
}

void RemoteFrameClientImpl::setOpener(Frame* opener)
{
    WebFrame* openerFrame = WebFrame::fromFrame(opener);
    if (m_webFrame->client() && m_webFrame->opener() != openerFrame)
        m_webFrame->client()->didChangeOpener(openerFrame);
    m_webFrame->setOpener(openerFrame);
}

Frame* RemoteFrameClientImpl::parent() const
{
    return toCoreFrame(m_webFrame->parent());
}

Frame* RemoteFrameClientImpl::top() const
{
    return toCoreFrame(m_webFrame->top());
}

Frame* RemoteFrameClientImpl::previousSibling() const
{
    return toCoreFrame(m_webFrame->previousSibling());
}

Frame* RemoteFrameClientImpl::nextSibling() const
{
    return toCoreFrame(m_webFrame->nextSibling());
}

Frame* RemoteFrameClientImpl::firstChild() const
{
    return toCoreFrame(m_webFrame->firstChild());
}

Frame* RemoteFrameClientImpl::lastChild() const
{
    return toCoreFrame(m_webFrame->lastChild());
}

void RemoteFrameClientImpl::frameFocused() const
{
    if (m_webFrame->client())
        m_webFrame->client()->frameFocused();
}

void RemoteFrameClientImpl::navigate(const ResourceRequest& request, bool shouldReplaceCurrentEntry)
{
    if (m_webFrame->client())
        m_webFrame->client()->navigate(WrappedResourceRequest(request), shouldReplaceCurrentEntry);
}

void RemoteFrameClientImpl::reload(FrameLoadType loadType, ClientRedirectPolicy clientRedirectPolicy)
{
    ASSERT(loadType == FrameLoadTypeReload || loadType == FrameLoadTypeReloadBypassingCache);
    if (m_webFrame->client())
        m_webFrame->client()->reload(static_cast<WebFrameLoadType>(loadType), static_cast<WebClientRedirectPolicy>(clientRedirectPolicy));
}

unsigned RemoteFrameClientImpl::backForwardLength()
{
    // TODO(creis,japhet): This method should return the real value for the
    // session history length. For now, return static value for the initial
    // navigation and the subsequent one moving the frame out-of-process.
    // See https://crbug.com/501116.
    return 2;
}

void RemoteFrameClientImpl::forwardPostMessage(
    MessageEvent* event, PassRefPtr<SecurityOrigin> target, LocalFrame* sourceFrame) const
{
    if (m_webFrame->client())
        m_webFrame->client()->forwardPostMessage(WebLocalFrameImpl::fromFrame(sourceFrame), m_webFrame, WebSecurityOrigin(target), WebDOMMessageEvent(event));
}

// FIXME: Remove this code once we have input routing in the browser
// process. See http://crbug.com/339659.
void RemoteFrameClientImpl::forwardInputEvent(Event* event)
{
    // It is possible for a platform event to cause the remote iframe element
    // to be hidden, which destroys the layout object (for instance, a mouse
    // event that moves between elements will trigger a mouseout on the old
    // element, which might hide the new element). In that case we do not
    // forward. This is divergent behavior from local frames, where the
    // content of the frame can receive events even after the frame is hidden.
    // We might need to revisit this after browser hit testing is fully
    // implemented, since this code path will need to be removed or refactored
    // anyway.
    // See https://crbug.com/520705.
    if (!m_webFrame->toImplBase()->frame()->ownerLayoutObject())
        return;

    // This is only called when we have out-of-process iframes, which
    // need to forward input events across processes.
    // FIXME: Add a check for out-of-process iframes enabled.
    std::unique_ptr<WebInputEvent> webEvent;
    if (event->isKeyboardEvent())
        webEvent = wrapUnique(new WebKeyboardEventBuilder(*static_cast<KeyboardEvent*>(event)));
    else if (event->isMouseEvent())
        webEvent = wrapUnique(new WebMouseEventBuilder(m_webFrame->frame()->view(), LayoutItem(m_webFrame->toImplBase()->frame()->ownerLayoutObject()), *static_cast<MouseEvent*>(event)));

    // Other or internal Blink events should not be forwarded.
    if (!webEvent || webEvent->type == WebInputEvent::Undefined)
        return;

    m_webFrame->client()->forwardInputEvent(webEvent.get());
}

void RemoteFrameClientImpl::frameRectsChanged(const IntRect& frameRect)
{
    m_webFrame->client()->frameRectsChanged(frameRect);
}

void RemoteFrameClientImpl::advanceFocus(WebFocusType type, LocalFrame* source)
{
    m_webFrame->client()->advanceFocus(type, WebLocalFrameImpl::fromFrame(source));
}

void RemoteFrameClientImpl::visibilityChanged(bool visible)
{
    m_webFrame->client()->visibilityChanged(visible);
}

} // namespace blink
