// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/shaping/ShapeResultSpacing.h"

#include "platform/fonts/FontDescription.h"
#include "platform/text/TextRun.h"

namespace blink {

ShapeResultSpacing::ShapeResultSpacing(const TextRun& run,
                                       const FontDescription& fontDescription)
    : m_textRun(run),
      m_letterSpacing(fontDescription.letterSpacing()),
      m_wordSpacing(fontDescription.wordSpacing()),
      m_expansion(run.expansion()),
      m_expansionPerOpportunity(0),
      m_expansionOpportunityCount(0),
      m_textJustify(TextJustify::TextJustifyAuto),
      m_hasSpacing(false),
      m_normalizeSpace(run.normalizeSpace()),
      m_allowTabs(run.allowTabs()),
      m_isAfterExpansion(false),
      m_isVerticalOffset(fontDescription.isVerticalAnyUpright()) {
  if (m_textRun.spacingDisabled())
    return;

  if (!m_letterSpacing && !m_wordSpacing && !m_expansion)
    return;

  m_hasSpacing = true;

  if (!m_expansion)
    return;

  // Setup for justifications (expansions.)
  m_textJustify = run.getTextJustify();
  m_isAfterExpansion = !run.allowsLeadingExpansion();

  bool isAfterExpansion = m_isAfterExpansion;
  m_expansionOpportunityCount =
      Character::expansionOpportunityCount(run, isAfterExpansion);
  if (isAfterExpansion && !run.allowsTrailingExpansion()) {
    ASSERT(m_expansionOpportunityCount > 0);
    --m_expansionOpportunityCount;
  }

  if (m_expansionOpportunityCount)
    m_expansionPerOpportunity = m_expansion / m_expansionOpportunityCount;
}

float ShapeResultSpacing::nextExpansion() {
  if (!m_expansionOpportunityCount) {
    ASSERT_NOT_REACHED();
    return 0;
  }

  m_isAfterExpansion = true;

  if (!--m_expansionOpportunityCount) {
    float remaining = m_expansion;
    m_expansion = 0;
    return remaining;
  }

  m_expansion -= m_expansionPerOpportunity;
  return m_expansionPerOpportunity;
}

bool ShapeResultSpacing::isFirstRun(const TextRun& run) const {
  if (&run == &m_textRun)
    return true;
  return run.is8Bit() ? run.characters8() == m_textRun.characters8()
                      : run.characters16() == m_textRun.characters16();
}

float ShapeResultSpacing::computeSpacing(const TextRun& run,
                                         size_t index,
                                         float& offset) {
  UChar32 character = run[index];
  bool treatAsSpace =
      (Character::treatAsSpace(character) ||
       (m_normalizeSpace &&
        Character::isNormalizedCanvasSpaceCharacter(character))) &&
      (character != '\t' || !m_allowTabs);
  if (treatAsSpace && character != noBreakSpaceCharacter)
    character = spaceCharacter;

  float spacing = 0;
  if (m_letterSpacing && !Character::treatAsZeroWidthSpace(character))
    spacing += m_letterSpacing;

  if (treatAsSpace &&
      (index || !isFirstRun(run) || character == noBreakSpaceCharacter))
    spacing += m_wordSpacing;

  if (!hasExpansion())
    return spacing;

  if (treatAsSpace)
    return spacing + nextExpansion();

  if (run.is8Bit() || m_textJustify != TextJustify::TextJustifyAuto)
    return spacing;

  // isCJKIdeographOrSymbol() has expansion opportunities both before and
  // after each character.
  // http://www.w3.org/TR/jlreq/#line_adjustment
  if (U16_IS_LEAD(character) && index + 1 < run.length() &&
      U16_IS_TRAIL(run[index + 1]))
    character = U16_GET_SUPPLEMENTARY(character, run[index + 1]);
  if (!Character::isCJKIdeographOrSymbol(character)) {
    m_isAfterExpansion = false;
    return spacing;
  }

  if (!m_isAfterExpansion) {
    // Take the expansion opportunity before this ideograph.
    float expandBefore = nextExpansion();
    if (expandBefore) {
      offset += expandBefore;
      spacing += expandBefore;
    }
    if (!hasExpansion())
      return spacing;
  }

  return spacing + nextExpansion();
}

}  // namespace blink
