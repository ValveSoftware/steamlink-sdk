// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ServiceWorkerMessageEvent_h
#define ServiceWorkerMessageEvent_h

#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/MessagePort.h"
#include "modules/EventModules.h"
#include "modules/ModulesExport.h"
#include "modules/serviceworkers/ServiceWorkerMessageEventInit.h"

namespace blink {

class MODULES_EXPORT ServiceWorkerMessageEvent final : public Event {
    DEFINE_WRAPPERTYPEINFO();
public:
    static ServiceWorkerMessageEvent* create()
    {
        return new ServiceWorkerMessageEvent;
    }

    static ServiceWorkerMessageEvent* create(const AtomicString& type, const ServiceWorkerMessageEventInit& initializer)
    {
        return new ServiceWorkerMessageEvent(type, initializer);
    }

    static ServiceWorkerMessageEvent* create(MessagePortArray* ports, PassRefPtr<SerializedScriptValue> data, ServiceWorker* source, const String& origin)
    {
        return new ServiceWorkerMessageEvent(data, origin, String(), source, ports);
    }

    ~ServiceWorkerMessageEvent() override;

    SerializedScriptValue* serializedData() const { return m_serializedData.get(); }
    void setSerializedData(PassRefPtr<SerializedScriptValue> serializedData) { m_serializedData = serializedData; }
    const String& origin() const { return m_origin; }
    const String& lastEventId() const { return m_lastEventId; }
    MessagePortArray ports(bool& isNull) const;
    MessagePortArray ports() const;
    void source(ServiceWorkerOrMessagePort& result) const;

    const AtomicString& interfaceName() const override;

    DECLARE_VIRTUAL_TRACE();

private:
    ServiceWorkerMessageEvent();
    ServiceWorkerMessageEvent(const AtomicString& type, const ServiceWorkerMessageEventInit& initializer);
    ServiceWorkerMessageEvent(PassRefPtr<SerializedScriptValue> data, const String& origin, const String& lastEventId, ServiceWorker* source, MessagePortArray* ports);

    RefPtr<SerializedScriptValue> m_serializedData;
    String m_origin;
    String m_lastEventId;
    Member<ServiceWorker> m_sourceAsServiceWorker;
    Member<MessagePort> m_sourceAsMessagePort;
    Member<MessagePortArray> m_ports;
};

} // namespace blink

#endif // ServiceWorkerMessageEvent_h
