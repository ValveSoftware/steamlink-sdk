/*
 * Copyright (C) 2007 Henry Mason (hmason@mac.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef MessageEvent_h
#define MessageEvent_h

#include "bindings/core/v8/SerializedScriptValue.h"
#include "core/CoreExport.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/MessagePort.h"
#include "core/events/Event.h"
#include "core/events/EventTarget.h"
#include "core/events/MessageEventInit.h"
#include "core/fileapi/Blob.h"
#include "core/frame/DOMWindow.h"
#include <memory>

namespace blink {

class CORE_EXPORT MessageEvent final : public Event {
    DEFINE_WRAPPERTYPEINFO();
public:
    static MessageEvent* create()
    {
        return new MessageEvent;
    }
    static MessageEvent* create(MessagePortArray* ports, const String& origin = String(), const String& lastEventId = String(), EventTarget* source = nullptr, const String& suborigin = String())
    {
        return new MessageEvent(origin, lastEventId, source, ports, suborigin);
    }
    static MessageEvent* create(MessagePortArray* ports, PassRefPtr<SerializedScriptValue> data, const String& origin = String(), const String& lastEventId = String(), EventTarget* source = nullptr, const String& suborigin = String())
    {
        return new MessageEvent(data, origin, lastEventId, source, ports, suborigin);
    }
    static MessageEvent* create(std::unique_ptr<MessagePortChannelArray> channels, PassRefPtr<SerializedScriptValue> data, const String& origin = String(), const String& lastEventId = String(), EventTarget* source = nullptr, const String& suborigin = String())
    {
        return new MessageEvent(data, origin, lastEventId, source, std::move(channels), suborigin);
    }
    static MessageEvent* create(const String& data, const String& origin = String(), const String& suborigin = String())
    {
        return new MessageEvent(data, origin, suborigin);
    }
    static MessageEvent* create(Blob* data, const String& origin = String(), const String& suborigin = String())
    {
        return new MessageEvent(data, origin, suborigin);
    }
    static MessageEvent* create(DOMArrayBuffer* data, const String& origin = String(), const String& suborigin = String())
    {
        return new MessageEvent(data, origin, suborigin);
    }
    static MessageEvent* create(const AtomicString& type, const MessageEventInit& initializer, ExceptionState&);
    ~MessageEvent() override;

    void initMessageEvent(const AtomicString& type, bool canBubble, bool cancelable, ScriptValue data, const String& origin, const String& lastEventId, DOMWindow* source, MessagePortArray*);
    void initMessageEvent(const AtomicString& type, bool canBubble, bool cancelable, PassRefPtr<SerializedScriptValue> data, const String& origin, const String& lastEventId, DOMWindow* source, MessagePortArray*);

    const String& origin() const { return m_origin; }
    const String& suborigin() const { return m_suborigin; }
    const String& lastEventId() const { return m_lastEventId; }
    EventTarget* source() const { return m_source.get(); }
    MessagePortArray ports(bool& isNull) const;
    MessagePortArray ports() const;
    MessagePortChannelArray* channels() const { return m_channels ? m_channels.get() : nullptr; }

    const AtomicString& interfaceName() const override;

    enum DataType {
        DataTypeScriptValue,
        DataTypeSerializedScriptValue,
        DataTypeString,
        DataTypeBlob,
        DataTypeArrayBuffer
    };
    DataType getDataType() const { return m_dataType; }
    ScriptValue dataAsScriptValue() const { ASSERT(m_dataType == DataTypeScriptValue); return m_dataAsScriptValue; }
    SerializedScriptValue* dataAsSerializedScriptValue() const { ASSERT(m_dataType == DataTypeSerializedScriptValue); return m_dataAsSerializedScriptValue.get(); }
    String dataAsString() const { ASSERT(m_dataType == DataTypeString); return m_dataAsString; }
    Blob* dataAsBlob() const { ASSERT(m_dataType == DataTypeBlob); return m_dataAsBlob.get(); }
    DOMArrayBuffer* dataAsArrayBuffer() const { ASSERT(m_dataType == DataTypeArrayBuffer); return m_dataAsArrayBuffer.get(); }

    void setSerializedData(PassRefPtr<SerializedScriptValue> data)
    {
        ASSERT(!m_dataAsSerializedScriptValue);
        m_dataAsSerializedScriptValue = data;
    }

    void entangleMessagePorts(ExecutionContext*);

    DECLARE_VIRTUAL_TRACE();

    v8::Local<v8::Object> associateWithWrapper(v8::Isolate*, const WrapperTypeInfo*, v8::Local<v8::Object> wrapper) override WARN_UNUSED_RETURN;

private:
    MessageEvent();
    MessageEvent(const AtomicString&, const MessageEventInit&);
    MessageEvent(const String& origin, const String& lastEventId, EventTarget* source, MessagePortArray*, const String& suborigin);
    MessageEvent(PassRefPtr<SerializedScriptValue> data, const String& origin, const String& lastEventId, EventTarget* source, MessagePortArray*, const String& suborigin);
    MessageEvent(PassRefPtr<SerializedScriptValue> data, const String& origin, const String& lastEventId, EventTarget* source, std::unique_ptr<MessagePortChannelArray>, const String& suborigin);

    MessageEvent(const String& data, const String& origin, const String& suborigin);
    MessageEvent(Blob* data, const String& origin, const String& suborigin);
    MessageEvent(DOMArrayBuffer* data, const String& origin, const String& suborigin);

    DataType m_dataType;
    ScriptValue m_dataAsScriptValue;
    RefPtr<SerializedScriptValue> m_dataAsSerializedScriptValue;
    String m_dataAsString;
    Member<Blob> m_dataAsBlob;
    Member<DOMArrayBuffer> m_dataAsArrayBuffer;
    String m_origin;
    String m_lastEventId;
    Member<EventTarget> m_source;
    // m_ports are the MessagePorts in an engtangled state, and m_channels are
    // the MessageChannels in a disentangled state. Only one of them can be
    // non-empty at a time. entangleMessagePorts() moves between the states.
    Member<MessagePortArray> m_ports;
    std::unique_ptr<MessagePortChannelArray> m_channels;
    String m_suborigin;
};

} // namespace blink

#endif // MessageEvent_h
