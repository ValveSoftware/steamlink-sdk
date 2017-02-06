/*
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999 Lars Knoll <knoll@kde.org>
 *                     1999 Antti Koivisto <koivisto@kde.org>
 *                     2000 Simon Hausmann <hausmann@kde.org>
 *                     2000 Stefan Schimanski <1Stein@gmx.de>
 *                     2001 George Staikos <staikos@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2005 Alexey Proskuryakov <ap@nypop.com>
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Google Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/frame/Frame.h"

#include "core/dom/DocumentType.h"
#include "core/events/Event.h"
#include "core/frame/FrameHost.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLFrameElementBase.h"
#include "core/input/EventHandler.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InstanceCounters.h"
#include "core/layout/LayoutPart.h"
#include "core/loader/EmptyClients.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "platform/Histogram.h"
#include "platform/UserGestureIndicator.h"

namespace blink {

using namespace HTMLNames;

Frame::~Frame()
{
    InstanceCounters::decrementCounter(InstanceCounters::FrameCounter);
    ASSERT(!m_owner);
}

DEFINE_TRACE(Frame)
{
    visitor->trace(m_treeNode);
    visitor->trace(m_host);
    visitor->trace(m_owner);
    visitor->trace(m_client);
}

void Frame::detach(FrameDetachType type)
{
    ASSERT(m_client);
    m_client->setOpener(0);
    domWindow()->resetLocation();
    disconnectOwnerElement();
    // After this, we must no longer talk to the client since this clears
    // its owning reference back to our owning LocalFrame.
    m_client->detached(type);
    m_client = nullptr;
    m_host = nullptr;
}

void Frame::detachChildren()
{
    typedef HeapVector<Member<Frame>> FrameVector;
    FrameVector childrenToDetach;
    childrenToDetach.reserveCapacity(tree().childCount());
    for (Frame* child = tree().firstChild(); child; child = child->tree().nextSibling())
        childrenToDetach.append(child);
    for (const auto& child : childrenToDetach)
        child->detach(FrameDetachType::Remove);
}

void Frame::disconnectOwnerElement()
{
    if (m_owner) {
        m_owner->clearContentFrame();
        m_owner = nullptr;
    }
}

Page* Frame::page() const
{
    if (m_host)
        return &m_host->page();
    return nullptr;
}

FrameHost* Frame::host() const
{
    return m_host;
}

bool Frame::isMainFrame() const
{
    return !tree().parent();
}

bool Frame::isLocalRoot() const
{
    if (isRemoteFrame())
        return false;

    if (!tree().parent())
        return true;

    return tree().parent()->isRemoteFrame();
}

HTMLFrameOwnerElement* Frame::deprecatedLocalOwner() const
{
    return m_owner && m_owner->isLocal() ? toHTMLFrameOwnerElement(m_owner) : nullptr;
}

static ChromeClient& emptyChromeClient()
{
    DEFINE_STATIC_LOCAL(EmptyChromeClient, client, (EmptyChromeClient::create()));
    return client;
}

ChromeClient& Frame::chromeClient() const
{
    if (Page* page = this->page())
        return page->chromeClient();
    return emptyChromeClient();
}

Frame* Frame::findFrameForNavigation(const AtomicString& name, Frame& activeFrame)
{
    Frame* frame = tree().find(name);
    if (!frame || !activeFrame.canNavigate(*frame))
        return nullptr;
    return frame;
}

static bool canAccessAncestor(const SecurityOrigin& activeSecurityOrigin, const Frame* targetFrame)
{
    // targetFrame can be 0 when we're trying to navigate a top-level frame
    // that has a 0 opener.
    if (!targetFrame)
        return false;

    const bool isLocalActiveOrigin = activeSecurityOrigin.isLocal();
    for (const Frame* ancestorFrame = targetFrame; ancestorFrame; ancestorFrame = ancestorFrame->tree().parent()) {
        const SecurityOrigin* ancestorSecurityOrigin = ancestorFrame->securityContext()->getSecurityOrigin();
        if (activeSecurityOrigin.canAccess(ancestorSecurityOrigin))
            return true;

        // Allow file URL descendant navigation even when allowFileAccessFromFileURLs is false.
        // FIXME: It's a bit strange to special-case local origins here. Should we be doing
        // something more general instead?
        if (isLocalActiveOrigin && ancestorSecurityOrigin->isLocal())
            return true;
    }

    return false;
}

bool Frame::canNavigate(const Frame& targetFrame)
{
    String errorReason;
    bool isAllowedNavigation = canNavigateWithoutFramebusting(targetFrame, errorReason);

    // Frame-busting is generally allowed, but blocked for sandboxed frames lacking the 'allow-top-navigation' flag.
    if (targetFrame != this && !securityContext()->isSandboxed(SandboxTopNavigation) && targetFrame == tree().top()) {
        DEFINE_STATIC_LOCAL(EnumerationHistogram, framebustHistogram, ("WebCore.Framebust", 4));
        const unsigned userGestureBit = 0x1;
        const unsigned allowedBit = 0x2;
        unsigned framebustParams = 0;
        UseCounter::count(&targetFrame, UseCounter::TopNavigationFromSubFrame);
        if (UserGestureIndicator::processingUserGesture())
            framebustParams |= userGestureBit;
        if (isAllowedNavigation)
            framebustParams |= allowedBit;
        framebustHistogram.count(framebustParams);
        return true;
    }
    if (!isAllowedNavigation && !errorReason.isNull())
        printNavigationErrorMessage(targetFrame, errorReason.latin1().data());
    return isAllowedNavigation;
}

bool Frame::canNavigateWithoutFramebusting(const Frame& targetFrame, String& reason)
{
    if (securityContext()->isSandboxed(SandboxNavigation)) {
        // Sandboxed frames can navigate their own children.
        if (targetFrame.tree().isDescendantOf(this))
            return true;

        // They can also navigate popups, if the 'allow-sandbox-escape-via-popup' flag is specified.
        if (targetFrame == targetFrame.tree().top() && targetFrame.tree().top() != tree().top() && !securityContext()->isSandboxed(SandboxPropagatesToAuxiliaryBrowsingContexts))
            return true;

        // Otherwise, block the navigation.
        if (securityContext()->isSandboxed(SandboxTopNavigation) && targetFrame == tree().top())
            reason = "The frame attempting navigation of the top-level window is sandboxed, but the 'allow-top-navigation' flag is not set.";
        else
            reason = "The frame attempting navigation is sandboxed, and is therefore disallowed from navigating its ancestors.";
        return false;
    }

    ASSERT(securityContext()->getSecurityOrigin());
    SecurityOrigin& origin = *securityContext()->getSecurityOrigin();

    // This is the normal case. A document can navigate its decendant frames,
    // or, more generally, a document can navigate a frame if the document is
    // in the same origin as any of that frame's ancestors (in the frame
    // hierarchy).
    //
    // See http://www.adambarth.com/papers/2008/barth-jackson-mitchell.pdf for
    // historical information about this security check.
    if (canAccessAncestor(origin, &targetFrame))
        return true;

    // Top-level frames are easier to navigate than other frames because they
    // display their URLs in the address bar (in most browsers). However, there
    // are still some restrictions on navigation to avoid nuisance attacks.
    // Specifically, a document can navigate a top-level frame if that frame
    // opened the document or if the document is the same-origin with any of
    // the top-level frame's opener's ancestors (in the frame hierarchy).
    //
    // In both of these cases, the document performing the navigation is in
    // some way related to the frame being navigate (e.g., by the "opener"
    // and/or "parent" relation). Requiring some sort of relation prevents a
    // document from navigating arbitrary, unrelated top-level frames.
    if (!targetFrame.tree().parent()) {
        if (targetFrame == client()->opener())
            return true;
        if (canAccessAncestor(origin, targetFrame.client()->opener()))
            return true;
    }

    reason = "The frame attempting navigation is neither same-origin with the target, nor is it the target's parent or opener.";
    return false;
}

Frame* Frame::findUnsafeParentScrollPropagationBoundary()
{
    Frame* currentFrame = this;
    Frame* ancestorFrame = tree().parent();

    while (ancestorFrame) {
        if (!ancestorFrame->securityContext()->getSecurityOrigin()->canAccess(securityContext()->getSecurityOrigin()))
            return currentFrame;
        currentFrame = ancestorFrame;
        ancestorFrame = ancestorFrame->tree().parent();
    }
    return nullptr;
}

LayoutPart* Frame::ownerLayoutObject() const
{
    if (!deprecatedLocalOwner())
        return nullptr;
    LayoutObject* object = deprecatedLocalOwner()->layoutObject();
    if (!object)
        return nullptr;
    // FIXME: If <object> is ever fixed to disassociate itself from frames
    // that it has started but canceled, then this can turn into an ASSERT
    // since ownerElement() would be 0 when the load is canceled.
    // https://bugs.webkit.org/show_bug.cgi?id=18585
    if (!object->isLayoutPart())
        return nullptr;
    return toLayoutPart(object);
}

Settings* Frame::settings() const
{
    if (m_host)
        return &m_host->settings();
    return nullptr;
}

bool Frame::isCrossOrigin() const
{
    // Check to see if the frame can script into the top level document.
    const SecurityOrigin* securityOrigin = securityContext()->getSecurityOrigin();
    Frame* top = tree().top();
    return top && !securityOrigin->canAccess(top->securityContext()->getSecurityOrigin());
}

void Frame::didChangeVisibilityState()
{
    HeapVector<Member<Frame>> childFrames;
    for (Frame* child = tree().firstChild(); child; child = child->tree().nextSibling())
        childFrames.append(child);
    for (size_t i = 0; i < childFrames.size(); ++i)
        childFrames[i]->didChangeVisibilityState();
}

Frame::Frame(FrameClient* client, FrameHost* host, FrameOwner* owner)
    : m_treeNode(this)
    , m_host(host)
    , m_owner(owner)
    , m_client(client)
    , m_isLoading(false)
{
    InstanceCounters::incrementCounter(InstanceCounters::FrameCounter);

    ASSERT(page());

    if (m_owner)
        m_owner->setContentFrame(*this);
    else
        page()->setMainFrame(this);
}

} // namespace blink
