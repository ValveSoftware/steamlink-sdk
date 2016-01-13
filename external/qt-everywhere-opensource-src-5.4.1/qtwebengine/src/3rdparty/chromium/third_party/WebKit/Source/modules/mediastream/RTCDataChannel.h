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

#include "bindings/v8/ScriptWrappable.h"
#include "modules/EventTargetModules.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebRTCDataChannelHandler.h"
#include "public/platform/WebRTCDataChannelHandlerClient.h"
#include "wtf/RefCounted.h"

namespace blink {
class WebRTCDataChannelHandler;
class WebRTCPeerConnectionHandler;
struct WebRTCDataChannelInit;
}

namespace WebCore {

class Blob;
class ExceptionState;
class RTCPeerConnection;

class RTCDataChannel FINAL : public RefCountedWillBeRefCountedGarbageCollected<RTCDataChannel>, public ScriptWrappable, public EventTargetWithInlineData, public blink::WebRTCDataChannelHandlerClient {
    REFCOUNTED_EVENT_TARGET(RTCDataChannel);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(RTCDataChannel);
public:
    static PassRefPtrWillBeRawPtr<RTCDataChannel> create(ExecutionContext*, RTCPeerConnection*, PassOwnPtr<blink::WebRTCDataChannelHandler>);
    static PassRefPtrWillBeRawPtr<RTCDataChannel> create(ExecutionContext*, RTCPeerConnection*, blink::WebRTCPeerConnectionHandler*, const String& label, const blink::WebRTCDataChannelInit&, ExceptionState&);
    virtual ~RTCDataChannel();

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
    unsigned long bufferedAmount() const;

    String binaryType() const;
    void setBinaryType(const String&, ExceptionState&);

    void send(const String&, ExceptionState&);
    void send(PassRefPtr<ArrayBuffer>, ExceptionState&);
    void send(PassRefPtr<ArrayBufferView>, ExceptionState&);
    void send(PassRefPtrWillBeRawPtr<Blob>, ExceptionState&);

    void close();

    DEFINE_ATTRIBUTE_EVENT_LISTENER(open);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(error);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(close);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(message);

    void stop();

    // EventTarget
    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ExecutionContext* executionContext() const OVERRIDE;

#if ENABLE(OILPAN)
    void clearWeakMembers(Visitor*);
#endif

    virtual void trace(Visitor*) OVERRIDE;

private:
    RTCDataChannel(ExecutionContext*, RTCPeerConnection*, PassOwnPtr<blink::WebRTCDataChannelHandler>);

    void scheduleDispatchEvent(PassRefPtrWillBeRawPtr<Event>);
    void scheduledEventTimerFired(Timer<RTCDataChannel>*);

    ExecutionContext* m_executionContext;

    // blink::WebRTCDataChannelHandlerClient
    virtual void didChangeReadyState(blink::WebRTCDataChannelHandlerClient::ReadyState) OVERRIDE;
    virtual void didReceiveStringData(const blink::WebString&) OVERRIDE;
    virtual void didReceiveRawData(const char*, size_t) OVERRIDE;
    virtual void didDetectError() OVERRIDE;

    OwnPtr<blink::WebRTCDataChannelHandler> m_handler;

    bool m_stopped;

    blink::WebRTCDataChannelHandlerClient::ReadyState m_readyState;

    enum BinaryType {
        BinaryTypeBlob,
        BinaryTypeArrayBuffer
    };
    BinaryType m_binaryType;

    Timer<RTCDataChannel> m_scheduledEventTimer;
    WillBeHeapVector<RefPtrWillBeMember<Event> > m_scheduledEvents;

#if ENABLE(OILPAN)
    WeakMember<RTCPeerConnection> m_connection;
#endif
};

} // namespace WebCore

#endif // RTCDataChannel_h
