/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef Event_h
#define Event_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/dom/DOMHighResTimeStamp.h"
#include "core/dom/DOMTimeStamp.h"
#include "core/events/EventInit.h"
#include "core/events/EventPath.h"
#include "platform/heap/Handle.h"
#include "wtf/text/AtomicString.h"

namespace blink {

class DOMWrapperWorld;
class EventTarget;
class ExecutionContext;

class CORE_EXPORT Event : public GarbageCollectedFinalized<Event>,  public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    enum PhaseType {
        NONE                = 0,
        CAPTURING_PHASE     = 1,
        AT_TARGET           = 2,
        BUBBLING_PHASE      = 3
    };

    enum EventType {
        MOUSEDOWN           = 1,
        MOUSEUP             = 2,
        MOUSEOVER           = 4,
        MOUSEOUT            = 8,
        MOUSEMOVE           = 16,
        MOUSEDRAG           = 32,
        CLICK               = 64,
        DBLCLICK            = 128,
        KEYDOWN             = 256,
        KEYUP               = 512,
        KEYPRESS            = 1024,
        DRAGDROP            = 2048,
        FOCUS               = 4096,
        BLUR                = 8192,
        SELECT              = 16384,
        CHANGE              = 32768
    };

    enum RailsMode {
        RailsModeFree       = 0,
        RailsModeHorizontal = 1,
        RailsModeVertical   = 2
    };

    enum class ComposedMode {
        Composed,
        Scoped,
    };

    static Event* create()
    {
        return new Event;
    }

    // A factory for a simple event. The event doesn't bubble, and isn't
    // cancelable.
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/webappapis.html#fire-a-simple-event
    static Event* create(const AtomicString& type)
    {
        return new Event(type, false, false);
    }
    static Event* createCancelable(const AtomicString& type)
    {
        return new Event(type, false, true);
    }
    static Event* createBubble(const AtomicString& type)
    {
        return new Event(type, true, false);
    }
    static Event* createCancelableBubble(const AtomicString& type)
    {
        return new Event(type, true, true);
    }

    static Event* create(const AtomicString& type, const EventInit& initializer)
    {
        return new Event(type, initializer);
    }

    virtual ~Event();

    void initEvent(const AtomicString& type, bool canBubble, bool cancelable);
    void initEvent(const AtomicString& eventTypeArg, bool canBubbleArg, bool cancelableArg, EventTarget* relatedTarget);

    const AtomicString& type() const { return m_type; }
    void setType(const AtomicString& type) { m_type = type; }

    EventTarget* target() const { return m_target.get(); }
    void setTarget(EventTarget*);

    EventTarget* currentTarget() const { return m_currentTarget; }
    void setCurrentTarget(EventTarget* currentTarget) { m_currentTarget = currentTarget; }

    // This callback is invoked when an event listener has been dispatched
    // at the current target. It should only be used to influence UMA metrics
    // and not change functionality since observing the presence of listeners
    // is dangerous.
    virtual void doneDispatchingEventAtCurrentTarget() {}

    unsigned short eventPhase() const { return m_eventPhase; }
    void setEventPhase(unsigned short eventPhase) { m_eventPhase = eventPhase; }

    bool bubbles() const { return m_canBubble; }
    bool cancelable() const { return m_cancelable; }
    bool composed() const { return m_composed; }
    bool isScopedInV0() const;

    // Event creation timestamp in milliseconds. If |HiResEventTimeStamp|
    // runtime feature is enabled it returns a DOMHighResTimeStamp using the
    // platform timestamp (see |m_platformTimeStamp|) otherwise it returns a
    // DOMTimeStamp that represents the current object's construction time (see
    // |m_createTime|). For more info see http://crbug.com/160524
    double timeStamp(ScriptState*) const;
    double platformTimeStamp() const { return m_platformTimeStamp; }
    DOMTimeStamp createTime() const { return m_createTime; }

    void stopPropagation() { m_propagationStopped = true; }
    void stopImmediatePropagation() { m_immediatePropagationStopped = true; }

    // IE Extensions
    EventTarget* srcElement() const { return target(); } // MSIE extension - "the object that fired the event"

    bool legacyReturnValue(ExecutionContext*) const;
    void setLegacyReturnValue(ExecutionContext*, bool returnValue);

    virtual const AtomicString& interfaceName() const;
    bool hasInterface(const AtomicString&) const;

    // These events are general classes of events.
    virtual bool isUIEvent() const;
    virtual bool isMouseEvent() const;
    virtual bool isFocusEvent() const;
    virtual bool isKeyboardEvent() const;
    virtual bool isTouchEvent() const;
    virtual bool isGestureEvent() const;
    virtual bool isWheelEvent() const;
    virtual bool isRelatedEvent() const;
    virtual bool isPointerEvent() const;
    virtual bool isInputEvent() const;

    // Drag events are a subset of mouse events.
    virtual bool isDragEvent() const;

    // These events lack a DOM interface.
    virtual bool isClipboardEvent() const;
    virtual bool isBeforeTextInsertedEvent() const;

    virtual bool isBeforeUnloadEvent() const;

    bool propagationStopped() const { return m_propagationStopped || m_immediatePropagationStopped; }
    bool immediatePropagationStopped() const { return m_immediatePropagationStopped; }
    bool wasInitialized() { return m_wasInitialized; }

    bool defaultPrevented() const { return m_defaultPrevented; }
    virtual void preventDefault();
    void setDefaultPrevented(bool defaultPrevented) { m_defaultPrevented = defaultPrevented; }

    bool defaultHandled() const { return m_defaultHandled; }
    void setDefaultHandled() { m_defaultHandled = true; }

    bool cancelBubble(ExecutionContext* = nullptr) const { return m_cancelBubble; }
    void setCancelBubble(ExecutionContext*, bool);

    Event* underlyingEvent() const { return m_underlyingEvent.get(); }
    void setUnderlyingEvent(Event*);

    bool hasEventPath() { return m_eventPath; }
    EventPath& eventPath() { ASSERT(m_eventPath); return *m_eventPath; }
    void initEventPath(Node&);

    HeapVector<Member<EventTarget>> path(ScriptState*) const;
    HeapVector<Member<EventTarget>> composedPath(ScriptState*) const;

    bool isBeingDispatched() const { return eventPhase(); }

    // Events that must not leak across isolated world, similar to how
    // ErrorEvent behaves, can override this method.
    virtual bool canBeDispatchedInWorld(const DOMWrapperWorld&) const { return true; }

    virtual EventDispatchMediator* createMediator();

    bool isTrusted() const { return m_isTrusted; }
    void setTrusted(bool value) { m_isTrusted = value; }

    void setHandlingPassive(bool value) { m_handlingPassive = value; }

    DECLARE_VIRTUAL_TRACE();

protected:
    Event();
    Event(const AtomicString& type, bool canBubble, bool cancelable, ComposedMode, double platformTimeStamp);
    Event(const AtomicString& type, bool canBubble, bool cancelable, double platformTimeStamp);
    Event(const AtomicString& type, bool canBubble, bool cancelable, ComposedMode = ComposedMode::Scoped);
    Event(const AtomicString& type, const EventInit&);

    virtual void receivedTarget();

    void setCanBubble(bool bubble) { m_canBubble = bubble; }

private:

    enum EventPathMode {
        EmptyAfterDispatch,
        NonEmptyAfterDispatch
    };

    HeapVector<Member<EventTarget>> pathInternal(ScriptState*, EventPathMode) const;

    AtomicString m_type;
    unsigned m_canBubble:1;
    unsigned m_cancelable:1;
    unsigned m_composed:1;
    unsigned m_isEventTypeScopedInV0:1;

    unsigned m_propagationStopped:1;
    unsigned m_immediatePropagationStopped:1;
    unsigned m_defaultPrevented:1;
    unsigned m_defaultHandled:1;
    unsigned m_cancelBubble:1;
    unsigned m_wasInitialized:1;
    unsigned m_isTrusted : 1;
    unsigned m_handlingPassive : 1;

    unsigned short m_eventPhase;
    Member<EventTarget> m_currentTarget;
    Member<EventTarget> m_target;
    DOMTimeStamp m_createTime;
    Member<Event> m_underlyingEvent;
    Member<EventPath> m_eventPath;
    // The monotonic platform time in seconds, for input events it is the
    // event timestamp provided by the host OS and reported in the original
    // WebInputEvent instance.
    double m_platformTimeStamp;
};

#define DEFINE_EVENT_TYPE_CASTS(typeName) \
    DEFINE_TYPE_CASTS(typeName, Event, event, event->is##typeName(), event.is##typeName())

} // namespace blink

#endif // Event_h
