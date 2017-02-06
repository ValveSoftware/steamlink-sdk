// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "CaseMappingHarfBuzzBufferFiller.h"

#include "wtf/text/WTFString.h"

namespace blink {

static const uint16_t* toUint16(const UChar* src)
{
    // FIXME: This relies on undefined behavior however it works on the
    // current versions of all compilers we care about and avoids making
    // a copy of the string.
    static_assert(sizeof(UChar) == sizeof(uint16_t), "UChar should be the same size as uint16_t");
    return reinterpret_cast<const uint16_t*>(src);
}


CaseMappingHarfBuzzBufferFiller::CaseMappingHarfBuzzBufferFiller(
    CaseMapIntend caseMapIntend,
    AtomicString locale,
    hb_buffer_t* harfBuzzBuffer,
    const UChar* buffer,
    unsigned bufferLength,
    unsigned startIndex,
    unsigned numCharacters)
    : m_harfBuzzBuffer(harfBuzzBuffer)
{

    if (caseMapIntend == CaseMapIntend::KeepSameCase) {
        hb_buffer_add_utf16(m_harfBuzzBuffer,
            toUint16(buffer),
            bufferLength,
            startIndex,
            numCharacters);
    } else {
        String caseMappedText;
        if (caseMapIntend == CaseMapIntend::UpperCase) {
            caseMappedText = String(buffer, bufferLength).upper(locale);
        } else {
            caseMappedText = String(buffer, bufferLength).lower(locale);
        }

        if (caseMappedText.length() != bufferLength) {
            fillSlowCase(caseMapIntend, locale, buffer, bufferLength, startIndex, numCharacters);
            return;
        }

        ASSERT(caseMappedText.length() == bufferLength);
        ASSERT(!caseMappedText.is8Bit());
        hb_buffer_add_utf16(m_harfBuzzBuffer, toUint16(caseMappedText.characters16()),
            bufferLength, startIndex, numCharacters);
    }
}

// TODO(drott): crbug.com/623940 Fix lack of context sensitive case mapping here.
void CaseMappingHarfBuzzBufferFiller::fillSlowCase(
    CaseMapIntend caseMapIntend,
    AtomicString locale,
    const UChar* buffer,
    unsigned bufferLength,
    unsigned startIndex,
    unsigned numCharacters)
{
    // Record pre-context.
    hb_buffer_add_utf16(m_harfBuzzBuffer, toUint16(buffer), bufferLength, startIndex, 0);

    for (unsigned charIndex = startIndex; charIndex < startIndex + numCharacters;) {
        unsigned newCharIndex = charIndex;
        U16_FWD_1(buffer, newCharIndex, numCharacters);
        String charByChar(&buffer[charIndex], newCharIndex - charIndex);
        String caseMappedChar;
        if (caseMapIntend == CaseMapIntend::UpperCase)
            caseMappedChar = charByChar.upper(locale);
        else
            caseMappedChar = charByChar.lower(locale);

        for (unsigned j = 0; j < caseMappedChar.length();) {
            UChar32 codepoint = 0;
            U16_NEXT(caseMappedChar.characters16(), j, caseMappedChar.length(), codepoint);
            // Add all characters of the case mapping result at the same cluster position.
            hb_buffer_add(m_harfBuzzBuffer, codepoint, charIndex);
        }
        charIndex = newCharIndex;
    }

    // Record post-context
    hb_buffer_add_utf16(m_harfBuzzBuffer, toUint16(buffer), bufferLength, startIndex + numCharacters, 0);
}

} // namespace blink
