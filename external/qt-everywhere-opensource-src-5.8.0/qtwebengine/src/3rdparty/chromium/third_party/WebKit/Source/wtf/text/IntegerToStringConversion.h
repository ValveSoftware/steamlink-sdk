/*
 * Copyright (C) 2012 Apple Inc. All Rights Reserved.
 * Copyright (C) 2012 Patrick Gansterer <paroga@paroga.com>
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

#ifndef IntegerToStringConversion_h
#define IntegerToStringConversion_h

#include "base/numerics/safe_conversions.h"
#include "wtf/StdLibExtras.h"
#include <limits>
#include <type_traits>

namespace WTF {

// TODO(esprehn): See if we can generalize IntToStringT in
// base/strings/string_number_conversions.cc, and use unsigned type expansion
// optimization here instead of CheckedNumeric::UnsignedAbs().
template<typename IntegerType>
class IntegerToStringConverter {
public:
    static_assert(std::is_integral<IntegerType>::value,
        "IntegerType must be a type of integer.");

    explicit IntegerToStringConverter(IntegerType input)
    {
        LChar* end = m_buffer + WTF_ARRAY_LENGTH(m_buffer);
        m_begin = end;

        // We need to switch to the unsigned type when negating the value since
        // abs(INT_MIN) == INT_MAX + 1.
        bool isNegative = base::IsValueNegative(input);
        UnsignedIntegerType value = isNegative ? 0u-input : input;

        do {
            --m_begin;
            ASSERT(m_begin != m_buffer);
            *m_begin = static_cast<LChar>((value % 10) + '0');
            value /= 10;
        } while (value);

        if (isNegative) {
            --m_begin;
            ASSERT(m_begin != m_buffer);
            *m_begin = static_cast<LChar>('-');
        }

        m_length = static_cast<unsigned>(end - m_begin);
    }

    const LChar* characters8() const { return m_begin; }
    unsigned length() const { return m_length; }

private:
    using UnsignedIntegerType = typename std::make_unsigned<IntegerType>::type;
    static const size_t kBufferSize = 3 * sizeof(UnsignedIntegerType) +
        std::numeric_limits<IntegerType>::is_signed;

    LChar m_buffer[kBufferSize];
    LChar* m_begin;
    unsigned m_length;
};

} // namespace WTF

#endif // IntegerToStringConversion_h
