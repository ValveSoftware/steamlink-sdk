/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/text/Character.h"

#include "wtf/StdLibExtras.h"
#include "wtf/text/StringBuilder.h"
#include <algorithm>
#include <unicode/uobject.h>
#include <unicode/uscript.h>

#if defined(USING_SYSTEM_ICU)
#include "platform/text/CharacterPropertyDataGenerator.h"
#include <unicode/uniset.h>
#else
#define MUTEX_H // Prevent compile failure of utrie2.h on Windows
#include <utrie2.h>
#endif

using namespace WTF;
using namespace Unicode;

namespace blink {

#if defined(USING_SYSTEM_ICU)
static icu::UnicodeSet* createUnicodeSet(
    const UChar32* characters, size_t charactersCount,
    const UChar32* ranges, size_t rangesCount)
{
    icu::UnicodeSet* unicodeSet = new icu::UnicodeSet();
    for (size_t i = 0; i < charactersCount; i++)
        unicodeSet->add(characters[i]);
    for (size_t i = 0; i < rangesCount; i += 2)
        unicodeSet->add(ranges[i], ranges[i + 1]);
    unicodeSet->freeze();
    return unicodeSet;
}

#define CREATE_UNICODE_SET(name) \
    createUnicodeSet( \
        name##Array, WTF_ARRAY_LENGTH(name##Array), \
        name##Ranges, WTF_ARRAY_LENGTH(name##Ranges))

#define RETURN_HAS_PROPERTY(c, name) \
    static icu::UnicodeSet* unicodeSet = nullptr; \
    if (!unicodeSet) \
        unicodeSet = CREATE_UNICODE_SET(name); \
    return unicodeSet->contains(c);
#else
// Freezed trie tree, see CharacterDataGenerator.cpp.
extern int32_t serializedCharacterDataSize;
extern uint8_t serializedCharacterData[];

static UTrie2* createTrie()
{
    // Create a Trie from the value array.
    UErrorCode error = U_ZERO_ERROR;
    UTrie2* trie = utrie2_openFromSerialized(
        UTrie2ValueBits::UTRIE2_16_VALUE_BITS,
        serializedCharacterData, serializedCharacterDataSize,
        nullptr, &error);
    ASSERT(error == U_ZERO_ERROR);
    return trie;
}

static bool hasProperty(UChar32 c, CharacterProperty property)
{
    static UTrie2* trie = nullptr;
    if (!trie)
        trie = createTrie();
    return UTRIE2_GET16(trie, c)
        & static_cast<CharacterPropertyType>(property);
}

#define RETURN_HAS_PROPERTY(c, name) \
    return hasProperty(c, CharacterProperty::name);
#endif

// Takes a flattened list of closed intervals
template <class T, size_t size>
bool valueInIntervalList(const T (&intervalList)[size], const T& value)
{
    const T* bound = std::upper_bound(&intervalList[0], &intervalList[size], value);
    if ((bound - intervalList) % 2 == 1)
        return true;
    return bound > intervalList && *(bound - 1) == value;
}

CodePath Character::characterRangeCodePath(const UChar* characters, unsigned len)
{
    static const UChar complexCodePathRanges[] = {
        // U+02E5 through U+02E9 (Modifier Letters : Tone letters)
        0x2E5, 0x2E9,
        // U+0300 through U+036F Combining diacritical marks
        0x300, 0x36F,
        // U+0591 through U+05CF excluding U+05BE Hebrew combining marks, ...
        0x0591, 0x05BD,
        // ... Hebrew punctuation Paseq, Sof Pasuq and Nun Hafukha
        0x05BF, 0x05CF,
        // U+0600 through U+109F Arabic, Syriac, Thaana, NKo, Samaritan, Mandaic,
        // Devanagari, Bengali, Gurmukhi, Gujarati, Oriya, Tamil, Telugu, Kannada,
        // Malayalam, Sinhala, Thai, Lao, Tibetan, Myanmar
        0x0600, 0x109F,
        // U+1100 through U+11FF Hangul Jamo (only Ancient Korean should be left
        // here if you precompose; Modern Korean will be precomposed as a result of step A)
        0x1100, 0x11FF,
        // U+135D through U+135F Ethiopic combining marks
        0x135D, 0x135F,
        // U+1780 through U+18AF Tagalog, Hanunoo, Buhid, Taghanwa,Khmer, Mongolian
        0x1700, 0x18AF,
        // U+1900 through U+194F Limbu (Unicode 4.0)
        0x1900, 0x194F,
        // U+1980 through U+19DF New Tai Lue
        0x1980, 0x19DF,
        // U+1A00 through U+1CFF Buginese, Tai Tham, Balinese, Batak, Lepcha, Vedic
        0x1A00, 0x1CFF,
        // U+1DC0 through U+1DFF Comining diacritical mark supplement
        0x1DC0, 0x1DFF,
        // U+20D0 through U+20FF Combining marks for symbols
        0x20D0, 0x20FF,
        // U+2CEF through U+2CF1 Combining marks for Coptic
        0x2CEF, 0x2CF1,
        // U+302A through U+302F Ideographic and Hangul Tone marks
        0x302A, 0x302F,
        // Combining Katakana-Hiragana Voiced/Semi-voiced Sound Mark
        0x3099, 0x309A,
        // U+A67C through U+A67D Combining marks for old Cyrillic
        0xA67C, 0xA67D,
        // U+A6F0 through U+A6F1 Combining mark for Bamum
        0xA6F0, 0xA6F1,
        // U+A800 through U+ABFF Nagri, Phags-pa, Saurashtra, Devanagari Extended,
        // Hangul Jamo Ext. A, Javanese, Myanmar Extended A, Tai Viet, Meetei Mayek
        0xA800, 0xABFF,
        // U+D7B0 through U+D7FF Hangul Jamo Ext. B
        0xD7B0, 0xD7FF,
        // U+E000..U+F8FF BMP Private Use Area
        0xE000, 0xF8FF,
        // U+FE00 through U+FE0F Unicode variation selectors
        0xFE00, 0xFE0F,
        // U+FE20 through U+FE2F Combining half marks
        0xFE20, 0xFE2F
    };

    CodePath result = SimplePath;
    for (unsigned i = 0; i < len; i++) {
        const UChar c = characters[i];

        // Shortcut for common case
        if (c < 0x2E5)
            continue;

        // Surrogate pairs
        if (c > 0xD7FF && c <= 0xDBFF) {
            if (i == len - 1)
                continue;

            UChar next = characters[++i];
            if (!U16_IS_TRAIL(next))
                continue;

            UChar32 supplementaryCharacter = U16_GET_SUPPLEMENTARY(c, next);

            if (supplementaryCharacter < 0x1F1E6) // U+1F1E6 through U+1F1FF Regional Indicator Symbols
                continue;
            if (supplementaryCharacter <= 0x1F1FF)
                return ComplexPath;

            // Emoji Fitzpatrick modifiers trigger upgrade to complex path for shaping them.
            if (supplementaryCharacter < 0x1F3FB)
                continue;
            if (supplementaryCharacter <= 0x1F3FF)
                return ComplexPath;

            if (supplementaryCharacter == eyeCharacter)
                return ComplexPath;

            // Man and Woman Emojies,
            // in order to support emoji joiner combinations for family and couple pictographs.
            // Compare http://unicode.org/reports/tr51/#Emoji_ZWJ_Sequences
            if (supplementaryCharacter < 0x1F468)
                continue;
            if (supplementaryCharacter <= 0x1F469)
                return ComplexPath;

            if (supplementaryCharacter == leftSpeechBubbleCharacter)
                return ComplexPath;

            if (supplementaryCharacter < 0xE0100) // U+E0100 through U+E01EF Unicode variation selectors.
                continue;
            if (supplementaryCharacter <= 0xE01EF)
                return ComplexPath;

            // Supplemental Private Use Area-A
            if (supplementaryCharacter < 0xF0000)
                continue;
            if (supplementaryCharacter <= 0xFFFFD)
                return ComplexPath;

            // Supplemental Private Use Area-B
            if (supplementaryCharacter < 0x100000)
                continue;
            if (supplementaryCharacter <= 0x10FFFD)
                return ComplexPath;

            // FIXME: Check for Brahmi (U+11000 block), Kaithi (U+11080 block) and other complex scripts
            // in plane 1 or higher.

            continue;
        }

        // Search for other Complex cases
        if (valueInIntervalList(complexCodePathRanges, c))
            return ComplexPath;
    }

    return result;
}

bool Character::isUprightInMixedVertical(UChar32 character)
{
    RETURN_HAS_PROPERTY(character, isUprightInMixedVertical)
}

bool Character::isCJKIdeographOrSymbol(UChar32 c)
{
    // Likely common case
    if (c < 0x2C7)
        return false;

    RETURN_HAS_PROPERTY(c, isCJKIdeographOrSymbol)
}

bool Character::isPotentialCustomElementNameChar(UChar32 character)
{
    RETURN_HAS_PROPERTY(character, isPotentialCustomElementNameChar);
}

unsigned Character::expansionOpportunityCount(const LChar* characters, size_t length, TextDirection direction, bool& isAfterExpansion, const TextJustify textJustify)
{
    unsigned count = 0;
    if (textJustify == TextJustifyDistribute) {
        isAfterExpansion = true;
        return length;
    }

    if (direction == LTR) {
        for (size_t i = 0; i < length; ++i) {
            if (treatAsSpace(characters[i])) {
                count++;
                isAfterExpansion = true;
            } else {
                isAfterExpansion = false;
            }
        }
    } else {
        for (size_t i = length; i > 0; --i) {
            if (treatAsSpace(characters[i - 1])) {
                count++;
                isAfterExpansion = true;
            } else {
                isAfterExpansion = false;
            }
        }
    }

    return count;
}

unsigned Character::expansionOpportunityCount(const UChar* characters, size_t length, TextDirection direction, bool& isAfterExpansion, const TextJustify textJustify)
{
    unsigned count = 0;
    if (direction == LTR) {
        for (size_t i = 0; i < length; ++i) {
            UChar32 character = characters[i];
            if (treatAsSpace(character)) {
                count++;
                isAfterExpansion = true;
                continue;
            }
            if (U16_IS_LEAD(character) && i + 1 < length && U16_IS_TRAIL(characters[i + 1])) {
                character = U16_GET_SUPPLEMENTARY(character, characters[i + 1]);
                i++;
            }
            if (textJustify == TextJustify::TextJustifyAuto && isCJKIdeographOrSymbol(character)) {
                if (!isAfterExpansion)
                    count++;
                count++;
                isAfterExpansion = true;
                continue;
            }
            isAfterExpansion = false;
        }
    } else {
        for (size_t i = length; i > 0; --i) {
            UChar32 character = characters[i - 1];
            if (treatAsSpace(character)) {
                count++;
                isAfterExpansion = true;
                continue;
            }
            if (U16_IS_TRAIL(character) && i > 1 && U16_IS_LEAD(characters[i - 2])) {
                character = U16_GET_SUPPLEMENTARY(characters[i - 2], character);
                i--;
            }
            if (textJustify == TextJustify::TextJustifyAuto && isCJKIdeographOrSymbol(character)) {
                if (!isAfterExpansion)
                    count++;
                count++;
                isAfterExpansion = true;
                continue;
            }
            isAfterExpansion = false;
        }
    }
    return count;
}

bool Character::canReceiveTextEmphasis(UChar32 c)
{
    CharCategory category = Unicode::category(c);
    if (category & (Separator_Space | Separator_Line | Separator_Paragraph | Other_NotAssigned | Other_Control | Other_Format))
        return false;

    // Additional word-separator characters listed in CSS Text Level 3 Editor's Draft 3 November 2010.
    if (c == ethiopicWordspaceCharacter || c == aegeanWordSeparatorLineCharacter || c == aegeanWordSeparatorDotCharacter
        || c == ugariticWordDividerCharacter || c == tibetanMarkIntersyllabicTshegCharacter || c == tibetanMarkDelimiterTshegBstarCharacter)
        return false;

    return true;
}

template <typename CharacterType>
static inline String normalizeSpacesInternal(const CharacterType* characters, unsigned length)
{
    StringBuilder normalized;
    normalized.reserveCapacity(length);

    for (unsigned i = 0; i < length; ++i)
        normalized.append(Character::normalizeSpaces(characters[i]));

    return normalized.toString();
}

String Character::normalizeSpaces(const LChar* characters, unsigned length)
{
    return normalizeSpacesInternal(characters, length);
}

String Character::normalizeSpaces(const UChar* characters, unsigned length)
{
    return normalizeSpacesInternal(characters, length);
}

bool Character::isCommonOrInheritedScript(UChar32 character)
{
    UErrorCode status = U_ZERO_ERROR;
    UScriptCode script = uscript_getScript(character, &status);
    return U_SUCCESS(status) && (script == USCRIPT_COMMON || script == USCRIPT_INHERITED);
}

} // namespace blink
