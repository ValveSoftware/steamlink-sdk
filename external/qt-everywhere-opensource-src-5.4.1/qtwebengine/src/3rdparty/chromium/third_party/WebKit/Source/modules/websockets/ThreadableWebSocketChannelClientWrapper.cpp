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

#include "config.h"
#include "modules/websockets/ThreadableWebSocketChannelClientWrapper.h"

namespace WebCore {

ThreadableWebSocketChannelClientWrapper::ThreadableWebSocketChannelClientWrapper(WebSocketChannelClient* client)
    : m_client(client)
{
}

PassRefPtrWillBeRawPtr<ThreadableWebSocketChannelClientWrapper> ThreadableWebSocketChannelClientWrapper::create(WebSocketChannelClient* client)
{
    return adoptRefWillBeNoop(new ThreadableWebSocketChannelClientWrapper(client));
}

void ThreadableWebSocketChannelClientWrapper::clearClient()
{
    m_client = 0;
}

void ThreadableWebSocketChannelClientWrapper::didConnect(const String& subprotocol, const String& extensions)
{
    if (m_client)
        m_client->didConnect(subprotocol, extensions);
}

void ThreadableWebSocketChannelClientWrapper::didReceiveMessage(const String& message)
{
    if (m_client)
        m_client->didReceiveMessage(message);
}

void ThreadableWebSocketChannelClientWrapper::didReceiveBinaryData(PassOwnPtr<Vector<char> > binaryData)
{
    if (m_client)
        m_client->didReceiveBinaryData(binaryData);
}

void ThreadableWebSocketChannelClientWrapper::didConsumeBufferedAmount(unsigned long consumed)
{
    if (m_client)
        m_client->didConsumeBufferedAmount(consumed);
}

void ThreadableWebSocketChannelClientWrapper::didStartClosingHandshake()
{
    if (m_client)
        m_client->didStartClosingHandshake();
}

void ThreadableWebSocketChannelClientWrapper::didClose(WebSocketChannelClient::ClosingHandshakeCompletionStatus closingHandshakeCompletion, unsigned short code, const String& reason)
{
    if (m_client)
        m_client->didClose(closingHandshakeCompletion, code, reason);
}

void ThreadableWebSocketChannelClientWrapper::didReceiveMessageError()
{
    if (m_client)
        m_client->didReceiveMessageError();
}

} // namespace WebCore
