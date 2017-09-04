/*
 * Copyright (C) 2004, 2006, 2009 Apple Inc. All rights reserved.
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

#ifndef SearchBuffer_h
#define SearchBuffer_h

#include "core/CoreExport.h"
#include "core/editing/EphemeralRange.h"
#include "core/editing/FindOptions.h"
#include "wtf/Allocator.h"
#include "wtf/Vector.h"
#include <memory>

namespace blink {

class TextSearcherICU;

// Buffer that knows how to compare with a search target.
// Keeps enough of the previous text to be able to search in the future, but no
// more. Non-breaking spaces are always equal to normal spaces. Case folding is
// also done if the CaseInsensitive option is specified. Matches are further
// filtered if the AtWordStarts option is specified, although some matches
// inside a word are permitted if TreatMedialCapitalAsWordStart is specified as
// well.
class SearchBuffer {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(SearchBuffer);

 public:
  SearchBuffer(const String& target, FindOptions);
  ~SearchBuffer();

  // Returns number of characters appended; guaranteed to be in the range
  // [1, length].
  template <typename CharType>
  void append(const CharType*, size_t length);
  size_t numberOfCharactersJustAppended() const {
    return m_numberOfCharactersJustAppended;
  }

  bool needsMoreContext() const;
  void prependContext(const UChar*, size_t length);
  void reachedBreak();

  // Result is the size in characters of what was found.
  // And <startOffset> is the number of characters back to the start of what
  // was found.
  size_t search(size_t& startOffset);
  bool atBreak() const;

 private:
  bool isBadMatch(const UChar*, size_t length) const;
  bool isWordStartMatch(size_t start, size_t length) const;

  Vector<UChar> m_target;
  FindOptions m_options;

  Vector<UChar> m_buffer;
  size_t m_overlap;
  size_t m_prefixLength;
  size_t m_numberOfCharactersJustAppended;
  bool m_atBreak;
  bool m_needsMoreContext;

  bool m_targetRequiresKanaWorkaround;
  Vector<UChar> m_normalizedTarget;
  mutable Vector<UChar> m_normalizedMatch;

  std::unique_ptr<TextSearcherICU> m_textSearcher;
};

CORE_EXPORT EphemeralRange findPlainText(const EphemeralRange& inputRange,
                                         const String&,
                                         FindOptions);
CORE_EXPORT EphemeralRangeInFlatTree
findPlainText(const EphemeralRangeInFlatTree& inputRange,
              const String&,
              FindOptions);

}  // namespace blink

#endif  // SearchBuffer_h
