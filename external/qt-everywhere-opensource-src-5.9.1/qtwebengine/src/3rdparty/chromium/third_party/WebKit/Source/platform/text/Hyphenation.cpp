// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/text/Hyphenation.h"

#include "platform/fonts/Font.h"
#include "wtf/text/StringView.h"

namespace blink {

Vector<size_t, 8> Hyphenation::hyphenLocations(const StringView& text) const {
  Vector<size_t, 8> hyphenLocations;
  size_t hyphenLocation = text.length();
  if (hyphenLocation <= minimumSuffixLength)
    return hyphenLocations;
  hyphenLocation -= minimumSuffixLength;

  while ((hyphenLocation = lastHyphenLocation(text, hyphenLocation)) >=
         minimumPrefixLength)
    hyphenLocations.append(hyphenLocation);

  return hyphenLocations;
}

int Hyphenation::minimumPrefixWidth(const Font& font) {
  // If the maximum width available for the prefix before the hyphen is small,
  // then it is very unlikely that an hyphenation opportunity exists, so do not
  // bother to look for it.  These are heuristic numbers for performance added
  // in http://wkb.ug/45606
  const int minimumPrefixWidthNumerator = 5;
  const int minimumPrefixWidthDenominator = 4;
  return font.getFontDescription().computedPixelSize() *
         minimumPrefixWidthNumerator / minimumPrefixWidthDenominator;
}

}  // namespace blink
