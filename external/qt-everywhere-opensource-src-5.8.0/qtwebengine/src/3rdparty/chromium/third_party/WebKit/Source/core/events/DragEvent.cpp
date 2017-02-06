// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/events/DragEvent.h"

#include "core/clipboard/DataTransfer.h"
#include "core/dom/Element.h"
#include "core/events/EventDispatcher.h"

namespace blink {

DragEvent* DragEvent::create(const AtomicString& type, bool canBubble, bool cancelable, AbstractView* view,
    int detail, int screenX, int screenY, int windowX, int windowY,
    int movementX, int movementY,
    PlatformEvent::Modifiers modifiers,
    short button, unsigned short buttons,
    EventTarget* relatedTarget,
    double platformTimeStamp, DataTransfer* dataTransfer,
    PlatformMouseEvent::SyntheticEventType syntheticEventType)
{
    return new DragEvent(type, canBubble, cancelable, view,
        detail, screenX, screenY, windowX, windowY,
        movementX, movementY,
        modifiers, button, buttons, relatedTarget, platformTimeStamp,
        dataTransfer, syntheticEventType);
}


DragEvent::DragEvent()
    : m_dataTransfer(nullptr)
{
}

DragEvent::DragEvent(DataTransfer* dataTransfer)
    : m_dataTransfer(dataTransfer)
{
}

DragEvent::DragEvent(const AtomicString& eventType, bool canBubble, bool cancelable, AbstractView* view,
    int detail, int screenX, int screenY, int windowX, int windowY,
    int movementX, int movementY,
    PlatformEvent::Modifiers modifiers,
    short button, unsigned short buttons, EventTarget* relatedTarget,
    double platformTimeStamp, DataTransfer* dataTransfer,
    PlatformMouseEvent::SyntheticEventType syntheticEventType)
    : MouseEvent(eventType, canBubble, cancelable, view, detail, screenX, screenY,
        windowX, windowY, movementX, movementY, modifiers, button, buttons, relatedTarget,
        platformTimeStamp, syntheticEventType,
        // TODO(zino): Should support canvas hit region because the drag event
        // is a kind of mouse event. Please see http://crbug.com/594073
        String())
    , m_dataTransfer(dataTransfer)

{
}

DragEvent::DragEvent(const AtomicString& type, const DragEventInit& initializer)
    : MouseEvent(type, initializer)
    , m_dataTransfer(initializer.getDataTransfer())
{
}

bool DragEvent::isDragEvent() const
{
    return true;
}

bool DragEvent::isMouseEvent() const
{
    return false;
}

EventDispatchMediator* DragEvent::createMediator()
{
    return DragEventDispatchMediator::create(this);
}

DEFINE_TRACE(DragEvent)
{
    visitor->trace(m_dataTransfer);
    MouseEvent::trace(visitor);
}

DragEventDispatchMediator* DragEventDispatchMediator::create(DragEvent* dragEvent)
{
    return new DragEventDispatchMediator(dragEvent);
}

DragEventDispatchMediator::DragEventDispatchMediator(DragEvent* dragEvent)
    : EventDispatchMediator(dragEvent)
{
}

DragEvent& DragEventDispatchMediator::event() const
{
    return toDragEvent(EventDispatchMediator::event());
}

DispatchEventResult DragEventDispatchMediator::dispatchEvent(EventDispatcher& dispatcher) const
{
    event().eventPath().adjustForRelatedTarget(dispatcher.node(), event().relatedTarget());
    return EventDispatchMediator::dispatchEvent(dispatcher);
}

} // namespace blink
