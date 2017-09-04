/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 *           (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
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
 *
 */

#include "core/events/EventTarget.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptEventListener.h"
#include "bindings/core/v8/SourceLocation.h"
#include "bindings/core/v8/V8DOMActivityLogger.h"
#include "core/dom/ExceptionCode.h"
#include "core/editing/Editor.h"
#include "core/events/Event.h"
#include "core/events/EventUtil.h"
#include "core/events/PointerEvent.h"
#include "core/frame/FrameHost.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/PerformanceMonitor.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "platform/EventDispatchForbiddenScope.h"
#include "platform/Histogram.h"
#include "wtf/PtrUtil.h"
#include "wtf/StdLibExtras.h"
#include "wtf/Threading.h"
#include "wtf/Vector.h"
#include <memory>

using namespace WTF;

namespace blink {
namespace {

enum PassiveForcedListenerResultType {
  PreventDefaultNotCalled,
  DocumentLevelTouchPreventDefaultCalled,
  PassiveForcedListenerResultTypeMax
};

Event::PassiveMode eventPassiveMode(
    const RegisteredEventListener& eventListener) {
  if (!eventListener.passive()) {
    if (eventListener.passiveSpecified())
      return Event::PassiveMode::NotPassive;
    return Event::PassiveMode::NotPassiveDefault;
  }
  if (eventListener.passiveForcedForDocumentTarget())
    return Event::PassiveMode::PassiveForcedDocumentLevel;
  if (eventListener.passiveSpecified())
    return Event::PassiveMode::Passive;
  return Event::PassiveMode::PassiveDefault;
}

Settings* windowSettings(LocalDOMWindow* executingWindow) {
  if (executingWindow) {
    if (LocalFrame* frame = executingWindow->frame()) {
      return frame->settings();
    }
  }
  return nullptr;
}

bool isTouchScrollBlockingEvent(const AtomicString& eventType) {
  return eventType == EventTypeNames::touchstart ||
         eventType == EventTypeNames::touchmove;
}

bool isScrollBlockingEvent(const AtomicString& eventType) {
  return isTouchScrollBlockingEvent(eventType) ||
         eventType == EventTypeNames::mousewheel ||
         eventType == EventTypeNames::wheel;
}

double blockedEventsWarningThreshold(ExecutionContext* context,
                                     const Event* event) {
  if (!event->cancelable())
    return 0.0;
  if (!isScrollBlockingEvent(event->type()))
    return 0.0;
  return PerformanceMonitor::threshold(context,
                                       PerformanceMonitor::kBlockedEvent);
}

void reportBlockedEvent(ExecutionContext* context,
                        const Event* event,
                        RegisteredEventListener* registeredListener,
                        double delayedSeconds) {
  if (registeredListener->listener()->type() !=
      EventListener::JSEventListenerType)
    return;

  String messageText = String::format(
      "Handling of '%s' input event was delayed for %ld ms due to main thread "
      "being busy. "
      "Consider marking event handler as 'passive' to make the page more "
      "responsive.",
      event->type().getString().utf8().data(), lround(delayedSeconds * 1000));

  PerformanceMonitor::reportGenericViolation(
      context, PerformanceMonitor::kBlockedEvent, messageText, delayedSeconds,
      getFunctionLocation(context, registeredListener->listener()).get());
  registeredListener->setBlockedEventWarningEmitted();
}

}  // namespace

EventTargetData::EventTargetData() {}

EventTargetData::~EventTargetData() {}

DEFINE_TRACE(EventTargetData) {
  visitor->trace(eventListenerMap);
}

DEFINE_TRACE_WRAPPERS(EventTarget) {
  EventListenerIterator iterator(const_cast<EventTarget*>(this));
  while (EventListener* listener = iterator.nextListener()) {
    if (listener->type() != EventListener::JSEventListenerType)
      continue;
    visitor->traceWrappers(static_cast<V8AbstractEventListener*>(listener));
  }
}

EventTarget::EventTarget() {}

EventTarget::~EventTarget() {}

Node* EventTarget::toNode() {
  return nullptr;
}

const DOMWindow* EventTarget::toDOMWindow() const {
  return nullptr;
}

const LocalDOMWindow* EventTarget::toLocalDOMWindow() const {
  return nullptr;
}

LocalDOMWindow* EventTarget::toLocalDOMWindow() {
  return nullptr;
}

MessagePort* EventTarget::toMessagePort() {
  return nullptr;
}

inline LocalDOMWindow* EventTarget::executingWindow() {
  if (ExecutionContext* context = getExecutionContext())
    return context->executingWindow();
  return nullptr;
}

void EventTarget::setDefaultAddEventListenerOptions(
    const AtomicString& eventType,
    AddEventListenerOptionsResolved& options) {
  options.setPassiveSpecified(options.hasPassive());

  if (!isScrollBlockingEvent(eventType)) {
    if (!options.hasPassive())
      options.setPassive(false);
    return;
  }

  if (LocalDOMWindow* executingWindow = this->executingWindow()) {
    if (options.hasPassive()) {
      UseCounter::count(executingWindow->document(),
                        options.passive()
                            ? UseCounter::AddEventListenerPassiveTrue
                            : UseCounter::AddEventListenerPassiveFalse);
    }
  }

  if (RuntimeEnabledFeatures::passiveDocumentEventListenersEnabled() &&
      isTouchScrollBlockingEvent(eventType)) {
    if (!options.hasPassive()) {
      if (Node* node = toNode()) {
        if (node->isDocumentNode() ||
            node->document().documentElement() == node ||
            node->document().body() == node) {
          options.setPassive(true);
          options.setPassiveForcedForDocumentTarget(true);
          return;
        }
      } else if (toLocalDOMWindow()) {
        options.setPassive(true);
        options.setPassiveForcedForDocumentTarget(true);
        return;
      }
    }
  }

  if (Settings* settings = windowSettings(executingWindow())) {
    switch (settings->passiveListenerDefault()) {
      case PassiveListenerDefault::False:
        if (!options.hasPassive())
          options.setPassive(false);
        break;
      case PassiveListenerDefault::True:
        if (!options.hasPassive())
          options.setPassive(true);
        break;
      case PassiveListenerDefault::ForceAllTrue:
        options.setPassive(true);
        break;
    }
  } else {
    if (!options.hasPassive())
      options.setPassive(false);
  }
}

bool EventTarget::addEventListener(const AtomicString& eventType,
                                   EventListener* listener,
                                   bool useCapture) {
  AddEventListenerOptionsResolved options;
  options.setCapture(useCapture);
  setDefaultAddEventListenerOptions(eventType, options);
  return addEventListenerInternal(eventType, listener, options);
}

bool EventTarget::addEventListener(
    const AtomicString& eventType,
    EventListener* listener,
    const AddEventListenerOptionsOrBoolean& optionsUnion) {
  if (optionsUnion.isBoolean())
    return addEventListener(eventType, listener, optionsUnion.getAsBoolean());
  if (optionsUnion.isAddEventListenerOptions()) {
    AddEventListenerOptionsResolved options =
        optionsUnion.getAsAddEventListenerOptions();
    return addEventListener(eventType, listener, options);
  }
  return addEventListener(eventType, listener);
}

bool EventTarget::addEventListener(const AtomicString& eventType,
                                   EventListener* listener,
                                   AddEventListenerOptionsResolved& options) {
  setDefaultAddEventListenerOptions(eventType, options);
  return addEventListenerInternal(eventType, listener, options);
}

bool EventTarget::addEventListenerInternal(
    const AtomicString& eventType,
    EventListener* listener,
    const AddEventListenerOptionsResolved& options) {
  if (!listener)
    return false;

  V8DOMActivityLogger* activityLogger =
      V8DOMActivityLogger::currentActivityLoggerIfIsolatedWorld();
  if (activityLogger) {
    Vector<String> argv;
    argv.append(toNode() ? toNode()->nodeName() : interfaceName());
    argv.append(eventType);
    activityLogger->logEvent("blinkAddEventListener", argv.size(), argv.data());
  }

  RegisteredEventListener registeredListener;
  bool added = ensureEventTargetData().eventListenerMap.add(
      eventType, listener, options, &registeredListener);
  if (added) {
    if (listener->type() == EventListener::JSEventListenerType) {
      ScriptWrappableVisitor::writeBarrier(
          this, static_cast<V8AbstractEventListener*>(listener));
    }
    addedEventListener(eventType, registeredListener);
  }
  return added;
}

void EventTarget::addedEventListener(
    const AtomicString& eventType,
    RegisteredEventListener& registeredListener) {
  if (eventType == EventTypeNames::auxclick) {
    if (LocalDOMWindow* executingWindow = this->executingWindow()) {
      UseCounter::count(executingWindow->document(),
                        UseCounter::AuxclickAddListenerCount);
    }
  } else if (EventUtil::isPointerEventType(eventType)) {
    if (LocalDOMWindow* executingWindow = this->executingWindow()) {
      UseCounter::count(executingWindow->document(),
                        UseCounter::PointerEventAddListenerCount);
    }
  } else if (eventType == EventTypeNames::slotchange) {
    if (LocalDOMWindow* executingWindow = this->executingWindow()) {
      UseCounter::count(executingWindow->document(),
                        UseCounter::SlotChangeEventAddListener);
    }
  }
}

bool EventTarget::removeEventListener(const AtomicString& eventType,
                                      const EventListener* listener,
                                      bool useCapture) {
  EventListenerOptions options;
  options.setCapture(useCapture);
  return removeEventListenerInternal(eventType, listener, options);
}

bool EventTarget::removeEventListener(
    const AtomicString& eventType,
    const EventListener* listener,
    const EventListenerOptionsOrBoolean& optionsUnion) {
  if (optionsUnion.isBoolean())
    return removeEventListener(eventType, listener,
                               optionsUnion.getAsBoolean());
  if (optionsUnion.isEventListenerOptions()) {
    EventListenerOptions options = optionsUnion.getAsEventListenerOptions();
    return removeEventListener(eventType, listener, options);
  }
  return removeEventListener(eventType, listener);
}

bool EventTarget::removeEventListener(const AtomicString& eventType,
                                      const EventListener* listener,
                                      EventListenerOptions& options) {
  return removeEventListenerInternal(eventType, listener, options);
}

bool EventTarget::removeEventListenerInternal(
    const AtomicString& eventType,
    const EventListener* listener,
    const EventListenerOptions& options) {
  if (!listener)
    return false;

  EventTargetData* d = eventTargetData();
  if (!d)
    return false;

  size_t indexOfRemovedListener;
  RegisteredEventListener registeredListener;

  if (!d->eventListenerMap.remove(eventType, listener, options,
                                  &indexOfRemovedListener, &registeredListener))
    return false;

  // Notify firing events planning to invoke the listener at 'index' that
  // they have one less listener to invoke.
  if (d->firingEventIterators) {
    for (size_t i = 0; i < d->firingEventIterators->size(); ++i) {
      FiringEventIterator& firingIterator = d->firingEventIterators->at(i);
      if (eventType != firingIterator.eventType)
        continue;

      if (indexOfRemovedListener >= firingIterator.end)
        continue;

      --firingIterator.end;
      // Note that when firing an event listener,
      // firingIterator.iterator indicates the next event listener
      // that would fire, not the currently firing event
      // listener. See EventTarget::fireEventListeners.
      if (indexOfRemovedListener < firingIterator.iterator)
        --firingIterator.iterator;
    }
  }
  removedEventListener(eventType, registeredListener);
  return true;
}

void EventTarget::removedEventListener(
    const AtomicString& eventType,
    const RegisteredEventListener& registeredListener) {}

bool EventTarget::setAttributeEventListener(const AtomicString& eventType,
                                            EventListener* listener) {
  clearAttributeEventListener(eventType);
  if (!listener)
    return false;
  return addEventListener(eventType, listener, false);
}

EventListener* EventTarget::getAttributeEventListener(
    const AtomicString& eventType) {
  EventListenerVector* listenerVector = getEventListeners(eventType);
  if (!listenerVector)
    return nullptr;

  for (auto& eventListener : *listenerVector) {
    EventListener* listener = eventListener.listener();
    if (listener->isAttribute() &&
        listener->belongsToTheCurrentWorld(getExecutionContext()))
      return listener;
  }
  return nullptr;
}

bool EventTarget::clearAttributeEventListener(const AtomicString& eventType) {
  EventListener* listener = getAttributeEventListener(eventType);
  if (!listener)
    return false;
  return removeEventListener(eventType, listener, false);
}

bool EventTarget::dispatchEventForBindings(Event* event,
                                           ExceptionState& exceptionState) {
  if (!event->wasInitialized()) {
    exceptionState.throwDOMException(InvalidStateError,
                                     "The event provided is uninitialized.");
    return false;
  }
  if (event->isBeingDispatched()) {
    exceptionState.throwDOMException(InvalidStateError,
                                     "The event is already being dispatched.");
    return false;
  }

  if (!getExecutionContext())
    return false;

  event->setTrusted(false);

  // Return whether the event was cancelled or not to JS not that it
  // might have actually been default handled; so check only against
  // CanceledByEventHandler.
  return dispatchEventInternal(event) !=
         DispatchEventResult::CanceledByEventHandler;
}

DispatchEventResult EventTarget::dispatchEvent(Event* event) {
  event->setTrusted(true);
  return dispatchEventInternal(event);
}

DispatchEventResult EventTarget::dispatchEventInternal(Event* event) {
  event->setTarget(this);
  event->setCurrentTarget(this);
  event->setEventPhase(Event::kAtTarget);
  DispatchEventResult dispatchResult = fireEventListeners(event);
  event->setEventPhase(0);
  return dispatchResult;
}

void EventTarget::uncaughtExceptionInEventHandler() {}

static const AtomicString& legacyType(const Event* event) {
  if (event->type() == EventTypeNames::transitionend)
    return EventTypeNames::webkitTransitionEnd;

  if (event->type() == EventTypeNames::animationstart)
    return EventTypeNames::webkitAnimationStart;

  if (event->type() == EventTypeNames::animationend)
    return EventTypeNames::webkitAnimationEnd;

  if (event->type() == EventTypeNames::animationiteration)
    return EventTypeNames::webkitAnimationIteration;

  if (event->type() == EventTypeNames::wheel)
    return EventTypeNames::mousewheel;

  return emptyAtom;
}

void EventTarget::countLegacyEvents(
    const AtomicString& legacyTypeName,
    EventListenerVector* listenersVector,
    EventListenerVector* legacyListenersVector) {
  UseCounter::Feature unprefixedFeature;
  UseCounter::Feature prefixedFeature;
  UseCounter::Feature prefixedAndUnprefixedFeature;
  if (legacyTypeName == EventTypeNames::webkitTransitionEnd) {
    prefixedFeature = UseCounter::PrefixedTransitionEndEvent;
    unprefixedFeature = UseCounter::UnprefixedTransitionEndEvent;
    prefixedAndUnprefixedFeature =
        UseCounter::PrefixedAndUnprefixedTransitionEndEvent;
  } else if (legacyTypeName == EventTypeNames::webkitAnimationEnd) {
    prefixedFeature = UseCounter::PrefixedAnimationEndEvent;
    unprefixedFeature = UseCounter::UnprefixedAnimationEndEvent;
    prefixedAndUnprefixedFeature =
        UseCounter::PrefixedAndUnprefixedAnimationEndEvent;
  } else if (legacyTypeName == EventTypeNames::webkitAnimationStart) {
    prefixedFeature = UseCounter::PrefixedAnimationStartEvent;
    unprefixedFeature = UseCounter::UnprefixedAnimationStartEvent;
    prefixedAndUnprefixedFeature =
        UseCounter::PrefixedAndUnprefixedAnimationStartEvent;
  } else if (legacyTypeName == EventTypeNames::webkitAnimationIteration) {
    prefixedFeature = UseCounter::PrefixedAnimationIterationEvent;
    unprefixedFeature = UseCounter::UnprefixedAnimationIterationEvent;
    prefixedAndUnprefixedFeature =
        UseCounter::PrefixedAndUnprefixedAnimationIterationEvent;
  } else if (legacyTypeName == EventTypeNames::mousewheel) {
    prefixedFeature = UseCounter::MouseWheelEvent;
    unprefixedFeature = UseCounter::WheelEvent;
    prefixedAndUnprefixedFeature = UseCounter::MouseWheelAndWheelEvent;
  } else {
    return;
  }

  if (LocalDOMWindow* executingWindow = this->executingWindow()) {
    if (legacyListenersVector) {
      if (listenersVector)
        UseCounter::count(executingWindow->document(),
                          prefixedAndUnprefixedFeature);
      else
        UseCounter::count(executingWindow->document(), prefixedFeature);
    } else if (listenersVector) {
      UseCounter::count(executingWindow->document(), unprefixedFeature);
    }
  }
}

DispatchEventResult EventTarget::fireEventListeners(Event* event) {
#if DCHECK_IS_ON()
  DCHECK(!EventDispatchForbiddenScope::isEventDispatchForbidden());
#endif
  DCHECK(event);
  DCHECK(event->wasInitialized());

  EventTargetData* d = eventTargetData();
  if (!d)
    return DispatchEventResult::NotCanceled;

  EventListenerVector* legacyListenersVector = nullptr;
  AtomicString legacyTypeName = legacyType(event);
  if (!legacyTypeName.isEmpty())
    legacyListenersVector = d->eventListenerMap.find(legacyTypeName);

  EventListenerVector* listenersVector =
      d->eventListenerMap.find(event->type());

  bool firedEventListeners = false;
  if (listenersVector) {
    firedEventListeners = fireEventListeners(event, d, *listenersVector);
  } else if (legacyListenersVector) {
    AtomicString unprefixedTypeName = event->type();
    event->setType(legacyTypeName);
    firedEventListeners = fireEventListeners(event, d, *legacyListenersVector);
    event->setType(unprefixedTypeName);
  }

  // Only invoke the callback if event listeners were fired for this phase.
  if (firedEventListeners) {
    event->doneDispatchingEventAtCurrentTarget();

    // Only count uma metrics if we really fired an event listener.
    Editor::countEvent(getExecutionContext(), event);
    countLegacyEvents(legacyTypeName, listenersVector, legacyListenersVector);
  }
  return dispatchEventResult(*event);
}

bool EventTarget::checkTypeThenUseCount(const Event* event,
                                        const AtomicString& eventTypeToCount,
                                        const UseCounter::Feature feature) {
  if (event->type() == eventTypeToCount) {
    if (LocalDOMWindow* executingWindow = this->executingWindow())
      UseCounter::count(executingWindow->document(), feature);
    return true;
  }
  return false;
}

bool EventTarget::fireEventListeners(Event* event,
                                     EventTargetData* d,
                                     EventListenerVector& entry) {
  // Fire all listeners registered for this event. Don't fire listeners removed
  // during event dispatch. Also, don't fire event listeners added during event
  // dispatch. Conveniently, all new event listeners will be added after or at
  // index |size|, so iterating up to (but not including) |size| naturally
  // excludes new event listeners.

  if (checkTypeThenUseCount(event, EventTypeNames::beforeunload,
                            UseCounter::DocumentBeforeUnloadFired)) {
    if (LocalDOMWindow* executingWindow = this->executingWindow()) {
      if (executingWindow != executingWindow->top())
        UseCounter::count(executingWindow->document(),
                          UseCounter::SubFrameBeforeUnloadFired);
    }
  } else if (checkTypeThenUseCount(event, EventTypeNames::unload,
                                   UseCounter::DocumentUnloadFired)) {
  } else if (checkTypeThenUseCount(event, EventTypeNames::DOMFocusIn,
                                   UseCounter::DOMFocusInOutEvent)) {
  } else if (checkTypeThenUseCount(event, EventTypeNames::DOMFocusOut,
                                   UseCounter::DOMFocusInOutEvent)) {
  } else if (checkTypeThenUseCount(event, EventTypeNames::focusin,
                                   UseCounter::FocusInOutEvent)) {
  } else if (checkTypeThenUseCount(event, EventTypeNames::focusout,
                                   UseCounter::FocusInOutEvent)) {
  } else if (checkTypeThenUseCount(event, EventTypeNames::textInput,
                                   UseCounter::TextInputFired)) {
  } else if (checkTypeThenUseCount(event, EventTypeNames::touchstart,
                                   UseCounter::TouchStartFired)) {
  } else if (checkTypeThenUseCount(event, EventTypeNames::mousedown,
                                   UseCounter::MouseDownFired)) {
  } else if (checkTypeThenUseCount(event, EventTypeNames::pointerdown,
                                   UseCounter::PointerDownFired)) {
    if (LocalDOMWindow* executingWindow = this->executingWindow()) {
      if (event->isPointerEvent() &&
          static_cast<PointerEvent*>(event)->pointerType() == "touch")
        UseCounter::count(executingWindow->document(),
                          UseCounter::PointerDownFiredForTouch);
    }
  } else if (checkTypeThenUseCount(event, EventTypeNames::pointerenter,
                                   UseCounter::PointerEnterLeaveFired)) {
  } else if (checkTypeThenUseCount(event, EventTypeNames::pointerleave,
                                   UseCounter::PointerEnterLeaveFired)) {
  } else if (checkTypeThenUseCount(event, EventTypeNames::pointerover,
                                   UseCounter::PointerOverOutFired)) {
  } else if (checkTypeThenUseCount(event, EventTypeNames::pointerout,
                                   UseCounter::PointerOverOutFired)) {
  }

  ExecutionContext* context = getExecutionContext();
  if (!context)
    return false;

  size_t i = 0;
  size_t size = entry.size();
  if (!d->firingEventIterators)
    d->firingEventIterators = wrapUnique(new FiringEventIteratorVector);
  d->firingEventIterators->append(FiringEventIterator(event->type(), i, size));

  double blockedEventThreshold = blockedEventsWarningThreshold(context, event);
  double now = 0.0;
  bool shouldReportBlockedEvent = false;
  if (blockedEventThreshold) {
    now = WTF::monotonicallyIncreasingTime();
    shouldReportBlockedEvent =
        now - event->platformTimeStamp() > blockedEventThreshold;
  }
  bool firedListener = false;

  while (i < size) {
    RegisteredEventListener registeredListener = entry[i];

    // Move the iterator past this event listener. This must match
    // the handling of the FiringEventIterator::iterator in
    // EventTarget::removeEventListener.
    ++i;

    if (event->eventPhase() == Event::kCapturingPhase &&
        !registeredListener.capture())
      continue;
    if (event->eventPhase() == Event::kBubblingPhase &&
        registeredListener.capture())
      continue;

    EventListener* listener = registeredListener.listener();
    // The listener will be retained by Member<EventListener> in the
    // registeredListener, i and size are updated with the firing event iterator
    // in case the listener is removed from the listener vector below.
    if (registeredListener.once())
      removeEventListener(event->type(), listener,
                          registeredListener.capture());

    // If stopImmediatePropagation has been called, we just break out
    // immediately, without handling any more events on this target.
    if (event->immediatePropagationStopped())
      break;

    event->setHandlingPassive(eventPassiveMode(registeredListener));
    bool passiveForced = registeredListener.passiveForcedForDocumentTarget();

    InspectorInstrumentation::NativeBreakpoint nativeBreakpoint(context, this,
                                                                event);
    PerformanceMonitor::HandlerCall handlerCall(context, listener);

    // To match Mozilla, the AT_TARGET phase fires both capturing and bubbling
    // event listeners, even though that violates some versions of the DOM spec.
    listener->handleEvent(context, event);
    firedListener = true;

    // If we're about to report this event listener as blocking, make sure it
    // wasn't removed while handling the event.
    if (shouldReportBlockedEvent && i > 0 &&
        entry[i - 1].listener() == listener && !entry[i - 1].passive() &&
        !entry[i - 1].blockedEventWarningEmitted() &&
        !event->defaultPrevented()) {
      reportBlockedEvent(context, event, &entry[i - 1],
                         now - event->platformTimeStamp());
    }

    if (passiveForced) {
      DEFINE_STATIC_LOCAL(EnumerationHistogram, passiveForcedHistogram,
                          ("Event.PassiveForcedEventDispatchCancelled",
                           PassiveForcedListenerResultTypeMax));
      PassiveForcedListenerResultType breakageType = PreventDefaultNotCalled;
      if (event->preventDefaultCalledDuringPassive())
        breakageType = DocumentLevelTouchPreventDefaultCalled;

      passiveForcedHistogram.count(breakageType);
    }

    event->setHandlingPassive(Event::PassiveMode::NotPassive);

    CHECK_LE(i, size);
  }
  d->firingEventIterators->pop_back();
  return firedListener;
}

DispatchEventResult EventTarget::dispatchEventResult(const Event& event) {
  if (event.defaultPrevented())
    return DispatchEventResult::CanceledByEventHandler;
  if (event.defaultHandled())
    return DispatchEventResult::CanceledByDefaultEventHandler;
  return DispatchEventResult::NotCanceled;
}

EventListenerVector* EventTarget::getEventListeners(
    const AtomicString& eventType) {
  EventTargetData* data = eventTargetData();
  if (!data)
    return nullptr;
  return data->eventListenerMap.find(eventType);
}

Vector<AtomicString> EventTarget::eventTypes() {
  EventTargetData* d = eventTargetData();
  return d ? d->eventListenerMap.eventTypes() : Vector<AtomicString>();
}

void EventTarget::removeAllEventListeners() {
  EventTargetData* d = eventTargetData();
  if (!d)
    return;
  d->eventListenerMap.clear();

  // Notify firing events planning to invoke the listener at 'index' that
  // they have one less listener to invoke.
  if (d->firingEventIterators) {
    for (size_t i = 0; i < d->firingEventIterators->size(); ++i) {
      d->firingEventIterators->at(i).iterator = 0;
      d->firingEventIterators->at(i).end = 0;
    }
  }
}

}  // namespace blink
