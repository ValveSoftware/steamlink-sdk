// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PresentationConnectionAvailableEvent_h
#define PresentationConnectionAvailableEvent_h

#include "modules/EventModules.h"
#include "modules/presentation/PresentationConnection.h"
#include "platform/heap/Handle.h"

namespace blink {

class PresentationConnectionAvailableEventInit;

// Presentation API event to be fired when a presentation has been triggered
// by the embedder using the default presentation URL and id.
// See https://code.google.com/p/chromium/issues/detail?id=459001 for details.
class PresentationConnectionAvailableEvent final : public Event {
    DEFINE_WRAPPERTYPEINFO();
public:
    ~PresentationConnectionAvailableEvent() override;

    static PresentationConnectionAvailableEvent* create()
    {
        return new PresentationConnectionAvailableEvent;
    }
    static PresentationConnectionAvailableEvent* create(const AtomicString& eventType, PresentationConnection* connection)
    {
        return new PresentationConnectionAvailableEvent(eventType, connection);
    }
    static PresentationConnectionAvailableEvent* create(const AtomicString& eventType, const PresentationConnectionAvailableEventInit& initializer)
    {
        return new PresentationConnectionAvailableEvent(eventType, initializer);
    }

    PresentationConnection* connection() { return m_connection.get(); }

    const AtomicString& interfaceName() const override;

    DECLARE_VIRTUAL_TRACE();

private:
    PresentationConnectionAvailableEvent();
    PresentationConnectionAvailableEvent(const AtomicString& eventType, PresentationConnection*);
    PresentationConnectionAvailableEvent(const AtomicString& eventType, const PresentationConnectionAvailableEventInit& initializer);

    Member<PresentationConnection> m_connection;
};

DEFINE_TYPE_CASTS(PresentationConnectionAvailableEvent, Event, event, event->interfaceName() == EventNames::PresentationConnectionAvailableEvent, event.interfaceName() == EventNames::PresentationConnectionAvailableEvent);

} // namespace blink

#endif // PresentationConnectionAvailableEvent_h
