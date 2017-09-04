/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2005 Alexey Proskuryakov.
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

#include "core/editing/iterators/TextSearcherICU.h"

#include "platform/text/TextBreakIteratorInternalICU.h"
#include "wtf/text/CharacterNames.h"
#include "wtf/text/WTFString.h"
#include <unicode/usearch.h>

namespace blink {

namespace {

UStringSearch* createSearcher() {
  // Provide a non-empty pattern and non-empty text so usearch_open will not
  // fail, but it doesn't matter exactly what it is, since we don't perform any
  // searches without setting both the pattern and the text.
  UErrorCode status = U_ZERO_ERROR;
  String searchCollatorName =
      currentSearchLocaleID() + String("@collation=search");
  UStringSearch* searcher =
      usearch_open(&newlineCharacter, 1, &newlineCharacter, 1,
                   searchCollatorName.utf8().data(), 0, &status);
  DCHECK(status == U_ZERO_ERROR || status == U_USING_FALLBACK_WARNING ||
         status == U_USING_DEFAULT_WARNING)
      << status;
  return searcher;
}

class ICULockableSearcher {
  WTF_MAKE_NONCOPYABLE(ICULockableSearcher);

 public:
  static UStringSearch* acquireSearcher() {
    instance().lock();
    return instance().m_searcher;
  }

  static void releaseSearcher() { instance().unlock(); }

 private:
  static ICULockableSearcher& instance() {
    static ICULockableSearcher searcher(createSearcher());
    return searcher;
  }

  explicit ICULockableSearcher(UStringSearch* searcher)
      : m_searcher(searcher) {}

  void lock() {
#if DCHECK_IS_ON()
    DCHECK(!m_locked);
    m_locked = true;
#endif
  }

  void unlock() {
#if DCHECK_IS_ON()
    DCHECK(m_locked);
    m_locked = false;
#endif
  }

  UStringSearch* const m_searcher = nullptr;

#if DCHECK_IS_ON()
  bool m_locked = false;
#endif
};

}  // namespace

// Grab the single global searcher.
// If we ever have a reason to do more than once search buffer at once, we'll
// have to move to multiple searchers.
TextSearcherICU::TextSearcherICU()
    : m_searcher(ICULockableSearcher::acquireSearcher()) {}

TextSearcherICU::~TextSearcherICU() {
  // Leave the static object pointing to valid strings (pattern=target,
  // text=buffer). Otheriwse, usearch_reset() will results in 'use-after-free'
  // error.
  setPattern(&newlineCharacter, 1);
  setText(&newlineCharacter, 1);
  ICULockableSearcher::releaseSearcher();
}

void TextSearcherICU::setPattern(const StringView& pattern,
                                 bool caseSensitive) {
  setCaseSensitivity(caseSensitive);
  setPattern(pattern.characters16(), pattern.length());
}

void TextSearcherICU::setText(const UChar* text, size_t length) {
  UErrorCode status = U_ZERO_ERROR;
  usearch_setText(m_searcher, text, length, &status);
  DCHECK_EQ(status, U_ZERO_ERROR);
  m_textLength = length;
}

void TextSearcherICU::setOffset(size_t offset) {
  UErrorCode status = U_ZERO_ERROR;
  usearch_setOffset(m_searcher, offset, &status);
  DCHECK_EQ(status, U_ZERO_ERROR);
}

bool TextSearcherICU::nextMatchResult(MatchResult& result) {
  UErrorCode status = U_ZERO_ERROR;
  const int matchStart = usearch_next(m_searcher, &status);
  DCHECK_EQ(status, U_ZERO_ERROR);

  // TODO(iceman): It is possible to use |usearch_getText| function
  // to retrieve text length and not store it explicitly.
  if (!(matchStart >= 0 && static_cast<size_t>(matchStart) < m_textLength)) {
    DCHECK_EQ(matchStart, USEARCH_DONE);
    result.start = 0;
    result.length = 0;
    return false;
  }

  result.start = static_cast<size_t>(matchStart);
  result.length = usearch_getMatchedLength(m_searcher);
  return true;
}

void TextSearcherICU::setPattern(const UChar* pattern, size_t length) {
  UErrorCode status = U_ZERO_ERROR;
  usearch_setPattern(m_searcher, pattern, length, &status);
  DCHECK_EQ(status, U_ZERO_ERROR);
}

void TextSearcherICU::setCaseSensitivity(bool caseSensitive) {
  const UCollationStrength strength =
      caseSensitive ? UCOL_TERTIARY : UCOL_PRIMARY;

  UCollator* const collator = usearch_getCollator(m_searcher);
  if (ucol_getStrength(collator) == strength)
    return;

  ucol_setStrength(collator, strength);
  usearch_reset(m_searcher);
}

}  // namespace blink
