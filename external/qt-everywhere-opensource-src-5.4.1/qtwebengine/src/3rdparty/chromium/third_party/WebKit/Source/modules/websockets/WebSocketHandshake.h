/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
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

#ifndef WebSocketHandshake_h
#define WebSocketHandshake_h

#include "modules/websockets/WebSocketExtensionDispatcher.h"
#include "modules/websockets/WebSocketExtensionProcessor.h"
#include "platform/network/WebSocketHandshakeRequest.h"
#include "platform/network/WebSocketHandshakeResponse.h"
#include "platform/weborigin/KURL.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class Document;

class WebSocketHandshake {
    WTF_MAKE_NONCOPYABLE(WebSocketHandshake); WTF_MAKE_FAST_ALLOCATED;
public:
    // This enum is reused for histogram. When this needs to be modified, add a
    // new enum for histogram and convert mode values into values in the new
    // enum to keep new data consistent with old one.
    enum Mode {
        Incomplete, Normal, Failed, Connected, ModeMax
    };
    WebSocketHandshake(const KURL&, const String& protocol, Document*);
    ~WebSocketHandshake();

    const KURL& url() const;
    void setURL(const KURL&);
    const String host() const;

    const String& clientProtocol() const;
    void setClientProtocol(const String&);

    bool secure() const;

    String clientOrigin() const;
    String clientLocation() const;

    // Builds a WebSocket opening handshake string to send to the server.
    // Cookie headers will be added later by the platform code for security
    // reason.
    CString clientHandshakeMessage() const;
    // Builds an object representing WebSocket opening handshake to pass to the
    // inspector.
    PassRefPtr<WebSocketHandshakeRequest> clientHandshakeRequest() const;

    // We're collecting data for histogram in the destructor. Note that calling
    // this method affects that.
    void reset();
    void clearDocument();

    int readServerHandshake(const char* header, size_t len);
    Mode mode() const;
    // Returns a string indicating the reason of failure if mode() == Failed.
    String failureReason() const;

    const AtomicString& serverWebSocketProtocol() const;
    const AtomicString& serverUpgrade() const;
    const AtomicString& serverConnection() const;
    const AtomicString& serverWebSocketAccept() const;
    String acceptedExtensions() const;

    const WebSocketHandshakeResponse& serverHandshakeResponse() const;

    void addExtensionProcessor(PassOwnPtr<WebSocketExtensionProcessor>);

    static String getExpectedWebSocketAccept(const String& secWebSocketKey);

private:
    KURL httpURLForAuthenticationAndCookies() const;

    int readStatusLine(const char* header, size_t headerLength, int& statusCode, String& statusText);

    // Reads all headers except for the two predefined ones.
    const char* readHTTPHeaders(const char* start, const char* end);
    void processHeaders();
    bool checkResponseHeaders();

    KURL m_url;
    String m_clientProtocol;
    bool m_secure;
    Document* m_document;

    Mode m_mode;

    // Stores a received server's handshake. The order of headers is not
    // guaranteed to be preserved. Duplicated headers may be dropped. Values may
    // be rebuilt after parsing, so they can be different from the original data
    // received from the server.
    WebSocketHandshakeResponse m_response;

    String m_failureReason;

    String m_secWebSocketKey;
    String m_expectedAccept;

    WebSocketExtensionDispatcher m_extensionDispatcher;
};

} // namespace WebCore

#endif // WebSocketHandshake_h
