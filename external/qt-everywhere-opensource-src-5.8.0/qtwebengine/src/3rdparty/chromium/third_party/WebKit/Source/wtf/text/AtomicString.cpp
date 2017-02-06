/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "wtf/text/AtomicString.h"

#include "wtf/dtoa.h"
#include "wtf/text/AtomicStringTable.h"
#include "wtf/text/IntegerToStringConversion.h"

namespace WTF {

static_assert(sizeof(AtomicString) == sizeof(String), "AtomicString and String must be same size");

AtomicString::AtomicString(const LChar* chars, unsigned length)
    : m_string(AtomicStringTable::instance().add(chars, length)) {}

AtomicString::AtomicString(const UChar* chars, unsigned length)
    : m_string(AtomicStringTable::instance().add(chars, length)) {}

AtomicString::AtomicString(const UChar* chars, unsigned length, unsigned existingHash)
    : m_string(AtomicStringTable::instance().add(chars, length, existingHash)) {}

AtomicString::AtomicString(const UChar* chars)
    : m_string(AtomicStringTable::instance().add(chars)) {}

AtomicString::AtomicString(StringImpl* string, unsigned offset, unsigned length)
    : m_string(AtomicStringTable::instance().add(string, offset, length)) {}

PassRefPtr<StringImpl> AtomicString::addSlowCase(StringImpl* chars)
{
    DCHECK(!chars->isAtomic());
    return AtomicStringTable::instance().add(chars);
}

AtomicString AtomicString::fromUTF8(const char* chars, size_t length)
{
    if (!chars)
        return nullAtom;
    if (!length)
        return emptyAtom;
    return AtomicString(AtomicStringTable::instance().addUTF8(chars, chars + length));
}

AtomicString AtomicString::fromUTF8(const char* chars)
{
    if (!chars)
        return nullAtom;
    if (!*chars)
        return emptyAtom;
    return AtomicString(AtomicStringTable::instance().addUTF8(chars, nullptr));
}

AtomicString AtomicString::lower() const
{
    // Note: This is a hot function in the Dromaeo benchmark.
    StringImpl* impl = this->impl();
    if (UNLIKELY(!impl))
        return *this;
    RefPtr<StringImpl> newImpl = impl->lower();
    if (LIKELY(newImpl == impl))
        return *this;
    return AtomicString(newImpl.release());
}

AtomicString AtomicString::lowerASCII() const
{
    StringImpl* impl = this->impl();
    if (UNLIKELY(!impl))
        return *this;
    RefPtr<StringImpl> newImpl = impl->lowerASCII();
    if (LIKELY(newImpl == impl))
        return *this;
    return AtomicString(newImpl.release());
}

template<typename IntegerType>
static AtomicString integerToAtomicString(IntegerType input)
{
    IntegerToStringConverter<IntegerType> converter(input);
    return AtomicString(converter.characters8(), converter.length());
}

AtomicString AtomicString::number(int number)
{
    return integerToAtomicString(number);
}

AtomicString AtomicString::number(unsigned number)
{
    return integerToAtomicString(number);
}

AtomicString AtomicString::number(long number)
{
    return integerToAtomicString(number);
}

AtomicString AtomicString::number(unsigned long number)
{
    return integerToAtomicString(number);
}

AtomicString AtomicString::number(long long number)
{
    return integerToAtomicString(number);
}

AtomicString AtomicString::number(unsigned long long number)
{
    return integerToAtomicString(number);
}

AtomicString AtomicString::number(double number, unsigned precision, TrailingZerosTruncatingPolicy trailingZerosTruncatingPolicy)
{
    NumberToStringBuffer buffer;
    return AtomicString(numberToFixedPrecisionString(number, precision, buffer, trailingZerosTruncatingPolicy == TruncateTrailingZeros));
}

std::ostream& operator<<(std::ostream& out, const AtomicString& s)
{
    return out << s.getString();
}

#ifndef NDEBUG
void AtomicString::show() const
{
    m_string.show();
}
#endif

} // namespace WTF
