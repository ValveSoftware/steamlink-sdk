/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#include "core/editing/SelectionModifier.h"

#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/VisibleUnits.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/layout/LayoutBlock.h"
#include "core/layout/line/InlineTextBox.h"
#include "core/page/SpatialNavigation.h"

namespace blink {

LayoutUnit NoXPosForVerticalArrowNavigation()
{
    return LayoutUnit::min();
}

static inline bool shouldAlwaysUseDirectionalSelection(LocalFrame* frame)
{
    return !frame || frame->editor().behavior().shouldConsiderSelectionAsDirectional();
}

SelectionModifier::SelectionModifier(const LocalFrame& frame, const VisibleSelection& selection, LayoutUnit xPosForVerticalArrowNavigation)
    : m_frame(const_cast<LocalFrame*>(&frame))
    , m_selection(selection)
    , m_xPosForVerticalArrowNavigation(xPosForVerticalArrowNavigation)
{
}

SelectionModifier::SelectionModifier(const LocalFrame& frame, const VisibleSelection& selection)
    : SelectionModifier(frame, selection, NoXPosForVerticalArrowNavigation())
{
}

TextDirection SelectionModifier::directionOfEnclosingBlock() const
{
    return blink::directionOfEnclosingBlock(m_selection.extent());
}

TextDirection SelectionModifier::directionOfSelection() const
{
    InlineBox* startBox = nullptr;
    InlineBox* endBox = nullptr;
    // Cache the VisiblePositions because visibleStart() and visibleEnd()
    // can cause layout, which has the potential to invalidate lineboxes.
    VisiblePosition startPosition = m_selection.visibleStart();
    VisiblePosition endPosition = m_selection.visibleEnd();
    if (startPosition.isNotNull())
        startBox = computeInlineBoxPosition(startPosition).inlineBox;
    if (endPosition.isNotNull())
        endBox = computeInlineBoxPosition(endPosition).inlineBox;
    if (startBox && endBox && startBox->direction() == endBox->direction())
        return startBox->direction();

    return directionOfEnclosingBlock();
}

void SelectionModifier::willBeModified(EAlteration alter, SelectionDirection direction)
{
    if (alter != FrameSelection::AlterationExtend)
        return;

    Position start = m_selection.start();
    Position end = m_selection.end();

    bool baseIsStart = true;

    if (m_selection.isDirectional()) {
        // Make base and extent match start and end so we extend the user-visible selection.
        // This only matters for cases where base and extend point to different positions than
        // start and end (e.g. after a double-click to select a word).
        if (m_selection.isBaseFirst())
            baseIsStart = true;
        else
            baseIsStart = false;
    } else {
        switch (direction) {
        case DirectionRight:
            if (directionOfSelection() == LTR)
                baseIsStart = true;
            else
                baseIsStart = false;
            break;
        case DirectionForward:
            baseIsStart = true;
            break;
        case DirectionLeft:
            if (directionOfSelection() == LTR)
                baseIsStart = false;
            else
                baseIsStart = true;
            break;
        case DirectionBackward:
            baseIsStart = false;
            break;
        }
    }
    if (baseIsStart) {
        m_selection.setBase(start);
        m_selection.setExtent(end);
    } else {
        m_selection.setBase(end);
        m_selection.setExtent(start);
    }
}

VisiblePosition SelectionModifier::positionForPlatform(bool isGetStart) const
{
    Settings* settings = frame() ? frame()->settings() : nullptr;
    if (settings && settings->editingBehaviorType() == EditingMacBehavior)
        return isGetStart ? m_selection.visibleStart() : m_selection.visibleEnd();
    // Linux and Windows always extend selections from the extent endpoint.
    // FIXME: VisibleSelection should be fixed to ensure as an invariant that
    // base/extent always point to the same nodes as start/end, but which points
    // to which depends on the value of isBaseFirst. Then this can be changed
    // to just return m_sel.extent().
    return m_selection.isBaseFirst() ? m_selection.visibleEnd() : m_selection.visibleStart();
}

VisiblePosition SelectionModifier::startForPlatform() const
{
    return positionForPlatform(true);
}

VisiblePosition SelectionModifier::endForPlatform() const
{
    return positionForPlatform(false);
}

VisiblePosition SelectionModifier::nextWordPositionForPlatform(const VisiblePosition &originalPosition)
{
    VisiblePosition positionAfterCurrentWord = nextWordPosition(originalPosition);

    if (frame() && frame()->editor().behavior().shouldSkipSpaceWhenMovingRight()) {
        // In order to skip spaces when moving right, we advance one
        // word further and then move one word back. Given the
        // semantics of previousWordPosition() this will put us at the
        // beginning of the word following.
        VisiblePosition positionAfterSpacingAndFollowingWord = nextWordPosition(positionAfterCurrentWord);
        if (positionAfterSpacingAndFollowingWord.isNotNull() && positionAfterSpacingAndFollowingWord.deepEquivalent() != positionAfterCurrentWord.deepEquivalent())
            positionAfterCurrentWord = previousWordPosition(positionAfterSpacingAndFollowingWord);

        bool movingBackwardsMovedPositionToStartOfCurrentWord = positionAfterCurrentWord.deepEquivalent() == previousWordPosition(nextWordPosition(originalPosition)).deepEquivalent();
        if (movingBackwardsMovedPositionToStartOfCurrentWord)
            positionAfterCurrentWord = positionAfterSpacingAndFollowingWord;
    }
    return positionAfterCurrentWord;
}

static void adjustPositionForUserSelectAll(VisiblePosition& pos, bool isForward)
{
    if (Node* rootUserSelectAll = EditingStrategy::rootUserSelectAllForNode(pos.deepEquivalent().anchorNode()))
        pos = createVisiblePosition(isForward ? mostForwardCaretPosition(Position::afterNode(rootUserSelectAll), CanCrossEditingBoundary) : mostBackwardCaretPosition(Position::beforeNode(rootUserSelectAll), CanCrossEditingBoundary));
}

VisiblePosition SelectionModifier::modifyExtendingRight(TextGranularity granularity)
{
    VisiblePosition pos = createVisiblePosition(m_selection.extent(), m_selection.affinity());

    // The difference between modifyExtendingRight and modifyExtendingForward is:
    // modifyExtendingForward always extends forward logically.
    // modifyExtendingRight behaves the same as modifyExtendingForward except for extending character or word,
    // it extends forward logically if the enclosing block is LTR direction,
    // but it extends backward logically if the enclosing block is RTL direction.
    switch (granularity) {
    case CharacterGranularity:
        if (directionOfEnclosingBlock() == LTR)
            pos = nextPositionOf(pos, CanSkipOverEditingBoundary);
        else
            pos = previousPositionOf(pos, CanSkipOverEditingBoundary);
        break;
    case WordGranularity:
        if (directionOfEnclosingBlock() == LTR)
            pos = nextWordPositionForPlatform(pos);
        else
            pos = previousWordPosition(pos);
        break;
    case LineBoundary:
        if (directionOfEnclosingBlock() == LTR)
            pos = modifyExtendingForward(granularity);
        else
            pos = modifyExtendingBackward(granularity);
        break;
    case SentenceGranularity:
    case LineGranularity:
    case ParagraphGranularity:
    case SentenceBoundary:
    case ParagraphBoundary:
    case DocumentBoundary:
        // FIXME: implement all of the above?
        pos = modifyExtendingForward(granularity);
        break;
    }
    adjustPositionForUserSelectAll(pos, directionOfEnclosingBlock() == LTR);
    return pos;
}

VisiblePosition SelectionModifier::modifyExtendingForward(TextGranularity granularity)
{
    VisiblePosition pos = createVisiblePosition(m_selection.extent(), m_selection.affinity());
    switch (granularity) {
    case CharacterGranularity:
        pos = nextPositionOf(pos, CanSkipOverEditingBoundary);
        break;
    case WordGranularity:
        pos = nextWordPositionForPlatform(pos);
        break;
    case SentenceGranularity:
        pos = nextSentencePosition(pos);
        break;
    case LineGranularity:
        pos = nextLinePosition(pos, lineDirectionPointForBlockDirectionNavigation(EXTENT));
        break;
    case ParagraphGranularity:
        pos = nextParagraphPosition(pos, lineDirectionPointForBlockDirectionNavigation(EXTENT));
        break;
    case SentenceBoundary:
        pos = endOfSentence(endForPlatform());
        break;
    case LineBoundary:
        pos = logicalEndOfLine(endForPlatform());
        break;
    case ParagraphBoundary:
        pos = endOfParagraph(endForPlatform());
        break;
    case DocumentBoundary:
        pos = endForPlatform();
        if (isEditablePosition(pos.deepEquivalent()))
            pos = endOfEditableContent(pos);
        else
            pos = endOfDocument(pos);
        break;
    }
    adjustPositionForUserSelectAll(pos, directionOfEnclosingBlock() == LTR);
    return pos;
}

VisiblePosition SelectionModifier::modifyMovingRight(TextGranularity granularity)
{
    VisiblePosition pos;
    switch (granularity) {
    case CharacterGranularity:
        if (m_selection.isRange()) {
            if (directionOfSelection() == LTR)
                pos = createVisiblePosition(m_selection.end(), m_selection.affinity());
            else
                pos = createVisiblePosition(m_selection.start(), m_selection.affinity());
        } else {
            pos = rightPositionOf(createVisiblePosition(m_selection.extent(), m_selection.affinity()));
        }
        break;
    case WordGranularity: {
        bool skipsSpaceWhenMovingRight = frame() && frame()->editor().behavior().shouldSkipSpaceWhenMovingRight();
        pos = rightWordPosition(createVisiblePosition(m_selection.extent(), m_selection.affinity()), skipsSpaceWhenMovingRight);
        break;
    }
    case SentenceGranularity:
    case LineGranularity:
    case ParagraphGranularity:
    case SentenceBoundary:
    case ParagraphBoundary:
    case DocumentBoundary:
        // FIXME: Implement all of the above.
        pos = modifyMovingForward(granularity);
        break;
    case LineBoundary:
        pos = rightBoundaryOfLine(startForPlatform(), directionOfEnclosingBlock());
        break;
    }
    return pos;
}

VisiblePosition SelectionModifier::modifyMovingForward(TextGranularity granularity)
{
    VisiblePosition pos;
    // FIXME: Stay in editable content for the less common granularities.
    switch (granularity) {
    case CharacterGranularity:
        if (m_selection.isRange())
            pos = createVisiblePosition(m_selection.end(), m_selection.affinity());
        else
            pos = nextPositionOf(createVisiblePosition(m_selection.extent(), m_selection.affinity()), CanSkipOverEditingBoundary);
        break;
    case WordGranularity:
        pos = nextWordPositionForPlatform(createVisiblePosition(m_selection.extent(), m_selection.affinity()));
        break;
    case SentenceGranularity:
        pos = nextSentencePosition(createVisiblePosition(m_selection.extent(), m_selection.affinity()));
        break;
    case LineGranularity: {
        // down-arrowing from a range selection that ends at the start of a line needs
        // to leave the selection at that line start (no need to call nextLinePosition!)
        pos = endForPlatform();
        if (!m_selection.isRange() || !isStartOfLine(pos))
            pos = nextLinePosition(pos, lineDirectionPointForBlockDirectionNavigation(START));
        break;
    }
    case ParagraphGranularity:
        pos = nextParagraphPosition(endForPlatform(), lineDirectionPointForBlockDirectionNavigation(START));
        break;
    case SentenceBoundary:
        pos = endOfSentence(endForPlatform());
        break;
    case LineBoundary:
        pos = logicalEndOfLine(endForPlatform());
        break;
    case ParagraphBoundary:
        pos = endOfParagraph(endForPlatform());
        break;
    case DocumentBoundary:
        pos = endForPlatform();
        if (isEditablePosition(pos.deepEquivalent()))
            pos = endOfEditableContent(pos);
        else
            pos = endOfDocument(pos);
        break;
    }
    return pos;
}

VisiblePosition SelectionModifier::modifyExtendingLeft(TextGranularity granularity)
{
    VisiblePosition pos = createVisiblePosition(m_selection.extent(), m_selection.affinity());

    // The difference between modifyExtendingLeft and modifyExtendingBackward is:
    // modifyExtendingBackward always extends backward logically.
    // modifyExtendingLeft behaves the same as modifyExtendingBackward except for extending character or word,
    // it extends backward logically if the enclosing block is LTR direction,
    // but it extends forward logically if the enclosing block is RTL direction.
    switch (granularity) {
    case CharacterGranularity:
        if (directionOfEnclosingBlock() == LTR)
            pos = previousPositionOf(pos, CanSkipOverEditingBoundary);
        else
            pos = nextPositionOf(pos, CanSkipOverEditingBoundary);
        break;
    case WordGranularity:
        if (directionOfEnclosingBlock() == LTR)
            pos = previousWordPosition(pos);
        else
            pos = nextWordPositionForPlatform(pos);
        break;
    case LineBoundary:
        if (directionOfEnclosingBlock() == LTR)
            pos = modifyExtendingBackward(granularity);
        else
            pos = modifyExtendingForward(granularity);
        break;
    case SentenceGranularity:
    case LineGranularity:
    case ParagraphGranularity:
    case SentenceBoundary:
    case ParagraphBoundary:
    case DocumentBoundary:
        pos = modifyExtendingBackward(granularity);
        break;
    }
    adjustPositionForUserSelectAll(pos, !(directionOfEnclosingBlock() == LTR));
    return pos;
}

VisiblePosition SelectionModifier::modifyExtendingBackward(TextGranularity granularity)
{
    VisiblePosition pos = createVisiblePosition(m_selection.extent(), m_selection.affinity());

    // Extending a selection backward by word or character from just after a table selects
    // the table.  This "makes sense" from the user perspective, esp. when deleting.
    // It was done here instead of in VisiblePosition because we want VPs to iterate
    // over everything.
    switch (granularity) {
    case CharacterGranularity:
        pos = previousPositionOf(pos, CanSkipOverEditingBoundary);
        break;
    case WordGranularity:
        pos = previousWordPosition(pos);
        break;
    case SentenceGranularity:
        pos = previousSentencePosition(pos);
        break;
    case LineGranularity:
        pos = previousLinePosition(pos, lineDirectionPointForBlockDirectionNavigation(EXTENT));
        break;
    case ParagraphGranularity:
        pos = previousParagraphPosition(pos, lineDirectionPointForBlockDirectionNavigation(EXTENT));
        break;
    case SentenceBoundary:
        pos = startOfSentence(startForPlatform());
        break;
    case LineBoundary:
        pos = logicalStartOfLine(startForPlatform());
        break;
    case ParagraphBoundary:
        pos = startOfParagraph(startForPlatform());
        break;
    case DocumentBoundary:
        pos = startForPlatform();
        if (isEditablePosition(pos.deepEquivalent()))
            pos = startOfEditableContent(pos);
        else
            pos = startOfDocument(pos);
        break;
    }
    adjustPositionForUserSelectAll(pos, !(directionOfEnclosingBlock() == LTR));
    return pos;
}

VisiblePosition SelectionModifier::modifyMovingLeft(TextGranularity granularity)
{
    VisiblePosition pos;
    switch (granularity) {
    case CharacterGranularity:
        if (m_selection.isRange()) {
            if (directionOfSelection() == LTR)
                pos = createVisiblePosition(m_selection.start(), m_selection.affinity());
            else
                pos = createVisiblePosition(m_selection.end(), m_selection.affinity());
        } else {
            pos = leftPositionOf(createVisiblePosition(m_selection.extent(), m_selection.affinity()));
        }
        break;
    case WordGranularity: {
        bool skipsSpaceWhenMovingRight = frame() && frame()->editor().behavior().shouldSkipSpaceWhenMovingRight();
        pos = leftWordPosition(createVisiblePosition(m_selection.extent(), m_selection.affinity()), skipsSpaceWhenMovingRight);
        break;
    }
    case SentenceGranularity:
    case LineGranularity:
    case ParagraphGranularity:
    case SentenceBoundary:
    case ParagraphBoundary:
    case DocumentBoundary:
        // FIXME: Implement all of the above.
        pos = modifyMovingBackward(granularity);
        break;
    case LineBoundary:
        pos = leftBoundaryOfLine(startForPlatform(), directionOfEnclosingBlock());
        break;
    }
    return pos;
}

VisiblePosition SelectionModifier::modifyMovingBackward(TextGranularity granularity)
{
    VisiblePosition pos;
    switch (granularity) {
    case CharacterGranularity:
        if (m_selection.isRange())
            pos = createVisiblePosition(m_selection.start(), m_selection.affinity());
        else
            pos = previousPositionOf(createVisiblePosition(m_selection.extent(), m_selection.affinity()), CanSkipOverEditingBoundary);
        break;
    case WordGranularity:
        pos = previousWordPosition(createVisiblePosition(m_selection.extent(), m_selection.affinity()));
        break;
    case SentenceGranularity:
        pos = previousSentencePosition(createVisiblePosition(m_selection.extent(), m_selection.affinity()));
        break;
    case LineGranularity:
        pos = previousLinePosition(startForPlatform(), lineDirectionPointForBlockDirectionNavigation(START));
        break;
    case ParagraphGranularity:
        pos = previousParagraphPosition(startForPlatform(), lineDirectionPointForBlockDirectionNavigation(START));
        break;
    case SentenceBoundary:
        pos = startOfSentence(startForPlatform());
        break;
    case LineBoundary:
        pos = logicalStartOfLine(startForPlatform());
        break;
    case ParagraphBoundary:
        pos = startOfParagraph(startForPlatform());
        break;
    case DocumentBoundary:
        pos = startForPlatform();
        if (isEditablePosition(pos.deepEquivalent()))
            pos = startOfEditableContent(pos);
        else
            pos = startOfDocument(pos);
        break;
    }
    return pos;
}

static bool isBoundary(TextGranularity granularity)
{
    return granularity == LineBoundary || granularity == ParagraphBoundary || granularity == DocumentBoundary;
}

static void setSelectionEnd(VisibleSelection* selection, const VisiblePosition& newEnd)
{
    if (selection->isBaseFirst()) {
        selection->setExtent(newEnd);
        return;
    }
    selection->setBase(newEnd);
}

static void setSelectionStart(VisibleSelection* selection, const VisiblePosition& newStart)
{
    if (selection->isBaseFirst()) {
        selection->setBase(newStart);
        return;
    }
    selection->setExtent(newStart);
}

bool SelectionModifier::modify(EAlteration alter, SelectionDirection direction, TextGranularity granularity)
{
    willBeModified(alter, direction);

    bool wasRange = m_selection.isRange();
    VisiblePosition originalStartPosition = m_selection.visibleStart();
    VisiblePosition position;
    switch (direction) {
    case DirectionRight:
        if (alter == FrameSelection::AlterationMove)
            position = modifyMovingRight(granularity);
        else
            position = modifyExtendingRight(granularity);
        break;
    case DirectionForward:
        if (alter == FrameSelection::AlterationExtend)
            position = modifyExtendingForward(granularity);
        else
            position = modifyMovingForward(granularity);
        break;
    case DirectionLeft:
        if (alter == FrameSelection::AlterationMove)
            position = modifyMovingLeft(granularity);
        else
            position = modifyExtendingLeft(granularity);
        break;
    case DirectionBackward:
        if (alter == FrameSelection::AlterationExtend)
            position = modifyExtendingBackward(granularity);
        else
            position = modifyMovingBackward(granularity);
        break;
    }

    if (position.isNull())
        return false;

    if (isSpatialNavigationEnabled(frame())) {
        if (!wasRange && alter == FrameSelection::AlterationMove && position.deepEquivalent() == originalStartPosition.deepEquivalent())
            return false;
    }

    // Some of the above operations set an xPosForVerticalArrowNavigation.
    // Setting a selection will clear it, so save it to possibly restore later.
    // Note: the START position type is arbitrary because it is unused, it would be
    // the requested position type if there were no xPosForVerticalArrowNavigation set.
    LayoutUnit x = lineDirectionPointForBlockDirectionNavigation(START);
    m_selection.setIsDirectional(shouldAlwaysUseDirectionalSelection(frame()) || alter == FrameSelection::AlterationExtend);

    switch (alter) {
    case FrameSelection::AlterationMove:
        m_selection = VisibleSelection(position, m_selection.isDirectional());
        break;
    case FrameSelection::AlterationExtend:

        if (!m_selection.isCaret()
            && (granularity == WordGranularity || granularity == ParagraphGranularity || granularity == LineGranularity)
            && frame() && !frame()->editor().behavior().shouldExtendSelectionByWordOrLineAcrossCaret()) {
            // Don't let the selection go across the base position directly. Needed to match mac
            // behavior when, for instance, word-selecting backwards starting with the caret in
            // the middle of a word and then word-selecting forward, leaving the caret in the
            // same place where it was, instead of directly selecting to the end of the word.
            VisibleSelection newSelection = m_selection;
            newSelection.setExtent(position);
            if (m_selection.isBaseFirst() != newSelection.isBaseFirst())
                position = m_selection.visibleBase();
        }

        // Standard Mac behavior when extending to a boundary is grow the
        // selection rather than leaving the base in place and moving the
        // extent. Matches NSTextView.
        if (!frame() || !frame()->editor().behavior().shouldAlwaysGrowSelectionWhenExtendingToBoundary()
            || m_selection.isCaret()
            || !isBoundary(granularity)) {
            m_selection.setExtent(position);
        } else {
            TextDirection textDirection = directionOfEnclosingBlock();
            if (direction == DirectionForward || (textDirection == LTR && direction == DirectionRight) || (textDirection == RTL && direction == DirectionLeft))
                setSelectionEnd(&m_selection, position);
            else
                setSelectionStart(&m_selection, position);
        }
        break;
    }

    if (granularity == LineGranularity || granularity == ParagraphGranularity)
        m_xPosForVerticalArrowNavigation = x;

    return true;
}

// TODO(yosin): Maybe baseline would be better?
static bool absoluteCaretY(const VisiblePosition &c, int &y)
{
    IntRect rect = absoluteCaretBoundsOf(c);
    if (rect.isEmpty())
        return false;
    y = rect.y() + rect.height() / 2;
    return true;
}

bool SelectionModifier::modifyWithPageGranularity(EAlteration alter, unsigned verticalDistance, VerticalDirection direction)
{
    if (!verticalDistance)
        return false;

    willBeModified(alter, direction == FrameSelection::DirectionUp ? DirectionBackward : DirectionForward);

    VisiblePosition pos;
    LayoutUnit xPos;
    switch (alter) {
    case FrameSelection::AlterationMove:
        pos = createVisiblePosition(direction == FrameSelection::DirectionUp ? m_selection.start() : m_selection.end(), m_selection.affinity());
        xPos = lineDirectionPointForBlockDirectionNavigation(direction == FrameSelection::DirectionUp ? START : END);
        m_selection.setAffinity(direction == FrameSelection::DirectionUp ? TextAffinity::Upstream : TextAffinity::Downstream);
        break;
    case FrameSelection::AlterationExtend:
        pos = createVisiblePosition(m_selection.extent(), m_selection.affinity());
        xPos = lineDirectionPointForBlockDirectionNavigation(EXTENT);
        m_selection.setAffinity(TextAffinity::Downstream);
        break;
    }

    int startY;
    if (!absoluteCaretY(pos, startY))
        return false;
    if (direction == FrameSelection::DirectionUp)
        startY = -startY;
    int lastY = startY;

    VisiblePosition result;
    VisiblePosition next;
    for (VisiblePosition p = pos; ; p = next) {
        if (direction == FrameSelection::DirectionUp)
            next = previousLinePosition(p, xPos);
        else
            next = nextLinePosition(p, xPos);

        if (next.isNull() || next.deepEquivalent() == p.deepEquivalent())
            break;
        int nextY;
        if (!absoluteCaretY(next, nextY))
            break;
        if (direction == FrameSelection::DirectionUp)
            nextY = -nextY;
        if (nextY - startY > static_cast<int>(verticalDistance))
            break;
        if (nextY >= lastY) {
            lastY = nextY;
            result = next;
        }
    }

    if (result.isNull())
        return false;

    switch (alter) {
    case FrameSelection::AlterationMove:
        m_selection = VisibleSelection(result, m_selection.isDirectional());
        break;
    case FrameSelection::AlterationExtend:
        m_selection.setExtent(result);
        break;
    }

    m_selection.setIsDirectional(shouldAlwaysUseDirectionalSelection(frame()) || alter == FrameSelection::AlterationExtend);

    return true;
}

// Abs x/y position of the caret ignoring transforms.
// TODO(yosin) navigation with transforms should be smarter.
static LayoutUnit lineDirectionPointForBlockDirectionNavigationOf(const VisiblePosition& visiblePosition)
{
    if (visiblePosition.isNull())
        return LayoutUnit();

    LayoutObject* layoutObject;
    LayoutRect localRect = localCaretRectOfPosition(visiblePosition.toPositionWithAffinity(), layoutObject);
    if (localRect.isEmpty() || !layoutObject)
        return LayoutUnit();

    // This ignores transforms on purpose, for now. Vertical navigation is done
    // without consulting transforms, so that 'up' in transformed text is 'up'
    // relative to the text, not absolute 'up'.
    FloatPoint caretPoint = layoutObject->localToAbsolute(FloatPoint(localRect.location()));
    LayoutObject* containingBlock = layoutObject->containingBlock();
    if (!containingBlock)
        containingBlock = layoutObject; // Just use ourselves to determine the writing mode if we have no containing block.
    return LayoutUnit(containingBlock->isHorizontalWritingMode() ? caretPoint.x() : caretPoint.y());
}

LayoutUnit SelectionModifier::lineDirectionPointForBlockDirectionNavigation(EPositionType type)
{
    LayoutUnit x;

    if (m_selection.isNone())
        return x;

    Position pos;
    switch (type) {
    case START:
        pos = m_selection.start();
        break;
    case END:
        pos = m_selection.end();
        break;
    case BASE:
        pos = m_selection.base();
        break;
    case EXTENT:
        pos = m_selection.extent();
        break;
    }

    LocalFrame* frame = pos.document()->frame();
    if (!frame)
        return x;

    if (m_xPosForVerticalArrowNavigation == NoXPosForVerticalArrowNavigation()) {
        VisiblePosition visiblePosition = createVisiblePosition(pos, m_selection.affinity());
        // VisiblePosition creation can fail here if a node containing the selection becomes visibility:hidden
        // after the selection is created and before this function is called.
        x = lineDirectionPointForBlockDirectionNavigationOf(visiblePosition);
        m_xPosForVerticalArrowNavigation = x;
    } else {
        x = m_xPosForVerticalArrowNavigation;
    }

    return x;
}

DEFINE_TRACE(SelectionModifier)
{
    visitor->trace(m_frame);
    visitor->trace(m_selection);
}

} // namespace blink
