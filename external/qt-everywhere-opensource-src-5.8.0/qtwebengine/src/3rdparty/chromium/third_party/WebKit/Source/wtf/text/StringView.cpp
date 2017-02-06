// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wtf/text/StringView.h"

namespace WTF {

StringView::StringView(const UChar* chars)
    : StringView(chars, chars ? lengthOfNullTerminatedString(chars) : 0) {}

#if DCHECK_IS_ON()
StringView::~StringView()
{
    DCHECK(m_impl);
    DCHECK(!m_impl->hasOneRef())
        << "StringView does not own the StringImpl, it must not have the last ref.";
}
#endif

String StringView::toString() const
{
    if (isNull())
        return String();
    if (isEmpty())
        return emptyString();
    if (StringImpl* impl = sharedImpl())
        return impl;
    if (is8Bit())
        return String(characters8(), m_length);
    return StringImpl::create8BitIfPossible(characters16(), m_length);
}

AtomicString StringView::toAtomicString() const
{
    if (isNull())
        return nullAtom;
    if (isEmpty())
        return emptyAtom;
    if (StringImpl* impl = sharedImpl())
        return AtomicString(impl);
    if (is8Bit())
        return AtomicString(characters8(), m_length);
    return AtomicString(characters16(), m_length);
}

bool equalStringView(const StringView& a, const StringView& b)
{
    if (a.isNull() || b.isNull())
        return a.isNull() == b.isNull();
    if (a.length() != b.length())
        return false;
    if (a.is8Bit()) {
        if (b.is8Bit())
            return equal(a.characters8(), b.characters8(), a.length());
        return equal(a.characters8(), b.characters16(), a.length());
    }
    if (b.is8Bit())
        return equal(a.characters16(), b.characters8(), a.length());
    return equal(a.characters16(), b.characters16(), a.length());
}

bool equalIgnoringASCIICase(const StringView& a, const StringView& b)
{
    if (a.isNull() || b.isNull())
        return a.isNull() == b.isNull();
    if (a.length() != b.length())
        return false;
    if (a.is8Bit()) {
        if (b.is8Bit())
            return equalIgnoringASCIICase(a.characters8(), b.characters8(), a.length());
        return equalIgnoringASCIICase(a.characters8(), b.characters16(), a.length());
    }
    if (b.is8Bit())
        return equalIgnoringASCIICase(a.characters16(), b.characters8(), a.length());
    return equalIgnoringASCIICase(a.characters16(), b.characters16(), a.length());
}

} // namespace WTF
