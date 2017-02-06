/*
 * Copyright 2008, The Android Open Source Project
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/events/TouchEvent.h"

#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/events/EventDispatcher.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/inspector/ConsoleMessage.h"
#include "platform/Histogram.h"

namespace blink {

namespace {

// These offsets change indicies into the ListenerHistogram
// enumeration. The addition of a series of offsets then
// produces the resulting ListenerHistogram value.
// TODO(dtapuska): Remove all of these histogram counts once
// https://crbug.com/599609 is fixed.
const size_t kTouchTargetHistogramRootScrollerOffset = 6;
const size_t kTouchTargetHistogramScrollableDocumentOffset = 3;
const size_t kTouchTargetHistogramAlreadyHandledOffset = 0;
const size_t kTouchTargetHistogramNotHandledOffset = 1;
const size_t kTouchTargetHistogramHandledOffset = 2;
const size_t kCapturingOffset = 0;
const size_t kAtTargetOffset = 12;
const size_t kBubblingOffset = 24;

enum TouchTargetAndDispatchResultType {
    // The following enums represent state captured during the CAPTURING_PHASE.
    CapturingNonRootScrollerNonScrollableAlreadyHandled, // Non-root-scroller, non-scrollable document, already handled.
    CapturingNonRootScrollerNonScrollableNotHandled, // Non-root-scroller, non-scrollable document, not handled.
    CapturingNonRootScrollerNonScrollableHandled, // Non-root-scroller, non-scrollable document, handled application.
    CapturingNonRootScrollerScrollableDocumentAlreadyHandled, // Non-root-scroller, scrollable document, already handled.
    CapturingNonRootScrollerScrollableDocumentNotHandled, // Non-root-scroller, scrollable document, not handled.
    CapturingNonRootScrollerScrollableDocumentHandled, // Non-root-scroller, scrollable document, handled application.
    CapturingRootScrollerNonScrollableAlreadyHandled, // Root-scroller, non-scrollable document, already handled.
    CapturingRootScrollerNonScrollableNotHandled, // Root-scroller, non-scrollable document, not handled.
    CapturingRootScrollerNonScrollableHandled, // Root-scroller, non-scrollable document, handled.
    CapturingRootScrollerScrollableDocumentAlreadyHandled, // Root-scroller, scrollable document, already handled.
    CapturingRootScrollerScrollableDocumentNotHandled, // Root-scroller, scrollable document, not handled.
    CapturingRootScrollerScrollableDocumentHandled, // Root-scroller, scrollable document, handled.

    // The following enums represent state captured during the AT_TARGET phase.
    NonRootScrollerNonScrollableAlreadyHandled, // Non-root-scroller, non-scrollable document, already handled.
    NonRootScrollerNonScrollableNotHandled, // Non-root-scroller, non-scrollable document, not handled.
    NonRootScrollerNonScrollableHandled, // Non-root-scroller, non-scrollable document, handled application.
    NonRootScrollerScrollableDocumentAlreadyHandled, // Non-root-scroller, scrollable document, already handled.
    NonRootScrollerScrollableDocumentNotHandled, // Non-root-scroller, scrollable document, not handled.
    NonRootScrollerScrollableDocumentHandled, // Non-root-scroller, scrollable document, handled application.
    RootScrollerNonScrollableAlreadyHandled, // Root-scroller, non-scrollable document, already handled.
    RootScrollerNonScrollableNotHandled, // Root-scroller, non-scrollable document, not handled.
    RootScrollerNonScrollableHandled, // Root-scroller, non-scrollable document, handled.
    RootScrollerScrollableDocumentAlreadyHandled, // Root-scroller, scrollable document, already handled.
    RootScrollerScrollableDocumentNotHandled, // Root-scroller, scrollable document, not handled.
    RootScrollerScrollableDocumentHandled, // Root-scroller, scrollable document, handled.

    // The following enums represent state captured during the BUBBLING_PHASE.
    BubblingNonRootScrollerNonScrollableAlreadyHandled, // Non-root-scroller, non-scrollable document, already handled.
    BubblingNonRootScrollerNonScrollableNotHandled, // Non-root-scroller, non-scrollable document, not handled.
    BubblingNonRootScrollerNonScrollableHandled, // Non-root-scroller, non-scrollable document, handled application.
    BubblingNonRootScrollerScrollableDocumentAlreadyHandled, // Non-root-scroller, scrollable document, already handled.
    BubblingNonRootScrollerScrollableDocumentNotHandled, // Non-root-scroller, scrollable document, not handled.
    BubblingNonRootScrollerScrollableDocumentHandled, // Non-root-scroller, scrollable document, handled application.
    BubblingRootScrollerNonScrollableAlreadyHandled, // Root-scroller, non-scrollable document, already handled.
    BubblingRootScrollerNonScrollableNotHandled, // Root-scroller, non-scrollable document, not handled.
    BubblingRootScrollerNonScrollableHandled, // Root-scroller, non-scrollable document, handled.
    BubblingRootScrollerScrollableDocumentAlreadyHandled, // Root-scroller, scrollable document, already handled.
    BubblingRootScrollerScrollableDocumentNotHandled, // Root-scroller, scrollable document, not handled.
    BubblingRootScrollerScrollableDocumentHandled, // Root-scroller, scrollable document, handled.
    TouchTargetAndDispatchResultTypeMax,
};

void logTouchTargetHistogram(EventTarget* eventTarget, unsigned short phase, bool defaultPreventedBeforeCurrentTarget, bool defaultPrevented)
{
    int result = 0;
    Document* document = nullptr;

    switch (phase) {
    default:
    case Event::NONE:
        return;
    case Event::CAPTURING_PHASE:
        result += kCapturingOffset;
        break;
    case Event::AT_TARGET:
        result += kAtTargetOffset;
        break;
    case Event::BUBBLING_PHASE:
        result += kBubblingOffset;
        break;
    }

    if (const LocalDOMWindow* domWindow = eventTarget->toLocalDOMWindow()) {
        // Treat the window as a root scroller as well.
        document = domWindow->document();
        result += kTouchTargetHistogramRootScrollerOffset;
    } else if (Node* node = eventTarget->toNode()) {
        // Report if the target node is the document or body.
        if (node->isDocumentNode() || node->document().documentElement() == node || node->document().body() == node) {
            result += kTouchTargetHistogramRootScrollerOffset;
        }
        document = &node->document();
    }

    if (document) {
        FrameView* view = document->view();
        if (view && view->isScrollable())
            result += kTouchTargetHistogramScrollableDocumentOffset;
    }

    if (defaultPreventedBeforeCurrentTarget)
        result += kTouchTargetHistogramAlreadyHandledOffset;
    else if (defaultPrevented)
        result += kTouchTargetHistogramHandledOffset;
    else
        result += kTouchTargetHistogramNotHandledOffset;

    DEFINE_STATIC_LOCAL(EnumerationHistogram, rootDocumentListenerHistogram, ("Event.Touch.TargetAndDispatchResult2", TouchTargetAndDispatchResultTypeMax));
    rootDocumentListenerHistogram.count(static_cast<TouchTargetAndDispatchResultType>(result));
}

} // namespace

TouchEvent::TouchEvent()
    : m_causesScrollingIfUncanceled(false)
    , m_firstTouchMoveOrStart(false)
    , m_defaultPreventedBeforeCurrentTarget(false)
{
}

TouchEvent::TouchEvent(TouchList* touches, TouchList* targetTouches,
    TouchList* changedTouches, const AtomicString& type,
    AbstractView* view,
    PlatformEvent::Modifiers modifiers, bool cancelable, bool causesScrollingIfUncanceled, bool firstTouchMoveOrStart,
    double platformTimeStamp)
    // Pass a sourceCapabilities including the ability to fire touchevents when creating this touchevent, which is always created from input device capabilities from EventHandler.
    : UIEventWithKeyState(type, true, cancelable, view, 0, modifiers, platformTimeStamp, InputDeviceCapabilities::firesTouchEventsSourceCapabilities()),
    m_touches(touches),
    m_targetTouches(targetTouches),
    m_changedTouches(changedTouches),
    m_causesScrollingIfUncanceled(causesScrollingIfUncanceled),
    m_firstTouchMoveOrStart(firstTouchMoveOrStart),
    m_defaultPreventedBeforeCurrentTarget(false)
{
}

TouchEvent::TouchEvent(const AtomicString& type, const TouchEventInit& initializer)
    : UIEventWithKeyState(type, initializer)
    , m_touches(TouchList::create(initializer.touches()))
    , m_targetTouches(TouchList::create(initializer.targetTouches()))
    , m_changedTouches(TouchList::create(initializer.changedTouches()))
    , m_causesScrollingIfUncanceled(false)
    , m_firstTouchMoveOrStart(false)
    , m_defaultPreventedBeforeCurrentTarget(false)
{
}

TouchEvent::~TouchEvent()
{
}

void TouchEvent::initTouchEvent(ScriptState* scriptState, TouchList* touches, TouchList* targetTouches,
    TouchList* changedTouches, const AtomicString& type,
    AbstractView* view,
    int, int, int, int,
    bool ctrlKey, bool altKey, bool shiftKey, bool metaKey)
{
    if (isBeingDispatched())
        return;

    if (scriptState->world().isIsolatedWorld())
        UIEventWithKeyState::didCreateEventInIsolatedWorld(ctrlKey, altKey, shiftKey, metaKey);

    bool cancelable = true;
    if (type == EventTypeNames::touchcancel)
        cancelable = false;

    initUIEvent(type, true, cancelable, view, 0);

    m_touches = touches;
    m_targetTouches = targetTouches;
    m_changedTouches = changedTouches;
    initModifiers(ctrlKey, altKey, shiftKey, metaKey);
}

const AtomicString& TouchEvent::interfaceName() const
{
    return EventNames::TouchEvent;
}

bool TouchEvent::isTouchEvent() const
{
    return true;
}

void TouchEvent::preventDefault()
{
    UIEventWithKeyState::preventDefault();

    // A common developer error is to wait too long before attempting to stop
    // scrolling by consuming a touchmove event. Generate a warning if this
    // event is uncancelable.
    if (!cancelable() && view() && view()->isLocalDOMWindow() && view()->frame()) {
        toLocalDOMWindow(view())->frame()->console().addMessage(ConsoleMessage::create(JSMessageSource, WarningMessageLevel,
            "Ignored attempt to cancel a " + type() + " event with cancelable=false, for example because scrolling is in progress and cannot be interrupted."));
    }
}

void TouchEvent::doneDispatchingEventAtCurrentTarget()
{
    // Do not log for non-cancelable events, events that don't block
    // scrolling, have more than one touch point or aren't on the main frame.
    if (!cancelable() || !m_firstTouchMoveOrStart || !(m_touches && m_touches->length() == 1) || !(view() && view()->frame() && view()->frame()->isMainFrame()))
        return;

    bool canceled = defaultPrevented();
    logTouchTargetHistogram(currentTarget(), eventPhase(), m_defaultPreventedBeforeCurrentTarget, canceled);
    m_defaultPreventedBeforeCurrentTarget = canceled;
}

EventDispatchMediator* TouchEvent::createMediator()
{
    return TouchEventDispatchMediator::create(this);
}

DEFINE_TRACE(TouchEvent)
{
    visitor->trace(m_touches);
    visitor->trace(m_targetTouches);
    visitor->trace(m_changedTouches);
    UIEventWithKeyState::trace(visitor);
}

TouchEventDispatchMediator* TouchEventDispatchMediator::create(TouchEvent* touchEvent)
{
    return new TouchEventDispatchMediator(touchEvent);
}

TouchEventDispatchMediator::TouchEventDispatchMediator(TouchEvent* touchEvent)
    : EventDispatchMediator(touchEvent)
{
}

TouchEvent& TouchEventDispatchMediator::event() const
{
    return toTouchEvent(EventDispatchMediator::event());
}

DispatchEventResult TouchEventDispatchMediator::dispatchEvent(EventDispatcher& dispatcher) const
{
    event().eventPath().adjustForTouchEvent(event());
    return dispatcher.dispatch();
}

} // namespace blink
