/*
 * Copyright (C) 2007, 2008, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2015 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/css/FontStyleMatcher.h"

#include "core/css/CSSSegmentedFontFace.h"
#include "wtf/Assertions.h"

#include <stdlib.h>

namespace blink {

static inline unsigned stretchDistanceToDesired(FontTraits desired,
                                                FontTraits candidate) {
  return abs(static_cast<int>(desired.stretch() - candidate.stretch()));
}

static inline unsigned styleScore(FontTraits desired, FontTraits candidate) {
  static_assert(FontStyleNormal == 0 && FontStyleItalic == 2,
                "Enumeration values need to match lookup table.");
  unsigned styleScoreLookupTable[][FontStyleItalic + 1] = {
      // "If the value is normal, normal faces are checked first, then oblique
      // faces, then italic faces."
      // i.e. normal has the highest score, then oblique, then italic.
      {2, 1, 0},
      // "If the value is oblique, oblique faces are checked first, then italic
      // faces and then normal faces."
      // i.e. normal gets the lowest score, oblique gets the highest, italic
      // second best.
      {0, 2, 1},
      // "If the value of font-style is italic, italic faces are checked first,
      // then oblique, then normal faces"
      // i.e. normal gets the lowest score, oblique is second best, italic
      // highest.
      {0, 1, 2}};

  SECURITY_DCHECK(desired.style() < FontStyleItalic + 1);
  SECURITY_DCHECK(candidate.style() < FontStyleItalic + 1);

  return styleScoreLookupTable[desired.style()][candidate.style()];
}

static inline unsigned weightScore(FontTraits desired, FontTraits candidate) {
  static_assert(FontWeight100 == 0 && FontWeight900 - FontWeight100 == 8,
                "Enumeration values need to match lookup table.");
  static const unsigned scoreLookupSize = FontWeight900 + 1;
  // https://drafts.csswg.org/css-fonts/#font-style-matching
  // "..if the desired weight is available that face matches. "
  static const unsigned weightScoreLookup[scoreLookupSize][scoreLookupSize] = {
      // "If the desired weight is less than 400, weights below the desired
      // weight are checked in descending order followed by weights above the
      // desired weight in ascending order until a match is found."
      {9, 8, 7, 6, 5, 4, 3, 2, 1},  // FontWeight100 desired
      {8, 9, 7, 6, 5, 4, 3, 2, 1},  // FontWeight200 desired
      {7, 8, 9, 6, 5, 4, 3, 2, 1},  // FontWeight300 desired

      // "If the desired weight is 400, 500 is checked first and then the rule
      // for desired weights less than 400 is used."
      {5, 6, 7, 9, 8, 4, 3, 2, 1},  // FontWeight400 desired

      // "If the desired weight is 500, 400 is checked first and then the rule
      // for desired weights less than 400 is used."
      {5, 6, 7, 8, 9, 4, 3, 2, 1},  // FontWeight500 desired

      // "If the desired weight is greater than 500, weights above the desired
      // weight are checked in ascending order followed by weights below the
      // desired weight in descending order until a match is found."
      {1, 2, 3, 4, 5, 9, 8, 7, 6},  // FontWeight600 desired
      {1, 2, 3, 4, 5, 6, 9, 8, 7},  // FontWeight700 desired
      {1, 2, 3, 4, 5, 6, 7, 9, 8},  // FontWeight800 desired
      {1, 2, 3, 4, 5, 6, 7, 8, 9}   // FontWeight900 desired
  };

  unsigned desiredScoresLookup = static_cast<unsigned>(desired.weight());
  unsigned candidateScoreLookup = static_cast<unsigned>(candidate.weight());
  SECURITY_DCHECK(desiredScoresLookup < scoreLookupSize);
  SECURITY_DCHECK(candidateScoreLookup < scoreLookupSize);

  return weightScoreLookup[desiredScoresLookup][candidateScoreLookup];
}

bool FontStyleMatcher::isCandidateBetter(CSSSegmentedFontFace* candidate,
                                         CSSSegmentedFontFace* current) {
  const FontTraits& candidateTraits = candidate->traits();
  const FontTraits& currentTraits = current->traits();

  // According to CSS3 Fonts Font Style matching, there is a precedence for
  // matching:
  // A better stretch match wins over a better style match, a better style match
  // wins over a better weight match, where "better" means closer to the desired
  // traits.
  int stretchComparison = 0, styleComparison = 0, weightComparison = 0;

  stretchComparison = stretchDistanceToDesired(m_fontTraits, candidateTraits) -
                      stretchDistanceToDesired(m_fontTraits, currentTraits);

  if (stretchComparison > 0)
    return false;
  if (stretchComparison < 0)
    return true;

  styleComparison = styleScore(m_fontTraits, candidateTraits) -
                    styleScore(m_fontTraits, currentTraits);

  if (styleComparison > 0)
    return true;
  if (styleComparison < 0)
    return false;

  weightComparison = weightScore(m_fontTraits, candidateTraits) -
                     weightScore(m_fontTraits, currentTraits);

  if (weightComparison > 0)
    return true;

  return false;
}

}  // namespace blink
