// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSTokenizerInputStream_h
#define CSSTokenizerInputStream_h

#include "wtf/text/StringView.h"
#include "wtf/text/WTFString.h"

namespace blink {

class CSSTokenizerInputStream {
  WTF_MAKE_NONCOPYABLE(CSSTokenizerInputStream);
  USING_FAST_MALLOC(CSSTokenizerInputStream);

 public:
  explicit CSSTokenizerInputStream(const String& input);

  // Gets the char in the stream replacing NUL characters with a unicode
  // replacement character. Will return (NUL) kEndOfFileMarker when at the
  // end of the stream.
  UChar nextInputChar() const {
    if (m_offset >= m_stringLength)
      return '\0';
    UChar result = (*m_string)[m_offset];
    return result ? result : 0xFFFD;
  }

  // Gets the char at lookaheadOffset from the current stream position. Will
  // return NUL (kEndOfFileMarker) if the stream position is at the end.
  // NOTE: This may *also* return NUL if there's one in the input! Never
  // compare the return value to '\0'.
  UChar peekWithoutReplacement(unsigned lookaheadOffset) const {
    if ((m_offset + lookaheadOffset) >= m_stringLength)
      return '\0';
    return (*m_string)[m_offset + lookaheadOffset];
  }

  void advance(unsigned offset = 1) { m_offset += offset; }
  void pushBack(UChar cc) {
    --m_offset;
    DCHECK(nextInputChar() == cc);
  }

  double getDouble(unsigned start, unsigned end) const;

  template <bool characterPredicate(UChar)>
  unsigned skipWhilePredicate(unsigned offset) {
    if (m_string->is8Bit()) {
      const LChar* characters8 = m_string->characters8();
      while ((m_offset + offset) < m_stringLength &&
             characterPredicate(characters8[m_offset + offset]))
        ++offset;
    } else {
      const UChar* characters16 = m_string->characters16();
      while ((m_offset + offset) < m_stringLength &&
             characterPredicate(characters16[m_offset + offset]))
        ++offset;
    }
    return offset;
  }

  void advanceUntilNonWhitespace();

  unsigned length() const { return m_stringLength; }
  unsigned offset() const { return std::min(m_offset, m_stringLength); }

  StringView rangeAt(unsigned start, unsigned length) const {
    DCHECK(start + length <= m_stringLength);
    return StringView(*m_string, start, length);
  }

 private:
  size_t m_offset;
  const size_t m_stringLength;
  const RefPtr<StringImpl> m_string;
};

}  // namespace blink

#endif  // CSSTokenizerInputStream_h
