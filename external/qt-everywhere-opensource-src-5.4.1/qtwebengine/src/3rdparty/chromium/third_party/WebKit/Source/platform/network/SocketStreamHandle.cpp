/*
 * Copyright (C) 2009, 2011, 2012 Google Inc. All rights reserved.
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
#include "platform/network/SocketStreamHandle.h"

#include "platform/Logging.h"
#include "platform/network/SocketStreamError.h"
#include "platform/network/SocketStreamHandleClient.h"
#include "platform/network/SocketStreamHandleInternal.h"
#include "public/platform/Platform.h"
#include "public/platform/WebData.h"
#include "public/platform/WebSocketStreamError.h"
#include "public/platform/WebSocketStreamHandle.h"
#include "wtf/PassOwnPtr.h"

namespace WebCore {

static const unsigned bufferSize = 100 * 1024 * 1024;

SocketStreamHandleInternal::SocketStreamHandleInternal(SocketStreamHandle* handle)
    : m_handle(handle)
    , m_socket(adoptPtr(blink::Platform::current()->createSocketStreamHandle()))
    , m_maxPendingSendAllowed(0)
    , m_pendingAmountSent(0)
{
}

SocketStreamHandleInternal::~SocketStreamHandleInternal()
{
    m_handle = 0;
}

void SocketStreamHandleInternal::connect(const KURL& url)
{
    WTF_LOG(Network, "SocketStreamHandleInternal %p connect()", this);

    ASSERT(m_socket);
    m_socket->connect(url, this);
}

int SocketStreamHandleInternal::send(const char* data, int len)
{
    WTF_LOG(Network, "SocketStreamHandleInternal %p send() len=%d", this, len);
    // FIXME: |m_socket| should not be null here, but it seems that there is the
    // case. We should figure out such a path and fix it rather than checking
    // null here.
    if (!m_socket) {
        WTF_LOG(Network, "SocketStreamHandleInternal %p send() m_socket is NULL", this);
        return 0;
    }
    if (m_pendingAmountSent + len > m_maxPendingSendAllowed)
        len = m_maxPendingSendAllowed - m_pendingAmountSent;

    if (len <= 0)
        return len;
    blink::WebData webdata(data, len);
    if (m_socket->send(webdata)) {
        m_pendingAmountSent += len;
        WTF_LOG(Network, "SocketStreamHandleInternal %p send() Sent %d bytes", this, len);
        return len;
    }
    WTF_LOG(Network, "SocketStreamHandleInternal %p send() m_socket->send() failed", this);
    return 0;
}

void SocketStreamHandleInternal::close()
{
    WTF_LOG(Network, "SocketStreamHandleInternal %p close()", this);
    if (m_socket)
        m_socket->close();
}

void SocketStreamHandleInternal::didOpenStream(blink::WebSocketStreamHandle* socketHandle, int maxPendingSendAllowed)
{
    WTF_LOG(Network, "SocketStreamHandleInternal %p didOpenStream() maxPendingSendAllowed=%d", this, maxPendingSendAllowed);
    ASSERT(maxPendingSendAllowed > 0);
    if (m_handle && m_socket) {
        ASSERT(socketHandle == m_socket.get());
        m_maxPendingSendAllowed = maxPendingSendAllowed;
        m_handle->m_state = SocketStreamHandle::Open;
        if (m_handle->m_client) {
            m_handle->m_client->didOpenSocketStream(m_handle);
            return;
        }
    }
    WTF_LOG(Network, "SocketStreamHandleInternal %p didOpenStream() m_handle or m_socket is NULL", this);
}

void SocketStreamHandleInternal::didSendData(blink::WebSocketStreamHandle* socketHandle, int amountSent)
{
    WTF_LOG(Network, "SocketStreamHandleInternal %p didSendData() amountSent=%d", this, amountSent);
    ASSERT(amountSent > 0);
    if (m_handle && m_socket) {
        ASSERT(socketHandle == m_socket.get());
        m_pendingAmountSent -= amountSent;
        ASSERT(m_pendingAmountSent >= 0);
        m_handle->sendPendingData();
    }
}

void SocketStreamHandleInternal::didReceiveData(blink::WebSocketStreamHandle* socketHandle, const blink::WebData& data)
{
    WTF_LOG(Network, "SocketStreamHandleInternal %p didReceiveData() Received %lu bytes", this, static_cast<unsigned long>(data.size()));
    if (m_handle && m_socket) {
        ASSERT(socketHandle == m_socket.get());
        if (m_handle->m_client)
            m_handle->m_client->didReceiveSocketStreamData(m_handle, data.data(), data.size());
    }
}

void SocketStreamHandleInternal::didClose(blink::WebSocketStreamHandle* socketHandle)
{
    WTF_LOG(Network, "SocketStreamHandleInternal %p didClose()", this);
    if (m_handle && m_socket) {
        ASSERT(socketHandle == m_socket.get());
        m_socket.clear();
        SocketStreamHandle* h = m_handle;
        m_handle = 0;
        if (h->m_client)
            h->m_client->didCloseSocketStream(h);
    }
}

void SocketStreamHandleInternal::didFail(blink::WebSocketStreamHandle* socketHandle, const blink::WebSocketStreamError& err)
{
    WTF_LOG(Network, "SocketStreamHandleInternal %p didFail()", this);
    if (m_handle && m_socket) {
        ASSERT(socketHandle == m_socket.get());
        if (m_handle->m_client)
            m_handle->m_client->didFailSocketStream(m_handle, *(PassRefPtr<SocketStreamError>(err)));
    }
}

// SocketStreamHandle ----------------------------------------------------------

SocketStreamHandle::SocketStreamHandle(SocketStreamHandleClient* client)
    : m_client(client)
    , m_state(Connecting)
{
    m_internal = SocketStreamHandleInternal::create(this);
}

void SocketStreamHandle::connect(const KURL& url)
{
    m_internal->connect(url);
}

SocketStreamHandle::~SocketStreamHandle()
{
    setClient(0);
    m_internal.clear();
}

SocketStreamHandle::SocketStreamState SocketStreamHandle::state() const
{
    return m_state;
}

bool SocketStreamHandle::send(const char* data, int length)
{
    if (m_state == Connecting || m_state == Closing)
        return false;
    if (!m_buffer.isEmpty()) {
        if (m_buffer.size() + length > bufferSize) {
            // FIXME: report error to indicate that buffer has no more space.
            return false;
        }
        m_buffer.append(data, length);
        return true;
    }
    int bytesWritten = 0;
    if (m_state == Open)
        bytesWritten = sendInternal(data, length);
    if (bytesWritten < 0)
        return false;
    if (m_client)
        m_client->didConsumeBufferedAmount(this, bytesWritten);
    if (m_buffer.size() + length - bytesWritten > bufferSize) {
        // FIXME: report error to indicate that buffer has no more space.
        return false;
    }
    if (bytesWritten < length) {
        m_buffer.append(data + bytesWritten, length - bytesWritten);
    }
    return true;
}

void SocketStreamHandle::close()
{
    if (m_state == Closed)
        return;
    m_state = Closing;
    if (!m_buffer.isEmpty())
        return;
    disconnect();
}

void SocketStreamHandle::disconnect()
{
    RefPtr<SocketStreamHandle> protect(this); // closeInternal calls the client, which may make the handle get deallocated immediately.

    closeInternal();
    m_state = Closed;
}

void SocketStreamHandle::setClient(SocketStreamHandleClient* client)
{
    ASSERT(!client || (!m_client && m_state == Connecting));
    m_client = client;
}

bool SocketStreamHandle::sendPendingData()
{
    if (m_state != Open && m_state != Closing)
        return false;
    if (m_buffer.isEmpty()) {
        if (m_state == Open)
            return false;
        if (m_state == Closing) {
            disconnect();
            return false;
        }
    }
    bool pending;
    do {
        int bytesWritten = sendInternal(m_buffer.firstBlockData(), m_buffer.firstBlockSize());
        pending = bytesWritten != static_cast<int>(m_buffer.firstBlockSize());
        if (bytesWritten <= 0)
            return false;
        ASSERT(m_buffer.size() - bytesWritten <= bufferSize);
        m_buffer.consume(bytesWritten);
        // FIXME: place didConsumeBufferedAmount out of do-while.
        if (m_client)
            m_client->didConsumeBufferedAmount(this, bytesWritten);
    } while (!pending && !m_buffer.isEmpty());
    return true;
}

int SocketStreamHandle::sendInternal(const char* buf, int len)
{
    if (!m_internal)
        return 0;
    return m_internal->send(buf, len);
}

void SocketStreamHandle::closeInternal()
{
    if (m_internal)
        m_internal->close();
}

} // namespace WebCore
