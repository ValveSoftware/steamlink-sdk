/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RTCDataChannel_h
#define RTCDataChannel_h

#include "base/gtest_prod_util.h"
#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "modules/EventTargetModules.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebRTCDataChannelHandler.h"
#include "public/platform/WebRTCDataChannelHandlerClient.h"
#include "wtf/Compiler.h"
#include <memory>

namespace blink {

class Blob;
class DOMArrayBuffer;
class DOMArrayBufferView;
class ExceptionState;
class RTCPeerConnection;
class WebRTCDataChannelHandler;
class WebRTCPeerConnectionHandler;
struct WebRTCDataChannelInit;

class MODULES_EXPORT RTCDataChannel final
    : public EventTargetWithInlineData
    , WTF_NON_EXPORTED_BASE(public WebRTCDataChannelHandlerClient)
    , public ActiveScriptWrappable
    , public ActiveDOMObject {
    USING_GARBAGE_COLLECTED_MIXIN(RTCDataChannel);
    DEFINE_WRAPPERTYPEINFO();
    USING_PRE_FINALIZER(RTCDataChannel, dispose);
public:
    static RTCDataChannel* create(ExecutionContext*, std::unique_ptr<WebRTCDataChannelHandler>);
    static RTCDataChannel* create(ExecutionContext*, WebRTCPeerConnectionHandler*, const String& label, const WebRTCDataChannelInit&, ExceptionState&);
    ~RTCDataChannel() override;

    ReadyState getHandlerState() const;

    String label() const;

    // DEPRECATED
    bool reliable() const;

    bool ordered() const;
    unsigned short maxRetransmitTime() const;
    unsigned short maxRetransmits() const;
    String protocol() const;
    bool negotiated() const;
    unsigned short id() const;
    String readyState() const;
    unsigned bufferedAmount() const;

    unsigned bufferedAmountLowThreshold() const;
    void setBufferedAmountLowThreshold(unsigned);

    String binaryType() const;
    void setBinaryType(const String&, ExceptionState&);

    void send(const String&, ExceptionState&);
    void send(DOMArrayBuffer*, ExceptionState&);
    void send(DOMArrayBufferView*, ExceptionState&);
    void send(Blob*, ExceptionState&);

    void close();

    DEFINE_ATTRIBUTE_EVENT_LISTENER(open);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(bufferedamountlow);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(error);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(close);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(message);

    // EventTarget
    const AtomicString& interfaceName() const override;
    ExecutionContext* getExecutionContext() const override;

    // ActiveDOMObject
    void suspend() override;
    void resume() override;
    void stop() override;

    // ActiveScriptWrappable
    bool hasPendingActivity() const override;

    DECLARE_VIRTUAL_TRACE();

    // WebRTCDataChannelHandlerClient
    void didChangeReadyState(WebRTCDataChannelHandlerClient::ReadyState) override;
    void didDecreaseBufferedAmount(unsigned) override;
    void didReceiveStringData(const WebString&) override;
    void didReceiveRawData(const char*, size_t) override;
    void didDetectError() override;

private:
    RTCDataChannel(ExecutionContext*, std::unique_ptr<WebRTCDataChannelHandler>);
    void dispose();

    void scheduleDispatchEvent(Event*);
    void scheduledEventTimerFired(Timer<RTCDataChannel>*);

    std::unique_ptr<WebRTCDataChannelHandler> m_handler;

    WebRTCDataChannelHandlerClient::ReadyState m_readyState;

    enum BinaryType {
        BinaryTypeBlob,
        BinaryTypeArrayBuffer
    };
    BinaryType m_binaryType;

    Timer<RTCDataChannel> m_scheduledEventTimer;
    HeapVector<Member<Event>> m_scheduledEvents;

    unsigned m_bufferedAmountLowThreshold;

    bool m_stopped;

    FRIEND_TEST_ALL_PREFIXES(RTCDataChannelTest, BufferedAmountLow);
};

} // namespace blink

#endif // RTCDataChannel_h
