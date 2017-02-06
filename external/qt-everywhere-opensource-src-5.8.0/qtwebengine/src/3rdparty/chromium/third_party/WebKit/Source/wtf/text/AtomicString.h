/*
 * Copyright (C) 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef AtomicString_h
#define AtomicString_h

#include "wtf/Allocator.h"
#include "wtf/HashTableDeletedValueType.h"
#include "wtf/WTFExport.h"
#include "wtf/text/CString.h"
#include "wtf/text/WTFString.h"
#include <cstring>
#include <iosfwd>

namespace WTF {

struct AtomicStringHash;

class WTF_EXPORT AtomicString {
    USING_FAST_MALLOC(AtomicString);
public:
    static void init();

    AtomicString() {}
    AtomicString(const LChar* chars)
        : AtomicString(chars, chars ? strlen(reinterpret_cast<const char*>(chars)) : 0) {}
    AtomicString(const char* chars)
        : AtomicString(reinterpret_cast<const LChar*>(chars)) {}
    AtomicString(const LChar* chars, unsigned length);
    AtomicString(const UChar* chars, unsigned length);
    AtomicString(const UChar* chars, unsigned length, unsigned existingHash);
    AtomicString(const UChar* chars);

    template<size_t inlineCapacity>
    explicit AtomicString(const Vector<UChar, inlineCapacity>& vector)
        : AtomicString(vector.data(), vector.size()) {}

    // Constructing an AtomicString from a String / StringImpl can be expensive if
    // the StringImpl is not already atomic.
    explicit AtomicString(StringImpl* impl) : m_string(add(impl)) { }
    explicit AtomicString(const String& s) : m_string(add(s.impl())) { }

    AtomicString(StringImpl* baseString, unsigned start, unsigned length);

    // Hash table deleted values, which are only constructed and never copied or destroyed.
    AtomicString(WTF::HashTableDeletedValueType) : m_string(WTF::HashTableDeletedValue) { }
    bool isHashTableDeletedValue() const { return m_string.isHashTableDeletedValue(); }

    operator const String&() const { return m_string; }
    const String& getString() const { return m_string; }

    StringImpl* impl() const { return m_string.impl(); }

    bool is8Bit() const { return m_string.is8Bit(); }
    const LChar* characters8() const { return m_string.characters8(); }
    const UChar* characters16() const { return m_string.characters16(); }
    unsigned length() const { return m_string.length(); }

    UChar operator[](unsigned i) const { return m_string[i]; }

    bool contains(UChar c) const { return m_string.contains(c); }
    bool contains(const LChar* s, TextCaseSensitivity caseSensitivity = TextCaseSensitive) const
        { return m_string.contains(s, caseSensitivity); }
    bool contains(const String& s, TextCaseSensitivity caseSensitivity = TextCaseSensitive) const
        { return m_string.contains(s, caseSensitivity); }

    size_t find(UChar c, size_t start = 0) const { return m_string.find(c, start); }
    size_t find(CharacterMatchFunctionPtr matchFunction, unsigned start = 0) const
        { return m_string.find(matchFunction, start); }
    size_t find(const LChar* s, size_t start = 0, TextCaseSensitivity caseSensitivity = TextCaseSensitive) const
        { return m_string.find(s, start, caseSensitivity); }
    size_t find(const String& s, size_t start = 0, TextCaseSensitivity caseSensitivity = TextCaseSensitive) const
        { return m_string.find(s, start, caseSensitivity); }

    bool startsWith(const String& s, TextCaseSensitivity caseSensitivity = TextCaseSensitive) const
        { return m_string.startsWith(s, caseSensitivity); }
    bool startsWith(UChar character) const
        { return m_string.startsWith(character); }
    template<unsigned matchLength>
    bool startsWith(const char (&prefix)[matchLength], TextCaseSensitivity caseSensitivity = TextCaseSensitive) const
        { return m_string.startsWith<matchLength>(prefix, caseSensitivity); }

    bool endsWith(const String& s, TextCaseSensitivity caseSensitivity = TextCaseSensitive) const
        { return m_string.endsWith(s, caseSensitivity); }
    bool endsWith(UChar character) const
        { return m_string.endsWith(character); }
    template<unsigned matchLength>
    bool endsWith(const char (&prefix)[matchLength], TextCaseSensitivity caseSensitivity = TextCaseSensitive) const
        { return m_string.endsWith<matchLength>(prefix, caseSensitivity); }

    AtomicString lower() const;
    AtomicString lowerASCII() const;
    AtomicString upper() const { return AtomicString(impl()->upper()); }

    int toInt(bool* ok = 0) const { return m_string.toInt(ok); }
    double toDouble(bool* ok = 0) const { return m_string.toDouble(ok); }
    float toFloat(bool* ok = 0) const { return m_string.toFloat(ok); }

    static AtomicString number(int);
    static AtomicString number(unsigned);
    static AtomicString number(long);
    static AtomicString number(unsigned long);
    static AtomicString number(long long);
    static AtomicString number(unsigned long long);

    static AtomicString number(double, unsigned precision = 6, TrailingZerosTruncatingPolicy = TruncateTrailingZeros);

    bool isNull() const { return m_string.isNull(); }
    bool isEmpty() const { return m_string.isEmpty(); }

#ifdef __OBJC__
    AtomicString(NSString* s) : m_string(add((CFStringRef)s)) { }
    operator NSString*() const { return m_string; }
#endif
    // AtomicString::fromUTF8 will return a null string if
    // the input data contains invalid UTF-8 sequences.
    // NOTE: Passing a zero size means use the whole string.
    static AtomicString fromUTF8(const char*, size_t length);
    static AtomicString fromUTF8(const char*);

    CString ascii() const { return m_string.ascii(); }
    CString latin1() const { return m_string.latin1(); }
    CString utf8(UTF8ConversionMode mode = LenientUTF8Conversion) const { return m_string.utf8(mode); }

#ifndef NDEBUG
    void show() const;
#endif

private:
    String m_string;

    ALWAYS_INLINE static PassRefPtr<StringImpl> add(StringImpl* r)
    {
        if (!r || r->isAtomic())
            return r;
        return addSlowCase(r);
    }
    static PassRefPtr<StringImpl> addSlowCase(StringImpl*);
#if OS(MACOSX)
    static PassRefPtr<StringImpl> add(CFStringRef);
#endif
};

inline bool operator==(const AtomicString& a, const AtomicString& b) { return a.impl() == b.impl(); }
WTF_EXPORT bool operator==(const AtomicString&, const LChar*);
inline bool operator==(const AtomicString& a, const char* b) { return WTF::equal(a.impl(), reinterpret_cast<const LChar*>(b)); }
inline bool operator==(const AtomicString& a, const Vector<UChar>& b) { return a.impl() && equal(a.impl(), b.data(), b.size()); }
inline bool operator==(const AtomicString& a, const String& b) { return equal(a.impl(), b.impl()); }
inline bool operator==(const LChar* a, const AtomicString& b) { return b == a; }
inline bool operator==(const char* a, const AtomicString& b) { return b == a; }
inline bool operator==(const String& a, const AtomicString& b) { return equal(a.impl(), b.impl()); }
inline bool operator==(const Vector<UChar>& a, const AtomicString& b) { return b == a; }

inline bool operator!=(const AtomicString& a, const AtomicString& b) { return a.impl() != b.impl(); }
inline bool operator!=(const AtomicString& a, const LChar* b) { return !(a == b); }
inline bool operator!=(const AtomicString& a, const char* b) { return !(a == b); }
inline bool operator!=(const AtomicString& a, const String& b) { return !equal(a.impl(), b.impl()); }
inline bool operator!=(const AtomicString& a, const Vector<UChar>& b) { return !(a == b); }
inline bool operator!=(const LChar* a, const AtomicString& b) { return !(b == a); }
inline bool operator!=(const char* a, const AtomicString& b) { return !(b == a); }
inline bool operator!=(const String& a, const AtomicString& b) { return !equal(a.impl(), b.impl()); }
inline bool operator!=(const Vector<UChar>& a, const AtomicString& b) { return !(a == b); }

inline bool equalIgnoringCase(const AtomicString& a, const AtomicString& b) { return equalIgnoringCase(a.impl(), b.impl()); }
inline bool equalIgnoringCase(const AtomicString& a, const LChar* b) { return equalIgnoringCase(a.impl(), b); }
inline bool equalIgnoringCase(const AtomicString& a, const char* b) { return equalIgnoringCase(a.impl(), reinterpret_cast<const LChar*>(b)); }
inline bool equalIgnoringCase(const AtomicString& a, const String& b) { return equalIgnoringCase(a.impl(), b.impl()); }
inline bool equalIgnoringCase(const LChar* a, const AtomicString& b) { return equalIgnoringCase(a, b.impl()); }
inline bool equalIgnoringCase(const char* a, const AtomicString& b) { return equalIgnoringCase(reinterpret_cast<const LChar*>(a), b.impl()); }
inline bool equalIgnoringCase(const String& a, const AtomicString& b) { return equalIgnoringCase(a.impl(), b.impl()); }

inline bool equalIgnoringASCIICase(const AtomicString& a, const AtomicString& b) { return equalIgnoringASCIICase(a.impl(), b.impl()); }
inline bool equalIgnoringASCIICase(const AtomicString& a, const char* b) { return equalIgnoringASCIICase(a.impl(), reinterpret_cast<const LChar*>(b)); }

// Define external global variables for the commonly used atomic strings.
// These are only usable from the main thread.
WTF_EXPORT extern const AtomicString& nullAtom;
WTF_EXPORT extern const AtomicString& emptyAtom;
WTF_EXPORT extern const AtomicString& starAtom;
WTF_EXPORT extern const AtomicString& xmlAtom;
WTF_EXPORT extern const AtomicString& xmlnsAtom;
WTF_EXPORT extern const AtomicString& xlinkAtom;


// AtomicStringHash is the default hash for AtomicString
template<typename T> struct DefaultHash;
template<> struct DefaultHash<AtomicString> {
    typedef AtomicStringHash Hash;
};

// Pretty printer for gtest and base/logging.*.  It prepends and appends
// double-quotes, and escapes chracters other than ASCII printables.
WTF_EXPORT std::ostream& operator<<(std::ostream&, const AtomicString&);

} // namespace WTF

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(AtomicString);

using WTF::AtomicString;
using WTF::nullAtom;
using WTF::emptyAtom;
using WTF::starAtom;
using WTF::xmlAtom;
using WTF::xmlnsAtom;
using WTF::xlinkAtom;

#include "wtf/text/StringConcatenate.h"
#endif // AtomicString_h
