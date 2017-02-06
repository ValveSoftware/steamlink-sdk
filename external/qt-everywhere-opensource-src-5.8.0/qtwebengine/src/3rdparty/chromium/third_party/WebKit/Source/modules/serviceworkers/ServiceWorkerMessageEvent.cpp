// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/ServiceWorkerMessageEvent.h"

namespace blink {

ServiceWorkerMessageEvent::ServiceWorkerMessageEvent()
{
}

ServiceWorkerMessageEvent::ServiceWorkerMessageEvent(const AtomicString& type, const ServiceWorkerMessageEventInit& initializer)
    : Event(type, initializer)
{
    if (initializer.hasOrigin())
        m_origin = initializer.origin();
    if (initializer.hasLastEventId())
        m_lastEventId = initializer.lastEventId();
    if (initializer.hasSource()) {
        if (initializer.source().isServiceWorker())
            m_sourceAsServiceWorker = initializer.source().getAsServiceWorker();
        else if (initializer.source().isMessagePort())
            m_sourceAsMessagePort = initializer.source().getAsMessagePort();
    }
    if (initializer.hasPorts())
        m_ports = new MessagePortArray(initializer.ports());
}

ServiceWorkerMessageEvent::ServiceWorkerMessageEvent(PassRefPtr<SerializedScriptValue> data, const String& origin, const String& lastEventId, ServiceWorker* source, MessagePortArray* ports)
    : Event(EventTypeNames::message, false, false)
    , m_serializedData(data)
    , m_origin(origin)
    , m_lastEventId(lastEventId)
    , m_sourceAsServiceWorker(source)
    , m_ports(ports)
{
    if (m_serializedData)
        m_serializedData->registerMemoryAllocatedWithCurrentScriptContext();
}

ServiceWorkerMessageEvent::~ServiceWorkerMessageEvent()
{
}

MessagePortArray ServiceWorkerMessageEvent::ports(bool& isNull) const
{
    // TODO(bashi): Currently we return a copied array because the binding
    // layer could modify the content of the array while executing JS callbacks.
    // Avoid copying once we can make sure that the binding layer won't
    // modify the content.
    if (m_ports) {
        isNull = false;
        return *m_ports;
    }
    isNull = true;
    return MessagePortArray();
}

MessagePortArray ServiceWorkerMessageEvent::ports() const
{
    bool unused;
    return ports(unused);
}

void ServiceWorkerMessageEvent::source(ServiceWorkerOrMessagePort& result) const
{
    if (m_sourceAsServiceWorker)
        result = ServiceWorkerOrMessagePort::fromServiceWorker(m_sourceAsServiceWorker);
    else if (m_sourceAsMessagePort)
        result = ServiceWorkerOrMessagePort::fromMessagePort(m_sourceAsMessagePort);
}

const AtomicString& ServiceWorkerMessageEvent::interfaceName() const
{
    return EventNames::ServiceWorkerMessageEvent;
}

DEFINE_TRACE(ServiceWorkerMessageEvent)
{
    visitor->trace(m_sourceAsServiceWorker);
    visitor->trace(m_sourceAsMessagePort);
    visitor->trace(m_ports);
    Event::trace(visitor);
}

} // namespace blink
