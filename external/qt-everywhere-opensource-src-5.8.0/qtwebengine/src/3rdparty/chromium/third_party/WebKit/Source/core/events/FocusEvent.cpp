/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/events/FocusEvent.h"

#include "core/events/Event.h"
#include "core/events/EventDispatcher.h"

namespace blink {

const AtomicString& FocusEvent::interfaceName() const
{
    return EventNames::FocusEvent;
}

bool FocusEvent::isFocusEvent() const
{
    return true;
}

FocusEvent::FocusEvent()
{
}

FocusEvent::FocusEvent(const AtomicString& type, bool canBubble, bool cancelable, AbstractView* view, int detail, EventTarget* relatedTarget, InputDeviceCapabilities* sourceCapabilities)
    : UIEvent(type, canBubble, cancelable, ComposedMode::Composed, view, detail, sourceCapabilities)
    , m_relatedTarget(relatedTarget)
{
}

FocusEvent::FocusEvent(const AtomicString& type, const FocusEventInit& initializer)
    : UIEvent(type, initializer)
{
    if (initializer.hasRelatedTarget())
        m_relatedTarget = initializer.relatedTarget();
}

EventDispatchMediator* FocusEvent::createMediator()
{
    return FocusEventDispatchMediator::create(this);
}

DEFINE_TRACE(FocusEvent)
{
    visitor->trace(m_relatedTarget);
    UIEvent::trace(visitor);
}

FocusEventDispatchMediator* FocusEventDispatchMediator::create(FocusEvent* focusEvent)
{
    return new FocusEventDispatchMediator(focusEvent);
}

FocusEventDispatchMediator::FocusEventDispatchMediator(FocusEvent* focusEvent)
    : EventDispatchMediator(focusEvent)
{
}

DispatchEventResult FocusEventDispatchMediator::dispatchEvent(EventDispatcher& dispatcher) const
{
    event().eventPath().adjustForRelatedTarget(dispatcher.node(), event().relatedTarget());
    return EventDispatchMediator::dispatchEvent(dispatcher);
}

} // namespace blink
