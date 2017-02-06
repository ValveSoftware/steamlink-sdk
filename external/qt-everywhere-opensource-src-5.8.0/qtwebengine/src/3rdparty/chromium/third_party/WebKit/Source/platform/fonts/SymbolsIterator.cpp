// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "SymbolsIterator.h"

#include "wtf/PtrUtil.h"
#include <unicode/uchar.h>
#include <unicode/uniset.h>

namespace blink {

using namespace WTF::Unicode;

SymbolsIterator::SymbolsIterator(const UChar* buffer, unsigned bufferSize)
    : m_utf16Iterator(wrapUnique(new UTF16TextIterator(buffer, bufferSize)))
    , m_bufferSize(bufferSize)
    , m_nextChar(0)
    , m_atEnd(bufferSize == 0)
    , m_currentFontFallbackPriority(FontFallbackPriority::Invalid)
{
}

FontFallbackPriority SymbolsIterator::fontFallbackPriorityForCharacter(UChar32 codepoint)
{

    // Those should only be Emoji presentation as combinations of two.
    if (Character::isEmojiKeycapBase(codepoint)
        || Character::isRegionalIndicator(codepoint))
        return FontFallbackPriority::Text;

    if (codepoint == combiningEnclosingKeycapCharacter
        || codepoint == combiningEnclosingCircleBackslashCharacter)
        return FontFallbackPriority::EmojiEmoji;

    if (Character::isEmojiEmojiDefault(codepoint)
        || Character::isEmojiModifierBase(codepoint)
        || Character::isModifier(codepoint))
        return FontFallbackPriority::EmojiEmoji;

    if (Character::isEmojiTextDefault(codepoint))
        return FontFallbackPriority::EmojiText;

    UBlockCode block = ublock_getCode(codepoint);

    switch (block) {
    case UBLOCK_PLAYING_CARDS:
    case UBLOCK_MISCELLANEOUS_SYMBOLS:
    case UBLOCK_MISCELLANEOUS_SYMBOLS_AND_ARROWS:
    case UBLOCK_MISCELLANEOUS_SYMBOLS_AND_PICTOGRAPHS:
    case UBLOCK_TRANSPORT_AND_MAP_SYMBOLS:
    case UBLOCK_ALCHEMICAL_SYMBOLS:
    case UBLOCK_RUNIC:
    case UBLOCK_DINGBATS:
    case UBLOCK_GOTHIC:
        return FontFallbackPriority::Symbols;
    case UBLOCK_ARROWS:
    case UBLOCK_MATHEMATICAL_OPERATORS:
    case UBLOCK_MISCELLANEOUS_TECHNICAL:
    case UBLOCK_GEOMETRIC_SHAPES:
    case UBLOCK_MISCELLANEOUS_MATHEMATICAL_SYMBOLS_A:
    case UBLOCK_SUPPLEMENTAL_ARROWS_A:
    case UBLOCK_SUPPLEMENTAL_ARROWS_B:
    case UBLOCK_MISCELLANEOUS_MATHEMATICAL_SYMBOLS_B:
    case UBLOCK_SUPPLEMENTAL_MATHEMATICAL_OPERATORS:
    case UBLOCK_MATHEMATICAL_ALPHANUMERIC_SYMBOLS:
    case UBLOCK_ARABIC_MATHEMATICAL_ALPHABETIC_SYMBOLS:
    case UBLOCK_GEOMETRIC_SHAPES_EXTENDED:
        return FontFallbackPriority::Math;
    default:
        return FontFallbackPriority::Text;
    }
}

bool SymbolsIterator::consume(unsigned *symbolsLimit, FontFallbackPriority* fontFallbackPriority)
{
    if (m_atEnd)
        return false;

    while (m_utf16Iterator->consume(m_nextChar)) {
        m_previousFontFallbackPriority = m_currentFontFallbackPriority;
        unsigned iteratorOffset = m_utf16Iterator->offset();
        m_utf16Iterator->advance();

        // Except at the beginning, ZWJ just carries over the emoji or neutral
        // text type, VS15 & VS16 we just carry over as well, since we already
        // resolved those through lookahead.  Also, the text presentation emoji
        // are upgraded to emoji presentation when combined through ZWJ in the
        // case of example U+1F441 U+200D U+1F5E8, eye + ZWJ + left speech
        // bubble, see below.
        if ((!(m_nextChar == zeroWidthJoinerCharacter
            && m_previousFontFallbackPriority == FontFallbackPriority::EmojiEmoji)
            && m_nextChar != variationSelector15Character
            && m_nextChar != variationSelector16Character
            && !Character::isRegionalIndicator(m_nextChar)
            && !(m_nextChar == leftSpeechBubbleCharacter
            && m_previousFontFallbackPriority == FontFallbackPriority::EmojiEmoji))
            || m_currentFontFallbackPriority == FontFallbackPriority::Invalid) {
            m_currentFontFallbackPriority = fontFallbackPriorityForCharacter(m_nextChar);
        }

        UChar32 peekChar = 0;
        if (m_utf16Iterator->consume(peekChar)
            && peekChar != 0) {

            // Variation Selectors
            if (m_currentFontFallbackPriority == FontFallbackPriority::EmojiEmoji
                && peekChar == variationSelector15Character) {
                m_currentFontFallbackPriority = FontFallbackPriority::EmojiText;
            }

            if (m_currentFontFallbackPriority == FontFallbackPriority::EmojiText
                && peekChar == variationSelector16Character) {
                m_currentFontFallbackPriority = FontFallbackPriority::EmojiEmoji;
            }

            // Combining characters Keycap...
            if (Character::isEmojiKeycapBase(m_nextChar)
                && peekChar == combiningEnclosingKeycapCharacter) {
                m_currentFontFallbackPriority = FontFallbackPriority::EmojiEmoji;
            };

            // ...and Combining Enclosing Circle Backslash.
            if (m_currentFontFallbackPriority == FontFallbackPriority::EmojiText
                && peekChar == combiningEnclosingCircleBackslashCharacter) {
                m_currentFontFallbackPriority = FontFallbackPriority::EmojiEmoji;
            }

            // Regional indicators
            if (Character::isRegionalIndicator(m_nextChar)
                && Character::isRegionalIndicator(peekChar)) {
                m_currentFontFallbackPriority = FontFallbackPriority::EmojiEmoji;
            }

            // Upgrade text presentation emoji to emoji presentation when followed by ZWJ,
            // Example U+1F441 U+200D U+1F5E8, eye + ZWJ + left speech bubble.
            if (m_nextChar == eyeCharacter && peekChar == zeroWidthJoinerCharacter) {
                m_currentFontFallbackPriority = FontFallbackPriority::EmojiEmoji;
            }
        }

        if (m_previousFontFallbackPriority != m_currentFontFallbackPriority
            && (m_previousFontFallbackPriority != FontFallbackPriority::Invalid)) {
            *symbolsLimit = iteratorOffset;
            *fontFallbackPriority = m_previousFontFallbackPriority;
            return true;
        }
    }
    *symbolsLimit = m_bufferSize;
    *fontFallbackPriority = m_currentFontFallbackPriority;
    m_atEnd = true;
    return true;
}

} // namespace blink
