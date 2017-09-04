// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSTokenizerInputStream.h"

#include "core/html/parser/HTMLParserIdioms.h"
#include "wtf/text/StringToNumber.h"

namespace blink {

CSSTokenizerInputStream::CSSTokenizerInputStream(const String& input)
    : m_offset(0), m_stringLength(input.length()), m_string(input.impl()) {}

void CSSTokenizerInputStream::advanceUntilNonWhitespace() {
  // Using HTML space here rather than CSS space since we don't do preprocessing
  if (m_string->is8Bit()) {
    const LChar* characters = m_string->characters8();
    while (m_offset < m_stringLength && isHTMLSpace(characters[m_offset]))
      ++m_offset;
  } else {
    const UChar* characters = m_string->characters16();
    while (m_offset < m_stringLength && isHTMLSpace(characters[m_offset]))
      ++m_offset;
  }
}

double CSSTokenizerInputStream::getDouble(unsigned start, unsigned end) const {
  DCHECK(start <= end && ((m_offset + end) <= m_stringLength));
  bool isResultOK = false;
  double result = 0.0;
  if (start < end) {
    if (m_string->is8Bit())
      result = charactersToDouble(m_string->characters8() + m_offset + start,
                                  end - start, &isResultOK);
    else
      result = charactersToDouble(m_string->characters16() + m_offset + start,
                                  end - start, &isResultOK);
  }
  // FIXME: It looks like callers ensure we have a valid number
  return isResultOK ? result : 0.0;
}

}  // namespace blink
