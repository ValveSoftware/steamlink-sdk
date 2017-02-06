/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/indexeddb/IDBKeyPath.h"

#include "wtf/ASCIICType.h"
#include "wtf/dtoa.h"
#include "wtf/text/CharacterNames.h"
#include "wtf/text/Unicode.h"

using namespace WTF::Unicode;

namespace blink {


namespace {

using namespace WTF::Unicode;

// The following correspond to grammar in ECMA-262.
const uint32_t unicodeLetter = Letter_Uppercase | Letter_Lowercase | Letter_Titlecase | Letter_Modifier | Letter_Other | Number_Letter;
const uint32_t unicodeCombiningMark = Mark_NonSpacing | Mark_SpacingCombining;
const uint32_t unicodeDigit = Number_DecimalDigit;
const uint32_t unicodeConnectorPunctuation = Punctuation_Connector;

static inline bool isIdentifierStartCharacter(UChar c)
{
    return (category(c) & unicodeLetter) || (c == '$') || (c == '_');
}

static inline bool isIdentifierCharacter(UChar c)
{
    return (category(c) & (unicodeLetter | unicodeCombiningMark | unicodeDigit | unicodeConnectorPunctuation)) || (c == '$') || (c == '_') || (c == zeroWidthNonJoinerCharacter) || (c == zeroWidthJoinerCharacter);
}

bool isIdentifier(const String& s)
{
    size_t length = s.length();
    if (!length)
        return false;
    if (!isIdentifierStartCharacter(s[0]))
        return false;
    for (size_t i = 1; i < length; ++i) {
        if (!isIdentifierCharacter(s[i]))
            return false;
    }
    return true;
}

} // namespace

bool IDBIsValidKeyPath(const String& keyPath)
{
    IDBKeyPathParseError error;
    Vector<String> keyPathElements;
    IDBParseKeyPath(keyPath, keyPathElements, error);
    return error == IDBKeyPathParseErrorNone;
}

void IDBParseKeyPath(const String& keyPath, Vector<String>& elements, IDBKeyPathParseError& error)
{
    // IDBKeyPath ::= EMPTY_STRING | identifier ('.' identifier)*

    if (keyPath.isEmpty()) {
        error = IDBKeyPathParseErrorNone;
        return;
    }

    keyPath.split('.', /*allowEmptyEntries*/true, elements);
    for (size_t i = 0; i < elements.size(); ++i) {
        if (!isIdentifier(elements[i])) {
            error = IDBKeyPathParseErrorIdentifier;
            return;
        }
    }
    error = IDBKeyPathParseErrorNone;
}

IDBKeyPath::IDBKeyPath(const String& string)
    : m_type(StringType)
    , m_string(string)
{
    ASSERT(!m_string.isNull());
}

IDBKeyPath::IDBKeyPath(const Vector<String>& array)
    : m_type(ArrayType)
    , m_array(array)
{
#if ENABLE(ASSERT)
    for (size_t i = 0; i < m_array.size(); ++i)
        ASSERT(!m_array[i].isNull());
#endif
}

IDBKeyPath::IDBKeyPath(const StringOrStringSequence& keyPath)
{
    if (keyPath.isNull()) {
        m_type = NullType;
    } else if (keyPath.isString()) {
        m_type = StringType;
        m_string = keyPath.getAsString();
        ASSERT(!m_string.isNull());
    } else {
        ASSERT(keyPath.isStringSequence());
        m_type = ArrayType;
        m_array = keyPath.getAsStringSequence();
#if ENABLE(ASSERT)
        for (size_t i = 0; i < m_array.size(); ++i)
            ASSERT(!m_array[i].isNull());
#endif
    }
}

IDBKeyPath::IDBKeyPath(const WebIDBKeyPath& keyPath)
{
    switch (keyPath.keyPathType()) {
    case WebIDBKeyPathTypeNull:
        m_type = NullType;
        return;

    case WebIDBKeyPathTypeString:
        m_type = StringType;
        m_string = keyPath.string();
        return;

    case WebIDBKeyPathTypeArray:
        m_type = ArrayType;
        for (size_t i = 0, size = keyPath.array().size(); i < size; ++i)
            m_array.append(keyPath.array()[i]);
        return;
    }
    ASSERT_NOT_REACHED();
}

IDBKeyPath::operator WebIDBKeyPath() const
{
    switch (m_type) {
    case NullType:
        return WebIDBKeyPath();
    case StringType:
        return WebIDBKeyPath(WebString(m_string));
    case ArrayType:
        return WebIDBKeyPath(m_array);
    }
    ASSERT_NOT_REACHED();
    return WebIDBKeyPath();
}

bool IDBKeyPath::isValid() const
{
    switch (m_type) {
    case NullType:
        return false;

    case StringType:
        return IDBIsValidKeyPath(m_string);

    case ArrayType:
        if (m_array.isEmpty())
            return false;
        for (size_t i = 0; i < m_array.size(); ++i) {
            if (!IDBIsValidKeyPath(m_array[i]))
                return false;
        }
        return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool IDBKeyPath::operator==(const IDBKeyPath& other) const
{
    if (m_type != other.m_type)
        return false;

    switch (m_type) {
    case NullType:
        return true;
    case StringType:
        return m_string == other.m_string;
    case ArrayType:
        return m_array == other.m_array;
    }
    ASSERT_NOT_REACHED();
    return false;
}

} // namespace blink
