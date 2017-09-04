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

#include "core/editing/iterators/SearchBuffer.h"

#include "core/dom/Document.h"
#include "core/editing/iterators/CharacterIterator.h"
#include "core/editing/iterators/SimplifiedBackwardsTextIterator.h"
#include "core/editing/iterators/TextSearcherICU.h"
#include "platform/text/Character.h"
#include "platform/text/TextBoundaries.h"
#include "platform/text/UnicodeUtilities.h"
#include "wtf/text/StringView.h"

namespace blink {

namespace {

const size_t kMinimumSearchBufferSize = 8192;

UChar32 getCodePointAt(const UChar* str, size_t index, size_t length) {
  UChar32 c;
  U16_GET(str, 0, index, length, c);
  return c;
}

}  // namespace

inline SearchBuffer::SearchBuffer(const String& target, FindOptions options)
    : m_options(options),
      m_prefixLength(0),
      m_numberOfCharactersJustAppended(0),
      m_atBreak(true),
      m_needsMoreContext(options & AtWordStarts),
      m_targetRequiresKanaWorkaround(containsKanaLetters(target)) {
  DCHECK(!target.isEmpty()) << target;
  target.appendTo(m_target);

  // FIXME: We'd like to tailor the searcher to fold quote marks for us instead
  // of doing it in a separate replacement pass here, but ICU doesn't offer a
  // way to add tailoring on top of the locale-specific tailoring as of this
  // writing.
  foldQuoteMarksAndSoftHyphens(m_target.data(), m_target.size());

  size_t targetLength = m_target.size();
  m_buffer.reserveInitialCapacity(
      std::max(targetLength * 8, kMinimumSearchBufferSize));
  m_overlap = m_buffer.capacity() / 4;

  if ((m_options & AtWordStarts) && targetLength) {
    const UChar32 targetFirstCharacter =
        getCodePointAt(m_target.data(), 0, targetLength);
    // Characters in the separator category never really occur at the beginning
    // of a word, so if the target begins with such a character, we just ignore
    // the AtWordStart option.
    if (isSeparator(targetFirstCharacter)) {
      m_options &= ~AtWordStarts;
      m_needsMoreContext = false;
    }
  }

  m_textSearcher = makeUnique<TextSearcherICU>();
  m_textSearcher->setPattern(StringView(m_target.data(), m_target.size()),
                             !(m_options & CaseInsensitive));

  // The kana workaround requires a normalized copy of the target string.
  if (m_targetRequiresKanaWorkaround)
    normalizeCharactersIntoNFCForm(m_target.data(), m_target.size(),
                                   m_normalizedTarget);
}

inline SearchBuffer::~SearchBuffer() {}

template <typename CharType>
inline void SearchBuffer::append(const CharType* characters, size_t length) {
  DCHECK(length);

  if (m_atBreak) {
    m_buffer.shrink(0);
    m_prefixLength = 0;
    m_atBreak = false;
  } else if (m_buffer.size() == m_buffer.capacity()) {
    memcpy(m_buffer.data(), m_buffer.data() + m_buffer.size() - m_overlap,
           m_overlap * sizeof(UChar));
    m_prefixLength -= std::min(m_prefixLength, m_buffer.size() - m_overlap);
    m_buffer.shrink(m_overlap);
  }

  size_t oldLength = m_buffer.size();
  size_t usableLength = std::min(m_buffer.capacity() - oldLength, length);
  DCHECK(usableLength);
  m_buffer.resize(oldLength + usableLength);
  UChar* destination = m_buffer.data() + oldLength;
  StringImpl::copyChars(destination, characters, usableLength);
  foldQuoteMarksAndSoftHyphens(destination, usableLength);
  m_numberOfCharactersJustAppended = usableLength;
}

inline bool SearchBuffer::needsMoreContext() const {
  return m_needsMoreContext;
}

inline void SearchBuffer::prependContext(const UChar* characters,
                                         size_t length) {
  DCHECK(m_needsMoreContext);
  DCHECK_EQ(m_prefixLength, m_buffer.size());

  if (!length)
    return;

  m_atBreak = false;

  size_t wordBoundaryContextStart = length;
  if (wordBoundaryContextStart) {
    U16_BACK_1(characters, 0, wordBoundaryContextStart);
    wordBoundaryContextStart =
        startOfLastWordBoundaryContext(characters, wordBoundaryContextStart);
  }

  size_t usableLength = std::min(m_buffer.capacity() - m_prefixLength,
                                 length - wordBoundaryContextStart);
  m_buffer.prepend(characters + length - usableLength, usableLength);
  m_prefixLength += usableLength;

  if (wordBoundaryContextStart || m_prefixLength == m_buffer.capacity())
    m_needsMoreContext = false;
}

inline bool SearchBuffer::atBreak() const {
  return m_atBreak;
}

inline void SearchBuffer::reachedBreak() {
  m_atBreak = true;
}

inline bool SearchBuffer::isBadMatch(const UChar* match,
                                     size_t matchLength) const {
  // This function implements the kana workaround. If usearch treats
  // it as a match, but we do not want to, then it's a "bad match".
  if (!m_targetRequiresKanaWorkaround)
    return false;

  // Normalize into a match buffer. We reuse a single buffer rather than
  // creating a new one each time.
  normalizeCharactersIntoNFCForm(match, matchLength, m_normalizedMatch);

  return !checkOnlyKanaLettersInStrings(
      m_normalizedTarget.begin(), m_normalizedTarget.size(),
      m_normalizedMatch.begin(), m_normalizedMatch.size());
}

inline bool SearchBuffer::isWordStartMatch(size_t start, size_t length) const {
  DCHECK(m_options & AtWordStarts);

  if (!start)
    return true;

  int size = m_buffer.size();
  int offset = start;
  UChar32 firstCharacter = getCodePointAt(m_buffer.data(), offset, size);

  if (m_options & TreatMedialCapitalAsWordStart) {
    UChar32 previousCharacter;
    U16_PREV(m_buffer.data(), 0, offset, previousCharacter);

    if (isSeparator(firstCharacter)) {
      // The start of a separator run is a word start (".org" in "webkit.org").
      if (!isSeparator(previousCharacter))
        return true;
    } else if (isASCIIUpper(firstCharacter)) {
      // The start of an uppercase run is a word start ("Kit" in "WebKit").
      if (!isASCIIUpper(previousCharacter))
        return true;
      // The last character of an uppercase run followed by a non-separator,
      // non-digit is a word start ("Request" in "XMLHTTPRequest").
      offset = start;
      U16_FWD_1(m_buffer.data(), offset, size);
      UChar32 nextCharacter = 0;
      if (offset < size)
        nextCharacter = getCodePointAt(m_buffer.data(), offset, size);
      if (!isASCIIUpper(nextCharacter) && !isASCIIDigit(nextCharacter) &&
          !isSeparator(nextCharacter))
        return true;
    } else if (isASCIIDigit(firstCharacter)) {
      // The start of a digit run is a word start ("2" in "WebKit2").
      if (!isASCIIDigit(previousCharacter))
        return true;
    } else if (isSeparator(previousCharacter) ||
               isASCIIDigit(previousCharacter)) {
      // The start of a non-separator, non-uppercase, non-digit run is a word
      // start, except after an uppercase. ("org" in "webkit.org", but not "ore"
      // in "WebCore").
      return true;
    }
  }

  // Chinese and Japanese lack word boundary marks, and there is no clear
  // agreement on what constitutes a word, so treat the position before any CJK
  // character as a word start.
  if (Character::isCJKIdeographOrSymbol(firstCharacter))
    return true;

  size_t wordBreakSearchStart = start + length;
  while (wordBreakSearchStart > start)
    wordBreakSearchStart =
        findNextWordFromIndex(m_buffer.data(), m_buffer.size(),
                              wordBreakSearchStart, false /* backwards */);
  if (wordBreakSearchStart != start)
    return false;
  if (m_options & WholeWord)
    return static_cast<int>(start + length) ==
           findWordEndBoundary(m_buffer.data(), m_buffer.size(),
                               wordBreakSearchStart);
  return true;
}

inline size_t SearchBuffer::search(size_t& start) {
  size_t size = m_buffer.size();
  if (m_atBreak) {
    if (!size)
      return 0;
  } else {
    if (size != m_buffer.capacity())
      return 0;
  }

  m_textSearcher->setText(m_buffer.data(), size);
  m_textSearcher->setOffset(m_prefixLength);

  MatchResult match;

nextMatch:
  if (!m_textSearcher->nextMatchResult(match))
    return 0;

  // Matches that start in the overlap area are only tentative.
  // The same match may appear later, matching more characters,
  // possibly including a combining character that's not yet in the buffer.
  if (!m_atBreak && match.start >= size - m_overlap) {
    size_t overlap = m_overlap;
    if (m_options & AtWordStarts) {
      // Ensure that there is sufficient context before matchStart the next time
      // around for determining if it is at a word boundary.
      int wordBoundaryContextStart = match.start;
      U16_BACK_1(m_buffer.data(), 0, wordBoundaryContextStart);
      wordBoundaryContextStart = startOfLastWordBoundaryContext(
          m_buffer.data(), wordBoundaryContextStart);
      overlap = std::min(size - 1,
                         std::max(overlap, size - wordBoundaryContextStart));
    }
    memcpy(m_buffer.data(), m_buffer.data() + size - overlap,
           overlap * sizeof(UChar));
    m_prefixLength -= std::min(m_prefixLength, size - overlap);
    m_buffer.shrink(overlap);
    return 0;
  }

  SECURITY_DCHECK(match.start + match.length <= size);

  // If this match is "bad", move on to the next match.
  if (isBadMatch(m_buffer.data() + match.start, match.length) ||
      ((m_options & AtWordStarts) &&
       !isWordStartMatch(match.start, match.length))) {
    goto nextMatch;
  }

  size_t newSize = size - (match.start + 1);
  memmove(m_buffer.data(), m_buffer.data() + match.start + 1,
          newSize * sizeof(UChar));
  m_prefixLength -= std::min<size_t>(m_prefixLength, match.start + 1);
  m_buffer.shrink(newSize);

  start = size - match.start;
  return match.length;
}

// Check if there's any unpaird surrogate code point.
// Non-character code points are not checked.
static bool isValidUTF16(const String& s) {
  if (s.is8Bit())
    return true;
  const UChar* ustr = s.characters16();
  size_t length = s.length();
  size_t position = 0;
  while (position < length) {
    UChar32 character;
    U16_NEXT(ustr, position, length, character);
    if (U_IS_SURROGATE(character))
      return false;
  }
  return true;
}

template <typename Strategy>
static size_t findPlainTextInternal(CharacterIteratorAlgorithm<Strategy>& it,
                                    const String& target,
                                    FindOptions options,
                                    size_t& matchStart) {
  matchStart = 0;
  size_t matchLength = 0;

  if (!isValidUTF16(target))
    return 0;

  SearchBuffer buffer(target, options);

  if (buffer.needsMoreContext()) {
    for (SimplifiedBackwardsTextIteratorAlgorithm<Strategy> backwardsIterator(
             PositionTemplate<Strategy>::firstPositionInNode(
                 it.ownerDocument()),
             PositionTemplate<Strategy>(it.currentContainer(),
                                        it.startOffset()));
         !backwardsIterator.atEnd(); backwardsIterator.advance()) {
      BackwardsTextBuffer characters;
      backwardsIterator.copyTextTo(&characters);
      buffer.prependContext(characters.data(), characters.size());
      if (!buffer.needsMoreContext())
        break;
    }
  }

  while (!it.atEnd()) {
    // TODO(xiaochengh): Should allow copying text to SearchBuffer directly
    ForwardsTextBuffer characters;
    it.copyTextTo(&characters);
    buffer.append(characters.data(), characters.size());
    it.advance(buffer.numberOfCharactersJustAppended());
  tryAgain:
    size_t matchStartOffset;
    if (size_t newMatchLength = buffer.search(matchStartOffset)) {
      // Note that we found a match, and where we found it.
      size_t lastCharacterInBufferOffset = it.characterOffset();
      DCHECK_GE(lastCharacterInBufferOffset, matchStartOffset);
      matchStart = lastCharacterInBufferOffset - matchStartOffset;
      matchLength = newMatchLength;
      // If searching forward, stop on the first match.
      // If searching backward, don't stop, so we end up with the last match.
      if (!(options & Backwards))
        break;
      goto tryAgain;
    }
    if (it.atBreak() && !buffer.atBreak()) {
      buffer.reachedBreak();
      goto tryAgain;
    }
  }

  return matchLength;
}

static const TextIteratorBehaviorFlags iteratorFlagsForFindPlainText =
    TextIteratorEntersTextControls | TextIteratorEntersOpenShadowRoots |
    TextIteratorDoesNotBreakAtReplacedElement |
    TextIteratorCollapseTrailingSpace;

template <typename Strategy>
static EphemeralRangeTemplate<Strategy> findPlainTextAlgorithm(
    const EphemeralRangeTemplate<Strategy>& inputRange,
    const String& target,
    FindOptions options) {
  // CharacterIterator requires layoutObjects to be up to date.
  if (!inputRange.startPosition().isConnected())
    return EphemeralRangeTemplate<Strategy>();
  DCHECK_EQ(inputRange.startPosition().document(),
            inputRange.endPosition().document());

  // FIXME: Reduce the code duplication with above (but how?).
  size_t matchStart;
  size_t matchLength;
  {
    TextIteratorBehaviorFlags behavior = iteratorFlagsForFindPlainText;
    if (options & FindAPICall)
      behavior |= TextIteratorForWindowFind;
    CharacterIteratorAlgorithm<Strategy> findIterator(inputRange, behavior);
    matchLength =
        findPlainTextInternal(findIterator, target, options, matchStart);
    if (!matchLength)
      return EphemeralRangeTemplate<Strategy>(options & Backwards
                                                  ? inputRange.startPosition()
                                                  : inputRange.endPosition());
  }

  CharacterIteratorAlgorithm<Strategy> computeRangeIterator(
      inputRange, iteratorFlagsForFindPlainText);
  return computeRangeIterator.calculateCharacterSubrange(matchStart,
                                                         matchLength);
}

EphemeralRange findPlainText(const EphemeralRange& inputRange,
                             const String& target,
                             FindOptions options) {
  return findPlainTextAlgorithm<EditingStrategy>(inputRange, target, options);
}

EphemeralRangeInFlatTree findPlainText(
    const EphemeralRangeInFlatTree& inputRange,
    const String& target,
    FindOptions options) {
  return findPlainTextAlgorithm<EditingInFlatTreeStrategy>(inputRange, target,
                                                           options);
}

}  // namespace blink
