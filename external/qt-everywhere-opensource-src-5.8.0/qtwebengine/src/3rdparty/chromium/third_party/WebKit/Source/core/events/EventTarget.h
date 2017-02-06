/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef EventTarget_h
#define EventTarget_h

#include "bindings/core/v8/AddEventListenerOptionsOrBoolean.h"
#include "bindings/core/v8/EventListenerOptionsOrBoolean.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/EventNames.h"
#include "core/EventTargetNames.h"
#include "core/EventTypeNames.h"
#include "core/events/EventDispatchResult.h"
#include "core/events/EventListenerMap.h"
#include "platform/heap/Handle.h"
#include "wtf/Allocator.h"
#include "wtf/text/AtomicString.h"
#include <memory>

namespace blink {

class DOMWindow;
class Event;
class LocalDOMWindow;
class ExceptionState;
class MessagePort;
class Node;

struct FiringEventIterator {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    FiringEventIterator(const AtomicString& eventType, size_t& iterator, size_t& end)
        : eventType(eventType)
        , iterator(iterator)
        , end(end)
    {
    }

    const AtomicString& eventType;
    size_t& iterator;
    size_t& end;
};
using FiringEventIteratorVector = Vector<FiringEventIterator, 1>;

class CORE_EXPORT EventTargetData final : public GarbageCollectedFinalized<EventTargetData> {
    WTF_MAKE_NONCOPYABLE(EventTargetData);
public:
    EventTargetData();
    ~EventTargetData();

    DECLARE_TRACE();

    EventListenerMap eventListenerMap;
    std::unique_ptr<FiringEventIteratorVector> firingEventIterators;
};

// This is the base class for all DOM event targets. To make your class an
// EventTarget, follow these steps:
// - Make your IDL interface inherit from EventTarget.
// - Inherit from EventTargetWithInlineData (only in rare cases should you
//   use EventTarget directly).
// - In your class declaration, EventTargetWithInlineData must come first in
//   the base class list. If your class is non-final, classes inheriting from
//   your class need to come first, too.
// - If you added an onfoo attribute, use DEFINE_ATTRIBUTE_EVENT_LISTENER(foo)
//   in your class declaration. Add "attribute EventHandler onfoo;" to the IDL
//   file.
// - Override EventTarget::interfaceName() and getExecutionContext(). The former
//   will typically return EventTargetNames::YourClassName. The latter will
//   return ActiveDOMObject::executionContext (if you are an ActiveDOMObject)
//   or the document you're in.
// - Your trace() method will need to call EventTargetWithInlineData::trace
//   depending on the base class of your class.
// - EventTargets do not support EAGERLY_FINALIZE. You need to use
//   a pre-finalizer instead.
class CORE_EXPORT EventTarget : public GarbageCollectedFinalized<EventTarget>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    virtual ~EventTarget();

    virtual const AtomicString& interfaceName() const = 0;
    virtual ExecutionContext* getExecutionContext() const = 0;

    virtual Node* toNode();
    virtual const DOMWindow* toDOMWindow() const;
    virtual const LocalDOMWindow* toLocalDOMWindow() const;
    virtual LocalDOMWindow* toLocalDOMWindow();
    virtual MessagePort* toMessagePort();

    bool addEventListener(const AtomicString& eventType, EventListener*, bool useCapture = false);
    bool addEventListener(const AtomicString& eventType, EventListener*, const AddEventListenerOptionsOrBoolean&);
    bool addEventListener(const AtomicString& eventType, EventListener*, AddEventListenerOptions&);

    bool removeEventListener(const AtomicString& eventType, const EventListener*, bool useCapture = false);
    bool removeEventListener(const AtomicString& eventType, const EventListener*, const EventListenerOptionsOrBoolean&);
    bool removeEventListener(const AtomicString& eventType, const EventListener*, EventListenerOptions&);
    virtual void removeAllEventListeners();

    DispatchEventResult dispatchEvent(Event*);

    // dispatchEventForBindings is intended to only be called from
    // javascript originated calls. This method will validate and may adjust
    // the Event object before dispatching.
    bool dispatchEventForBindings(Event*, ExceptionState&);
    virtual void uncaughtExceptionInEventHandler();

    // Used for legacy "onEvent" attribute APIs.
    bool setAttributeEventListener(const AtomicString& eventType, EventListener*);
    EventListener* getAttributeEventListener(const AtomicString& eventType);

    bool hasEventListeners() const;
    bool hasEventListeners(const AtomicString& eventType) const;
    bool hasCapturingEventListeners(const AtomicString& eventType);
    EventListenerVector* getEventListeners(const AtomicString& eventType);
    Vector<AtomicString> eventTypes();

    DispatchEventResult fireEventListeners(Event*);

    static DispatchEventResult dispatchEventResult(const Event&);

    DEFINE_INLINE_VIRTUAL_TRACE() { }

    DECLARE_VIRTUAL_TRACE_WRAPPERS();

    virtual bool keepEventInNode(Event*) { return false; }

protected:
    EventTarget();

    virtual bool addEventListenerInternal(const AtomicString& eventType, EventListener*, const AddEventListenerOptions&);
    virtual bool removeEventListenerInternal(const AtomicString& eventType, const EventListener*, const EventListenerOptions&);

    // Called when an event listener has been successfully added.
    virtual void addedEventListener(const AtomicString& eventType, RegisteredEventListener&);

    // Called when an event listener is removed. The original registration parameters of this
    // event listener are available to be queried.
    virtual void removedEventListener(const AtomicString& eventType, const RegisteredEventListener&);

    virtual DispatchEventResult dispatchEventInternal(Event*);

    // Subclasses should likely not override these themselves; instead, they should subclass EventTargetWithInlineData.
    virtual EventTargetData* eventTargetData() = 0;
    virtual EventTargetData& ensureEventTargetData() = 0;

private:
    LocalDOMWindow* executingWindow();
    void setDefaultAddEventListenerOptions(const AtomicString& eventType, AddEventListenerOptions&);
    bool fireEventListeners(Event*, EventTargetData*, EventListenerVector&);
    void countLegacyEvents(const AtomicString& legacyTypeName, EventListenerVector*, EventListenerVector*);

    bool clearAttributeEventListener(const AtomicString& eventType);

    friend class EventListenerIterator;
};

// EventTargetData is a GCed object, so it should not be used as a part of
// object. However, we intentionally use it as a part of object for performance,
// assuming that no one extracts a pointer of
// EventTargetWithInlineData::m_eventTargetData and store it to a Member etc.
class GC_PLUGIN_IGNORE("513199") CORE_EXPORT EventTargetWithInlineData : public EventTarget {
public:
    ~EventTargetWithInlineData() override { }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_eventTargetData);
        EventTarget::trace(visitor);
    }

protected:
    EventTargetData* eventTargetData() final { return &m_eventTargetData; }
    EventTargetData& ensureEventTargetData() final { return m_eventTargetData; }

private:
    EventTargetData m_eventTargetData;
};

// FIXME: These macros should be split into separate DEFINE and DECLARE
// macros to avoid causing so many header includes.
#define DEFINE_ATTRIBUTE_EVENT_LISTENER(attribute) \
    EventListener* on##attribute() { return this->getAttributeEventListener(EventTypeNames::attribute); } \
    void setOn##attribute(EventListener* listener) { this->setAttributeEventListener(EventTypeNames::attribute, listener); } \

#define DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(attribute) \
    static EventListener* on##attribute(EventTarget& eventTarget) { return eventTarget.getAttributeEventListener(EventTypeNames::attribute); } \
    static void setOn##attribute(EventTarget& eventTarget, EventListener* listener) { eventTarget.setAttributeEventListener(EventTypeNames::attribute, listener); } \

#define DEFINE_WINDOW_ATTRIBUTE_EVENT_LISTENER(attribute) \
    EventListener* on##attribute() { return document().getWindowAttributeEventListener(EventTypeNames::attribute); } \
    void setOn##attribute(EventListener* listener) { document().setWindowAttributeEventListener(EventTypeNames::attribute, listener); } \

#define DEFINE_STATIC_WINDOW_ATTRIBUTE_EVENT_LISTENER(attribute) \
    static EventListener* on##attribute(EventTarget& eventTarget) { \
        if (Node* node = eventTarget.toNode()) \
            return node->document().getWindowAttributeEventListener(EventTypeNames::attribute); \
        ASSERT(eventTarget.toLocalDOMWindow()); \
        return eventTarget.getAttributeEventListener(EventTypeNames::attribute); \
    } \
    static void setOn##attribute(EventTarget& eventTarget, EventListener* listener) { \
        if (Node* node = eventTarget.toNode()) \
            node->document().setWindowAttributeEventListener(EventTypeNames::attribute, listener); \
        else { \
            ASSERT(eventTarget.toLocalDOMWindow()); \
            eventTarget.setAttributeEventListener(EventTypeNames::attribute, listener); \
        } \
    }

#define DEFINE_MAPPED_ATTRIBUTE_EVENT_LISTENER(attribute, eventName) \
    EventListener* on##attribute() { return getAttributeEventListener(EventTypeNames::eventName); } \
    void setOn##attribute(EventListener* listener) { setAttributeEventListener(EventTypeNames::eventName, listener); } \

inline bool EventTarget::hasEventListeners() const
{
    // FIXME: We should have a const version of eventTargetData.
    if (const EventTargetData* d = const_cast<EventTarget*>(this)->eventTargetData())
        return !d->eventListenerMap.isEmpty();
    return false;
}

inline bool EventTarget::hasEventListeners(const AtomicString& eventType) const
{
    // FIXME: We should have const version of eventTargetData.
    if (const EventTargetData* d = const_cast<EventTarget*>(this)->eventTargetData())
        return d->eventListenerMap.contains(eventType);
    return false;
}

inline bool EventTarget::hasCapturingEventListeners(const AtomicString& eventType)
{
    EventTargetData* d = eventTargetData();
    if (!d)
        return false;
    return d->eventListenerMap.containsCapturing(eventType);
}

} // namespace blink

#endif // EventTarget_h
