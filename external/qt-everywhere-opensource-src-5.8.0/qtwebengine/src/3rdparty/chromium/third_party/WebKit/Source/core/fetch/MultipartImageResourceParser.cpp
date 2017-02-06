// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fetch/MultipartImageResourceParser.h"

#include "public/platform/Platform.h"
#include "public/platform/WebURLResponse.h"
#include "wtf/NotFound.h"
#include "wtf/text/WTFString.h"

#include <algorithm>

namespace blink {

MultipartImageResourceParser::MultipartImageResourceParser(const ResourceResponse& response, const Vector<char>& boundary, Client* client)
    : m_originalResponse(response)
    , m_boundary(boundary)
    , m_client(client)
{
    // Some servers report a boundary prefixed with "--".  See https://crbug.com/5786.
    if (m_boundary.size() < 2 || m_boundary[0] != '-' || m_boundary[1] != '-')
        m_boundary.prepend("--", 2);
}

void MultipartImageResourceParser::appendData(const char* bytes, size_t size)
{
    ASSERT(!isCancelled());
    // m_sawLastBoundary means that we've already received the final boundary
    // token. The server should stop sending us data at this point, but if it
    // does, we just throw it away.
    if (m_sawLastBoundary)
        return;
    m_data.append(bytes, size);

    if (m_isParsingTop) {
        // Eat leading \r\n
        size_t pos = skippableLength(m_data, 0);
        // +2 for "--"
        if (m_data.size() < m_boundary.size() + 2 + pos) {
            // We don't have enough data yet to make a boundary token.  Just
            // wait until the next chunk of data arrives.
            return;
        }
        if (pos)
            m_data.remove(0, pos);

        // Some servers don't send a boundary token before the first chunk of
        // data.  We handle this case anyway (Gecko does too).
        if (0 != memcmp(m_data.data(), m_boundary.data(), m_boundary.size())) {
            m_data.prepend("\n", 1);
            m_data.prependVector(m_boundary);
        }
        m_isParsingTop = false;
    }

    // Headers
    if (m_isParsingHeaders) {
        if (!parseHeaders()) {
            // Get more data before trying again.
            return;
        }
        // Successfully parsed headers.
        m_isParsingHeaders = false;
        if (isCancelled())
            return;
    }

    size_t boundaryPosition;
    while ((boundaryPosition = findBoundary(m_data, &m_boundary)) != kNotFound) {
        // Strip out trailing \r\n characters in the buffer preceding the
        // boundary on the same lines as does Firefox.
        size_t dataSize = boundaryPosition;
        if (boundaryPosition > 0 && m_data[boundaryPosition - 1] == '\n') {
            dataSize--;
            if (boundaryPosition > 1 && m_data[boundaryPosition - 2] == '\r') {
                dataSize--;
            }
        }
        if (dataSize) {
            m_client->multipartDataReceived(m_data.data(), dataSize);
            if (isCancelled())
                return;
        }
        size_t boundaryEndPosition = boundaryPosition + m_boundary.size();
        if (boundaryEndPosition < m_data.size() && '-' == m_data[boundaryEndPosition]) {
            // This was the last boundary so we can stop processing.
            m_sawLastBoundary = true;
            m_data.clear();
            return;
        }

        // We can now throw out data up through the boundary
        m_data.remove(0, boundaryEndPosition);

        // Ok, back to parsing headers
        if (!parseHeaders()) {
            m_isParsingHeaders = true;
            break;
        }
        if (isCancelled())
            return;
    }

    // At this point, we should send over any data we have, but keep enough
    // data buffered to handle a boundary that may have been truncated.
    // "+2" for CRLF, as we may ignore the last CRLF.
    if (!m_isParsingHeaders && m_data.size() > m_boundary.size() + 2) {
        size_t sendLength = m_data.size() - m_boundary.size() - 2;
        m_client->multipartDataReceived(m_data.data(), sendLength);
        m_data.remove(0, sendLength);
    }
}

void MultipartImageResourceParser::finish()
{
    ASSERT(!isCancelled());
    if (m_sawLastBoundary)
        return;
    // If we have any pending data and we're not in a header, go ahead and send
    // it to the client.
    if (!m_isParsingHeaders && !m_data.isEmpty())
        m_client->multipartDataReceived(m_data.data(), m_data.size());
    m_data.clear();
    m_sawLastBoundary = true;
}

size_t MultipartImageResourceParser::skippableLength(const Vector<char>& data, size_t pos)
{
    if (data.size() >= pos + 2 && data[pos] == '\r' && data[pos + 1] == '\n')
        return 2;
    if (data.size() >= pos + 1 && data[pos] == '\n')
        return 1;
    return 0;
}

bool MultipartImageResourceParser::parseHeaders()
{
    // Eat leading \r\n
    size_t pos = skippableLength(m_data, 0);

    // Create a WebURLResponse based on the original set of headers + the
    // replacement headers. We only replace the same few headers that gecko
    // does. See netwerk/streamconv/converters/nsMultiMixedConv.cpp.
    WebURLResponse response(m_originalResponse.url());
    for (const auto& header : m_originalResponse.httpHeaderFields())
        response.addHTTPHeaderField(header.key, header.value);

    size_t end = 0;
    if (!Platform::current()->parseMultipartHeadersFromBody(m_data.data() + pos, m_data.size() - pos, &response, &end))
        return false;
    m_data.remove(0, end + pos);
    // Send the response!
    m_client->onePartInMultipartReceived(response.toResourceResponse());
    return true;
}

// Boundaries are supposed to be preceeded with --, but it looks like gecko
// doesn't require the dashes to exist.  See nsMultiMixedConv::FindToken.
size_t MultipartImageResourceParser::findBoundary(const Vector<char>& data, Vector<char>* boundary)
{
    auto it = std::search(data.data(), data.data() + data.size(), boundary->data(), boundary->data() + boundary->size());
    if (it == data.data() + data.size())
        return kNotFound;

    size_t boundaryPosition = it - data.data();
    // Back up over -- for backwards compat
    // TODO(tc): Don't we only want to do this once?  Gecko code doesn't
    // seem to care.
    if (boundaryPosition >= 2) {
        if (data[boundaryPosition - 1] == '-' && data[boundaryPosition - 2] == '-') {
            boundaryPosition -= 2;
            Vector<char> v(2, '-');
            v.appendVector(*boundary);
            *boundary = v;
        }
    }
    return boundaryPosition;
}

DEFINE_TRACE(MultipartImageResourceParser)
{
    visitor->trace(m_client);
}

} // namespace blink
