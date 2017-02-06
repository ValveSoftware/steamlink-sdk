/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "wtf/text/StringBuilder.h"

#include "wtf/dtoa.h"
#include "wtf/text/IntegerToStringConversion.h"
#include "wtf/text/WTFString.h"
#include <algorithm>

namespace WTF {

String StringBuilder::toString()
{
    if (!m_length)
        return emptyString();
    if (m_string.isNull()) {
        if (m_is8Bit)
            m_string = String(characters8(), m_length);
        else
            m_string = String(characters16(), m_length);
    }
    return m_string;
}

AtomicString StringBuilder::toAtomicString()
{
    if (!m_length)
        return emptyAtom;
    if (m_string.isNull()) {
        if (m_is8Bit)
            m_string = AtomicString(characters8(), m_length);
        else
            m_string = AtomicString(characters16(), m_length);
    }
    return AtomicString(m_string);
}

String StringBuilder::substring(unsigned start, unsigned length) const
{
    if (start >= m_length)
        return emptyString();
    if (!m_string.isNull())
        return m_string.substring(start, length);
    length = std::min(length, m_length - start);
    if (m_is8Bit)
        return String(characters8() + start, length);
    return String(characters16() + start, length);
}

void StringBuilder::swap(StringBuilder& builder)
{
    std::swap(m_string, builder.m_string);
    std::swap(m_buffer, builder.m_buffer);
    std::swap(m_length, builder.m_length);
    std::swap(m_is8Bit, builder.m_is8Bit);
}

void StringBuilder::clear()
{
    m_string = String();
    if (m_is8Bit)
        delete m_buffer8;
    else
        delete m_buffer16;
    m_buffer = nullptr;
    m_length = 0;
    m_is8Bit = true;
}

unsigned StringBuilder::capacity() const
{
    if (!hasBuffer())
        return 0;
    if (m_is8Bit)
        return m_buffer8->capacity();
    return m_buffer16->capacity();
}

void StringBuilder::reserveCapacity(unsigned newCapacity)
{
    if (m_is8Bit) {
        ensureBuffer8();
        m_buffer8->reserveCapacity(newCapacity);
    } else {
        ensureBuffer16();
        m_buffer16->reserveCapacity(newCapacity);
    }
}

void StringBuilder::resize(unsigned newSize)
{
    DCHECK_LE(newSize, m_length);
    m_length = newSize;
    m_string = String();
    if (!hasBuffer())
        return;
    if (m_is8Bit)
        m_buffer8->resize(newSize);
    else
        m_buffer16->resize(newSize);
}

void StringBuilder::createBuffer8()
{
    DCHECK(!hasBuffer());
    DCHECK(m_is8Bit);
    m_buffer8 = new Buffer8;
    m_length = 0;
    // Must keep a ref to the string since append will clear it.
    String string(m_string);
    append(string);
}

void StringBuilder::createBuffer16()
{
    DCHECK(m_is8Bit || !hasBuffer());
    Buffer8 buffer8;
    unsigned length = m_length;
    if (m_buffer8) {
        m_buffer8->swap(buffer8);
        delete m_buffer8;
    }
    m_buffer16 = new Buffer16;
    m_is8Bit = false;
    m_length = 0;
    if (!buffer8.isEmpty()) {
        append(buffer8.data(), length);
        return;
    }
    // Must keep a ref to the string since append will clear it.
    String string(m_string);
    append(string);
}

void StringBuilder::append(const UChar* characters, unsigned length)
{
    if (!length)
        return;
    DCHECK(characters);

    // If there's only one char we use append(UChar) instead since it will
    // check for latin1 and avoid converting to 16bit if possible.
    if (length == 1) {
        append(*characters);
        return;
    }

    ensureBuffer16();
    m_string = String();
    m_buffer16->append(characters, length);
    m_length += length;
}

void StringBuilder::append(const LChar* characters, unsigned length)
{
    if (!length)
        return;
    DCHECK(characters);

    if (m_is8Bit) {
        ensureBuffer8();
        m_string = String();
        m_buffer8->append(characters, length);
        m_length += length;
        return;
    }

    ensureBuffer16();
    m_string = String();
    m_buffer16->append(characters, length);
    m_length += length;
}

template<typename IntegerType>
static void appendIntegerInternal(StringBuilder& builder, IntegerType input)
{
    IntegerToStringConverter<IntegerType> converter(input);
    builder.append(converter.characters8(), converter.length());
}

void StringBuilder::appendNumber(int number)
{
    appendIntegerInternal(*this, number);
}

void StringBuilder::appendNumber(unsigned number)
{
    appendIntegerInternal(*this, number);
}

void StringBuilder::appendNumber(long number)
{
    appendIntegerInternal(*this, number);
}

void StringBuilder::appendNumber(unsigned long number)
{
    appendIntegerInternal(*this, number);
}

void StringBuilder::appendNumber(long long number)
{
    appendIntegerInternal(*this, number);
}

void StringBuilder::appendNumber(unsigned long long number)
{
    appendIntegerInternal(*this, number);
}

void StringBuilder::appendNumber(double number, unsigned precision, TrailingZerosTruncatingPolicy trailingZerosTruncatingPolicy)
{
    NumberToStringBuffer buffer;
    append(numberToFixedPrecisionString(number, precision, buffer, trailingZerosTruncatingPolicy == TruncateTrailingZeros));
}

} // namespace WTF
