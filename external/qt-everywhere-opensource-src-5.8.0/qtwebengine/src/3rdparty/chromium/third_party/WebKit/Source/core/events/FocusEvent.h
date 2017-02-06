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

#ifndef FocusEvent_h
#define FocusEvent_h

#include "core/events/EventTarget.h"
#include "core/events/FocusEventInit.h"
#include "core/events/UIEvent.h"

namespace blink {

class FocusEvent final : public UIEvent {
    DEFINE_WRAPPERTYPEINFO();
public:
    static FocusEvent* create()
    {
        return new FocusEvent;
    }

    static FocusEvent* create(const AtomicString& type, bool canBubble, bool cancelable, AbstractView* view, int detail, EventTarget* relatedTarget, InputDeviceCapabilities* sourceCapabilities)
    {
        return new FocusEvent(type, canBubble, cancelable, view, detail, relatedTarget, sourceCapabilities);
    }

    static FocusEvent* create(const AtomicString& type, const FocusEventInit& initializer)
    {
        return new FocusEvent(type, initializer);
    }

    EventTarget* relatedTarget() const { return m_relatedTarget.get(); }
    void setRelatedTarget(EventTarget* relatedTarget) { m_relatedTarget = relatedTarget; }

    const AtomicString& interfaceName() const override;
    bool isFocusEvent() const override;

    EventDispatchMediator* createMediator() override;

    DECLARE_VIRTUAL_TRACE();

private:
    FocusEvent();
    FocusEvent(const AtomicString& type, bool canBubble, bool cancelable, AbstractView*, int, EventTarget*, InputDeviceCapabilities*);
    FocusEvent(const AtomicString& type, const FocusEventInit&);

    Member<EventTarget> m_relatedTarget;
};

DEFINE_EVENT_TYPE_CASTS(FocusEvent);

class FocusEventDispatchMediator final : public EventDispatchMediator {
public:
    static FocusEventDispatchMediator* create(FocusEvent*);
private:
    explicit FocusEventDispatchMediator(FocusEvent*);
    FocusEvent& event() const { return static_cast<FocusEvent&>(EventDispatchMediator::event()); }
    DispatchEventResult dispatchEvent(EventDispatcher&) const override;
};

} // namespace blink

#endif // FocusEvent_h
