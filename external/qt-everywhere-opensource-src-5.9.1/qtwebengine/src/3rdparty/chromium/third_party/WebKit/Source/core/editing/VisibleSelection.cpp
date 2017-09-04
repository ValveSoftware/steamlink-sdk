/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#include "core/editing/VisibleSelection.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Range.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/SelectionAdjuster.h"
#include "core/editing/iterators/CharacterIterator.h"
#include "platform/geometry/LayoutPoint.h"
#include "wtf/Assertions.h"
#include "wtf/text/CString.h"
#include "wtf/text/CharacterNames.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

template <typename Strategy>
VisibleSelectionTemplate<Strategy>::VisibleSelectionTemplate()
    : m_affinity(TextAffinity::Downstream),
      m_selectionType(NoSelection),
      m_baseIsFirst(true),
      m_isDirectional(false),
      m_granularity(CharacterGranularity),
      m_hasTrailingWhitespace(false) {}

template <typename Strategy>
VisibleSelectionTemplate<Strategy>::VisibleSelectionTemplate(
    const SelectionTemplate<Strategy>& selection)
    : m_base(selection.base()),
      m_extent(selection.extent()),
      m_affinity(selection.affinity()),
      m_selectionType(NoSelection),
      m_isDirectional(selection.isDirectional()),
      m_granularity(selection.granularity()),
      m_hasTrailingWhitespace(selection.hasTrailingWhitespace()) {
  validate(m_granularity);
}

template <typename Strategy>
VisibleSelectionTemplate<Strategy> VisibleSelectionTemplate<Strategy>::create(
    const SelectionTemplate<Strategy>& selection) {
  return VisibleSelectionTemplate(selection);
}

VisibleSelection createVisibleSelection(const SelectionInDOMTree& selection) {
  return VisibleSelection::create(selection);
}

VisibleSelectionInFlatTree createVisibleSelection(
    const SelectionInFlatTree& selection) {
  return VisibleSelectionInFlatTree::create(selection);
}

template <typename Strategy>
static SelectionType computeSelectionType(
    const PositionTemplate<Strategy>& start,
    const PositionTemplate<Strategy>& end) {
  if (start.isNull()) {
    DCHECK(end.isNull());
    return NoSelection;
  }
  if (start == end)
    return CaretSelection;
  // TODO(yosin) We should call |Document::updateStyleAndLayout()| here for
  // |mostBackwardCaretPosition()|. However, we are here during
  // |Node::removeChild()|.
  start.anchorNode()->updateDistribution();
  end.anchorNode()->updateDistribution();
  if (mostBackwardCaretPosition(start) == mostBackwardCaretPosition(end))
    return CaretSelection;
  return RangeSelection;
}

template <typename Strategy>
VisibleSelectionTemplate<Strategy>::VisibleSelectionTemplate(
    const VisibleSelectionTemplate<Strategy>& other)
    : m_base(other.m_base),
      m_extent(other.m_extent),
      m_start(other.m_start),
      m_end(other.m_end),
      m_affinity(other.m_affinity),
      m_selectionType(other.m_selectionType),
      m_baseIsFirst(other.m_baseIsFirst),
      m_isDirectional(other.m_isDirectional),
      m_granularity(other.m_granularity),
      m_hasTrailingWhitespace(other.m_hasTrailingWhitespace) {}

template <typename Strategy>
VisibleSelectionTemplate<Strategy>& VisibleSelectionTemplate<Strategy>::
operator=(const VisibleSelectionTemplate<Strategy>& other) {
  m_base = other.m_base;
  m_extent = other.m_extent;
  m_start = other.m_start;
  m_end = other.m_end;
  m_affinity = other.m_affinity;
  m_selectionType = other.m_selectionType;
  m_baseIsFirst = other.m_baseIsFirst;
  m_isDirectional = other.m_isDirectional;
  m_granularity = other.m_granularity;
  m_hasTrailingWhitespace = other.m_hasTrailingWhitespace;
  return *this;
}

template <typename Strategy>
SelectionTemplate<Strategy> VisibleSelectionTemplate<Strategy>::asSelection()
    const {
  typename SelectionTemplate<Strategy>::Builder builder;
  if (m_base.isNotNull())
    builder.setBaseAndExtent(m_base, m_extent);
  return builder.setAffinity(m_affinity)
      .setGranularity(m_granularity)
      .setIsDirectional(m_isDirectional)
      .setHasTrailingWhitespace(m_hasTrailingWhitespace)
      .build();
}

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::setBase(
    const PositionTemplate<Strategy>& position) {
  DCHECK(!needsLayoutTreeUpdate(position));
  m_base = position;
  validate();
}

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::setBase(
    const VisiblePositionTemplate<Strategy>& visiblePosition) {
  DCHECK(visiblePosition.isValid());
  m_base = visiblePosition.deepEquivalent();
  validate();
}

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::setExtent(
    const PositionTemplate<Strategy>& position) {
  DCHECK(!needsLayoutTreeUpdate(position));
  m_extent = position;
  validate();
}

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::setExtent(
    const VisiblePositionTemplate<Strategy>& visiblePosition) {
  DCHECK(visiblePosition.isValid());
  m_extent = visiblePosition.deepEquivalent();
  validate();
}

EphemeralRange firstEphemeralRangeOf(const VisibleSelection& selection) {
  if (selection.isNone())
    return EphemeralRange();
  Position start = selection.start().parentAnchoredEquivalent();
  Position end = selection.end().parentAnchoredEquivalent();
  return EphemeralRange(start, end);
}

Range* firstRangeOf(const VisibleSelection& selection) {
  return createRange(firstEphemeralRangeOf(selection));
}

template <typename Strategy>
EphemeralRangeTemplate<Strategy>
VisibleSelectionTemplate<Strategy>::toNormalizedEphemeralRange() const {
  if (isNone())
    return EphemeralRangeTemplate<Strategy>();

  // Make sure we have an updated layout since this function is called
  // in the course of running edit commands which modify the DOM.
  // Failing to ensure this can result in equivalentXXXPosition calls returning
  // incorrect results.
  DCHECK(!m_start.document()->needsLayoutTreeUpdate());

  if (isCaret()) {
    // If the selection is a caret, move the range start upstream. This
    // helps us match the conventions of text editors tested, which make
    // style determinations based on the character before the caret, if any.
    const PositionTemplate<Strategy> start =
        mostBackwardCaretPosition(m_start).parentAnchoredEquivalent();
    return EphemeralRangeTemplate<Strategy>(start, start);
  }
  // If the selection is a range, select the minimum range that encompasses
  // the selection. Again, this is to match the conventions of text editors
  // tested, which make style determinations based on the first character of
  // the selection. For instance, this operation helps to make sure that the
  // "X" selected below is the only thing selected. The range should not be
  // allowed to "leak" out to the end of the previous text node, or to the
  // beginning of the next text node, each of which has a different style.
  //
  // On a treasure map, <b>X</b> marks the spot.
  //                       ^ selected
  //
  DCHECK(isRange());
  return normalizeRange(EphemeralRangeTemplate<Strategy>(m_start, m_end));
}

template <typename Strategy>
static EphemeralRangeTemplate<Strategy> makeSearchRange(
    const PositionTemplate<Strategy>& pos) {
  Node* node = pos.anchorNode();
  if (!node)
    return EphemeralRangeTemplate<Strategy>();
  Document& document = node->document();
  if (!document.documentElement())
    return EphemeralRangeTemplate<Strategy>();
  Element* boundary = enclosingBlockFlowElement(*node);
  if (!boundary)
    return EphemeralRangeTemplate<Strategy>();

  return EphemeralRangeTemplate<Strategy>(
      pos, PositionTemplate<Strategy>::lastPositionInNode(boundary));
}

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::appendTrailingWhitespace() {
  if (isNone())
    return;
  DCHECK_EQ(m_granularity, WordGranularity);
  if (!isRange())
    return;
  const EphemeralRangeTemplate<Strategy> searchRange = makeSearchRange(end());
  if (searchRange.isNull())
    return;

  CharacterIteratorAlgorithm<Strategy> charIt(
      searchRange.startPosition(), searchRange.endPosition(),
      TextIteratorEmitsCharactersBetweenAllVisiblePositions);
  bool changed = false;

  for (; charIt.length(); charIt.advance(1)) {
    UChar c = charIt.characterAt(0);
    if ((!isSpaceOrNewline(c) && c != noBreakSpaceCharacter) || c == '\n')
      break;
    m_end = charIt.endPosition();
    changed = true;
  }
  if (!changed)
    return;
  m_hasTrailingWhitespace = true;
}

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::setBaseAndExtentToDeepEquivalents() {
  // Move the selection to rendered positions, if possible.
  bool baseAndExtentEqual = m_base == m_extent;
  if (m_base.isNotNull()) {
    m_base = createVisiblePosition(m_base, m_affinity).deepEquivalent();
    if (baseAndExtentEqual)
      m_extent = m_base;
  }
  if (m_extent.isNotNull() && !baseAndExtentEqual)
    m_extent = createVisiblePosition(m_extent, m_affinity).deepEquivalent();

  // Make sure we do not have a dangling base or extent.
  if (m_base.isNull() && m_extent.isNull()) {
    m_baseIsFirst = true;
  } else if (m_base.isNull()) {
    m_base = m_extent;
    m_baseIsFirst = true;
  } else if (m_extent.isNull()) {
    m_extent = m_base;
    m_baseIsFirst = true;
  } else {
    m_baseIsFirst = m_base.compareTo(m_extent) <= 0;
  }
}

template <typename Strategy>
static PositionTemplate<Strategy> computeStartRespectingGranularity(
    const PositionWithAffinityTemplate<Strategy>& passedStart,
    TextGranularity granularity) {
  DCHECK(passedStart.isNotNull());

  switch (granularity) {
    case CharacterGranularity:
      // Don't do any expansion.
      return passedStart.position();
    case WordGranularity: {
      // General case: Select the word the caret is positioned inside of.
      // If the caret is on the word boundary, select the word according to
      // |wordSide|.
      // Edge case: If the caret is after the last word in a soft-wrapped line
      // or the last word in the document, select that last word
      // (LeftWordIfOnBoundary).
      // Edge case: If the caret is after the last word in a paragraph, select
      // from the the end of the last word to the line break (also
      // RightWordIfOnBoundary);
      const VisiblePositionTemplate<Strategy> visibleStart =
          createVisiblePosition(passedStart);
      if (isEndOfEditableOrNonEditableContent(visibleStart) ||
          (isEndOfLine(visibleStart) && !isStartOfLine(visibleStart) &&
           !isEndOfParagraph(visibleStart))) {
        return startOfWord(visibleStart, LeftWordIfOnBoundary).deepEquivalent();
      }
      return startOfWord(visibleStart, RightWordIfOnBoundary).deepEquivalent();
    }
    case SentenceGranularity:
      return startOfSentence(createVisiblePosition(passedStart))
          .deepEquivalent();
    case LineGranularity:
      return startOfLine(createVisiblePosition(passedStart)).deepEquivalent();
    case LineBoundary:
      return startOfLine(createVisiblePosition(passedStart)).deepEquivalent();
    case ParagraphGranularity: {
      const VisiblePositionTemplate<Strategy> pos =
          createVisiblePosition(passedStart);
      if (isStartOfLine(pos) && isEndOfEditableOrNonEditableContent(pos))
        return startOfParagraph(previousPositionOf(pos)).deepEquivalent();
      return startOfParagraph(pos).deepEquivalent();
    }
    case DocumentBoundary:
      return startOfDocument(createVisiblePosition(passedStart))
          .deepEquivalent();
    case ParagraphBoundary:
      return startOfParagraph(createVisiblePosition(passedStart))
          .deepEquivalent();
    case SentenceBoundary:
      return startOfSentence(createVisiblePosition(passedStart))
          .deepEquivalent();
  }

  NOTREACHED();
  return passedStart.position();
}

template <typename Strategy>
static PositionTemplate<Strategy> computeEndRespectingGranularity(
    const PositionTemplate<Strategy>& start,
    const PositionWithAffinityTemplate<Strategy>& passedEnd,
    TextGranularity granularity) {
  DCHECK(passedEnd.isNotNull());

  switch (granularity) {
    case CharacterGranularity:
      // Don't do any expansion.
      return passedEnd.position();
    case WordGranularity: {
      // General case: Select the word the caret is positioned inside of.
      // If the caret is on the word boundary, select the word according to
      // |wordSide|.
      // Edge case: If the caret is after the last word in a soft-wrapped line
      // or the last word in the document, select that last word
      // (|LeftWordIfOnBoundary|).
      // Edge case: If the caret is after the last word in a paragraph, select
      // from the the end of the last word to the line break (also
      // |RightWordIfOnBoundary|);
      const VisiblePositionTemplate<Strategy> originalEnd =
          createVisiblePosition(passedEnd);
      EWordSide side = RightWordIfOnBoundary;
      if (isEndOfEditableOrNonEditableContent(originalEnd) ||
          (isEndOfLine(originalEnd) && !isStartOfLine(originalEnd) &&
           !isEndOfParagraph(originalEnd)))
        side = LeftWordIfOnBoundary;

      const VisiblePositionTemplate<Strategy> wordEnd =
          endOfWord(originalEnd, side);
      if (!isEndOfParagraph(originalEnd))
        return wordEnd.deepEquivalent();
      if (isEmptyTableCell(start.anchorNode()))
        return wordEnd.deepEquivalent();

      // Select the paragraph break (the space from the end of a paragraph
      // to the start of the next one) to match TextEdit.
      const VisiblePositionTemplate<Strategy> end = nextPositionOf(wordEnd);
      Element* const table = tableElementJustBefore(end);
      if (!table) {
        if (end.isNull())
          return wordEnd.deepEquivalent();
        return end.deepEquivalent();
      }

      if (!isEnclosingBlock(table))
        return wordEnd.deepEquivalent();

      // The paragraph break after the last paragraph in the last cell
      // of a block table ends at the start of the paragraph after the
      // table.
      const VisiblePositionTemplate<Strategy> next =
          nextPositionOf(end, CannotCrossEditingBoundary);
      if (next.isNull())
        return wordEnd.deepEquivalent();
      return next.deepEquivalent();
    }
    case SentenceGranularity:
      return endOfSentence(createVisiblePosition(passedEnd)).deepEquivalent();
    case LineGranularity: {
      const VisiblePositionTemplate<Strategy> end =
          endOfLine(createVisiblePosition(passedEnd));
      if (!isEndOfParagraph(end))
        return end.deepEquivalent();
      // If the end of this line is at the end of a paragraph, include the
      // space after the end of the line in the selection.
      const VisiblePositionTemplate<Strategy> next = nextPositionOf(end);
      if (next.isNull())
        return end.deepEquivalent();
      return next.deepEquivalent();
    }
    case LineBoundary:
      return endOfLine(createVisiblePosition(passedEnd)).deepEquivalent();
    case ParagraphGranularity: {
      const VisiblePositionTemplate<Strategy> visibleParagraphEnd =
          endOfParagraph(createVisiblePosition(passedEnd));

      // Include the "paragraph break" (the space from the end of this
      // paragraph to the start of the next one) in the selection.
      const VisiblePositionTemplate<Strategy> end =
          nextPositionOf(visibleParagraphEnd);

      Element* const table = tableElementJustBefore(end);
      if (!table) {
        if (end.isNull())
          return visibleParagraphEnd.deepEquivalent();
        return end.deepEquivalent();
      }

      if (!isEnclosingBlock(table)) {
        // There is no paragraph break after the last paragraph in the
        // last cell of an inline table.
        return visibleParagraphEnd.deepEquivalent();
      }

      // The paragraph break after the last paragraph in the last cell of
      // a block table ends at the start of the paragraph after the table,
      // not at the position just after the table.
      const VisiblePositionTemplate<Strategy> next =
          nextPositionOf(end, CannotCrossEditingBoundary);
      if (next.isNull())
        return visibleParagraphEnd.deepEquivalent();
      return next.deepEquivalent();
    }
    case DocumentBoundary:
      return endOfDocument(createVisiblePosition(passedEnd)).deepEquivalent();
    case ParagraphBoundary:
      return endOfParagraph(createVisiblePosition(passedEnd)).deepEquivalent();
    case SentenceBoundary:
      return endOfSentence(createVisiblePosition(passedEnd)).deepEquivalent();
  }
  NOTREACHED();
  return passedEnd.position();
}

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::updateSelectionType() {
  m_selectionType = computeSelectionType(m_start, m_end);

  // Affinity only makes sense for a caret
  if (m_selectionType != CaretSelection)
    m_affinity = TextAffinity::Downstream;
}

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::validate(TextGranularity granularity) {
  DCHECK(!needsLayoutTreeUpdate(m_base));
  DCHECK(!needsLayoutTreeUpdate(m_extent));
  // TODO(xiaochengh): Add a DocumentLifecycle::DisallowTransitionScope here.

  m_granularity = granularity;
  m_hasTrailingWhitespace = false;
  setBaseAndExtentToDeepEquivalents();
  if (m_base.isNull() || m_extent.isNull()) {
    m_base = m_extent = m_start = m_end = PositionTemplate<Strategy>();
    updateSelectionType();
    return;
  }

  const PositionTemplate<Strategy> start = m_baseIsFirst ? m_base : m_extent;
  const PositionTemplate<Strategy> newStart = computeStartRespectingGranularity(
      PositionWithAffinityTemplate<Strategy>(start, m_affinity), granularity);
  m_start = newStart.isNotNull() ? newStart : start;

  const PositionTemplate<Strategy> end = m_baseIsFirst ? m_extent : m_base;
  const PositionTemplate<Strategy> newEnd = computeEndRespectingGranularity(
      m_start, PositionWithAffinityTemplate<Strategy>(end, m_affinity),
      granularity);
  m_end = newEnd.isNotNull() ? newEnd : end;

  adjustSelectionToAvoidCrossingShadowBoundaries();
  adjustSelectionToAvoidCrossingEditingBoundaries();
  updateSelectionType();

  if (getSelectionType() == RangeSelection) {
    // "Constrain" the selection to be the smallest equivalent range of
    // nodes. This is a somewhat arbitrary choice, but experience shows that
    // it is useful to make to make the selection "canonical" (if only for
    // purposes of comparing selections). This is an ideal point of the code
    // to do this operation, since all selection changes that result in a
    // RANGE come through here before anyone uses it.
    // TODO(yosin) Canonicalizing is good, but haven't we already done it
    // (when we set these two positions to |VisiblePosition|
    // |deepEquivalent()|s above)?
    m_start = mostForwardCaretPosition(m_start);
    m_end = mostBackwardCaretPosition(m_end);
  }
}

template <typename Strategy>
bool VisibleSelectionTemplate<Strategy>::isValidFor(
    const Document& document) const {
  if (isNone())
    return true;

  return m_base.document() == &document && !m_base.isOrphan() &&
         !m_extent.isOrphan() && !m_start.isOrphan() && !m_end.isOrphan();
}

// TODO(yosin) This function breaks the invariant of this class.
// But because we use VisibleSelection to store values in editing commands for
// use when undoing the command, we need to be able to create a selection that
// while currently invalid, will be valid once the changes are undone. This is a
// design problem. To fix it we either need to change the invariants of
// |VisibleSelection| or create a new class for editing to use that can
// manipulate selections that are not currently valid.
template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::setWithoutValidation(
    const PositionTemplate<Strategy>& base,
    const PositionTemplate<Strategy>& extent) {
  if (base.isNull() || extent.isNull()) {
    m_base = m_extent = m_start = m_end = PositionTemplate<Strategy>();
    updateSelectionType();
    return;
  }

  m_base = base;
  m_extent = extent;
  m_baseIsFirst = base.compareTo(extent) <= 0;
  if (m_baseIsFirst) {
    m_start = base;
    m_end = extent;
  } else {
    m_start = extent;
    m_end = base;
  }
  m_selectionType = base == extent ? CaretSelection : RangeSelection;
  if (m_selectionType != CaretSelection) {
    // Since |m_affinity| for non-|CaretSelection| is always |Downstream|,
    // we should keep this invariant. Note: This function can be called with
    // |m_affinity| is |TextAffinity::Upstream|.
    m_affinity = TextAffinity::Downstream;
  }
}

template <typename Strategy>
void VisibleSelectionTemplate<
    Strategy>::adjustSelectionToAvoidCrossingShadowBoundaries() {
  if (m_base.isNull() || m_start.isNull() || m_base.isNull())
    return;
  SelectionAdjuster::adjustSelectionToAvoidCrossingShadowBoundaries(this);
}

static Element* lowestEditableAncestor(Node* node) {
  while (node) {
    if (hasEditableStyle(*node))
      return rootEditableElement(*node);
    if (isHTMLBodyElement(*node))
      break;
    node = node->parentNode();
  }

  return nullptr;
}

template <typename Strategy>
void VisibleSelectionTemplate<
    Strategy>::adjustSelectionToAvoidCrossingEditingBoundaries() {
  if (m_base.isNull() || m_start.isNull() || m_end.isNull())
    return;

  ContainerNode* baseRoot = highestEditableRoot(m_base);
  ContainerNode* startRoot = highestEditableRoot(m_start);
  ContainerNode* endRoot = highestEditableRoot(m_end);

  Element* baseEditableAncestor =
      lowestEditableAncestor(m_base.computeContainerNode());

  // The base, start and end are all in the same region.  No adjustment
  // necessary.
  if (baseRoot == startRoot && baseRoot == endRoot)
    return;

  // The selection is based in editable content.
  if (baseRoot) {
    // If the start is outside the base's editable root, cap it at the start of
    // that root.
    // If the start is in non-editable content that is inside the base's
    // editable root, put it at the first editable position after start inside
    // the base's editable root.
    if (startRoot != baseRoot) {
      const VisiblePositionTemplate<Strategy> first =
          firstEditableVisiblePositionAfterPositionInRoot(m_start, *baseRoot);
      m_start = first.deepEquivalent();
      if (m_start.isNull()) {
        NOTREACHED();
        m_start = m_end;
      }
    }
    // If the end is outside the base's editable root, cap it at the end of that
    // root.
    // If the end is in non-editable content that is inside the base's root, put
    // it at the last editable position before the end inside the base's root.
    if (endRoot != baseRoot) {
      const VisiblePositionTemplate<Strategy> last =
          lastEditableVisiblePositionBeforePositionInRoot(m_end, *baseRoot);
      m_end = last.deepEquivalent();
      if (m_end.isNull())
        m_end = m_start;
    }
    // The selection is based in non-editable content.
  } else {
    // FIXME: Non-editable pieces inside editable content should be atomic, in
    // the same way that editable pieces in non-editable content are atomic.

    // The selection ends in editable content or non-editable content inside a
    // different editable ancestor, move backward until non-editable content
    // inside the same lowest editable ancestor is reached.
    Element* endEditableAncestor =
        lowestEditableAncestor(m_end.computeContainerNode());
    if (endRoot || endEditableAncestor != baseEditableAncestor) {
      PositionTemplate<Strategy> p = previousVisuallyDistinctCandidate(m_end);
      Element* shadowAncestor = endRoot ? endRoot->ownerShadowHost() : nullptr;
      if (p.isNull() && shadowAncestor)
        p = PositionTemplate<Strategy>::afterNode(shadowAncestor);
      while (p.isNotNull() &&
             !(lowestEditableAncestor(p.computeContainerNode()) ==
                   baseEditableAncestor &&
               !isEditablePosition(p))) {
        Element* root = rootEditableElementOf(p);
        shadowAncestor = root ? root->ownerShadowHost() : nullptr;
        p = isAtomicNode(p.computeContainerNode())
                ? PositionTemplate<Strategy>::inParentBeforeNode(
                      *p.computeContainerNode())
                : previousVisuallyDistinctCandidate(p);
        if (p.isNull() && shadowAncestor)
          p = PositionTemplate<Strategy>::afterNode(shadowAncestor);
      }
      const VisiblePositionTemplate<Strategy> previous =
          createVisiblePosition(p);

      if (previous.isNull()) {
        // The selection crosses an Editing boundary.  This is a
        // programmer error in the editing code.  Happy debugging!
        NOTREACHED();
        m_base = PositionTemplate<Strategy>();
        m_extent = PositionTemplate<Strategy>();
        validate();
        return;
      }
      m_end = previous.deepEquivalent();
    }

    // The selection starts in editable content or non-editable content inside a
    // different editable ancestor, move forward until non-editable content
    // inside the same lowest editable ancestor is reached.
    Element* startEditableAncestor =
        lowestEditableAncestor(m_start.computeContainerNode());
    if (startRoot || startEditableAncestor != baseEditableAncestor) {
      PositionTemplate<Strategy> p = nextVisuallyDistinctCandidate(m_start);
      Element* shadowAncestor =
          startRoot ? startRoot->ownerShadowHost() : nullptr;
      if (p.isNull() && shadowAncestor)
        p = PositionTemplate<Strategy>::beforeNode(shadowAncestor);
      while (p.isNotNull() &&
             !(lowestEditableAncestor(p.computeContainerNode()) ==
                   baseEditableAncestor &&
               !isEditablePosition(p))) {
        Element* root = rootEditableElementOf(p);
        shadowAncestor = root ? root->ownerShadowHost() : nullptr;
        p = isAtomicNode(p.computeContainerNode())
                ? PositionTemplate<Strategy>::inParentAfterNode(
                      *p.computeContainerNode())
                : nextVisuallyDistinctCandidate(p);
        if (p.isNull() && shadowAncestor)
          p = PositionTemplate<Strategy>::beforeNode(shadowAncestor);
      }
      const VisiblePositionTemplate<Strategy> next = createVisiblePosition(p);

      if (next.isNull()) {
        // The selection crosses an Editing boundary.  This is a
        // programmer error in the editing code.  Happy debugging!
        NOTREACHED();
        m_base = PositionTemplate<Strategy>();
        m_extent = PositionTemplate<Strategy>();
        validate();
        return;
      }
      m_start = next.deepEquivalent();
    }
  }

  // Correct the extent if necessary.
  if (baseEditableAncestor !=
      lowestEditableAncestor(m_extent.computeContainerNode()))
    m_extent = m_baseIsFirst ? m_end : m_start;
}

template <typename Strategy>
bool VisibleSelectionTemplate<Strategy>::isContentEditable() const {
  return isEditablePosition(start());
}

template <typename Strategy>
bool VisibleSelectionTemplate<Strategy>::hasEditableStyle() const {
  return isEditablePosition(start());
}

template <typename Strategy>
bool VisibleSelectionTemplate<Strategy>::isContentRichlyEditable() const {
  return isRichlyEditablePosition(toPositionInDOMTree(start()));
}

template <typename Strategy>
Element* VisibleSelectionTemplate<Strategy>::rootEditableElement() const {
  return rootEditableElementOf(start());
}

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::updateIfNeeded() {
  Document* document = m_base.document();
  if (!document)
    return;
  DCHECK(!document->needsLayoutTreeUpdate());
  const bool hasTrailingWhitespace = m_hasTrailingWhitespace;
  validate(m_granularity);
  if (!hasTrailingWhitespace)
    return;
  appendTrailingWhitespace();
}

template <typename Strategy>
static bool equalSelectionsAlgorithm(
    const VisibleSelectionTemplate<Strategy>& selection1,
    const VisibleSelectionTemplate<Strategy>& selection2) {
  if (selection1.affinity() != selection2.affinity() ||
      selection1.isDirectional() != selection2.isDirectional())
    return false;

  if (selection1.isNone())
    return selection2.isNone();

  const VisibleSelectionTemplate<Strategy> selectionWrapper1(selection1);
  const VisibleSelectionTemplate<Strategy> selectionWrapper2(selection2);

  return selectionWrapper1.start() == selectionWrapper2.start() &&
         selectionWrapper1.end() == selectionWrapper2.end() &&
         selectionWrapper1.base() == selectionWrapper2.base() &&
         selectionWrapper1.extent() == selectionWrapper2.extent();
}

template <typename Strategy>
bool VisibleSelectionTemplate<Strategy>::operator==(
    const VisibleSelectionTemplate<Strategy>& other) const {
  return equalSelectionsAlgorithm<Strategy>(*this, other);
}

#ifndef NDEBUG

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::showTreeForThis() const {
  if (!start().anchorNode())
    return;
  LOG(INFO) << "\n"
            << start()
                   .anchorNode()
                   ->toMarkedTreeString(start().anchorNode(), "S",
                                        end().anchorNode(), "E")
                   .utf8()
                   .data()
            << "start: " << start().toAnchorTypeAndOffsetString().utf8().data()
            << "\n"
            << "end: " << end().toAnchorTypeAndOffsetString().utf8().data();
}

#endif

template <typename Strategy>
void VisibleSelectionTemplate<Strategy>::PrintTo(
    const VisibleSelectionTemplate<Strategy>& selection,
    std::ostream* ostream) {
  if (selection.isNone()) {
    *ostream << "VisibleSelection()";
    return;
  }
  *ostream << "VisibleSelection(base: " << selection.base()
           << " extent:" << selection.extent()
           << " start: " << selection.start() << " end: " << selection.end()
           << ' ' << selection.affinity() << ' '
           << (selection.isDirectional() ? "Directional" : "NonDirectional")
           << ')';
}

template class CORE_TEMPLATE_EXPORT VisibleSelectionTemplate<EditingStrategy>;
template class CORE_TEMPLATE_EXPORT
    VisibleSelectionTemplate<EditingInFlatTreeStrategy>;

std::ostream& operator<<(std::ostream& ostream,
                         const VisibleSelection& selection) {
  VisibleSelection::PrintTo(selection, &ostream);
  return ostream;
}

std::ostream& operator<<(std::ostream& ostream,
                         const VisibleSelectionInFlatTree& selection) {
  VisibleSelectionInFlatTree::PrintTo(selection, &ostream);
  return ostream;
}

}  // namespace blink

#ifndef NDEBUG

void showTree(const blink::VisibleSelection& sel) {
  sel.showTreeForThis();
}

void showTree(const blink::VisibleSelection* sel) {
  if (sel)
    sel->showTreeForThis();
}

void showTree(const blink::VisibleSelectionInFlatTree& sel) {
  sel.showTreeForThis();
}

void showTree(const blink::VisibleSelectionInFlatTree* sel) {
  if (sel)
    sel->showTreeForThis();
}
#endif
