// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wtf/text/StringToNumber.h"

#include "wtf/ASCIICType.h"
#include "wtf/dtoa.h"
#include "wtf/text/StringImpl.h"

namespace WTF {

static bool isCharacterAllowedInBase(UChar c, int base)
{
    if (c > 0x7F)
        return false;
    if (isASCIIDigit(c))
        return c - '0' < base;
    if (isASCIIAlpha(c)) {
        if (base > 36)
            base = 36;
        return (c >= 'a' && c < 'a' + base - 10)
            || (c >= 'A' && c < 'A' + base - 10);
    }
    return false;
}

template <typename IntegralType, typename CharType>
static inline IntegralType toIntegralType(const CharType* data, size_t length, bool* ok, int base)
{
    static const IntegralType integralMax = std::numeric_limits<IntegralType>::max();
    static const bool isSigned = std::numeric_limits<IntegralType>::is_signed;
    const IntegralType maxMultiplier = integralMax / base;

    IntegralType value = 0;
    bool isOk = false;
    bool isNegative = false;

    if (!data)
        goto bye;

    // skip leading whitespace
    while (length && isSpaceOrNewline(*data)) {
        --length;
        ++data;
    }

    if (isSigned && length && *data == '-') {
        --length;
        ++data;
        isNegative = true;
    } else if (length && *data == '+') {
        --length;
        ++data;
    }

    if (!length || !isCharacterAllowedInBase(*data, base))
        goto bye;

    while (length && isCharacterAllowedInBase(*data, base)) {
        --length;
        IntegralType digitValue;
        CharType c = *data;
        if (isASCIIDigit(c))
            digitValue = c - '0';
        else if (c >= 'a')
            digitValue = c - 'a' + 10;
        else
            digitValue = c - 'A' + 10;

        if (value > maxMultiplier || (value == maxMultiplier && digitValue > (integralMax % base) + isNegative))
            goto bye;

        value = base * value + digitValue;
        ++data;
    }

#if COMPILER(MSVC)
#pragma warning(push, 0)
#pragma warning(disable:4146)
#endif

    if (isNegative)
        value = -value;

#if COMPILER(MSVC)
#pragma warning(pop)
#endif

    // skip trailing space
    while (length && isSpaceOrNewline(*data)) {
        --length;
        ++data;
    }

    if (!length)
        isOk = true;
bye:
    if (ok)
        *ok = isOk;
    return isOk ? value : 0;
}

template <typename CharType>
static unsigned lengthOfCharactersAsInteger(const CharType* data, size_t length)
{
    size_t i = 0;

    // Allow leading spaces.
    for (; i != length; ++i) {
        if (!isSpaceOrNewline(data[i]))
            break;
    }

    // Allow sign.
    if (i != length && (data[i] == '+' || data[i] == '-'))
        ++i;

    // Allow digits.
    for (; i != length; ++i) {
        if (!isASCIIDigit(data[i]))
            break;
    }

    return i;
}

int charactersToIntStrict(const LChar* data, size_t length, bool* ok, int base)
{
    return toIntegralType<int, LChar>(data, length, ok, base);
}

int charactersToIntStrict(const UChar* data, size_t length, bool* ok, int base)
{
    return toIntegralType<int, UChar>(data, length, ok, base);
}

unsigned charactersToUIntStrict(const LChar* data, size_t length, bool* ok, int base)
{
    return toIntegralType<unsigned, LChar>(data, length, ok, base);
}

unsigned charactersToUIntStrict(const UChar* data, size_t length, bool* ok, int base)
{
    return toIntegralType<unsigned, UChar>(data, length, ok, base);
}

int64_t charactersToInt64Strict(const LChar* data, size_t length, bool* ok, int base)
{
    return toIntegralType<int64_t, LChar>(data, length, ok, base);
}

int64_t charactersToInt64Strict(const UChar* data, size_t length, bool* ok, int base)
{
    return toIntegralType<int64_t, UChar>(data, length, ok, base);
}

uint64_t charactersToUInt64Strict(const LChar* data, size_t length, bool* ok, int base)
{
    return toIntegralType<uint64_t, LChar>(data, length, ok, base);
}

uint64_t charactersToUInt64Strict(const UChar* data, size_t length, bool* ok, int base)
{
    return toIntegralType<uint64_t, UChar>(data, length, ok, base);
}

int charactersToInt(const LChar* data, size_t length, bool* ok)
{
    return toIntegralType<int, LChar>(data, lengthOfCharactersAsInteger<LChar>(data, length), ok, 10);
}

int charactersToInt(const UChar* data, size_t length, bool* ok)
{
    return toIntegralType<int, UChar>(data, lengthOfCharactersAsInteger(data, length), ok, 10);
}

unsigned charactersToUInt(const LChar* data, size_t length, bool* ok)
{
    return toIntegralType<unsigned, LChar>(data, lengthOfCharactersAsInteger<LChar>(data, length), ok, 10);
}

unsigned charactersToUInt(const UChar* data, size_t length, bool* ok)
{
    return toIntegralType<unsigned, UChar>(data, lengthOfCharactersAsInteger<UChar>(data, length), ok, 10);
}

int64_t charactersToInt64(const LChar* data, size_t length, bool* ok)
{
    return toIntegralType<int64_t, LChar>(data, lengthOfCharactersAsInteger<LChar>(data, length), ok, 10);
}

int64_t charactersToInt64(const UChar* data, size_t length, bool* ok)
{
    return toIntegralType<int64_t, UChar>(data, lengthOfCharactersAsInteger<UChar>(data, length), ok, 10);
}

uint64_t charactersToUInt64(const LChar* data, size_t length, bool* ok)
{
    return toIntegralType<uint64_t, LChar>(data, lengthOfCharactersAsInteger<LChar>(data, length), ok, 10);
}

uint64_t charactersToUInt64(const UChar* data, size_t length, bool* ok)
{
    return toIntegralType<uint64_t, UChar>(data, lengthOfCharactersAsInteger<UChar>(data, length), ok, 10);
}

enum TrailingJunkPolicy { DisallowTrailingJunk, AllowTrailingJunk };

template <typename CharType, TrailingJunkPolicy policy>
static inline double toDoubleType(const CharType* data, size_t length, bool* ok, size_t& parsedLength)
{
    size_t leadingSpacesLength = 0;
    while (leadingSpacesLength < length && isASCIISpace(data[leadingSpacesLength]))
        ++leadingSpacesLength;

    double number = parseDouble(data + leadingSpacesLength, length - leadingSpacesLength, parsedLength);
    if (!parsedLength) {
        if (ok)
            *ok = false;
        return 0.0;
    }

    parsedLength += leadingSpacesLength;
    if (ok)
        *ok = policy == AllowTrailingJunk || parsedLength == length;
    return number;
}

double charactersToDouble(const LChar* data, size_t length, bool* ok)
{
    size_t parsedLength;
    return toDoubleType<LChar, DisallowTrailingJunk>(data, length, ok, parsedLength);
}

double charactersToDouble(const UChar* data, size_t length, bool* ok)
{
    size_t parsedLength;
    return toDoubleType<UChar, DisallowTrailingJunk>(data, length, ok, parsedLength);
}

double charactersToDouble(const LChar* data, size_t length, size_t& parsedLength)
{
    return toDoubleType<LChar, AllowTrailingJunk>(data, length, nullptr, parsedLength);
}

double charactersToDouble(const UChar* data, size_t length, size_t& parsedLength)
{
    return toDoubleType<UChar, AllowTrailingJunk>(data, length, nullptr, parsedLength);
}

float charactersToFloat(const LChar* data, size_t length, bool* ok)
{
    // FIXME: This will return ok even when the string fits into a double but
    // not a float.
    size_t parsedLength;
    return static_cast<float>(toDoubleType<LChar, DisallowTrailingJunk>(data, length, ok, parsedLength));
}

float charactersToFloat(const UChar* data, size_t length, bool* ok)
{
    // FIXME: This will return ok even when the string fits into a double but
    // not a float.
    size_t parsedLength;
    return static_cast<float>(toDoubleType<UChar, DisallowTrailingJunk>(data, length, ok, parsedLength));
}

float charactersToFloat(const LChar* data, size_t length, size_t& parsedLength)
{
    // FIXME: This will return ok even when the string fits into a double but
    // not a float.
    return static_cast<float>(toDoubleType<LChar, AllowTrailingJunk>(data, length, 0, parsedLength));
}

float charactersToFloat(const UChar* data, size_t length, size_t& parsedLength)
{
    // FIXME: This will return ok even when the string fits into a double but
    // not a float.
    return static_cast<float>(toDoubleType<UChar, AllowTrailingJunk>(data, length, 0, parsedLength));
}

} // namespace WTF
