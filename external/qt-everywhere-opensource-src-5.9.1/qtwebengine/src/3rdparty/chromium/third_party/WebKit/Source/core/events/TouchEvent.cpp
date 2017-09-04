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
#include "core/html/HTMLElement.h"
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

  // Non-root-scroller, non-scrollable document, already handled.
  CapturingNonRootScrollerNonScrollableAlreadyHandled,
  // Non-root-scroller, non-scrollable document, not handled.
  CapturingNonRootScrollerNonScrollableNotHandled,
  // Non-root-scroller, non-scrollable document, handled application.
  CapturingNonRootScrollerNonScrollableHandled,
  // Non-root-scroller, scrollable document, already handled.
  CapturingNonRootScrollerScrollableDocumentAlreadyHandled,
  // Non-root-scroller, scrollable document, not handled.
  CapturingNonRootScrollerScrollableDocumentNotHandled,
  // Non-root-scroller, scrollable document, handled application.
  CapturingNonRootScrollerScrollableDocumentHandled,
  // Root-scroller, non-scrollable document, already handled.
  CapturingRootScrollerNonScrollableAlreadyHandled,
  // Root-scroller, non-scrollable document, not handled.
  CapturingRootScrollerNonScrollableNotHandled,
  // Root-scroller, non-scrollable document, handled.
  CapturingRootScrollerNonScrollableHandled,
  // Root-scroller, scrollable document, already handled.
  CapturingRootScrollerScrollableDocumentAlreadyHandled,
  // Root-scroller, scrollable document, not handled.
  CapturingRootScrollerScrollableDocumentNotHandled,
  // Root-scroller, scrollable document, handled.
  CapturingRootScrollerScrollableDocumentHandled,

  // The following enums represent state captured during the AT_TARGET phase.

  // Non-root-scroller, non-scrollable document, already handled.
  NonRootScrollerNonScrollableAlreadyHandled,
  // Non-root-scroller, non-scrollable document, not handled.
  NonRootScrollerNonScrollableNotHandled,
  // Non-root-scroller, non-scrollable document, handled application.
  NonRootScrollerNonScrollableHandled,
  // Non-root-scroller, scrollable document, already handled.
  NonRootScrollerScrollableDocumentAlreadyHandled,
  // Non-root-scroller, scrollable document, not handled.
  NonRootScrollerScrollableDocumentNotHandled,
  // Non-root-scroller, scrollable document, handled application.
  NonRootScrollerScrollableDocumentHandled,
  // Root-scroller, non-scrollable document, already handled.
  RootScrollerNonScrollableAlreadyHandled,
  // Root-scroller, non-scrollable document, not handled.
  RootScrollerNonScrollableNotHandled,
  // Root-scroller, non-scrollable document, handled.
  RootScrollerNonScrollableHandled,
  // Root-scroller, scrollable document, already handled.
  RootScrollerScrollableDocumentAlreadyHandled,
  // Root-scroller, scrollable document, not handled.
  RootScrollerScrollableDocumentNotHandled,
  // Root-scroller, scrollable document, handled.
  RootScrollerScrollableDocumentHandled,

  // The following enums represent state captured during the BUBBLING_PHASE.

  // Non-root-scroller, non-scrollable document, already handled.
  BubblingNonRootScrollerNonScrollableAlreadyHandled,
  // Non-root-scroller, non-scrollable document, not handled.
  BubblingNonRootScrollerNonScrollableNotHandled,
  // Non-root-scroller, non-scrollable document, handled application.
  BubblingNonRootScrollerNonScrollableHandled,
  // Non-root-scroller, scrollable document, already handled.
  BubblingNonRootScrollerScrollableDocumentAlreadyHandled,
  // Non-root-scroller, scrollable document, not handled.
  BubblingNonRootScrollerScrollableDocumentNotHandled,
  // Non-root-scroller, scrollable document, handled application.
  BubblingNonRootScrollerScrollableDocumentHandled,
  // Root-scroller, non-scrollable document, already handled.
  BubblingRootScrollerNonScrollableAlreadyHandled,
  // Root-scroller, non-scrollable document, not handled.
  BubblingRootScrollerNonScrollableNotHandled,
  // Root-scroller, non-scrollable document, handled.
  BubblingRootScrollerNonScrollableHandled,
  // Root-scroller, scrollable document, already handled.
  BubblingRootScrollerScrollableDocumentAlreadyHandled,
  // Root-scroller, scrollable document, not handled.
  BubblingRootScrollerScrollableDocumentNotHandled,
  // Root-scroller, scrollable document, handled.
  BubblingRootScrollerScrollableDocumentHandled,

  TouchTargetAndDispatchResultTypeMax,
};

void logTouchTargetHistogram(EventTarget* eventTarget,
                             unsigned short phase,
                             bool defaultPreventedBeforeCurrentTarget,
                             bool defaultPrevented) {
  int result = 0;
  Document* document = nullptr;

  switch (phase) {
    default:
    case Event::kNone:
      return;
    case Event::kCapturingPhase:
      result += kCapturingOffset;
      break;
    case Event::kAtTarget:
      result += kAtTargetOffset;
      break;
    case Event::kBubblingPhase:
      result += kBubblingOffset;
      break;
  }

  if (const LocalDOMWindow* domWindow = eventTarget->toLocalDOMWindow()) {
    // Treat the window as a root scroller as well.
    document = domWindow->document();
    result += kTouchTargetHistogramRootScrollerOffset;
  } else if (Node* node = eventTarget->toNode()) {
    // Report if the target node is the document or body.
    if (node->isDocumentNode() || node->document().documentElement() == node ||
        node->document().body() == node) {
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

  DEFINE_STATIC_LOCAL(EnumerationHistogram, rootDocumentListenerHistogram,
                      ("Event.Touch.TargetAndDispatchResult2",
                       TouchTargetAndDispatchResultTypeMax));
  rootDocumentListenerHistogram.count(
      static_cast<TouchTargetAndDispatchResultType>(result));
}

}  // namespace

TouchEvent::TouchEvent()
    : m_causesScrollingIfUncanceled(false),
      m_firstTouchMoveOrStart(false),
      m_defaultPreventedBeforeCurrentTarget(false),
      m_currentTouchAction(TouchActionAuto) {}

TouchEvent::TouchEvent(TouchList* touches,
                       TouchList* targetTouches,
                       TouchList* changedTouches,
                       const AtomicString& type,
                       AbstractView* view,
                       PlatformEvent::Modifiers modifiers,
                       bool cancelable,
                       bool causesScrollingIfUncanceled,
                       bool firstTouchMoveOrStart,
                       double platformTimeStamp,
                       TouchAction currentTouchAction,
                       WebPointerProperties::PointerType pointerType)
    // Pass a sourceCapabilities including the ability to fire touchevents when
    // creating this touchevent, which is always created from input device
    // capabilities from EventHandler.
    : UIEventWithKeyState(
          type,
          true,
          cancelable,
          view,
          0,
          modifiers,
          platformTimeStamp,
          InputDeviceCapabilities::firesTouchEventsSourceCapabilities()),
      m_touches(touches),
      m_targetTouches(targetTouches),
      m_changedTouches(changedTouches),
      m_causesScrollingIfUncanceled(causesScrollingIfUncanceled),
      m_firstTouchMoveOrStart(firstTouchMoveOrStart),
      m_defaultPreventedBeforeCurrentTarget(false),
      m_currentTouchAction(currentTouchAction),
      m_pointerType(pointerType) {}

TouchEvent::TouchEvent(const AtomicString& type,
                       const TouchEventInit& initializer)
    : UIEventWithKeyState(type, initializer),
      m_touches(TouchList::create(initializer.touches())),
      m_targetTouches(TouchList::create(initializer.targetTouches())),
      m_changedTouches(TouchList::create(initializer.changedTouches())),
      m_causesScrollingIfUncanceled(false),
      m_firstTouchMoveOrStart(false),
      m_defaultPreventedBeforeCurrentTarget(false),
      m_currentTouchAction(TouchActionAuto),
      m_pointerType(WebPointerProperties::PointerType::Unknown) {}

TouchEvent::~TouchEvent() {}

const AtomicString& TouchEvent::interfaceName() const {
  return EventNames::TouchEvent;
}

bool TouchEvent::isTouchEvent() const {
  return true;
}

void TouchEvent::preventDefault() {
  UIEventWithKeyState::preventDefault();

  // A common developer error is to wait too long before attempting to stop
  // scrolling by consuming a touchmove event. Generate a warning if this
  // event is uncancelable.
  String warningMessage;
  switch (handlingPassive()) {
    case PassiveMode::NotPassive:
    case PassiveMode::NotPassiveDefault:
      if (!cancelable()) {
        warningMessage = "Ignored attempt to cancel a " + type() +
                         " event with cancelable=false, for example "
                         "because scrolling is in progress and "
                         "cannot be interrupted.";
      }
      break;
    case PassiveMode::PassiveForcedDocumentLevel:
      // Only enable the warning when the current touch action is auto because
      // an author may use touch action but call preventDefault for interop with
      // browsers that don't support touch-action.
      if (m_currentTouchAction == TouchActionAuto) {
        warningMessage =
            "Unable to preventDefault inside passive event listener due to "
            "target being treated as passive. See "
            "https://www.chromestatus.com/features/5093566007214080";
      }
      break;
    default:
      break;
  }

  if (!warningMessage.isEmpty() && view() && view()->isLocalDOMWindow() &&
      view()->frame()) {
    toLocalDOMWindow(view())->frame()->console().addMessage(
        ConsoleMessage::create(JSMessageSource, WarningMessageLevel,
                               warningMessage));
  }

  if ((type() == EventTypeNames::touchstart ||
       type() == EventTypeNames::touchmove) &&
      view() && view()->frame() && m_currentTouchAction == TouchActionAuto) {
    switch (handlingPassive()) {
      case PassiveMode::NotPassiveDefault:
        UseCounter::count(view()->frame(),
                          UseCounter::TouchEventPreventedNoTouchAction);
        break;
      case PassiveMode::PassiveForcedDocumentLevel:
        UseCounter::count(
            view()->frame(),
            UseCounter::TouchEventPreventedForcedDocumentPassiveNoTouchAction);
        break;
      default:
        break;
    }
  }
}

void TouchEvent::doneDispatchingEventAtCurrentTarget() {
  // Do not log for non-cancelable events, events that don't block
  // scrolling, have more than one touch point or aren't on the main frame.
  if (!cancelable() || !m_firstTouchMoveOrStart ||
      !(m_touches && m_touches->length() == 1) ||
      !(view() && view()->frame() && view()->frame()->isMainFrame()))
    return;

  bool canceled = defaultPrevented();
  logTouchTargetHistogram(currentTarget(), eventPhase(),
                          m_defaultPreventedBeforeCurrentTarget, canceled);
  m_defaultPreventedBeforeCurrentTarget = canceled;
}

EventDispatchMediator* TouchEvent::createMediator() {
  return TouchEventDispatchMediator::create(this);
}

DEFINE_TRACE(TouchEvent) {
  visitor->trace(m_touches);
  visitor->trace(m_targetTouches);
  visitor->trace(m_changedTouches);
  UIEventWithKeyState::trace(visitor);
}

TouchEventDispatchMediator* TouchEventDispatchMediator::create(
    TouchEvent* touchEvent) {
  return new TouchEventDispatchMediator(touchEvent);
}

TouchEventDispatchMediator::TouchEventDispatchMediator(TouchEvent* touchEvent)
    : EventDispatchMediator(touchEvent) {}

TouchEvent& TouchEventDispatchMediator::event() const {
  return toTouchEvent(EventDispatchMediator::event());
}

DispatchEventResult TouchEventDispatchMediator::dispatchEvent(
    EventDispatcher& dispatcher) const {
  event().eventPath().adjustForTouchEvent(event());
  return dispatcher.dispatch();
}

}  // namespace blink
