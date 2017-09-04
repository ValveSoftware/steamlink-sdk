// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/state_machines/StateMachineUtil.h"

#include "platform/text/Character.h"
#include "wtf/Assertions.h"
#include "wtf/text/CharacterNames.h"
#include "wtf/text/Unicode.h"

namespace blink {

namespace {

// Returns true if the code point has E_Basae_GAZ grapheme break property.
// See
// http://www.unicode.org/Public/9.0.0/ucd/auxiliary/GraphemeBreakProperty-9.0.0d18.txt
bool isEBaseGAZ(uint32_t codePoint) {
  return codePoint == WTF::Unicode::boyCharacter ||
         codePoint == WTF::Unicode::girlCharacter ||
         codePoint == WTF::Unicode::manCharacter ||
         codePoint == WTF::Unicode::womanCharacter;
}

// The list of code points which has Indic_Syllabic_Category=Virama property.
// Must be sorted.
// See http://www.unicode.org/Public/9.0.0/ucd/IndicSyllabicCategory-9.0.0d2.txt
const uint32_t kIndicSyllabicCategoryViramaList[] = {
    0x094D,  0x09CD,  0x0A4D,  0x0ACD,  0x0B4D,  0x0BCD,  0x0C4D,  0x0CCD,
    0x0D4D,  0x0DCA,  0x1B44,  0xA8C4,  0xA9C0,  0x11046, 0x110B9, 0x111C0,
    0x11235, 0x1134D, 0x11442, 0x114C2, 0x115BF, 0x1163F, 0x116B6, 0x11C3F,
};

// Returns true if the code point has Indic_Syllabic_Category=Virama property.
// See http://www.unicode.org/Public/9.0.0/ucd/IndicSyllabicCategory-9.0.0d2.txt
bool isIndicSyllabicCategoryVirama(uint32_t codePoint) {
  const int length = WTF_ARRAY_LENGTH(kIndicSyllabicCategoryViramaList);
  return std::binary_search(kIndicSyllabicCategoryViramaList,
                            kIndicSyllabicCategoryViramaList + length,
                            codePoint);
}

}  // namespace

bool isGraphemeBreak(UChar32 prevCodePoint, UChar32 nextCodePoint) {
  // The following breaking rules come from Unicode Standard Annex #29 on
  // Unicode Text Segmaentation. See http://www.unicode.org/reports/tr29/
  // Note that some of rules are in proposal.
  // Also see http://www.unicode.org/reports/tr29/proposed.html
  int prevProp =
      u_getIntPropertyValue(prevCodePoint, UCHAR_GRAPHEME_CLUSTER_BREAK);
  int nextProp =
      u_getIntPropertyValue(nextCodePoint, UCHAR_GRAPHEME_CLUSTER_BREAK);

  // Rule1 GB1 sot ÷
  // Rule2 GB2 ÷ eot
  // Should be handled by caller.

  // Rule GB3, CR x LF
  if (prevProp == U_GCB_CR && nextProp == U_GCB_LF)
    return false;

  // Rule GB4, (Control | CR | LF) ÷
  if (prevProp == U_GCB_CONTROL || prevProp == U_GCB_CR || prevProp == U_GCB_LF)
    return true;

  // Rule GB5, ÷ (Control | CR | LF)
  if (nextProp == U_GCB_CONTROL || nextProp == U_GCB_CR || nextProp == U_GCB_LF)
    return true;

  // Rule GB6, L x (L | V | LV | LVT)
  if (prevProp == U_GCB_L && (nextProp == U_GCB_L || nextProp == U_GCB_V ||
                              nextProp == U_GCB_LV || nextProp == U_GCB_LVT))
    return false;

  // Rule GB7, (LV | V) x (V | T)
  if ((prevProp == U_GCB_LV || prevProp == U_GCB_V) &&
      (nextProp == U_GCB_V || nextProp == U_GCB_T))
    return false;

  // Rule GB8, (LVT | T) x T
  if ((prevProp == U_GCB_LVT || prevProp == U_GCB_T) && nextProp == U_GCB_T)
    return false;

  // Rule GB8a
  //
  // sot   (RI RI)* RI x RI
  // [^RI] (RI RI)* RI x RI
  //                RI ÷ RI
  if (Character::isRegionalIndicator(prevCodePoint) &&
      Character::isRegionalIndicator(nextCodePoint))
    NOTREACHED() << "Do not use this function for regional indicators.";

  // Rule GB9, x (Extend | ZWJ)
  // Rule GB9a, x SpacingMark
  if (nextProp == U_GCB_EXTEND || nextCodePoint == zeroWidthJoinerCharacter ||
      nextProp == U_GCB_SPACING_MARK)
    return false;

  // Rule GB9b, Prepend x
  if (prevProp == U_GCB_PREPEND)
    return false;

  // Cluster Indic syllables together.
  if (isIndicSyllabicCategoryVirama(prevCodePoint) &&
      u_getIntPropertyValue(nextCodePoint, UCHAR_GENERAL_CATEGORY) ==
          U_OTHER_LETTER)
    return false;

  // Proposed Rule GB10, (E_Base | EBG) x E_Modifier
  if ((Character::isEmojiModifierBase(prevCodePoint) ||
       isEBaseGAZ(prevCodePoint)) &&
      Character::isModifier(nextCodePoint))
    return false;

  // Proposed Rule GB11, ZWJ x Emoji
  if (prevCodePoint == zeroWidthJoinerCharacter &&
      (Character::isEmoji(nextCodePoint)))
    return false;

  // Rule GB999 any ÷ any
  return true;
}

}  // namespace blink
