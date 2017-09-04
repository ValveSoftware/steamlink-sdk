/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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

#include "core/editing/spellcheck/TextCheckingParagraph.h"

#include "core/dom/Range.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/VisibleUnits.h"
#include "core/editing/iterators/CharacterIterator.h"

namespace blink {

static EphemeralRange expandToParagraphBoundary(const EphemeralRange& range) {
  const VisiblePosition& start = createVisiblePosition(range.startPosition());
  DCHECK(start.isNotNull()) << range.startPosition();
  const Position& paragraphStart = startOfParagraph(start).deepEquivalent();
  DCHECK(paragraphStart.isNotNull()) << range.startPosition();

  const VisiblePosition& end = createVisiblePosition(range.endPosition());
  DCHECK(end.isNotNull()) << range.endPosition();
  const Position& paragraphEnd = endOfParagraph(end).deepEquivalent();
  DCHECK(paragraphEnd.isNotNull()) << range.endPosition();

  // TODO(xiaochengh): There are some cases (crbug.com/640112) where we get
  // |paragraphStart > paragraphEnd|, which is the reason we cannot directly
  // return |EphemeralRange(paragraphStart, paragraphEnd)|. This is not
  // desired, though. We should do more investigation to ensure that why
  // |paragraphStart <= paragraphEnd| is violated.
  const Position& resultStart =
      paragraphStart.isNotNull() && paragraphStart <= range.startPosition()
          ? paragraphStart
          : range.startPosition();
  const Position& resultEnd =
      paragraphEnd.isNotNull() && paragraphEnd >= range.endPosition()
          ? paragraphEnd
          : range.endPosition();
  return EphemeralRange(resultStart, resultEnd);
}

TextCheckingParagraph::TextCheckingParagraph(
    const EphemeralRange& checkingRange)
    : m_checkingRange(checkingRange),
      m_checkingStart(-1),
      m_checkingEnd(-1),
      m_checkingLength(-1) {}

TextCheckingParagraph::TextCheckingParagraph(
    const EphemeralRange& checkingRange,
    const EphemeralRange& paragraphRange)
    : m_checkingRange(checkingRange),
      m_paragraphRange(paragraphRange),
      m_checkingStart(-1),
      m_checkingEnd(-1),
      m_checkingLength(-1) {}

TextCheckingParagraph::TextCheckingParagraph(Range* checkingRange,
                                             Range* paragraphRange)
    : m_checkingRange(checkingRange),
      m_paragraphRange(paragraphRange),
      m_checkingStart(-1),
      m_checkingEnd(-1),
      m_checkingLength(-1) {}

TextCheckingParagraph::~TextCheckingParagraph() {}

void TextCheckingParagraph::expandRangeToNextEnd() {
  DCHECK(m_checkingRange.isNotNull());
  setParagraphRange(
      EphemeralRange(paragraphRange().startPosition(),
                     endOfParagraph(startOfNextParagraph(createVisiblePosition(
                                        paragraphRange().startPosition())))
                         .deepEquivalent()));
  invalidateParagraphRangeValues();
}

void TextCheckingParagraph::invalidateParagraphRangeValues() {
  m_checkingStart = m_checkingEnd = -1;
  m_offsetAsRange = EphemeralRange();
  m_text = String();
}

int TextCheckingParagraph::rangeLength() const {
  DCHECK(m_checkingRange.isNotNull());
  return TextIterator::rangeLength(paragraphRange().startPosition(),
                                   paragraphRange().endPosition());
}

EphemeralRange TextCheckingParagraph::paragraphRange() const {
  DCHECK(m_checkingRange.isNotNull());
  if (m_paragraphRange.isNull())
    m_paragraphRange = expandToParagraphBoundary(checkingRange());
  return m_paragraphRange;
}

void TextCheckingParagraph::setParagraphRange(const EphemeralRange& range) {
  m_paragraphRange = range;
}

EphemeralRange TextCheckingParagraph::subrange(int characterOffset,
                                               int characterCount) const {
  DCHECK(m_checkingRange.isNotNull());
  return calculateCharacterSubrange(paragraphRange(), characterOffset,
                                    characterCount);
}

int TextCheckingParagraph::offsetTo(const Position& position) const {
  DCHECK(m_checkingRange.isNotNull());
  return TextIterator::rangeLength(offsetAsRange().startPosition(), position);
}

bool TextCheckingParagraph::isEmpty() const {
  // Both predicates should have same result, but we check both just to be sure.
  // We need to investigate to remove this redundancy.
  return isRangeEmpty() || isTextEmpty();
}

EphemeralRange TextCheckingParagraph::offsetAsRange() const {
  DCHECK(m_checkingRange.isNotNull());
  if (m_offsetAsRange.isNotNull())
    return m_offsetAsRange;
  const Position& paragraphStart = paragraphRange().startPosition();
  const Position& checkingStart = checkingRange().startPosition();
  if (paragraphStart <= checkingStart) {
    m_offsetAsRange = EphemeralRange(paragraphStart, checkingStart);
    return m_offsetAsRange;
  }
  // editing/pasteboard/paste-table-001.html and more reach here.
  m_offsetAsRange = EphemeralRange(checkingStart, paragraphStart);
  return m_offsetAsRange;
}

const String& TextCheckingParagraph::text() const {
  DCHECK(m_checkingRange.isNotNull());
  if (m_text.isEmpty())
    m_text = plainText(paragraphRange());
  return m_text;
}

int TextCheckingParagraph::checkingStart() const {
  DCHECK(m_checkingRange.isNotNull());
  if (m_checkingStart == -1)
    m_checkingStart = TextIterator::rangeLength(offsetAsRange().startPosition(),
                                                offsetAsRange().endPosition());
  return m_checkingStart;
}

int TextCheckingParagraph::checkingEnd() const {
  DCHECK(m_checkingRange.isNotNull());
  if (m_checkingEnd == -1)
    m_checkingEnd = checkingStart() +
                    TextIterator::rangeLength(checkingRange().startPosition(),
                                              checkingRange().endPosition());
  return m_checkingEnd;
}

int TextCheckingParagraph::checkingLength() const {
  DCHECK(m_checkingRange.isNotNull());
  if (-1 == m_checkingLength)
    m_checkingLength = TextIterator::rangeLength(
        checkingRange().startPosition(), checkingRange().endPosition());
  return m_checkingLength;
}

}  // namespace blink
