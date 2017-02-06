/*
 * Copyright (C) 2009, 2010, 2012, 2013 Apple Inc. All rights reserved.
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

#ifndef StringBuilder_h
#define StringBuilder_h

#include "wtf/WTFExport.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/StringView.h"
#include "wtf/text/WTFString.h"

namespace WTF {

class WTF_EXPORT StringBuilder {
    WTF_MAKE_NONCOPYABLE(StringBuilder);
public:
    StringBuilder()
        : m_buffer(nullptr)
        , m_length(0)
        , m_is8Bit(true) {}

    ~StringBuilder() { clear(); }

    void append(const UChar*, unsigned length);
    void append(const LChar*, unsigned length);

    ALWAYS_INLINE void append(const char* characters, unsigned length) { append(reinterpret_cast<const LChar*>(characters), length); }

    void append(const StringBuilder& other)
    {
        if (!other.m_length)
            return;

        if (!m_length && !hasBuffer() && !other.m_string.isNull()) {
            m_string = other.m_string;
            m_length = other.m_string.length();
            m_is8Bit = other.m_string.is8Bit();
            return;
        }

        if (other.is8Bit())
            append(other.characters8(), other.m_length);
        else
            append(other.characters16(), other.m_length);
    }

    // NOTE: The semantics of this are different than StringView(..., offset, length)
    // in that an invalid offset or invalid length is a no-op instead of an
    // error.
    // TODO(esprehn): We should probably unify the semantics instead.
    void append(const StringView& string, unsigned offset, unsigned length)
    {
        unsigned extent = offset + length;
        if (extent < offset || extent > string.length())
            return;

        // We can't do this before the above check since StringView's constructor
        // doesn't accept invalid offsets or lengths.
        append(StringView(string, offset, length));
    }

    void append(const StringView& string)
    {
        if (string.isEmpty())
            return;

        // If we're appending to an empty builder, and there is not a buffer
        // (reserveCapacity has not been called), then share the impl if
        // possible.
        //
        // This is important to avoid string copies inside dom operations like
        // Node::textContent when there's only a single Text node child, or
        // inside the parser in the common case when flushing buffered text to
        // a Text node.
        StringImpl* impl = string.sharedImpl();
        if (!m_length && !hasBuffer() && impl) {
            m_string = impl;
            m_length = impl->length();
            m_is8Bit = impl->is8Bit();
            return;
        }

        if (string.is8Bit())
            append(string.characters8(), string.length());
        else
            append(string.characters16(), string.length());
    }

    void append(UChar c)
    {
        if (m_is8Bit && c <= 0xFF) {
            append(static_cast<LChar>(c));
            return;
        }
        ensureBuffer16();
        m_string = String();
        m_buffer16->append(c);
        ++m_length;
    }

    void append(LChar c)
    {
        if (!m_is8Bit) {
            append(static_cast<UChar>(c));
            return;
        }
        ensureBuffer8();
        m_string = String();
        m_buffer8->append(c);
        ++m_length;
    }

    void append(char c)
    {
        append(static_cast<LChar>(c));
    }

    void append(UChar32 c)
    {
        if (U_IS_BMP(c)) {
            append(static_cast<UChar>(c));
            return;
        }
        append(U16_LEAD(c));
        append(U16_TRAIL(c));
    }

    void appendNumber(int);
    void appendNumber(unsigned);
    void appendNumber(long);
    void appendNumber(unsigned long);
    void appendNumber(long long);
    void appendNumber(unsigned long long);
    void appendNumber(double, unsigned precision = 6, TrailingZerosTruncatingPolicy = TruncateTrailingZeros);

    String toString();
    AtomicString toAtomicString();
    String substring(unsigned start, unsigned length) const;

    unsigned length() const { return m_length; }
    bool isEmpty() const { return !m_length; }

    unsigned capacity() const;
    void reserveCapacity(unsigned newCapacity);

    // TODO(esprehn): Rename to shrink().
    void resize(unsigned newSize);

    UChar operator[](unsigned i) const
    {
        ASSERT_WITH_SECURITY_IMPLICATION(i < m_length);
        if (m_is8Bit)
            return characters8()[i];
        return characters16()[i];
    }

    const LChar* characters8() const
    {
        DCHECK(m_is8Bit);
        if (!length())
            return nullptr;
        if (!m_string.isNull())
            return m_string.characters8();
        DCHECK(m_buffer8);
        return m_buffer8->data();
    }

    const UChar* characters16() const
    {
        DCHECK(!m_is8Bit);
        if (!length())
            return nullptr;
        if (!m_string.isNull())
            return m_string.characters16();
        DCHECK(m_buffer16);
        return m_buffer16->data();
    }

    bool is8Bit() const { return m_is8Bit; }

    void clear();
    void swap(StringBuilder&);

private:
    typedef Vector<LChar, 16> Buffer8;
    typedef Vector<UChar, 16> Buffer16;

    void ensureBuffer8()
    {
        DCHECK(m_is8Bit);
        if (!hasBuffer())
            createBuffer8();
    }

    void ensureBuffer16()
    {
        if (m_is8Bit || !hasBuffer())
            createBuffer16();
    }

    void createBuffer8();
    void createBuffer16();

    bool hasBuffer() const { return m_buffer; }

    String m_string;
    union {
        Buffer8* m_buffer8;
        Buffer16* m_buffer16;
        void* m_buffer;
    };
    unsigned m_length;
    bool m_is8Bit;
};

template <typename CharType>
bool equal(const StringBuilder& s, const CharType* buffer, unsigned length)
{
    if (s.length() != length)
        return false;

    if (s.is8Bit())
        return equal(s.characters8(), buffer, length);

    return equal(s.characters16(), buffer, length);
}

template<typename CharType>
bool equalIgnoringCase(const StringBuilder& s, const CharType* buffer, unsigned length)
{
    if (s.length() != length)
        return false;

    if (s.is8Bit())
        return equalIgnoringCase(s.characters8(), buffer, length);

    return equalIgnoringCase(s.characters16(), buffer, length);
}

inline bool equalIgnoringCase(const StringBuilder& s, const char* string)
{
    return equalIgnoringCase(s, reinterpret_cast<const LChar*>(string), strlen(string));
}

template <typename StringType>
bool equal(const StringBuilder& a, const StringType& b)
{
    if (a.length() != b.length())
        return false;

    if (!a.length())
        return true;

    if (a.is8Bit()) {
        if (b.is8Bit())
            return equal(a.characters8(), b.characters8(), a.length());
        return equal(a.characters8(), b.characters16(), a.length());
    }

    if (b.is8Bit())
        return equal(a.characters16(), b.characters8(), a.length());
    return equal(a.characters16(), b.characters16(), a.length());
}

template <typename StringType>
bool equalIgnoringCase(const StringBuilder& a, const StringType& b)
{
    if (a.length() != b.length())
        return false;

    if (!a.length())
        return true;

    if (a.is8Bit()) {
        if (b.is8Bit())
            return equalIgnoringCase(a.characters8(), b.characters8(), a.length());
        return equalIgnoringCase(a.characters8(), b.characters16(), a.length());
    }

    if (b.is8Bit())
        return equalIgnoringCase(a.characters16(), b.characters8(), a.length());
    return equalIgnoringCase(a.characters16(), b.characters16(), a.length());
}

inline bool operator==(const StringBuilder& a, const StringBuilder& b) { return equal(a, b); }
inline bool operator!=(const StringBuilder& a, const StringBuilder& b) { return !equal(a, b); }
inline bool operator==(const StringBuilder& a, const String& b) { return equal(a, b); }
inline bool operator!=(const StringBuilder& a, const String& b) { return !equal(a, b); }
inline bool operator==(const String& a, const StringBuilder& b) { return equal(b, a); }
inline bool operator!=(const String& a, const StringBuilder& b) { return !equal(b, a); }

} // namespace WTF

using WTF::StringBuilder;

#endif // StringBuilder_h
