/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 *
 */

#include "core/dom/Fullscreen.h"

#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/StyleChangeReason.h"
#include "core/dom/StyleEngine.h"
#include "core/events/Event.h"
#include "core/frame/FrameHost.h"
#include "core/frame/HostsUsingFeatures.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/html/HTMLMediaElement.h"
#include "core/input/EventHandler.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/LayoutFullScreen.h"
#include "core/layout/api/LayoutFullScreenItem.h"
#include "core/page/ChromeClient.h"
#include "platform/UserGestureIndicator.h"

namespace blink {

using namespace HTMLNames;

namespace {

bool fullscreenIsAllowedForAllOwners(const Document& document)
{
    if (!document.frame())
        return false;

    for (const Frame* frame = document.frame(); frame->owner(); frame = frame->tree().parent()) {
        if (!frame->owner()->allowFullscreen())
            return false;
    }
    return true;
}

bool fullscreenIsSupported(const Document& document)
{
    // Fullscreen is supported if there is no previously-established user preference,
    // security risk, or platform limitation.
    return !document.settings() || document.settings()->fullscreenSupported();
}

bool fullscreenElementReady(const Element& element)
{
    // A fullscreen element ready check for an element |element| returns true if all of the
    // following are true, and false otherwise:

    // |element| is in a document.
    if (!element.inShadowIncludingDocument())
        return false;

    // |element|'s node document's fullscreen enabled flag is set.
    if (!fullscreenIsAllowedForAllOwners(element.document()))
        return false;

    // |element|'s node document's fullscreen element stack is either empty or its top element is an
    // inclusive ancestor of |element|.
    if (const Element* topElement = Fullscreen::fullscreenElementFrom(element.document())) {
        if (!topElement->contains(&element))
            return false;
    }

    // |element| has no ancestor element whose local name is iframe and namespace is the HTML
    // namespace.
    if (Traversal<HTMLIFrameElement>::firstAncestor(element))
        return false;

    // |element|'s node document's browsing context either has a browsing context container and the
    // fullscreen element ready check returns true for |element|'s node document's browsing
    // context's browsing context container, or it has no browsing context container.
    if (const Element* owner = element.document().localOwner()) {
        if (!fullscreenElementReady(*owner))
            return false;
    }

    return true;
}

bool isPrefixed(const AtomicString& type)
{
    return type == EventTypeNames::webkitfullscreenchange || type == EventTypeNames::webkitfullscreenerror;
}

Event* createEvent(const AtomicString& type, EventTarget& target)
{
    EventInit initializer;
    initializer.setBubbles(isPrefixed(type));
    Event* event = Event::create(type, initializer);
    event->setTarget(&target);
    return event;
}

// Helper to walk the ancestor chain and return the Document of the topmost
// local ancestor frame. Note that this is not the same as the topmost frame's
// Document, which might be unavailable in OOPIF scenarios. For example, with
// OOPIFs, when called on the bottom frame's Document in a A-B-C-B hierarchy in
// process B, this will skip remote frame C and return this frame: A-[B]-C-B.
Document& topmostLocalAncestor(Document& document)
{
    Document* topmost = &document;
    Frame* frame = document.frame();
    while (frame) {
        frame = frame->tree().parent();
        if (frame && frame->isLocalFrame())
            topmost = toLocalFrame(frame)->document();
    }
    return *topmost;
}

// Helper to find the browsing context container in |doc| that embeds the
// |descendant| Document, possibly through multiple levels of nesting.  This
// works even in OOPIF scenarios like A-B-A, where there may be remote frames
// in between |doc| and |descendant|.
HTMLFrameOwnerElement* findContainerForDescendant(const Document& doc, const Document& descendant)
{
    Frame* frame = descendant.frame();
    while (frame->tree().parent() != doc.frame())
        frame = frame->tree().parent();
    return toHTMLFrameOwnerElement(frame->owner());
}

} // anonymous namespace

const char* Fullscreen::supplementName()
{
    return "Fullscreen";
}

Fullscreen& Fullscreen::from(Document& document)
{
    Fullscreen* fullscreen = fromIfExists(document);
    if (!fullscreen) {
        fullscreen = new Fullscreen(document);
        Supplement<Document>::provideTo(document, supplementName(), fullscreen);
    }

    return *fullscreen;
}

Fullscreen* Fullscreen::fromIfExistsSlow(Document& document)
{
    return static_cast<Fullscreen*>(Supplement<Document>::from(document, supplementName()));
}

Element* Fullscreen::fullscreenElementFrom(Document& document)
{
    if (Fullscreen* found = fromIfExists(document))
        return found->fullscreenElement();
    return 0;
}

Element* Fullscreen::currentFullScreenElementFrom(Document& document)
{
    if (Fullscreen* found = fromIfExists(document))
        return found->webkitCurrentFullScreenElement();
    return 0;
}

bool Fullscreen::isFullScreen(Document& document)
{
    return currentFullScreenElementFrom(document);
}

Fullscreen::Fullscreen(Document& document)
    : ContextLifecycleObserver(&document)
    , m_fullScreenLayoutObject(nullptr)
    , m_eventQueueTimer(this, &Fullscreen::eventQueueTimerFired)
    , m_forCrossProcessAncestor(false)
{
    document.setHasFullscreenSupplement();
}

Fullscreen::~Fullscreen()
{
}

inline Document* Fullscreen::document()
{
    return toDocument(lifecycleContext());
}

void Fullscreen::contextDestroyed()
{
    m_eventQueue.clear();

    if (m_fullScreenLayoutObject)
        m_fullScreenLayoutObject->destroy();

    m_fullScreenElement = nullptr;
    m_fullScreenElementStack.clear();

}

void Fullscreen::requestFullscreen(Element& element, RequestType requestType, bool forCrossProcessAncestor)
{
    // Use counters only need to be incremented in the process of the actual
    // fullscreen element.
    if (!forCrossProcessAncestor) {
        if (document()->isSecureContext()) {
            UseCounter::count(document(), UseCounter::FullscreenSecureOrigin);
        } else {
            UseCounter::count(document(), UseCounter::FullscreenInsecureOrigin);
            HostsUsingFeatures::countAnyWorld(*document(), HostsUsingFeatures::Feature::FullscreenInsecureHost);
        }
    }

    // Ignore this request if the document is not in a live frame.
    if (!document()->isActive())
        return;

    // If |element| is on top of |doc|'s fullscreen element stack, terminate these substeps.
    if (&element == fullscreenElement())
        return;

    do {
        // 1. If any of the following conditions are true, terminate these steps and queue a task to fire
        // an event named fullscreenerror with its bubbles attribute set to true on the context object's
        // node document:

        // The fullscreen element ready check returns false.
        if (!fullscreenElementReady(element))
            break;

        // This algorithm is not allowed to show a pop-up:
        //   An algorithm is allowed to show a pop-up if, in the task in which the algorithm is running, either:
        //   - an activation behavior is currently being processed whose click event was trusted, or
        //   - the event listener for a trusted click event is being handled.
        //
        // If |forCrossProcessAncestor| is true, requestFullscreen was already
        // called on an element in another process, and getting here means that
        // it already passed the user gesture check.
        if (!UserGestureIndicator::utilizeUserGesture() && !forCrossProcessAncestor) {
            String message = ExceptionMessages::failedToExecute("requestFullScreen",
                "Element", "API can only be initiated by a user gesture.");
            document()->addConsoleMessage(
                ConsoleMessage::create(JSMessageSource, WarningMessageLevel, message));
            break;
        }

        // Fullscreen is not supported.
        if (!fullscreenIsSupported(element.document()))
            break;

        // 2. Let doc be element's node document. (i.e. "this")
        Document* currentDoc = document();

        // 3. Let docs be all doc's ancestor browsing context's documents (if any) and doc.
        //
        // For OOPIF scenarios, |docs| will only contain documents for local
        // ancestors, and remote ancestors will be processed in their
        // respective processes.  This preserves the spec's event firing order
        // for local ancestors, but not for remote ancestors.  However, that
        // difference shouldn't be observable in practice: a fullscreenchange
        // event handler would need to postMessage a frame in another renderer
        // process, where the message should be queued up and processed after
        // the IPC that dispatches fullscreenchange.
        HeapDeque<Member<Document>> docs;

        docs.prepend(currentDoc);
        for (Frame* frame = currentDoc->frame()->tree().parent(); frame; frame = frame->tree().parent()) {
            if (frame->isLocalFrame())
                docs.prepend(toLocalFrame(frame)->document());
        }

        // 4. For each document in docs, run these substeps:
        HeapDeque<Member<Document>>::iterator current = docs.begin(), following = docs.begin();

        do {
            ++following;

            // 1. Let following document be the document after document in docs, or null if there is no
            // such document.
            Document* currentDoc = *current;
            Document* followingDoc = following != docs.end() ? *following : nullptr;

            // 2. If following document is null, push context object on document's fullscreen element
            // stack, and queue a task to fire an event named fullscreenchange with its bubbles attribute
            // set to true on the document.
            if (!followingDoc) {
                from(*currentDoc).pushFullscreenElementStack(element, requestType);
                enqueueChangeEvent(*currentDoc, requestType);
                continue;
            }

            // 3. Otherwise, if document's fullscreen element stack is either empty or its top element
            // is not following document's browsing context container,
            Element* topElement = fullscreenElementFrom(*currentDoc);
            HTMLFrameOwnerElement* followingOwner = findContainerForDescendant(*currentDoc, *followingDoc);
            if (!topElement || topElement != followingOwner) {
                // ...push following document's browsing context container on document's fullscreen element
                // stack, and queue a task to fire an event named fullscreenchange with its bubbles attribute
                // set to true on document.
                from(*currentDoc).pushFullscreenElementStack(*followingOwner, requestType);
                enqueueChangeEvent(*currentDoc, requestType);
                continue;
            }

            // 4. Otherwise, do nothing for this document. It stays the same.
        } while (++current != docs.end());

        m_forCrossProcessAncestor = forCrossProcessAncestor;

        // 5. Return, and run the remaining steps asynchronously.
        // 6. Optionally, perform some animation.
        document()->frameHost()->chromeClient().enterFullScreenForElement(&element);

        // 7. Optionally, display a message indicating how the user can exit displaying the context object fullscreen.
        return;
    } while (false);

    enqueueErrorEvent(element, requestType);
}

void Fullscreen::fullyExitFullscreen(Document& document)
{
    // To fully exit fullscreen, run these steps:

    // 1. Let |doc| be the top-level browsing context's document.
    //
    // Since the top-level browsing context's document might be unavailable in
    // OOPIF scenarios (i.e., when the top frame is remote), this actually uses
    // the Document of the topmost local ancestor frame.  Without OOPIF, this
    // will be the top frame's document.  With OOPIF, each renderer process for
    // the current page will separately call fullyExitFullscreen to cover all
    // local frames in each process.
    Document& doc = topmostLocalAncestor(document);

    // 2. If |doc|'s fullscreen element stack is empty, terminate these steps.
    if (!fullscreenElementFrom(doc))
        return;

    // 3. Remove elements from |doc|'s fullscreen element stack until only the top element is left.
    size_t stackSize = from(doc).m_fullScreenElementStack.size();
    from(doc).m_fullScreenElementStack.remove(0, stackSize - 1);
    DCHECK_EQ(from(doc).m_fullScreenElementStack.size(), 1u);

    // 4. Act as if the exitFullscreen() method was invoked on |doc|.
    from(doc).exitFullscreen();
}

void Fullscreen::exitFullscreen()
{
    // The exitFullscreen() method must run these steps:

    // 1. Let doc be the context object. (i.e. "this")
    Document* currentDoc = document();
    if (!currentDoc->isActive())
        return;

    // 2. If doc's fullscreen element stack is empty, terminate these steps.
    if (m_fullScreenElementStack.isEmpty())
        return;

    // 3. Let descendants be all the doc's descendant browsing context's documents with a non-empty fullscreen
    // element stack (if any), ordered so that the child of the doc is last and the document furthest
    // away from the doc is first.
    HeapDeque<Member<Document>> descendants;
    for (Frame* descendant = document()->frame() ? document()->frame()->tree().traverseNext() : 0; descendant; descendant = descendant->tree().traverseNext()) {
        if (!descendant->isLocalFrame())
            continue;
        DCHECK(toLocalFrame(descendant)->document());
        if (fullscreenElementFrom(*toLocalFrame(descendant)->document()))
            descendants.prepend(toLocalFrame(descendant)->document());
    }

    // 4. For each descendant in descendants, empty descendant's fullscreen element stack, and queue a
    // task to fire an event named fullscreenchange with its bubbles attribute set to true on descendant.
    for (auto& descendant : descendants) {
        DCHECK(descendant);
        RequestType requestType = from(*descendant).m_fullScreenElementStack.last().second;
        from(*descendant).clearFullscreenElementStack();
        enqueueChangeEvent(*descendant, requestType);
    }

    // 5. While doc is not null, run these substeps:
    Element* newTop = 0;
    while (currentDoc) {
        RequestType requestType = from(*currentDoc).m_fullScreenElementStack.last().second;

        // 1. Pop the top element of doc's fullscreen element stack.
        from(*currentDoc).popFullscreenElementStack();

        //    If doc's fullscreen element stack is non-empty and the element now at the top is either
        //    not in a document or its node document is not doc, repeat this substep.
        newTop = fullscreenElementFrom(*currentDoc);
        if (newTop && (!newTop->inShadowIncludingDocument() || newTop->document() != currentDoc))
            continue;

        // 2. Queue a task to fire an event named fullscreenchange with its bubbles attribute set to true
        // on doc.
        enqueueChangeEvent(*currentDoc, requestType);

        // 3. If doc's fullscreen element stack is empty and doc's browsing context has a browsing context
        // container, set doc to that browsing context container's node document.
        //
        // OOPIF: If browsing context container's document is in another
        // process, keep moving up the ancestor chain and looking for a
        // browsing context container with a local document.
        // TODO(alexmos): Deal with nested fullscreen cases, see
        // https://crbug.com/617369.
        if (!newTop) {
            Frame* frame = currentDoc->frame()->tree().parent();
            while (frame && frame->isRemoteFrame())
                frame = frame->tree().parent();
            if (frame) {
                currentDoc = toLocalFrame(frame)->document();
                continue;
            }
        }

        // 4. Otherwise, set doc to null.
        currentDoc = 0;
    }

    // 6. Return, and run the remaining steps asynchronously.
    // 7. Optionally, perform some animation.

    FrameHost* host = document()->frameHost();

    // Speculative fix for engaget.com/videos per crbug.com/336239.
    // FIXME: This check is wrong. We DCHECK(document->isActive()) above
    // so this should be redundant and should be removed!
    if (!host)
        return;

    // Only exit out of full screen window mode if there are no remaining elements in the
    // full screen stack.
    if (!newTop) {
        // FIXME: if the frame exiting fullscreen is not the frame that entered
        // fullscreen (but a parent frame for example), m_fullScreenElement
        // might be null. We want to pass an element that is part of the
        // document so we will pass the documentElement in that case.
        // This should be fix by exiting fullscreen for a frame instead of an
        // element, see https://crbug.com/441259
        host->chromeClient().exitFullScreenForElement(
            m_fullScreenElement ? m_fullScreenElement.get() : document()->documentElement());
        return;
    }

    // Otherwise, notify the chrome of the new full screen element.
    host->chromeClient().enterFullScreenForElement(newTop);
}

bool Fullscreen::fullscreenEnabled(Document& document)
{
    // 4. The fullscreenEnabled attribute must return true if the context object has its
    //    fullscreen enabled flag set and fullscreen is supported, and false otherwise.

    // Top-level browsing contexts are implied to have their allowFullScreen attribute set.
    return fullscreenIsAllowedForAllOwners(document) && fullscreenIsSupported(document);
}

void Fullscreen::didEnterFullScreenForElement(Element* element)
{
    DCHECK(element);
    if (!document()->isActive())
        return;

    if (m_fullScreenLayoutObject)
        m_fullScreenLayoutObject->unwrapLayoutObject();

    m_fullScreenElement = element;

    // Create a placeholder block for a the full-screen element, to keep the page from reflowing
    // when the element is removed from the normal flow. Only do this for a LayoutBox, as only
    // a box will have a frameRect. The placeholder will be created in setFullScreenLayoutObject()
    // during layout.
    LayoutObject* layoutObject = m_fullScreenElement->layoutObject();
    bool shouldCreatePlaceholder = layoutObject && layoutObject->isBox();
    if (shouldCreatePlaceholder) {
        m_savedPlaceholderFrameRect = toLayoutBox(layoutObject)->frameRect();
        m_savedPlaceholderComputedStyle = ComputedStyle::clone(layoutObject->styleRef());
    }

    // TODO(alexmos): When |m_forCrossProcessAncestor| is true, some of
    // this layout work has already been done in another process, so it should
    // not be necessary to repeat it here.
    if (m_fullScreenElement != document()->documentElement())
        LayoutFullScreen::wrapLayoutObject(layoutObject, layoutObject ? layoutObject->parent() : 0, document());

    if (m_forCrossProcessAncestor) {
        DCHECK(m_fullScreenElement->isFrameOwnerElement());
        DCHECK(toHTMLFrameOwnerElement(m_fullScreenElement)->contentFrame()->isRemoteFrame());
        m_fullScreenElement->setContainsFullScreenElement(true);
    }

    m_fullScreenElement->setContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(true);

    document()->styleEngine().ensureFullscreenUAStyle();
    m_fullScreenElement->pseudoStateChanged(CSSSelector::PseudoFullScreen);

    // FIXME: This should not call updateStyleAndLayoutTree.
    document()->updateStyleAndLayoutTree();

    m_fullScreenElement->didBecomeFullscreenElement();

    if (document()->frame())
        document()->frame()->eventHandler().scheduleHoverStateUpdate();

    m_eventQueueTimer.startOneShot(0, BLINK_FROM_HERE);
}

void Fullscreen::didExitFullScreenForElement()
{
    if (!m_fullScreenElement)
        return;

    if (!document()->isActive())
        return;

    m_fullScreenElement->willStopBeingFullscreenElement();

    if (m_forCrossProcessAncestor)
        m_fullScreenElement->setContainsFullScreenElement(false);

    m_fullScreenElement->setContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(false);

    if (m_fullScreenLayoutObject)
        LayoutFullScreenItem(m_fullScreenLayoutObject).unwrapLayoutObject();

    document()->styleEngine().ensureFullscreenUAStyle();
    m_fullScreenElement->pseudoStateChanged(CSSSelector::PseudoFullScreen);
    m_fullScreenElement = nullptr;

    if (document()->frame())
        document()->frame()->eventHandler().scheduleHoverStateUpdate();

    // When fullyExitFullscreen is called, we call exitFullscreen on the topDocument(). That means
    // that the events will be queued there. So if we have no events here, start the timer on the
    // exiting document.
    Document* exitingDocument = document();
    if (m_eventQueue.isEmpty())
        exitingDocument = &topmostLocalAncestor(*document());
    DCHECK(exitingDocument);
    from(*exitingDocument).m_eventQueueTimer.startOneShot(0, BLINK_FROM_HERE);

    m_forCrossProcessAncestor = false;
}

void Fullscreen::setFullScreenLayoutObject(LayoutFullScreen* layoutObject)
{
    if (layoutObject == m_fullScreenLayoutObject)
        return;

    if (layoutObject && m_savedPlaceholderComputedStyle) {
        layoutObject->createPlaceholder(m_savedPlaceholderComputedStyle.release(), m_savedPlaceholderFrameRect);
    } else if (layoutObject && m_fullScreenLayoutObject && m_fullScreenLayoutObject->placeholder()) {
        LayoutBlockFlow* placeholder = m_fullScreenLayoutObject->placeholder();
        layoutObject->createPlaceholder(ComputedStyle::clone(placeholder->styleRef()), placeholder->frameRect());
    }

    if (m_fullScreenLayoutObject)
        m_fullScreenLayoutObject->unwrapLayoutObject();
    DCHECK(!m_fullScreenLayoutObject);

    m_fullScreenLayoutObject = layoutObject;
}

void Fullscreen::fullScreenLayoutObjectDestroyed()
{
    m_fullScreenLayoutObject = nullptr;
}

void Fullscreen::enqueueChangeEvent(Document& document, RequestType requestType)
{
    Event* event;
    if (requestType == UnprefixedRequest) {
        event = createEvent(EventTypeNames::fullscreenchange, document);
    } else {
        DCHECK(document.hasFullscreenSupplement());
        Fullscreen& fullscreen = from(document);
        EventTarget* target = fullscreen.fullscreenElement();
        if (!target)
            target = fullscreen.webkitCurrentFullScreenElement();
        if (!target)
            target = &document;
        event = createEvent(EventTypeNames::webkitfullscreenchange, *target);
    }
    m_eventQueue.append(event);
    // NOTE: The timer is started in didEnterFullScreenForElement/didExitFullScreenForElement.
}

void Fullscreen::enqueueErrorEvent(Element& element, RequestType requestType)
{
    Event* event;
    if (requestType == UnprefixedRequest)
        event = createEvent(EventTypeNames::fullscreenerror, element.document());
    else
        event = createEvent(EventTypeNames::webkitfullscreenerror, element);
    m_eventQueue.append(event);
    m_eventQueueTimer.startOneShot(0, BLINK_FROM_HERE);
}

void Fullscreen::eventQueueTimerFired(Timer<Fullscreen>*)
{
    HeapDeque<Member<Event>> eventQueue;
    m_eventQueue.swap(eventQueue);

    while (!eventQueue.isEmpty()) {
        Event* event = eventQueue.takeFirst();
        Node* target = event->target()->toNode();

        // If the element was removed from our tree, also message the documentElement.
        if (!target->inShadowIncludingDocument() && document()->documentElement()) {
            DCHECK(isPrefixed(event->type()));
            eventQueue.append(createEvent(event->type(), *document()->documentElement()));
        }

        target->dispatchEvent(event);
    }
}

void Fullscreen::elementRemoved(Element& oldNode)
{
    // Whenever the removing steps run with an |oldNode| and |oldNode| is in its node document's
    // fullscreen element stack, run these steps:

    // 1. If |oldNode| is at the top of its node document's fullscreen element stack, act as if the
    //    exitFullscreen() method was invoked on that document.
    if (fullscreenElement() == &oldNode) {
        exitFullscreen();
        return;
    }

    // 2. Otherwise, remove |oldNode| from its node document's fullscreen element stack.
    for (size_t i = 0; i < m_fullScreenElementStack.size(); ++i) {
        if (m_fullScreenElementStack[i].first.get() == &oldNode) {
            m_fullScreenElementStack.remove(i);
            return;
        }
    }

    // NOTE: |oldNode| was not in the fullscreen element stack.
}

void Fullscreen::clearFullscreenElementStack()
{
    m_fullScreenElementStack.clear();
}

void Fullscreen::popFullscreenElementStack()
{
    if (m_fullScreenElementStack.isEmpty())
        return;

    m_fullScreenElementStack.removeLast();
}

void Fullscreen::pushFullscreenElementStack(Element& element, RequestType requestType)
{
    m_fullScreenElementStack.append(std::make_pair(&element, requestType));
}

DEFINE_TRACE(Fullscreen)
{
    visitor->trace(m_fullScreenElement);
    visitor->trace(m_fullScreenElementStack);
    visitor->trace(m_eventQueue);
    Supplement<Document>::trace(visitor);
    ContextLifecycleObserver::trace(visitor);
}

} // namespace blink
