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

#include "config.h"

#include "modules/websockets/WebSocketPerMessageDeflate.h"

#include "modules/websockets/WebSocketExtensionParser.h"
#include "public/platform/Platform.h"
#include "wtf/HashMap.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class CompressionMessageExtensionProcessor FINAL : public WebSocketExtensionProcessor {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(CompressionMessageExtensionProcessor);
public:
    static PassOwnPtr<CompressionMessageExtensionProcessor> create(WebSocketPerMessageDeflate& compress)
    {
        return adoptPtr(new CompressionMessageExtensionProcessor(compress));
    }
    virtual ~CompressionMessageExtensionProcessor() { }

    virtual String handshakeString() OVERRIDE;
    virtual bool processResponse(const HashMap<String, String>&) OVERRIDE;
    virtual String failureReason() OVERRIDE { return m_failureReason; }

private:
    explicit CompressionMessageExtensionProcessor(WebSocketPerMessageDeflate&);

    WebSocketPerMessageDeflate& m_compress;
    bool m_responseProcessed;
    String m_failureReason;
};

CompressionMessageExtensionProcessor::CompressionMessageExtensionProcessor(WebSocketPerMessageDeflate& compress)
    : WebSocketExtensionProcessor("permessage-deflate")
    , m_compress(compress)
    , m_responseProcessed(false)
{
}

String CompressionMessageExtensionProcessor::handshakeString()
{
    return "permessage-deflate; client_max_window_bits";
}

bool CompressionMessageExtensionProcessor::processResponse(const HashMap<String, String>& parameters)
{
    unsigned numProcessedParameters = 0;
    if (m_responseProcessed) {
        m_failureReason = "Received duplicate permessage-deflate response";
        return false;
    }
    m_responseProcessed = true;
    WebSocketDeflater::ContextTakeOverMode mode = WebSocketDeflater::TakeOverContext;
    int windowBits = 15;

    HashMap<String, String>::const_iterator clientNoContextTakeover = parameters.find("client_no_context_takeover");
    HashMap<String, String>::const_iterator clientMaxWindowBits = parameters.find("client_max_window_bits");
    HashMap<String, String>::const_iterator serverNoContextTakeover = parameters.find("server_no_context_takeover");
    HashMap<String, String>::const_iterator serverMaxWindowBits = parameters.find("server_max_window_bits");

    if (clientNoContextTakeover != parameters.end()) {
        if (!clientNoContextTakeover->value.isNull()) {
            m_failureReason = "Received invalid client_no_context_takeover parameter";
            return false;
        }
        mode = WebSocketDeflater::DoNotTakeOverContext;
        ++numProcessedParameters;
    }
    if (clientMaxWindowBits != parameters.end()) {
        if (!clientMaxWindowBits->value.length()) {
            m_failureReason = "client_max_window_bits parameter must have value";
            return false;
        }
        bool ok = false;
        windowBits = clientMaxWindowBits->value.toIntStrict(&ok);
        if (!ok || windowBits < 8 || windowBits > 15 || clientMaxWindowBits->value[0] == '+' || clientMaxWindowBits->value[0] == '0') {
            m_failureReason = "Received invalid client_max_window_bits parameter";
            return false;
        }
        ++numProcessedParameters;
    }
    if (serverNoContextTakeover != parameters.end()) {
        if (!serverNoContextTakeover->value.isNull()) {
            m_failureReason = "Received invalid server_no_context_takeover parameter";
            return false;
        }
        ++numProcessedParameters;
    }
    if (serverMaxWindowBits != parameters.end()) {
        if (!serverMaxWindowBits->value.length()) {
            m_failureReason = "server_max_window_bits parameter must have value";
            return false;
        }
        bool ok = false;
        int bits = serverMaxWindowBits->value.toIntStrict(&ok);
        if (!ok || bits < 8 || bits > 15 || serverMaxWindowBits->value[0] == '+' || serverMaxWindowBits->value[0] == '0') {
            m_failureReason = "Received invalid server_max_window_bits parameter";
            return false;
        }
        ++numProcessedParameters;
    }

    if (numProcessedParameters != parameters.size()) {
        m_failureReason = "Received an unexpected permessage-deflate extension parameter";
        return false;
    }
    blink::Platform::current()->histogramEnumeration("WebCore.WebSocket.PerMessageDeflateContextTakeOverMode", mode, WebSocketDeflater::ContextTakeOverModeMax);
    m_compress.enable(windowBits, mode);
    // Since we don't request server_no_context_takeover and server_max_window_bits, they should be ignored.
    return true;
}

WebSocketPerMessageDeflate::WebSocketPerMessageDeflate()
    : m_enabled(false)
    , m_deflateOngoing(false)
    , m_inflateOngoing(false)
{
}

PassOwnPtr<WebSocketExtensionProcessor> WebSocketPerMessageDeflate::createExtensionProcessor()
{
    return CompressionMessageExtensionProcessor::create(*this);
}

void WebSocketPerMessageDeflate::enable(int windowBits, WebSocketDeflater::ContextTakeOverMode mode)
{
    m_deflater = WebSocketDeflater::create(windowBits, mode);
    m_inflater = WebSocketInflater::create();
    if (!m_deflater->initialize() || !m_inflater->initialize()) {
        m_deflater.clear();
        m_inflater.clear();
        return;
    }
    m_enabled = true;
    m_deflateOngoing = false;
    m_inflateOngoing = false;
}

bool WebSocketPerMessageDeflate::deflate(WebSocketFrame& frame)
{
    if (!enabled())
        return true;
    if (frame.compress) {
        m_failureReason = "Some extension already uses the compress bit.";
        return false;
    }
    if (!WebSocketFrame::isNonControlOpCode(frame.opCode))
        return true;

    if ((frame.opCode == WebSocketFrame::OpCodeText || frame.opCode == WebSocketFrame::OpCodeBinary)
        && frame.final
        && frame.payloadLength <= 2) {
        // A trivial optimization: If a message consists of one frame and its
        // payload length is very short, we don't compress it.
        return true;
    }

    if (frame.payloadLength > 0 && !m_deflater->addBytes(frame.payload, frame.payloadLength)) {
        m_failureReason = "Failed to deflate a frame";
        return false;
    }
    if (frame.final && !m_deflater->finish()) {
        m_failureReason = "Failed to finish compression";
        return false;
    }

    frame.compress = !m_deflateOngoing;
    frame.payload = m_deflater->data();
    frame.payloadLength = m_deflater->size();
    m_deflateOngoing = !frame.final;
    return true;
}

void WebSocketPerMessageDeflate::resetDeflateBuffer()
{
    if (m_deflater) {
        if (m_deflateOngoing)
            m_deflater->softReset();
        else
            m_deflater->reset();
    }
}

bool WebSocketPerMessageDeflate::inflate(WebSocketFrame& frame)
{
    if (!enabled())
        return true;
    if (!WebSocketFrame::isNonControlOpCode(frame.opCode)) {
        if (frame.compress) {
            m_failureReason = "Received unexpected compressed frame";
            return false;
        }
        return true;
    }
    if (frame.compress) {
        if (m_inflateOngoing) {
            m_failureReason = "Received a frame that sets compressed bit while another decompression is ongoing";
            return false;
        }
        m_inflateOngoing = true;
    }

    if (!m_inflateOngoing)
        return true;

    if (frame.payloadLength > 0 && !m_inflater->addBytes(frame.payload, frame.payloadLength)) {
        m_failureReason = "Failed to inflate a frame";
        return false;
    }
    if (frame.final && !m_inflater->finish()) {
        m_failureReason = "Failed to finish decompression";
        return false;
    }
    frame.compress = false;
    frame.payload = m_inflater->data();
    frame.payloadLength = m_inflater->size();
    m_inflateOngoing = !frame.final;
    return true;
}

void WebSocketPerMessageDeflate::resetInflateBuffer()
{
    if (m_inflater)
        m_inflater->reset();
}

void WebSocketPerMessageDeflate::didFail()
{
    resetDeflateBuffer();
    resetInflateBuffer();
}

} // namespace WebCore
