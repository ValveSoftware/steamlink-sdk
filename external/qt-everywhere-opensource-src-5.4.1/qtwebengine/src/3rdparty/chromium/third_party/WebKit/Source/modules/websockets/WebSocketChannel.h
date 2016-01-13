/*
 * Copyright (C) 2009 Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebSocketChannel_h
#define WebSocketChannel_h

#include "core/frame/ConsoleTypes.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class BlobDataHandle;
class KURL;
class ExecutionContext;
class WebSocketChannelClient;

// FIXME: WebSocketChannel needs to be RefCountedGarbageCollected to support manual ref/deref
// in MainThreadWebSocketChannelImpl. We should change it to GarbageCollectedFinalized once
// we remove MainThreadWebSocketChannelImpl.
class WebSocketChannel : public RefCountedWillBeRefCountedGarbageCollected<WebSocketChannel> {
    WTF_MAKE_NONCOPYABLE(WebSocketChannel);
public:
    WebSocketChannel() { }
    static PassRefPtrWillBeRawPtr<WebSocketChannel> create(ExecutionContext*, WebSocketChannelClient*);

    enum SendResult {
        SendSuccess,
        SendFail,
        InvalidMessage
    };

    enum CloseEventCode {
        CloseEventCodeNotSpecified = -1,
        CloseEventCodeNormalClosure = 1000,
        CloseEventCodeGoingAway = 1001,
        CloseEventCodeProtocolError = 1002,
        CloseEventCodeUnsupportedData = 1003,
        CloseEventCodeFrameTooLarge = 1004,
        CloseEventCodeNoStatusRcvd = 1005,
        CloseEventCodeAbnormalClosure = 1006,
        CloseEventCodeInvalidFramePayloadData = 1007,
        CloseEventCodePolicyViolation = 1008,
        CloseEventCodeMessageTooBig = 1009,
        CloseEventCodeMandatoryExt = 1010,
        CloseEventCodeInternalError = 1011,
        CloseEventCodeTLSHandshake = 1015,
        CloseEventCodeMinimumUserDefined = 3000,
        CloseEventCodeMaximumUserDefined = 4999
    };

    virtual bool connect(const KURL&, const String& protocol) = 0;
    virtual SendResult send(const String& message) = 0;
    virtual SendResult send(const ArrayBuffer&, unsigned byteOffset, unsigned byteLength) = 0;
    virtual SendResult send(PassRefPtr<BlobDataHandle>) = 0;

    // For WorkerThreadableWebSocketChannel.
    virtual SendResult send(PassOwnPtr<Vector<char> >) = 0;

    virtual void close(int code, const String& reason) = 0;

    // Log the reason text and close the connection. Will call didClose().
    // The MessageLevel parameter will be used for the level of the message
    // shown at the devtool console.
    // sourceURL and lineNumber parameters may be shown with the reason text
    // at the devtool console.
    // Even if sourceURL and lineNumber are specified, they may be ignored
    // and the "current" url and the line number in the sense of
    // JavaScript execution may be shown if this method is called in
    // a JS execution context.
    // You can specify String() and 0 for sourceURL and lineNumber
    // respectively, if you can't / needn't provide the information.
    virtual void fail(const String& reason, MessageLevel, const String& sourceURL, unsigned lineNumber) = 0;

    virtual void disconnect() = 0; // Will suppress didClose().

    virtual void suspend() = 0;
    virtual void resume() = 0;

    virtual ~WebSocketChannel() { }

    virtual void trace(Visitor*) { }
};

} // namespace WebCore

#endif // WebSocketChannel_h
