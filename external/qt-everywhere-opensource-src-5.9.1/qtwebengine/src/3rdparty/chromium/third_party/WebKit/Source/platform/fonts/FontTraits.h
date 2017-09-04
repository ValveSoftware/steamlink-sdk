/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2014 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FontTraits_h
#define FontTraits_h

#include "wtf/Allocator.h"
#include "wtf/Assertions.h"

namespace blink {

enum FontWeight {
  FontWeight100 = 0,
  FontWeight200 = 1,
  FontWeight300 = 2,
  FontWeight400 = 3,
  FontWeight500 = 4,
  FontWeight600 = 5,
  FontWeight700 = 6,
  FontWeight800 = 7,
  FontWeight900 = 8,
  FontWeightNormal = FontWeight400,
  FontWeightBold = FontWeight700
};

// Converts a FontWeight to its corresponding numeric value
inline int numericFontWeight(FontWeight weight) {
  return (weight - FontWeight100 + 1) * 100;
}

// Numeric values matching OS/2 & Windows Metrics usWidthClass table.
// https://www.microsoft.com/typography/otspec/os2.htm
enum FontStretch {
  FontStretchUltraCondensed = 1,
  FontStretchExtraCondensed = 2,
  FontStretchCondensed = 3,
  FontStretchSemiCondensed = 4,
  FontStretchNormal = 5,
  FontStretchSemiExpanded = 6,
  FontStretchExpanded = 7,
  FontStretchExtraExpanded = 8,
  FontStretchUltraExpanded = 9
};

enum FontStyle {
  FontStyleNormal = 0,
  FontStyleOblique = 1,
  FontStyleItalic = 2
};

typedef unsigned FontTraitsBitfield;

struct FontTraits {
  DISALLOW_NEW();
  FontTraits(FontStyle style, FontWeight weight, FontStretch stretch) {
    m_traits.m_style = style;
    m_traits.m_weight = weight;
    m_traits.m_stretch = stretch;
    m_traits.m_filler = 0;
    DCHECK_EQ(m_bitfield >> 10, 0u);
  }
  FontTraits(FontTraitsBitfield bitfield) : m_bitfield(bitfield) {
    DCHECK_EQ(m_traits.m_filler, 0u);
    DCHECK_EQ(m_bitfield >> 10, 0u);
  }
  FontStyle style() const { return static_cast<FontStyle>(m_traits.m_style); }
  FontWeight weight() const {
    return static_cast<FontWeight>(m_traits.m_weight);
  }
  FontStretch stretch() const {
    return static_cast<FontStretch>(m_traits.m_stretch);
  }
  FontTraitsBitfield bitfield() const { return m_bitfield; }

  union {
    struct {
      unsigned m_style : 2;
      unsigned m_weight : 4;
      unsigned m_stretch : 4;
      unsigned m_filler : 22;
    } m_traits;
    FontTraitsBitfield m_bitfield;
  };
};

}  // namespace blink
#endif  // FontTraits_h
