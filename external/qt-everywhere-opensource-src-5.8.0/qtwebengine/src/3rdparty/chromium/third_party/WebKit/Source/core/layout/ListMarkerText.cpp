/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
 * Copyright (C) 2010 Daniel Bates (dbates@intudata.com)
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

#include "core/layout/ListMarkerText.h"

#include "core/layout/TextRunConstructor.h"
#include "wtf/text/CharacterNames.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

namespace ListMarkerText {

enum SequenceType { NumericSequence, AlphabeticSequence };

static String toRoman(int number, bool upper)
{
    // FIXME: CSS3 describes how to make this work for much larger numbers,
    // using overbars and special characters. It also specifies the characters
    // in the range U+2160 to U+217F instead of standard ASCII ones.
    ASSERT(number >= 1 && number <= 3999);

    // Big enough to store largest roman number less than 3999 which
    // is 3888 (MMMDCCCLXXXVIII)
    const int lettersSize = 15;
    LChar letters[lettersSize];

    int length = 0;
    const LChar ldigits[] = { 'i', 'v', 'x', 'l', 'c', 'd', 'm' };
    const LChar udigits[] = { 'I', 'V', 'X', 'L', 'C', 'D', 'M' };
    const LChar* digits = upper ? udigits : ldigits;
    int d = 0;
    do {
        int num = number % 10;
        if (num % 5 < 4)
            for (int i = num % 5; i > 0; i--)
                letters[lettersSize - ++length] = digits[d];
        if (num >= 4 && num <= 8)
            letters[lettersSize - ++length] = digits[d + 1];
        if (num == 9)
            letters[lettersSize - ++length] = digits[d + 2];
        if (num % 5 == 4)
            letters[lettersSize - ++length] = digits[d];
        number /= 10;
        d += 2;
    } while (number);

    ASSERT(length <= lettersSize);
    return String(&letters[lettersSize - length], length);
}

// The typedef is needed because taking sizeof(number) in the const expression below doesn't work with some compilers.
// This is likely the case because of the template.
typedef int numberType;

template <typename CharacterType>
static inline String toAlphabeticOrNumeric(numberType number, const CharacterType* sequence, unsigned sequenceSize, SequenceType type)
{
    ASSERT(sequenceSize >= 2);

    const int lettersSize = sizeof(numberType) * 8 + 1; // Binary is the worst case; requires one character per bit plus a minus sign.

    CharacterType letters[lettersSize];

    bool isNegativeNumber = false;
    unsigned numberShadow = number;
    if (type == AlphabeticSequence) {
        ASSERT(number > 0);
        --numberShadow;
    } else if (number < 0) {
        numberShadow = -number;
        isNegativeNumber = true;
    }
    letters[lettersSize - 1] = sequence[numberShadow % sequenceSize];
    int length = 1;

    if (type == AlphabeticSequence) {
        while ((numberShadow /= sequenceSize) > 0) {
            --numberShadow;
            letters[lettersSize - ++length] = sequence[numberShadow % sequenceSize];
        }
    } else {
        while ((numberShadow /= sequenceSize) > 0)
            letters[lettersSize - ++length] = sequence[numberShadow % sequenceSize];
    }
    if (isNegativeNumber)
        letters[lettersSize - ++length] = hyphenMinusCharacter;

    ASSERT(length <= lettersSize);
    return String(&letters[lettersSize - length], length);
}

template <typename CharacterType>
static String toSymbolic(int number, const CharacterType* symbols, unsigned symbolsSize)
{
    ASSERT(number > 0);
    ASSERT(symbolsSize >= 1);
    unsigned numberShadow = number;
    --numberShadow;

    // The asterisks list-style-type is the worst case; we show |numberShadow| asterisks.
    StringBuilder letters;
    letters.append(symbols[numberShadow % symbolsSize]);
    unsigned numSymbols = numberShadow / symbolsSize;
    while (numSymbols--)
        letters.append(symbols[numberShadow % symbolsSize]);
    return letters.toString();
}

template <typename CharacterType>
static String toAlphabetic(int number, const CharacterType* alphabet, unsigned alphabetSize)
{
    return toAlphabeticOrNumeric(number, alphabet, alphabetSize, AlphabeticSequence);
}

template <typename CharacterType>
static String toNumeric(int number, const CharacterType* numerals, unsigned numeralsSize)
{
    return toAlphabeticOrNumeric(number, numerals, numeralsSize, NumericSequence);
}

template <typename CharacterType, size_t size>
static inline String toAlphabetic(int number, const CharacterType(&alphabet)[size])
{
    return toAlphabetic(number, alphabet, size);
}

template <typename CharacterType, size_t size>
static inline String toNumeric(int number, const CharacterType(&alphabet)[size])
{
    return toNumeric(number, alphabet, size);
}

template <typename CharacterType, size_t size>
static inline String toSymbolic(int number, const CharacterType(&alphabet)[size])
{
    return toSymbolic(number, alphabet, size);
}

static void toHebrewUnder1000(int number, Vector<UChar>& letters)
{
    // FIXME: CSS3 mentions various refinements not implemented here.
    // FIXME: Should take a look at Mozilla's HebrewToText function (in nsBulletFrame).
    ASSERT(number >= 0 && number < 1000);
    int fourHundreds = number / 400;
    for (int i = 0; i < fourHundreds; i++)
        letters.prepend(1511 + 3);
    number %= 400;
    if (number / 100)
        letters.prepend(1511 + (number / 100) - 1);
    number %= 100;
    if (number == 15 || number == 16) {
        letters.prepend(1487 + 9);
        letters.prepend(1487 + number - 9);
    } else {
        if (int tens = number / 10) {
            static const UChar hebrewTens[9] = { 1497, 1499, 1500, 1502, 1504, 1505, 1506, 1508, 1510 };
            letters.prepend(hebrewTens[tens - 1]);
        }
        if (int ones = number % 10)
            letters.prepend(1487 + ones);
    }
}

static String toHebrew(int number)
{
    // FIXME: CSS3 mentions ways to make this work for much larger numbers.
    ASSERT(number >= 0 && number <= 999999);

    if (number == 0) {
        static const UChar hebrewZero[3] = { 0x05E1, 0x05E4, 0x05D0 };
        return String(hebrewZero, 3);
    }

    Vector<UChar> letters;
    if (number > 999) {
        toHebrewUnder1000(number / 1000, letters);
        letters.prepend('\'');
        number = number % 1000;
    }
    toHebrewUnder1000(number, letters);
    return String(letters);
}

static int toArmenianUnder10000(int number, bool upper, bool addCircumflex, UChar letters[9])
{
    ASSERT(number >= 0 && number < 10000);
    int length = 0;

    int lowerOffset = upper ? 0 : 0x0030;

    if (int thousands = number / 1000) {
        if (thousands == 7) {
            letters[length++] = 0x0552 + lowerOffset;
            if (addCircumflex)
                letters[length++] = 0x0302;
        } else {
            letters[length++] = (0x054C - 1 + lowerOffset) + thousands;
            if (addCircumflex)
                letters[length++] = 0x0302;
        }
    }

    if (int hundreds = (number / 100) % 10) {
        letters[length++] = (0x0543 - 1 + lowerOffset) + hundreds;
        if (addCircumflex)
            letters[length++] = 0x0302;
    }

    if (int tens = (number / 10) % 10) {
        letters[length++] = (0x053A - 1 + lowerOffset) + tens;
        if (addCircumflex)
            letters[length++] = 0x0302;
    }

    if (int ones = number % 10) {
        letters[length++] = (0x531 - 1 + lowerOffset) + ones;
        if (addCircumflex)
            letters[length++] = 0x0302;
    }

    return length;
}

static String toArmenian(int number, bool upper)
{
    ASSERT(number >= 1 && number <= 99999999);

    const int lettersSize = 18; // twice what toArmenianUnder10000 needs
    UChar letters[lettersSize];

    int length = toArmenianUnder10000(number / 10000, upper, true, letters);
    length += toArmenianUnder10000(number % 10000, upper, false, letters + length);

    ASSERT(length <= lettersSize);
    return String(letters, length);
}

static String toGeorgian(int number)
{
    ASSERT(number >= 1 && number <= 19999);

    const int lettersSize = 5;
    UChar letters[lettersSize];

    int length = 0;

    if (number > 9999)
        letters[length++] = 0x10F5;

    if (int thousands = (number / 1000) % 10) {
        static const UChar georgianThousands[9] = {
            0x10E9, 0x10EA, 0x10EB, 0x10EC, 0x10ED, 0x10EE, 0x10F4, 0x10EF, 0x10F0
        };
        letters[length++] = georgianThousands[thousands - 1];
    }

    if (int hundreds = (number / 100) % 10) {
        static const UChar georgianHundreds[9] = {
            0x10E0, 0x10E1, 0x10E2, 0x10F3, 0x10E4, 0x10E5, 0x10E6, 0x10E7, 0x10E8
        };
        letters[length++] = georgianHundreds[hundreds - 1];
    }

    if (int tens = (number / 10) % 10) {
        static const UChar georgianTens[9] = {
            0x10D8, 0x10D9, 0x10DA, 0x10DB, 0x10DC, 0x10F2, 0x10DD, 0x10DE, 0x10DF
        };
        letters[length++] = georgianTens[tens - 1];
    }

    if (int ones = number % 10) {
        static const UChar georgianOnes[9] = {
            0x10D0, 0x10D1, 0x10D2, 0x10D3, 0x10D4, 0x10D5, 0x10D6, 0x10F1, 0x10D7
        };
        letters[length++] = georgianOnes[ones - 1];
    }

    ASSERT(length <= lettersSize);
    return String(letters, length);
}

enum CJKLang {
    Chinese = 1,
    Korean,
    Japanese
};

enum CJKStyle {
    Formal,
    Informal
};

// The table uses the order from the CSS3 specification:
// first 3 group markers, then 3 digit markers, then ten digits, then negative symbols.
static String toCJKIdeographic(int number, const UChar table[26], CJKStyle cjkStyle)
{
    enum AbstractCJKChar {
        NoChar = 0,
        Lang = 0,
        // FourthGroupMarker for simplified chinese has two codepoints, to simplify
        // the main algorithm below use two codepoints for all group markers.
        SecondGroupMarker = 1, ThirdGroupMarker = 3, FourthGroupMarker = 5,
        SecondDigitMarker = 7, ThirdDigitMarker, FourthDigitMarker,
        Digit0, Digit1, Digit2, Digit3, Digit4,
        Digit5, Digit6, Digit7, Digit8, Digit9,
        Neg1, Neg2, Neg3, Neg4, Neg5
    };

    if (number == 0)
        return String(&table[Digit0], 1);

    const bool negative = number < 0;
    if (negative)
        number = -number;

    const int groupLength = 9; // 4 digits, 3 digit markers, group marker of size 2.
    const int bufferLength = 4 * groupLength;
    AbstractCJKChar buffer[bufferLength] = { NoChar };

    for (int i = 0; i < 4; ++i) {
        int groupValue = number % 10000;
        number /= 10000;

        // Process least-significant group first, but put it in the buffer last.
        AbstractCJKChar* group = &buffer[(3 - i) * groupLength];

        if (groupValue && i) {
            group[8] = static_cast<AbstractCJKChar>(SecondGroupMarker + i);
            group[7] = static_cast<AbstractCJKChar>(SecondGroupMarker - 1 + i);
        }

        // Put in the four digits and digit markers for any non-zero digits.
        int digitValue = (groupValue % 10);
        bool trailingZero = table[Lang] == Chinese && !digitValue;
        if (digitValue) {
            bool dropOne = table[Lang] == Korean && cjkStyle == Informal && digitValue == 1 && i > 0;
            if (!dropOne)
                group[6] = static_cast<AbstractCJKChar>(Digit0 + (groupValue % 10));
        }
        if (number != 0 || groupValue > 9) {
            digitValue = ((groupValue / 10) % 10);
            bool dropOne = table[Lang] == Korean && cjkStyle == Informal && digitValue == 1;
            if ((digitValue && !dropOne) || (!digitValue && !trailingZero))
                group[4] = static_cast<AbstractCJKChar>(Digit0 + digitValue);
            trailingZero &= !digitValue;
            if (digitValue)
                group[5] = SecondDigitMarker;
        }
        if (number != 0 || groupValue > 99) {
            digitValue = ((groupValue / 100) % 10);
            bool dropOne = table[Lang] == Korean && cjkStyle == Informal && digitValue == 1;
            if ((digitValue && !dropOne) || (!digitValue && !trailingZero))
                group[2] = static_cast<AbstractCJKChar>(Digit0 + digitValue);
            trailingZero &= !digitValue;
            if (digitValue)
                group[3] = ThirdDigitMarker;
        }
        if (number != 0 || groupValue > 999) {
            digitValue = groupValue / 1000;
            bool dropOne = table[Lang] == Korean && cjkStyle == Informal && digitValue == 1;
            if ((digitValue && !dropOne) || (!digitValue && !trailingZero))
                group[0] = static_cast<AbstractCJKChar>(Digit0 + digitValue);
            if (digitValue)
                group[1] = FourthDigitMarker;
        }

        if (trailingZero && i > 0) {
            group[6] = group[7];
            group[7] = group[8];
            group[8] = Digit0;
        }

        // Remove the tens digit, but leave the marker, for any group that has
        // a value of less than 20.
        if (table[Lang] == Chinese && cjkStyle == Informal && groupValue < 20) {
            ASSERT(group[4] == NoChar || group[4] == Digit0 || group[4] == Digit1);
            group[4] = NoChar;
        }

        if (number == 0)
            break;
    }

    // Convert into characters, omitting consecutive runs of Digit0 and
    // any trailing Digit0.
    int length = 0;
    const int maxLengthForNegativeSymbols = 5;
    UChar characters[bufferLength + maxLengthForNegativeSymbols];
    AbstractCJKChar last = NoChar;
    if (negative) {
        while (UChar a = table[Neg1 + length])
            characters[length++] = a;
    }
    for (int i = 0; i < bufferLength; ++i) {
        AbstractCJKChar a = buffer[i];
        if (a != NoChar) {
            if (a != Digit0 || (table[Lang] == Chinese && last != Digit0)) {
                UChar newChar = table[a];
                if (newChar != NoChar) {
                    characters[length++] = table[a];
                    if (table[Lang] == Korean && (a == SecondGroupMarker || a == ThirdGroupMarker || a == FourthGroupMarker))
                        characters[length++] = ' ';
                }
            }
            last = a;
        }
    }
    if ((table[Lang] == Chinese && last == Digit0) || characters[length - 1] == ' ')
        --length;

    return String(characters, length);
}

static EListStyleType effectiveListMarkerType(EListStyleType type, int count)
{
    // Note, the following switch statement has been explicitly grouped
    // by list-style-type ordinal range.
    switch (type) {
    case ArabicIndic:
    case Bengali:
    case Cambodian:
    case Circle:
    case DecimalLeadingZero:
    case DecimalListStyle:
    case Devanagari:
    case Disc:
    case Gujarati:
    case Gurmukhi:
    case Kannada:
    case Khmer:
    case Lao:
    case Malayalam:
    case Mongolian:
    case Myanmar:
    case NoneListStyle:
    case Oriya:
    case Persian:
    case Square:
    case Telugu:
    case Thai:
    case Tibetan:
    case Urdu:
    case KoreanHangulFormal:
    case KoreanHanjaFormal:
    case KoreanHanjaInformal:
    case CJKIdeographic:
    case SimpChineseFormal:
    case SimpChineseInformal:
    case TradChineseFormal:
    case TradChineseInformal:
        return type; // Can represent all ordinals.
    case Armenian:
    case LowerArmenian:
    case UpperArmenian:
        return (count < 1 || count > 99999999) ? DecimalListStyle : type;
    case Georgian:
        return (count < 1 || count > 19999) ? DecimalListStyle : type;
    case Hebrew:
        return (count < 0 || count > 999999) ? DecimalListStyle : type;
    case LowerRoman:
    case UpperRoman:
        return (count < 1 || count > 3999) ? DecimalListStyle : type;
    case CjkEarthlyBranch:
    case CjkHeavenlyStem:
    case EthiopicHalehameAm:
    case EthiopicHalehame:
    case EthiopicHalehameTiEr:
    case EthiopicHalehameTiEt:
    case Hangul:
    case HangulConsonant:
    case Hiragana:
    case HiraganaIroha:
    case Katakana:
    case KatakanaIroha:
    case LowerAlpha:
    case LowerGreek:
    case LowerLatin:
    case UpperAlpha:
    case UpperLatin:
        return (count < 1) ? DecimalListStyle : type;
    }

    ASSERT_NOT_REACHED();
    return type;
}

UChar suffix(EListStyleType type, int count)
{
    // If the list-style-type cannot represent |count| because it's outside its
    // ordinal range then we fall back to some list style that can represent |count|.
    EListStyleType effectiveType = effectiveListMarkerType(type, count);

    // Note, the following switch statement has been explicitly
    // grouped by list-style-type suffix.
    switch (effectiveType) {
    case Circle:
    case Disc:
    case NoneListStyle:
    case Square:
        return ' ';
    case EthiopicHalehame:
    case EthiopicHalehameAm:
    case EthiopicHalehameTiEr:
    case EthiopicHalehameTiEt:
        return ethiopicPrefaceColonCharacter;
    case Armenian:
    case ArabicIndic:
    case Bengali:
    case Cambodian:
    case CJKIdeographic:
    case CjkEarthlyBranch:
    case CjkHeavenlyStem:
    case DecimalLeadingZero:
    case DecimalListStyle:
    case Devanagari:
    case Georgian:
    case Gujarati:
    case Gurmukhi:
    case Hangul:
    case HangulConsonant:
    case Hebrew:
    case Hiragana:
    case HiraganaIroha:
    case Kannada:
    case Katakana:
    case KatakanaIroha:
    case Khmer:
    case Lao:
    case LowerAlpha:
    case LowerArmenian:
    case LowerGreek:
    case LowerLatin:
    case LowerRoman:
    case Malayalam:
    case Mongolian:
    case Myanmar:
    case Oriya:
    case Persian:
    case Telugu:
    case Thai:
    case Tibetan:
    case UpperAlpha:
    case UpperArmenian:
    case UpperLatin:
    case UpperRoman:
    case Urdu:
        return '.';
    case SimpChineseFormal:
    case SimpChineseInformal:
    case TradChineseFormal:
    case TradChineseInformal:
    case KoreanHangulFormal:
    case KoreanHanjaFormal:
    case KoreanHanjaInformal:
        return 0x3001;
    }

    ASSERT_NOT_REACHED();
    return '.';
}

String text(EListStyleType type, int count)
{
    // If the list-style-type, say hebrew, cannot represent |count| because it's outside
    // its ordinal range then we fallback to some list style that can represent |count|.
    switch (effectiveListMarkerType(type, count)) {
    case NoneListStyle:
        return "";

    // We use the same characters for text security.
    // See LayoutText::setInternalString.
    case Circle:
        return String(&whiteBulletCharacter, 1);
    case Disc:
        return String(&bulletCharacter, 1);
    case Square:
        // The CSS 2.1 test suite uses U+25EE BLACK MEDIUM SMALL SQUARE
        // instead, but I think this looks better.
        return String(&blackSquareCharacter, 1);

    case DecimalListStyle:
        return String::number(count);
    case DecimalLeadingZero:
        if (count < -9 || count > 9)
            return String::number(count);
        if (count < 0)
            return "-0" + String::number(-count); // -01 to -09
        return "0" + String::number(count); // 00 to 09

    case ArabicIndic: {
        static const UChar arabicIndicNumerals[10] = {
            0x0660, 0x0661, 0x0662, 0x0663, 0x0664, 0x0665, 0x0666, 0x0667, 0x0668, 0x0669
        };
        return toNumeric(count, arabicIndicNumerals);
    }
    case Bengali: {
        static const UChar bengaliNumerals[10] = {
            0x09E6, 0x09E7, 0x09E8, 0x09E9, 0x09EA, 0x09EB, 0x09EC, 0x09ED, 0x09EE, 0x09EF
        };
        return toNumeric(count, bengaliNumerals);
    }
    case Cambodian:
    case Khmer: {
        static const UChar khmerNumerals[10] = {
            0x17E0, 0x17E1, 0x17E2, 0x17E3, 0x17E4, 0x17E5, 0x17E6, 0x17E7, 0x17E8, 0x17E9
        };
        return toNumeric(count, khmerNumerals);
    }
    case Devanagari: {
        static const UChar devanagariNumerals[10] = {
            0x0966, 0x0967, 0x0968, 0x0969, 0x096A, 0x096B, 0x096C, 0x096D, 0x096E, 0x096F
        };
        return toNumeric(count, devanagariNumerals);
    }
    case Gujarati: {
        static const UChar gujaratiNumerals[10] = {
            0x0AE6, 0x0AE7, 0x0AE8, 0x0AE9, 0x0AEA, 0x0AEB, 0x0AEC, 0x0AED, 0x0AEE, 0x0AEF
        };
        return toNumeric(count, gujaratiNumerals);
    }
    case Gurmukhi: {
        static const UChar gurmukhiNumerals[10] = {
            0x0A66, 0x0A67, 0x0A68, 0x0A69, 0x0A6A, 0x0A6B, 0x0A6C, 0x0A6D, 0x0A6E, 0x0A6F
        };
        return toNumeric(count, gurmukhiNumerals);
    }
    case Kannada: {
        static const UChar kannadaNumerals[10] = {
            0x0CE6, 0x0CE7, 0x0CE8, 0x0CE9, 0x0CEA, 0x0CEB, 0x0CEC, 0x0CED, 0x0CEE, 0x0CEF
        };
        return toNumeric(count, kannadaNumerals);
    }
    case Lao: {
        static const UChar laoNumerals[10] = {
            0x0ED0, 0x0ED1, 0x0ED2, 0x0ED3, 0x0ED4, 0x0ED5, 0x0ED6, 0x0ED7, 0x0ED8, 0x0ED9
        };
        return toNumeric(count, laoNumerals);
    }
    case Malayalam: {
        static const UChar malayalamNumerals[10] = {
            0x0D66, 0x0D67, 0x0D68, 0x0D69, 0x0D6A, 0x0D6B, 0x0D6C, 0x0D6D, 0x0D6E, 0x0D6F
        };
        return toNumeric(count, malayalamNumerals);
    }
    case Mongolian: {
        static const UChar mongolianNumerals[10] = {
            0x1810, 0x1811, 0x1812, 0x1813, 0x1814, 0x1815, 0x1816, 0x1817, 0x1818, 0x1819
        };
        return toNumeric(count, mongolianNumerals);
    }
    case Myanmar: {
        static const UChar myanmarNumerals[10] = {
            0x1040, 0x1041, 0x1042, 0x1043, 0x1044, 0x1045, 0x1046, 0x1047, 0x1048, 0x1049
        };
        return toNumeric(count, myanmarNumerals);
    }
    case Oriya: {
        static const UChar oriyaNumerals[10] = {
            0x0B66, 0x0B67, 0x0B68, 0x0B69, 0x0B6A, 0x0B6B, 0x0B6C, 0x0B6D, 0x0B6E, 0x0B6F
        };
        return toNumeric(count, oriyaNumerals);
    }
    case Persian:
    case Urdu: {
        static const UChar urduNumerals[10] = {
            0x06F0, 0x06F1, 0x06F2, 0x06F3, 0x06F4, 0x06F5, 0x06F6, 0x06F7, 0x06F8, 0x06F9
        };
        return toNumeric(count, urduNumerals);
    }
    case Telugu: {
        static const UChar teluguNumerals[10] = {
            0x0C66, 0x0C67, 0x0C68, 0x0C69, 0x0C6A, 0x0C6B, 0x0C6C, 0x0C6D, 0x0C6E, 0x0C6F
        };
        return toNumeric(count, teluguNumerals);
    }
    case Tibetan: {
        static const UChar tibetanNumerals[10] = {
            0x0F20, 0x0F21, 0x0F22, 0x0F23, 0x0F24, 0x0F25, 0x0F26, 0x0F27, 0x0F28, 0x0F29
        };
        return toNumeric(count, tibetanNumerals);
    }
    case Thai: {
        static const UChar thaiNumerals[10] = {
            0x0E50, 0x0E51, 0x0E52, 0x0E53, 0x0E54, 0x0E55, 0x0E56, 0x0E57, 0x0E58, 0x0E59
        };
        return toNumeric(count, thaiNumerals);
    }

    case LowerAlpha:
    case LowerLatin: {
        static const LChar lowerLatinAlphabet[26] = {
            'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
            'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'
        };
        return toAlphabetic(count, lowerLatinAlphabet);
    }
    case UpperAlpha:
    case UpperLatin: {
        static const LChar upperLatinAlphabet[26] = {
            'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
            'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'
        };
        return toAlphabetic(count, upperLatinAlphabet);
    }
    case LowerGreek: {
        static const UChar lowerGreekAlphabet[24] = {
            0x03B1, 0x03B2, 0x03B3, 0x03B4, 0x03B5, 0x03B6, 0x03B7, 0x03B8,
            0x03B9, 0x03BA, 0x03BB, 0x03BC, 0x03BD, 0x03BE, 0x03BF, 0x03C0,
            0x03C1, 0x03C3, 0x03C4, 0x03C5, 0x03C6, 0x03C7, 0x03C8, 0x03C9
        };
        return toAlphabetic(count, lowerGreekAlphabet);
    }

    case Hiragana: {
        // FIXME: This table comes from the CSS3 draft, and is probably
        // incorrect, given the comments in that draft.
        static const UChar hiraganaAlphabet[48] = {
            0x3042, 0x3044, 0x3046, 0x3048, 0x304A, 0x304B, 0x304D, 0x304F,
            0x3051, 0x3053, 0x3055, 0x3057, 0x3059, 0x305B, 0x305D, 0x305F,
            0x3061, 0x3064, 0x3066, 0x3068, 0x306A, 0x306B, 0x306C, 0x306D,
            0x306E, 0x306F, 0x3072, 0x3075, 0x3078, 0x307B, 0x307E, 0x307F,
            0x3080, 0x3081, 0x3082, 0x3084, 0x3086, 0x3088, 0x3089, 0x308A,
            0x308B, 0x308C, 0x308D, 0x308F, 0x3090, 0x3091, 0x3092, 0x3093
        };
        return toAlphabetic(count, hiraganaAlphabet);
    }
    case HiraganaIroha: {
        // FIXME: This table comes from the CSS3 draft, and is probably
        // incorrect, given the comments in that draft.
        static const UChar hiraganaIrohaAlphabet[47] = {
            0x3044, 0x308D, 0x306F, 0x306B, 0x307B, 0x3078, 0x3068, 0x3061,
            0x308A, 0x306C, 0x308B, 0x3092, 0x308F, 0x304B, 0x3088, 0x305F,
            0x308C, 0x305D, 0x3064, 0x306D, 0x306A, 0x3089, 0x3080, 0x3046,
            0x3090, 0x306E, 0x304A, 0x304F, 0x3084, 0x307E, 0x3051, 0x3075,
            0x3053, 0x3048, 0x3066, 0x3042, 0x3055, 0x304D, 0x3086, 0x3081,
            0x307F, 0x3057, 0x3091, 0x3072, 0x3082, 0x305B, 0x3059
        };
        return toAlphabetic(count, hiraganaIrohaAlphabet);
    }
    case Katakana: {
        // FIXME: This table comes from the CSS3 draft, and is probably
        // incorrect, given the comments in that draft.
        static const UChar katakanaAlphabet[48] = {
            0x30A2, 0x30A4, 0x30A6, 0x30A8, 0x30AA, 0x30AB, 0x30AD, 0x30AF,
            0x30B1, 0x30B3, 0x30B5, 0x30B7, 0x30B9, 0x30BB, 0x30BD, 0x30BF,
            0x30C1, 0x30C4, 0x30C6, 0x30C8, 0x30CA, 0x30CB, 0x30CC, 0x30CD,
            0x30CE, 0x30CF, 0x30D2, 0x30D5, 0x30D8, 0x30DB, 0x30DE, 0x30DF,
            0x30E0, 0x30E1, 0x30E2, 0x30E4, 0x30E6, 0x30E8, 0x30E9, 0x30EA,
            0x30EB, 0x30EC, 0x30ED, 0x30EF, 0x30F0, 0x30F1, 0x30F2, 0x30F3
        };
        return toAlphabetic(count, katakanaAlphabet);
    }
    case KatakanaIroha: {
        // FIXME: This table comes from the CSS3 draft, and is probably
        // incorrect, given the comments in that draft.
        static const UChar katakanaIrohaAlphabet[47] = {
            0x30A4, 0x30ED, 0x30CF, 0x30CB, 0x30DB, 0x30D8, 0x30C8, 0x30C1,
            0x30EA, 0x30CC, 0x30EB, 0x30F2, 0x30EF, 0x30AB, 0x30E8, 0x30BF,
            0x30EC, 0x30BD, 0x30C4, 0x30CD, 0x30CA, 0x30E9, 0x30E0, 0x30A6,
            0x30F0, 0x30CE, 0x30AA, 0x30AF, 0x30E4, 0x30DE, 0x30B1, 0x30D5,
            0x30B3, 0x30A8, 0x30C6, 0x30A2, 0x30B5, 0x30AD, 0x30E6, 0x30E1,
            0x30DF, 0x30B7, 0x30F1, 0x30D2, 0x30E2, 0x30BB, 0x30B9
        };
        return toAlphabetic(count, katakanaIrohaAlphabet);
    }

    case CjkEarthlyBranch: {
        static const UChar cjkEarthlyBranchAlphabet[12] = {
            0x5B50, 0x4E11, 0x5BC5, 0x536F, 0x8FB0, 0x5DF3, 0x5348, 0x672A, 0x7533,
            0x9149, 0x620C, 0x4EA5
        };
        return toAlphabetic(count, cjkEarthlyBranchAlphabet);
    }
    case CjkHeavenlyStem: {
        static const UChar cjkHeavenlyStemAlphabet[10] = {
            0x7532, 0x4E59, 0x4E19, 0x4E01, 0x620A, 0x5DF1, 0x5E9A, 0x8F9B, 0x58EC,
            0x7678
        };
        return toAlphabetic(count, cjkHeavenlyStemAlphabet);
    }
    case HangulConsonant: {
        static const UChar hangulConsonantAlphabet[14] = {
            0x3131, 0x3134, 0x3137, 0x3139, 0x3141, 0x3142, 0x3145, 0x3147, 0x3148,
            0x314A, 0x314B, 0x314C, 0x314D, 0x314E
        };
        return toAlphabetic(count, hangulConsonantAlphabet);
    }
    case Hangul: {
        static const UChar hangulAlphabet[14] = {
            0xAC00, 0xB098, 0xB2E4, 0xB77C, 0xB9C8, 0xBC14, 0xC0AC, 0xC544, 0xC790,
            0xCC28, 0xCE74, 0xD0C0, 0xD30C, 0xD558
        };
        return toAlphabetic(count, hangulAlphabet);
    }
    case EthiopicHalehame: {
        static const UChar ethiopicHalehameGezAlphabet[26] = {
            0x1200, 0x1208, 0x1210, 0x1218, 0x1220, 0x1228, 0x1230, 0x1240, 0x1260,
            0x1270, 0x1280, 0x1290, 0x12A0, 0x12A8, 0x12C8, 0x12D0, 0x12D8, 0x12E8,
            0x12F0, 0x1308, 0x1320, 0x1330, 0x1338, 0x1340, 0x1348, 0x1350
        };
        return toAlphabetic(count, ethiopicHalehameGezAlphabet);
    }
    case EthiopicHalehameAm: {
        static const UChar ethiopicHalehameAmAlphabet[33] = {
            0x1200, 0x1208, 0x1210, 0x1218, 0x1220, 0x1228, 0x1230, 0x1238, 0x1240,
            0x1260, 0x1270, 0x1278, 0x1280, 0x1290, 0x1298, 0x12A0, 0x12A8, 0x12B8,
            0x12C8, 0x12D0, 0x12D8, 0x12E0, 0x12E8, 0x12F0, 0x1300, 0x1308, 0x1320,
            0x1328, 0x1330, 0x1338, 0x1340, 0x1348, 0x1350
        };
        return toAlphabetic(count, ethiopicHalehameAmAlphabet);
    }
    case EthiopicHalehameTiEr: {
        static const UChar ethiopicHalehameTiErAlphabet[31] = {
            0x1200, 0x1208, 0x1210, 0x1218, 0x1228, 0x1230, 0x1238, 0x1240, 0x1250,
            0x1260, 0x1270, 0x1278, 0x1290, 0x1298, 0x12A0, 0x12A8, 0x12B8, 0x12C8,
            0x12D0, 0x12D8, 0x12E0, 0x12E8, 0x12F0, 0x1300, 0x1308, 0x1320, 0x1328,
            0x1330, 0x1338, 0x1348, 0x1350
        };
        return toAlphabetic(count, ethiopicHalehameTiErAlphabet);
    }
    case EthiopicHalehameTiEt: {
        static const UChar ethiopicHalehameTiEtAlphabet[34] = {
            0x1200, 0x1208, 0x1210, 0x1218, 0x1220, 0x1228, 0x1230, 0x1238, 0x1240,
            0x1250, 0x1260, 0x1270, 0x1278, 0x1280, 0x1290, 0x1298, 0x12A0, 0x12A8,
            0x12B8, 0x12C8, 0x12D0, 0x12D8, 0x12E0, 0x12E8, 0x12F0, 0x1300, 0x1308,
            0x1320, 0x1328, 0x1330, 0x1338, 0x1340, 0x1348, 0x1350
        };
        return toAlphabetic(count, ethiopicHalehameTiEtAlphabet);
    }
    case KoreanHangulFormal: {
        static const UChar koreanHangulFormalTable[26] = {
            Korean,
            0xB9CC, 0x0000, 0xC5B5, 0x0000, 0xC870, 0x0000,
            0xC2ED, 0xBC31, 0xCC9C,
            0xC601, 0xC77C, 0xC774, 0xC0BC, 0xC0AC,
            0xC624, 0xC721, 0xCE60, 0xD314, 0xAD6C,
            0xB9C8, 0xC774, 0xB108, 0xC2A4, 0x0020, 0x0000
        };
        return toCJKIdeographic(count, koreanHangulFormalTable, Formal);
    }
    case KoreanHanjaFormal: {
        static const UChar koreanHanjaFormalTable[26] = {
            Korean,
            0x842C, 0x0000, 0x5104, 0x0000, 0x5146, 0x0000,
            0x62FE, 0x767E, 0x4EDF,
            0x96F6, 0x58F9, 0x8CB3, 0x53C3, 0x56DB,
            0x4E94, 0x516D, 0x4E03, 0x516B, 0x4E5D,
            0xB9C8, 0xC774, 0xB108, 0xC2A4, 0x0020, 0x0000
        };
        return toCJKIdeographic(count, koreanHanjaFormalTable, Formal);
    }
    case KoreanHanjaInformal: {
        static const UChar koreanHanjaInformalTable[26] = {
            Korean,
            0x842C, 0x0000, 0x5104, 0x0000, 0x5146, 0x0000,
            0x5341, 0x767E, 0x5343,
            0x96F6, 0x4E00, 0x4E8C, 0x4E09, 0x56DB,
            0x4E94, 0x516D, 0x4E03, 0x516B, 0x4E5D,
            0xB9C8, 0xC774, 0xB108, 0xC2A4, 0x0020, 0x0000
        };
        return toCJKIdeographic(count, koreanHanjaInformalTable, Informal);
    }
    case CJKIdeographic:
    case TradChineseInformal: {
        static const UChar traditionalChineseInformalTable[22] = {
            Chinese,
            0x842C, 0x0000, 0x5104, 0x0000, 0x5146, 0x0000,
            0x5341, 0x767E, 0x5343,
            0x96F6, 0x4E00, 0x4E8C, 0x4E09, 0x56DB,
            0x4E94, 0x516D, 0x4E03, 0x516B, 0x4E5D,
            0x8CA0, 0x0000
        };
        return toCJKIdeographic(count, traditionalChineseInformalTable, Informal);
    }
    case SimpChineseInformal: {
        static const UChar simpleChineseInformalTable[22] = {
            Chinese,
            0x4E07, 0x0000, 0x4EBF, 0x0000, 0x4E07, 0x4EBF,
            0x5341, 0x767E, 0x5343,
            0x96F6, 0x4E00, 0x4E8C, 0x4E09, 0x56DB,
            0x4E94, 0x516D, 0x4E03, 0x516B, 0x4E5D,
            0x8D1F, 0x0000
        };
        return toCJKIdeographic(count, simpleChineseInformalTable, Informal);
    }
    case TradChineseFormal: {
        static const UChar traditionalChineseFormalTable[22] = {
            Chinese,
            0x842C, 0x0000, 0x5104, 0x0000, 0x5146, 0x0000,
            0x62FE, 0x4F70, 0x4EDF,
            0x96F6, 0x58F9, 0x8CB3, 0x53C3, 0x8086,
            0x4F0D, 0x9678, 0x67D2, 0x634C, 0x7396,
            0x8CA0, 0x0000
        };
        return toCJKIdeographic(count, traditionalChineseFormalTable, Formal);
    }
    case SimpChineseFormal: {
        static const UChar simpleChineseFormalTable[22] = {
            Chinese,
            0x4E07, 0x0000, 0x4EBF, 0x0000, 0x4E07, 0x4EBF,
            0x62FE, 0x4F70, 0x4EDF,
            0x96F6, 0x58F9, 0x8D30, 0x53C1, 0x8086,
            0x4F0D, 0x9646, 0x67D2, 0x634C, 0x7396,
            0x8D1F, 0x0000
        };
        return toCJKIdeographic(count, simpleChineseFormalTable, Formal);
    }

    case LowerRoman:
        return toRoman(count, false);
    case UpperRoman:
        return toRoman(count, true);

    case Armenian:
    case UpperArmenian:
        // CSS3 says "armenian" means "lower-armenian".
        // But the CSS2.1 test suite contains uppercase test results for "armenian",
        // so we'll match the test suite.
        return toArmenian(count, true);
    case LowerArmenian:
        return toArmenian(count, false);
    case Georgian:
        return toGeorgian(count);
    case Hebrew:
        return toHebrew(count);
    }

    ASSERT_NOT_REACHED();
    return "";
}

} // namespace ListMarkerText

} // namespace blink
