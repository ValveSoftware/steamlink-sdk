// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ExtendableMessageEvent_h
#define ExtendableMessageEvent_h

#include "modules/EventModules.h"
#include "modules/ModulesExport.h"
#include "modules/serviceworkers/ExtendableEvent.h"
#include "modules/serviceworkers/ExtendableMessageEventInit.h"

namespace blink {

class MODULES_EXPORT ExtendableMessageEvent final : public ExtendableEvent {
    DEFINE_WRAPPERTYPEINFO();

public:
    static ExtendableMessageEvent* create();
    static ExtendableMessageEvent* create(const AtomicString& type, const ExtendableMessageEventInit& initializer);
    static ExtendableMessageEvent* create(const AtomicString& type, const ExtendableMessageEventInit& initializer, WaitUntilObserver*);
    static ExtendableMessageEvent* create(PassRefPtr<SerializedScriptValue> data, const String& origin, MessagePortArray* ports, WaitUntilObserver*);
    static ExtendableMessageEvent* create(PassRefPtr<SerializedScriptValue> data, const String& origin, MessagePortArray* ports, ServiceWorkerClient* source, WaitUntilObserver*);
    static ExtendableMessageEvent* create(PassRefPtr<SerializedScriptValue> data, const String& origin, MessagePortArray* ports, ServiceWorker* source, WaitUntilObserver*);

    SerializedScriptValue* serializedData() const { return m_serializedData.get(); }
    void setSerializedData(PassRefPtr<SerializedScriptValue> serializedData) { m_serializedData = serializedData; }
    const String& origin() const { return m_origin; }
    const String& lastEventId() const { return m_lastEventId; }
    MessagePortArray ports(bool& isNull) const;
    MessagePortArray ports() const;
    void source(ClientOrServiceWorkerOrMessagePort& result) const;

    const AtomicString& interfaceName() const override;

    DECLARE_VIRTUAL_TRACE();

private:
    ExtendableMessageEvent();
    ExtendableMessageEvent(const AtomicString& type, const ExtendableMessageEventInit& initializer);
    ExtendableMessageEvent(const AtomicString& type, const ExtendableMessageEventInit& initializer, WaitUntilObserver*);
    ExtendableMessageEvent(PassRefPtr<SerializedScriptValue> data, const String& origin, MessagePortArray* ports, WaitUntilObserver*);

    RefPtr<SerializedScriptValue> m_serializedData;
    String m_origin;
    String m_lastEventId;
    Member<ServiceWorkerClient> m_sourceAsClient;
    Member<ServiceWorker> m_sourceAsServiceWorker;
    Member<MessagePort> m_sourceAsMessagePort;
    Member<MessagePortArray> m_ports;
};

} // namespace blink

#endif // ExtendableMessageEvent_h
