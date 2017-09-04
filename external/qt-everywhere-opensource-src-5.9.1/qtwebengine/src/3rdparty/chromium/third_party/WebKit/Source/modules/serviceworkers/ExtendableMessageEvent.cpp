// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/ExtendableMessageEvent.h"

namespace blink {

ExtendableMessageEvent* ExtendableMessageEvent::create(
    const AtomicString& type,
    const ExtendableMessageEventInit& initializer) {
  return new ExtendableMessageEvent(type, initializer);
}

ExtendableMessageEvent* ExtendableMessageEvent::create(
    const AtomicString& type,
    const ExtendableMessageEventInit& initializer,
    WaitUntilObserver* observer) {
  return new ExtendableMessageEvent(type, initializer, observer);
}

ExtendableMessageEvent* ExtendableMessageEvent::create(
    PassRefPtr<SerializedScriptValue> data,
    const String& origin,
    MessagePortArray* ports,
    WaitUntilObserver* observer) {
  return new ExtendableMessageEvent(std::move(data), origin, ports, observer);
}

ExtendableMessageEvent* ExtendableMessageEvent::create(
    PassRefPtr<SerializedScriptValue> data,
    const String& origin,
    MessagePortArray* ports,
    ServiceWorkerClient* source,
    WaitUntilObserver* observer) {
  ExtendableMessageEvent* event =
      new ExtendableMessageEvent(std::move(data), origin, ports, observer);
  event->m_sourceAsClient = source;
  return event;
}

ExtendableMessageEvent* ExtendableMessageEvent::create(
    PassRefPtr<SerializedScriptValue> data,
    const String& origin,
    MessagePortArray* ports,
    ServiceWorker* source,
    WaitUntilObserver* observer) {
  ExtendableMessageEvent* event =
      new ExtendableMessageEvent(std::move(data), origin, ports, observer);
  event->m_sourceAsServiceWorker = source;
  return event;
}

MessagePortArray ExtendableMessageEvent::ports(bool& isNull) const {
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

MessagePortArray ExtendableMessageEvent::ports() const {
  bool unused;
  return ports(unused);
}

void ExtendableMessageEvent::source(
    ClientOrServiceWorkerOrMessagePort& result) const {
  if (m_sourceAsClient)
    result = ClientOrServiceWorkerOrMessagePort::fromClient(m_sourceAsClient);
  else if (m_sourceAsServiceWorker)
    result = ClientOrServiceWorkerOrMessagePort::fromServiceWorker(
        m_sourceAsServiceWorker);
  else if (m_sourceAsMessagePort)
    result = ClientOrServiceWorkerOrMessagePort::fromMessagePort(
        m_sourceAsMessagePort);
  else
    result = ClientOrServiceWorkerOrMessagePort();
}

const AtomicString& ExtendableMessageEvent::interfaceName() const {
  return EventNames::ExtendableMessageEvent;
}

DEFINE_TRACE(ExtendableMessageEvent) {
  visitor->trace(m_sourceAsClient);
  visitor->trace(m_sourceAsServiceWorker);
  visitor->trace(m_sourceAsMessagePort);
  visitor->trace(m_ports);
  ExtendableEvent::trace(visitor);
}

ExtendableMessageEvent::ExtendableMessageEvent(
    const AtomicString& type,
    const ExtendableMessageEventInit& initializer)
    : ExtendableMessageEvent(type, initializer, nullptr) {}

ExtendableMessageEvent::ExtendableMessageEvent(
    const AtomicString& type,
    const ExtendableMessageEventInit& initializer,
    WaitUntilObserver* observer)
    : ExtendableEvent(type, initializer, observer) {
  if (initializer.hasOrigin())
    m_origin = initializer.origin();
  if (initializer.hasLastEventId())
    m_lastEventId = initializer.lastEventId();
  if (initializer.hasSource()) {
    if (initializer.source().isClient())
      m_sourceAsClient = initializer.source().getAsClient();
    else if (initializer.source().isServiceWorker())
      m_sourceAsServiceWorker = initializer.source().getAsServiceWorker();
    else if (initializer.source().isMessagePort())
      m_sourceAsMessagePort = initializer.source().getAsMessagePort();
  }
  if (initializer.hasPorts())
    m_ports = new MessagePortArray(initializer.ports());
}

ExtendableMessageEvent::ExtendableMessageEvent(
    PassRefPtr<SerializedScriptValue> data,
    const String& origin,
    MessagePortArray* ports,
    WaitUntilObserver* observer)
    : ExtendableEvent(EventTypeNames::message,
                      ExtendableMessageEventInit(),
                      observer),
      m_serializedData(data),
      m_origin(origin),
      m_lastEventId(String()),
      m_ports(ports) {
  if (m_serializedData)
    m_serializedData->registerMemoryAllocatedWithCurrentScriptContext();
}

}  // namespace blink
