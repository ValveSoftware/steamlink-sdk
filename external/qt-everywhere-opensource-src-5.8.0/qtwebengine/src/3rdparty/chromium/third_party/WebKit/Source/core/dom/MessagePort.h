/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#ifndef MessagePort_h
#define MessagePort_h

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "core/CoreExport.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/events/EventListener.h"
#include "core/events/EventTarget.h"
#include "public/platform/WebMessagePortChannel.h"
#include "public/platform/WebMessagePortChannelClient.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include <memory>

namespace blink {

class ExceptionState;
class ExecutionContext;
class MessagePort;
class ScriptState;
class SerializedScriptValue;

// Not to be confused with WebMessagePortChannelArray; this one uses Vector and std::unique_ptr instead of WebVector and raw pointers.
typedef Vector<WebMessagePortChannelUniquePtr, 1> MessagePortChannelArray;

class CORE_EXPORT MessagePort
    : public EventTargetWithInlineData
    , public ActiveScriptWrappable
    , public ActiveDOMObject
    , public WebMessagePortChannelClient {
    DEFINE_WRAPPERTYPEINFO();
    USING_GARBAGE_COLLECTED_MIXIN(MessagePort);
public:
    static MessagePort* create(ExecutionContext&);
    ~MessagePort() override;

    void postMessage(ExecutionContext*, PassRefPtr<SerializedScriptValue> message, const MessagePortArray&, ExceptionState&);

    void start();
    void close();

    void entangle(WebMessagePortChannelUniquePtr);
    WebMessagePortChannelUniquePtr disentangle();

    // Returns nullptr if the passed-in array is nullptr/empty.
    static std::unique_ptr<WebMessagePortChannelArray> toWebMessagePortChannelArray(std::unique_ptr<MessagePortChannelArray>);

    // Returns an empty array if the passed array is empty.
    static MessagePortArray* toMessagePortArray(ExecutionContext*, const WebMessagePortChannelArray&);

    // Returns nullptr if there is an exception, or if the passed-in array is nullptr/empty.
    static std::unique_ptr<MessagePortChannelArray> disentanglePorts(ExecutionContext*, const MessagePortArray&, ExceptionState&);

    // Returns an empty array if the passed array is nullptr/empty.
    static MessagePortArray* entanglePorts(ExecutionContext&, std::unique_ptr<MessagePortChannelArray>);

    bool started() const { return m_started; }

    const AtomicString& interfaceName() const override;
    ExecutionContext* getExecutionContext() const override { return ActiveDOMObject::getExecutionContext(); }
    MessagePort* toMessagePort() override { return this; }

    // ActiveScriptWrappable implementation.
    bool hasPendingActivity() const final;

    // ActiveDOMObject implementation.
    void stop() override { close(); }

    void setOnmessage(EventListener* listener)
    {
        setAttributeEventListener(EventTypeNames::message, listener);
        start();
    }
    EventListener* onmessage() { return getAttributeEventListener(EventTypeNames::message); }

    // A port starts out its life entangled, and remains entangled until it is closed or is cloned.
    bool isEntangled() const { return !m_closed && !isNeutered(); }

    // A port gets neutered when it is transferred to a new owner via postMessage().
    bool isNeutered() const { return !m_entangledChannel; }

    DECLARE_VIRTUAL_TRACE();

protected:
    explicit MessagePort(ExecutionContext&);
    bool tryGetMessage(RefPtr<SerializedScriptValue>& message, std::unique_ptr<MessagePortChannelArray>& channels);

private:
    // WebMessagePortChannelClient implementation.
    void messageAvailable() override;
    void dispatchMessages();

    WebMessagePortChannelUniquePtr m_entangledChannel;

    bool m_started;
    bool m_closed;

    RefPtr<ScriptState> m_scriptStateForConversion;
};

} // namespace blink

#endif // MessagePort_h
