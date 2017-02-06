// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/presentation/PresentationConnectionAvailableEvent.h"

#include "modules/presentation/PresentationConnectionAvailableEventInit.h"

namespace blink {

PresentationConnectionAvailableEvent::~PresentationConnectionAvailableEvent()
{
}

PresentationConnectionAvailableEvent::PresentationConnectionAvailableEvent()
{
}

PresentationConnectionAvailableEvent::PresentationConnectionAvailableEvent(const AtomicString& eventType, PresentationConnection* connection)
    : Event(eventType, false /* canBubble */, false /* cancelable */)
    , m_connection(connection)
{
}

PresentationConnectionAvailableEvent::PresentationConnectionAvailableEvent(const AtomicString& eventType, const PresentationConnectionAvailableEventInit& initializer)
    : Event(eventType, initializer)
    , m_connection(initializer.connection())
{
}

const AtomicString& PresentationConnectionAvailableEvent::interfaceName() const
{
    return EventNames::PresentationConnectionAvailableEvent;
}

DEFINE_TRACE(PresentationConnectionAvailableEvent)
{
    visitor->trace(m_connection);
    Event::trace(visitor);
}

} // namespace blink
