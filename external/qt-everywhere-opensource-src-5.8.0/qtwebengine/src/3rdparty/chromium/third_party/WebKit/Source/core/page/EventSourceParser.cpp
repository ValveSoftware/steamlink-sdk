// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/EventSourceParser.h"

#include "core/EventTypeNames.h"
#include "core/page/EventSource.h"
#include "wtf/ASCIICType.h"
#include "wtf/Assertions.h"
#include "wtf/NotFound.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/TextEncoding.h"
#include "wtf/text/TextEncodingRegistry.h"

namespace blink {

EventSourceParser::EventSourceParser(const AtomicString& lastEventId, Client* client)
    : m_id(lastEventId)
    , m_lastEventId(lastEventId)
    , m_client(client)
    , m_codec(newTextCodec(UTF8Encoding()))
{
}

void EventSourceParser::addBytes(const char* bytes, size_t size)
{
    // A line consists of |m_line| followed by
    // |bytes[start..(next line break)]|.
    size_t start = 0;
    const unsigned char kBOM[] = {0xef, 0xbb, 0xbf};
    for (size_t i = 0; i < size && !m_isStopped; ++i) {
        // As kBOM contains neither CR nor LF, we can think BOM and the line
        // break separately.
        if (m_isRecognizingBOM && m_line.size() + (i - start) == WTF_ARRAY_LENGTH(kBOM)) {
            Vector<char> line = m_line;
            line.append(&bytes[start], i - start);
            ASSERT(line.size() == WTF_ARRAY_LENGTH(kBOM));
            m_isRecognizingBOM = false;
            if (memcmp(line.data(), kBOM, sizeof(kBOM)) == 0) {
                start = i;
                m_line.clear();
                continue;
            }
        }
        if (m_isRecognizingCRLF && bytes[i] == '\n') {
            // This is the latter part of "\r\n".
            m_isRecognizingCRLF = false;
            ++start;
            continue;
        }
        m_isRecognizingCRLF = false;
        if (bytes[i] == '\r' || bytes[i] == '\n') {
            m_line.append(&bytes[start], i - start);
            parseLine();
            m_line.clear();
            start = i + 1;
            m_isRecognizingCRLF = bytes[i] == '\r';
            m_isRecognizingBOM = false;
        }
    }
    if (m_isStopped)
        return;
    m_line.append(&bytes[start], size - start);
}

void EventSourceParser::parseLine()
{
    if (m_line.size() == 0) {
        m_lastEventId = m_id;
        // We dispatch an event when seeing an empty line.
        if (!m_data.isEmpty()) {
            ASSERT(m_data[m_data.size() - 1] == '\n');
            String data = fromUTF8(m_data.data(), m_data.size() - 1);
            m_client->onMessageEvent(m_eventType.isEmpty() ? EventTypeNames::message : m_eventType, data, m_lastEventId);
            m_data.clear();
        }
        m_eventType = nullAtom;
        return;
    }
    size_t fieldNameEnd = m_line.find(':');
    size_t fieldValueStart;
    if (fieldNameEnd == WTF::kNotFound) {
        fieldNameEnd = m_line.size();
        fieldValueStart = fieldNameEnd;
    } else {
        fieldValueStart = fieldNameEnd + 1;
        if (fieldValueStart < m_line.size() && m_line[fieldValueStart] == ' ') {
            ++fieldValueStart;
        }
    }
    size_t fieldValueSize = m_line.size() - fieldValueStart;
    String fieldName = fromUTF8(m_line.data(), fieldNameEnd);
    if (fieldName == "event") {
        m_eventType = AtomicString(fromUTF8(m_line.data() + fieldValueStart, fieldValueSize));
        return;
    }
    if (fieldName == "data") {
        m_data.append(m_line.data() + fieldValueStart, fieldValueSize);
        m_data.append('\n');
        return;
    }
    if (fieldName == "id") {
        m_id = AtomicString(fromUTF8(m_line.data() + fieldValueStart, fieldValueSize));
        return;
    }
    if (fieldName == "retry") {
        bool hasOnlyDigits = true;
        for (size_t i = fieldValueStart; i < m_line.size() && hasOnlyDigits; ++i)
            hasOnlyDigits = isASCIIDigit(m_line[i]);
        if (fieldValueStart == m_line.size()) {
            m_client->onReconnectionTimeSet(EventSource::defaultReconnectDelay);
        } else if (hasOnlyDigits) {
            bool ok;
            auto reconnectionTime = fromUTF8(m_line.data() + fieldValueStart, fieldValueSize).toUInt64Strict(&ok);
            if (ok)
                m_client->onReconnectionTimeSet(reconnectionTime);
        }
        return;
    }
    // Unrecognized field name. Ignore!
}

String EventSourceParser::fromUTF8(const char* bytes, size_t size)
{
    return m_codec->decode(bytes, size, WTF::DataEOF);
}

DEFINE_TRACE(EventSourceParser)
{
    visitor->trace(m_client);
}

} // namespace blink

