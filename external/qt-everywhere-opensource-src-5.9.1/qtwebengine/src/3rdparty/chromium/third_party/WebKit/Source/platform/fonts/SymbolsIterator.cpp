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
    : m_utf16Iterator(makeUnique<UTF16TextIterator>(buffer, bufferSize)),
      m_bufferSize(bufferSize),
      m_nextChar(0),
      m_atEnd(bufferSize == 0),
      m_currentFontFallbackPriority(FontFallbackPriority::Invalid) {}

FontFallbackPriority SymbolsIterator::fontFallbackPriorityForCharacter(
    UChar32 codepoint) {
  // Those should only be Emoji presentation as combinations of two.
  if (Character::isEmojiKeycapBase(codepoint) ||
      Character::isRegionalIndicator(codepoint))
    return FontFallbackPriority::Text;

  if (codepoint == combiningEnclosingKeycapCharacter ||
      codepoint == combiningEnclosingCircleBackslashCharacter)
    return FontFallbackPriority::EmojiEmoji;

  if (Character::isEmojiEmojiDefault(codepoint) ||
      Character::isEmojiModifierBase(codepoint) ||
      Character::isModifier(codepoint))
    return FontFallbackPriority::EmojiEmoji;

  if (Character::isEmojiTextDefault(codepoint))
    return FontFallbackPriority::EmojiText;

  // Here we could segment into Symbols and Math categories as well, similar
  // to what the Windows font fallback does. Map the math Unicode and Symbols
  // blocks to Text for now since we don't have a good cross-platform way to
  // select suitable math fonts.
  return FontFallbackPriority::Text;
}

bool SymbolsIterator::consume(unsigned* symbolsLimit,
                              FontFallbackPriority* fontFallbackPriority) {
  if (m_atEnd)
    return false;

  while (m_utf16Iterator->consume(m_nextChar)) {
    m_previousFontFallbackPriority = m_currentFontFallbackPriority;
    unsigned iteratorOffset = m_utf16Iterator->offset();
    m_utf16Iterator->advance();

    // Except at the beginning, ZWJ just carries over the emoji or neutral
    // text type, VS15 & VS16 we just carry over as well, since we already
    // resolved those through lookahead. Also, don't downgrade to text
    // presentation for emoji that are part of a ZWJ sequence, example
    // U+1F441 U+200D U+1F5E8, eye (text presentation) + ZWJ + left speech
    // bubble, see below.
    if ((!(m_nextChar == zeroWidthJoinerCharacter &&
           m_previousFontFallbackPriority ==
               FontFallbackPriority::EmojiEmoji) &&
         m_nextChar != variationSelector15Character &&
         m_nextChar != variationSelector16Character &&
         !Character::isRegionalIndicator(m_nextChar) &&
         !((m_nextChar == leftSpeechBubbleCharacter ||
            m_nextChar == rainbowCharacter || m_nextChar == maleSignCharacter ||
            m_nextChar == femaleSignCharacter ||
            m_nextChar == staffOfAesculapiusCharacter) &&
           m_previousFontFallbackPriority ==
               FontFallbackPriority::EmojiEmoji)) ||
        m_currentFontFallbackPriority == FontFallbackPriority::Invalid) {
      m_currentFontFallbackPriority =
          fontFallbackPriorityForCharacter(m_nextChar);
    }

    UChar32 peekChar = 0;
    if (m_utf16Iterator->consume(peekChar) && peekChar != 0) {
      // Variation Selectors
      if (m_currentFontFallbackPriority == FontFallbackPriority::EmojiEmoji &&
          peekChar == variationSelector15Character) {
        m_currentFontFallbackPriority = FontFallbackPriority::EmojiText;
      }

      if (m_currentFontFallbackPriority == FontFallbackPriority::EmojiText &&
          peekChar == variationSelector16Character) {
        m_currentFontFallbackPriority = FontFallbackPriority::EmojiEmoji;
      }

      // Combining characters Keycap...
      if (Character::isEmojiKeycapBase(m_nextChar) &&
          peekChar == combiningEnclosingKeycapCharacter) {
        m_currentFontFallbackPriority = FontFallbackPriority::EmojiEmoji;
      };

      // ...and Combining Enclosing Circle Backslash.
      if (m_currentFontFallbackPriority == FontFallbackPriority::EmojiText &&
          peekChar == combiningEnclosingCircleBackslashCharacter) {
        m_currentFontFallbackPriority = FontFallbackPriority::EmojiEmoji;
      }

      // Regional indicators
      if (Character::isRegionalIndicator(m_nextChar) &&
          Character::isRegionalIndicator(peekChar)) {
        m_currentFontFallbackPriority = FontFallbackPriority::EmojiEmoji;
      }

      // Upgrade text presentation emoji to emoji presentation when followed by
      // ZWJ, Example U+1F441 U+200D U+1F5E8, eye + ZWJ + left speech bubble.
      if ((m_nextChar == eyeCharacter ||
           m_nextChar == wavingWhiteFlagCharacter) &&
          peekChar == zeroWidthJoinerCharacter) {
        m_currentFontFallbackPriority = FontFallbackPriority::EmojiEmoji;
      }
    }

    if (m_previousFontFallbackPriority != m_currentFontFallbackPriority &&
        (m_previousFontFallbackPriority != FontFallbackPriority::Invalid)) {
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

}  // namespace blink
