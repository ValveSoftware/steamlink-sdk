// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WTF_StringView_h
#define WTF_StringView_h

#include "wtf/Allocator.h"
#include "wtf/GetPtr.h"
#if DCHECK_IS_ON()
#include "wtf/RefPtr.h"
#endif
#include "wtf/text/AtomicString.h"
#include "wtf/text/StringImpl.h"
#include "wtf/text/Unicode.h"
#include "wtf/text/WTFString.h"
#include <cstring>

namespace WTF {

// A string like object that wraps either an 8bit or 16bit byte sequence
// and keeps track of the length and the type, it does NOT own the bytes.
//
// Since StringView does not own the bytes creating a StringView from a String,
// then calling clear() on the String will result in a use-after-free. Asserts
// in ~StringView attempt to enforce this for most common cases.
//
// See base/strings/string_piece.h for more details.
class WTF_EXPORT StringView {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
public:
    // Null string.
    StringView() { clear(); }

    // From a StringView:
    StringView(const StringView&, unsigned offset, unsigned length);
    StringView(const StringView& view, unsigned offset)
        : StringView(view, offset, view.m_length - offset) {}

    // From a StringImpl:
    StringView(StringImpl*);
    StringView(StringImpl*, unsigned offset);
    StringView(StringImpl*, unsigned offset, unsigned length);

    // From a String:
    StringView(const String& string, unsigned offset, unsigned length)
        : StringView(string.impl(), offset, length) {}
    StringView(const String& string, unsigned offset)
        : StringView(string.impl(), offset) {}
    StringView(const String& string)
        : StringView(string.impl()) {}

    // From an AtomicString:
    StringView(const AtomicString& string, unsigned offset, unsigned length)
        : StringView(string.impl(), offset, length) {}
    StringView(const AtomicString& string, unsigned offset)
        : StringView(string.impl(), offset) {}
    StringView(const AtomicString& string)
        : StringView(string.impl()) {}

    // From a literal string or LChar buffer:
    StringView(const LChar* chars, unsigned length)
        : StringView(reinterpret_cast<const void*>(chars), length, true) {}
    StringView(const char* chars, unsigned length)
        : StringView(reinterpret_cast<const LChar*>(chars), length) {}
    StringView(const LChar* chars)
        : StringView(chars, chars ? strlen(reinterpret_cast<const char*>(chars)) : 0) {}
    StringView(const char* chars)
        : StringView(reinterpret_cast<const LChar*>(chars)) {}

    // From a wide literal string or UChar buffer.
    StringView(const UChar* chars, unsigned length)
        : StringView(reinterpret_cast<const void*>(chars), length, false) {}
    StringView(const UChar* chars);

    // From a byte pointer.
    StringView(const void* bytes, unsigned length, bool is8Bit)
        : m_impl(is8Bit ? StringImpl::empty() : StringImpl::empty16Bit())
        , m_bytes(bytes)
        , m_length(length) {}

#if DCHECK_IS_ON()
    ~StringView();
#endif

    bool isNull() const { return !m_bytes; }
    bool isEmpty() const { return !m_length; }

    unsigned length() const { return m_length; }

    bool is8Bit() const { DCHECK(m_impl); return m_impl->is8Bit(); }

    void clear();

    UChar operator[](unsigned i) const
    {
        SECURITY_DCHECK(i < length());
        if (is8Bit())
            return characters8()[i];
        return characters16()[i];
    }

    const LChar* characters8() const
    {
        DCHECK(is8Bit());
        return m_characters8;
    }

    const UChar* characters16() const
    {
        DCHECK(!is8Bit());
        return m_characters16;
    }

    const void* bytes() const { return m_bytes; }

    // This is not named impl() like String because it has different semantics.
    // String::impl() is never null if String::isNull() is false. For StringView
    // sharedImpl() can be null if the StringView was created with a non-zero
    // offset, or a length that made it shorter than the underlying impl.
    StringImpl* sharedImpl() const
    {
        // If this StringView is backed by a StringImpl, and was constructed
        // with a zero offset and the same length we can just access the impl
        // directly since this == StringView(m_impl).
        if (m_impl->bytes() == bytes() && m_length == m_impl->length())
            return getPtr(m_impl);
        return nullptr;
    }

    String toString() const;
    AtomicString toAtomicString() const;

private:
    void set(StringImpl&, unsigned offset, unsigned length);

    // We use the StringImpl to mark for 8bit or 16bit, even for strings where
    // we were constructed from a char pointer. So m_impl->bytes() might have
    // nothing to do with this view's bytes().
#if DCHECK_IS_ON()
    RefPtr<StringImpl> m_impl;
#else
    StringImpl* m_impl;
#endif
    union {
        const LChar* m_characters8;
        const UChar* m_characters16;
        const void* m_bytes;
    };
    unsigned m_length;
};

inline StringView::StringView(const StringView& view, unsigned offset, unsigned length)
    : m_impl(view.m_impl)
    , m_length(length)
{
    SECURITY_DCHECK(offset + length <= view.length());
    if (is8Bit())
        m_characters8 = view.characters8() + offset;
    else
        m_characters16 = view.characters16() + offset;
}

inline StringView::StringView(StringImpl* impl)
{
    if (!impl) {
        clear();
        return;
    }
    m_impl = impl;
    m_length = impl->length();
    m_bytes = impl->bytes();
}

inline StringView::StringView(StringImpl* impl, unsigned offset)
{
    impl ? set(*impl, offset, impl->length() - offset) : clear();
}

inline StringView::StringView(StringImpl* impl, unsigned offset, unsigned length)
{
    impl ? set(*impl, offset, length) : clear();
}

inline void StringView::clear()
{
    m_length = 0;
    m_bytes = nullptr;
    m_impl = StringImpl::empty(); // mark as 8 bit.
}

inline void StringView::set(StringImpl& impl, unsigned offset, unsigned length)
{
    SECURITY_DCHECK(offset + length <= impl.length());
    m_length = length;
    m_impl = &impl;
    if (impl.is8Bit())
        m_characters8 = impl.characters8() + offset;
    else
        m_characters16 = impl.characters16() + offset;
}

WTF_EXPORT bool equalIgnoringASCIICase(const StringView& a, const StringView& b);

// TODO(esprehn): Can't make this an overload of WTF::equal since that makes
// calls to equal() that pass literal strings ambiguous. Figure out if we can
// replace all the callers with equalStringView and then rename it to equal().
WTF_EXPORT bool equalStringView(const StringView&, const StringView&);

inline bool operator==(const StringView& a, const StringView& b)
{
    return equalStringView(a, b);
}

inline bool operator!=(const StringView& a, const StringView& b)
{
    return !(a == b);
}

} // namespace WTF

using WTF::StringView;

#endif
