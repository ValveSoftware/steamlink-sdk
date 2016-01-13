/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef WebSocketPerMessageDeflate_h
#define WebSocketPerMessageDeflate_h

#include "modules/websockets/WebSocketDeflater.h"
#include "modules/websockets/WebSocketExtensionProcessor.h"
#include "modules/websockets/WebSocketFrame.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"

namespace WebCore {

// WebSocketPerMessageDeflate is a deflater / inflater for a WebSocket message with DEFLATE algorithm.
// See http://tools.ietf.org/html/draft-ietf-hybi-permessage-compression-08
class WebSocketPerMessageDeflate {
public:
    WebSocketPerMessageDeflate();

    PassOwnPtr<WebSocketExtensionProcessor> createExtensionProcessor();

    void enable(int windowBits, WebSocketDeflater::ContextTakeOverMode);
    bool enabled() const { return m_enabled; }

    // Deflate the given frame.
    // Return true if the method succeeds.
    // The method succeeds and does nothing if this object is not enabled.
    // This method may rewrite the parameter.
    // If the compression is done, the payload member of the frame will point the internal buffer
    // and the pointer will be valid until resetDeflateBuffer is called.
    bool deflate(WebSocketFrame&);

    // Reset the internal buffer used by deflate.
    // You must call this method between consecutive calls of the deflate method.
    void resetDeflateBuffer();

    // Inflate the given frame.
    // Return true if the method succeeds.
    // The method succeeds and does nothing if this object is not enabled.
    // This method may rewrite the parameter.
    // If the decompression is done, the payload member of the frame will point the internal buffer
    // and the pointer will be valid until resetInflateBuffer is called.
    bool inflate(WebSocketFrame&);

    // Reset the internal buffer used by inflate.
    // You must call this method between consecutive calls of the inflate method.
    void resetInflateBuffer();

    // Return the reason of the last failure in inflate or deflate.
    String failureReason() { return m_failureReason; }

    void didFail();

private:
    bool m_enabled;
    bool m_deflateOngoing;
    bool m_inflateOngoing;
    String m_failureReason;
    OwnPtr<WebSocketDeflater> m_deflater;
    OwnPtr<WebSocketInflater> m_inflater;
};

}

#endif // WebSocketPerMessageDeflate_h
