// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/web/WebFrame.h"

#include "bindings/core/v8/WindowProxyManager.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/RemoteFrame.h"
#include "core/html/HTMLFrameElementBase.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/page/Page.h"
#include "platform/UserGestureIndicator.h"
#include "platform/heap/Handle.h"
#include "public/web/WebElement.h"
#include "public/web/WebFrameOwnerProperties.h"
#include "public/web/WebSandboxFlags.h"
#include "web/OpenedFrameTracker.h"
#include "web/RemoteFrameOwner.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebRemoteFrameImpl.h"
#include <algorithm>

namespace blink {

bool WebFrame::swap(WebFrame* frame)
{
    using std::swap;
    Frame* oldFrame = toImplBase()->frame();
    if (oldFrame->isDetaching())
        return false;

    // Unload the current Document in this frame: this calls unload handlers,
    // detaches child frames, etc. Since this runs script, make sure this frame
    // wasn't detached before continuing with the swap.
    // FIXME: There is no unit test for this condition, so one needs to be
    // written.
    if (!oldFrame->prepareForCommit())
        return false;

    if (m_parent) {
        if (m_parent->m_firstChild == this)
            m_parent->m_firstChild = frame;
        if (m_parent->m_lastChild == this)
            m_parent->m_lastChild = frame;
        // FIXME: This is due to the fact that the |frame| may be a provisional
        // local frame, because we don't know if the navigation will result in
        // an actual page or something else, like a download. The PlzNavigate
        // project will remove the need for provisional local frames.
        frame->m_parent = m_parent;
    }

    if (m_previousSibling) {
        m_previousSibling->m_nextSibling = frame;
        swap(m_previousSibling, frame->m_previousSibling);
    }
    if (m_nextSibling) {
        m_nextSibling->m_previousSibling = frame;
        swap(m_nextSibling, frame->m_nextSibling);
    }

    if (m_opener) {
        frame->setOpener(m_opener);
        setOpener(nullptr);
    }
    m_openedFrameTracker->transferTo(frame);

    FrameHost* host = oldFrame->host();
    AtomicString name = oldFrame->tree().name();
    AtomicString uniqueName = oldFrame->tree().uniqueName();
    FrameOwner* owner = oldFrame->owner();

    v8::HandleScope handleScope(v8::Isolate::GetCurrent());
    HashMap<DOMWrapperWorld*, v8::Local<v8::Object>> globals;
    oldFrame->getWindowProxyManager()->clearForNavigation();
    oldFrame->getWindowProxyManager()->releaseGlobals(globals);

    // Although the Document in this frame is now unloaded, many resources
    // associated with the frame itself have not yet been freed yet.
    oldFrame->detach(FrameDetachType::Swap);

    // Clone the state of the current Frame into the one being swapped in.
    // FIXME: This is a bit clunky; this results in pointless decrements and
    // increments of connected subframes.
    if (frame->isWebLocalFrame()) {
        // TODO(dcheng): in an ideal world, both branches would just use
        // WebFrameImplBase's initializeCoreFrame() helper. However, Blink
        // currently requires a 'provisional' local frame to serve as a
        // placeholder for loading state when swapping to a local frame.
        // In this case, the core LocalFrame is already initialized, so just
        // update a bit of state.
        LocalFrame& localFrame = *toWebLocalFrameImpl(frame)->frame();
        DCHECK_EQ(owner, localFrame.owner());
        if (owner) {
            owner->setContentFrame(localFrame);
            if (owner->isLocal())
                toHTMLFrameOwnerElement(owner)->setWidget(localFrame.view());
        } else {
            localFrame.page()->setMainFrame(&localFrame);
        }
    } else {
        toWebRemoteFrameImpl(frame)->initializeCoreFrame(host, owner, name, uniqueName);
    }

    frame->toImplBase()->frame()->getWindowProxyManager()->setGlobals(globals);

    m_parent = nullptr;

    return true;
}

void WebFrame::detach()
{
    toImplBase()->frame()->detach(FrameDetachType::Remove);
}

WebSecurityOrigin WebFrame::getSecurityOrigin() const
{
    return WebSecurityOrigin(toImplBase()->frame()->securityContext()->getSecurityOrigin());
}


void WebFrame::setFrameOwnerSandboxFlags(WebSandboxFlags flags)
{
    // At the moment, this is only used to replicate sandbox flags
    // for frames with a remote owner.
    FrameOwner* owner = toImplBase()->frame()->owner();
    DCHECK(owner);
    toRemoteFrameOwner(owner)->setSandboxFlags(static_cast<SandboxFlags>(flags));
}

WebInsecureRequestPolicy WebFrame::getInsecureRequestPolicy() const
{
    return toImplBase()->frame()->securityContext()->getInsecureRequestPolicy();
}

void WebFrame::setFrameOwnerProperties(const WebFrameOwnerProperties& properties)
{
    // At the moment, this is only used to replicate frame owner properties
    // for frames with a remote owner.
    RemoteFrameOwner* owner = toRemoteFrameOwner(toImplBase()->frame()->owner());
    DCHECK(owner);
    owner->setScrollingMode(properties.scrollingMode);
    owner->setMarginWidth(properties.marginWidth);
    owner->setMarginHeight(properties.marginHeight);
    owner->setAllowFullscreen(properties.allowFullscreen);
    owner->setDelegatedpermissions(properties.delegatedPermissions);
}

WebFrame* WebFrame::opener() const
{
    return m_opener;
}

void WebFrame::setOpener(WebFrame* opener)
{
    if (m_opener)
        m_opener->m_openedFrameTracker->remove(this);
    if (opener)
        opener->m_openedFrameTracker->add(this);
    m_opener = opener;
}

void WebFrame::insertAfter(WebFrame* newChild, WebFrame* previousSibling)
{
    newChild->m_parent = this;

    WebFrame* next;
    if (!previousSibling) {
        // Insert at the beginning if no previous sibling is specified.
        next = m_firstChild;
        m_firstChild = newChild;
    } else {
        DCHECK_EQ(previousSibling->m_parent, this);
        next = previousSibling->m_nextSibling;
        previousSibling->m_nextSibling = newChild;
        newChild->m_previousSibling = previousSibling;
    }

    if (next) {
        newChild->m_nextSibling = next;
        next->m_previousSibling = newChild;
    } else {
        m_lastChild = newChild;
    }

    toImplBase()->frame()->tree().invalidateScopedChildCount();
    toImplBase()->frame()->host()->incrementSubframeCount();
}

void WebFrame::appendChild(WebFrame* child)
{
    // TODO(dcheng): Original code asserts that the frames have the same Page.
    // We should add an equivalent check... figure out what.
    insertAfter(child, m_lastChild);
}

void WebFrame::removeChild(WebFrame* child)
{
    child->m_parent = 0;

    if (m_firstChild == child)
        m_firstChild = child->m_nextSibling;
    else
        child->m_previousSibling->m_nextSibling = child->m_nextSibling;

    if (m_lastChild == child)
        m_lastChild = child->m_previousSibling;
    else
        child->m_nextSibling->m_previousSibling = child->m_previousSibling;

    child->m_previousSibling = child->m_nextSibling = 0;

    toImplBase()->frame()->tree().invalidateScopedChildCount();
    toImplBase()->frame()->host()->decrementSubframeCount();
}

void WebFrame::setParent(WebFrame* parent)
{
    m_parent = parent;
}

WebFrame* WebFrame::parent() const
{
    return m_parent;
}

WebFrame* WebFrame::top() const
{
    WebFrame* frame = const_cast<WebFrame*>(this);
    for (WebFrame* parent = frame; parent; parent = parent->m_parent)
        frame = parent;
    return frame;
}

WebFrame* WebFrame::firstChild() const
{
    return m_firstChild;
}

WebFrame* WebFrame::lastChild() const
{
    return m_lastChild;
}

WebFrame* WebFrame::previousSibling() const
{
    return m_previousSibling;
}

WebFrame* WebFrame::nextSibling() const
{
    return m_nextSibling;
}

WebFrame* WebFrame::traversePrevious(bool wrap) const
{
    if (Frame* frame = toImplBase()->frame())
        return fromFrame(frame->tree().traversePreviousWithWrap(wrap));
    return 0;
}

WebFrame* WebFrame::traverseNext(bool wrap) const
{
    if (Frame* frame = toImplBase()->frame())
        return fromFrame(frame->tree().traverseNextWithWrap(wrap));
    return 0;
}

WebFrame* WebFrame::findChildByName(const WebString& name) const
{
    Frame* frame = toImplBase()->frame();
    if (!frame)
        return 0;
    // FIXME: It's not clear this should ever be called to find a remote frame.
    // Perhaps just disallow that completely?
    return fromFrame(frame->tree().child(name));
}

WebFrame* WebFrame::fromFrameOwnerElement(const WebElement& webElement)
{
    Element* element = webElement;

    if (!element->isFrameOwnerElement())
        return nullptr;
    return fromFrame(toHTMLFrameOwnerElement(element)->contentFrame());
}

bool WebFrame::isLoading() const
{
    if (Frame* frame = toImplBase()->frame())
        return frame->isLoading();
    return false;
}

WebFrame* WebFrame::fromFrame(Frame* frame)
{
    if (!frame)
        return 0;

    if (frame->isLocalFrame())
        return WebLocalFrameImpl::fromFrame(toLocalFrame(*frame));
    return WebRemoteFrameImpl::fromFrame(toRemoteFrame(*frame));
}

WebFrame::WebFrame(WebTreeScopeType scope)
    : m_scope(scope)
    , m_parent(0)
    , m_previousSibling(0)
    , m_nextSibling(0)
    , m_firstChild(0)
    , m_lastChild(0)
    , m_opener(0)
    , m_openedFrameTracker(new OpenedFrameTracker)
{
}

WebFrame::~WebFrame()
{
    m_openedFrameTracker.reset(0);
}

ALWAYS_INLINE bool WebFrame::isFrameAlive(const WebFrame* frame)
{
    if (!frame)
        return true;

    if (frame->isWebLocalFrame())
        return ThreadHeap::isHeapObjectAlive(toWebLocalFrameImpl(frame));

    return ThreadHeap::isHeapObjectAlive(toWebRemoteFrameImpl(frame));
}

template <typename VisitorDispatcher>
ALWAYS_INLINE void WebFrame::traceFrameImpl(VisitorDispatcher visitor, WebFrame* frame)
{
    if (!frame)
        return;

    if (frame->isWebLocalFrame())
        visitor->trace(toWebLocalFrameImpl(frame));
    else
        visitor->trace(toWebRemoteFrameImpl(frame));
}

template <typename VisitorDispatcher>
ALWAYS_INLINE void WebFrame::traceFramesImpl(VisitorDispatcher visitor, WebFrame* frame)
{
    DCHECK(frame);
    traceFrame(visitor, frame->m_parent);
    for (WebFrame* child = frame->firstChild(); child; child = child->nextSibling())
        traceFrame(visitor, child);
    // m_opener is a weak reference.
    frame->m_openedFrameTracker->traceFrames(visitor);
}

template <typename VisitorDispatcher>
ALWAYS_INLINE void WebFrame::clearWeakFramesImpl(VisitorDispatcher visitor)
{
    if (!isFrameAlive(m_opener))
        m_opener = nullptr;
}

#define DEFINE_VISITOR_METHOD(VisitorDispatcher)                                                                               \
    void WebFrame::traceFrame(VisitorDispatcher visitor, WebFrame* frame) { traceFrameImpl(visitor, frame); }                  \
    void WebFrame::traceFrames(VisitorDispatcher visitor, WebFrame* frame) { traceFramesImpl(visitor, frame); }                \
    void WebFrame::clearWeakFrames(VisitorDispatcher visitor) { clearWeakFramesImpl(visitor); }

DEFINE_VISITOR_METHOD(Visitor*)
DEFINE_VISITOR_METHOD(InlinedGlobalMarkingVisitor)

#undef DEFINE_VISITOR_METHOD

} // namespace blink
