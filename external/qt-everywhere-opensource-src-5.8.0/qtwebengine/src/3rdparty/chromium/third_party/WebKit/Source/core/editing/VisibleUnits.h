/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#ifndef VisibleUnits_h
#define VisibleUnits_h

#include "core/CoreExport.h"
#include "core/editing/EditingBoundary.h"
#include "core/editing/PositionWithAffinity.h"
#include "core/editing/VisiblePosition.h"
#include "platform/text/TextDirection.h"

namespace blink {

class LayoutRect;
class LayoutUnit;
class LayoutObject;
class Node;
class IntPoint;
class InlineBox;
class LocalFrame;

enum EWordSide { RightWordIfOnBoundary = false, LeftWordIfOnBoundary = true };

struct InlineBoxPosition {
    InlineBox* inlineBox;
    int offsetInBox;

    InlineBoxPosition()
        : inlineBox(nullptr), offsetInBox(0)
    {
    }

    InlineBoxPosition(InlineBox* inlineBox, int offsetInBox)
        : inlineBox(inlineBox), offsetInBox(offsetInBox)
    {
    }

    bool operator==(const InlineBoxPosition& other) const
    {
        return inlineBox == other.inlineBox && offsetInBox == other.offsetInBox;
    }

    bool operator!=(const InlineBoxPosition& other) const
    {
        return !operator==(other);
    }
};

// The print for |InlineBoxPosition| is available only for testing
// in "webkit_unit_tests", and implemented in
// "core/editing/VisibleUnitsTest.cpp".
std::ostream& operator<<(std::ostream&, const InlineBoxPosition&);

CORE_EXPORT LayoutObject* associatedLayoutObjectOf(const Node&, int offsetInNode);

// offset functions on Node
CORE_EXPORT int caretMinOffset(const Node*);
CORE_EXPORT int caretMaxOffset(const Node*);

// Position
// mostForward/BackwardCaretPosition are used for moving back and forth between
// visually equivalent candidates.
// For example, for the text node "foo     bar" where whitespace is
// collapsible, there are two candidates that map to the VisiblePosition
// between 'b' and the space, after first space and before last space.
//
// mostBackwardCaretPosition returns the left candidate and also returs
// [boundary, 0] for any of the positions from [boundary, 0] to the first
// candidate in boundary, where
// endsOfNodeAreVisuallyDistinctPositions(boundary) is true.
//
// mostForwardCaretPosition() returns the right one and also returns the
// last position in the last atomic node in boundary for all of the positions
// in boundary after the last candidate, where
// endsOfNodeAreVisuallyDistinctPositions(boundary).
// FIXME: This function should never be called when the line box tree is dirty.
// See https://bugs.webkit.org/show_bug.cgi?id=97264
CORE_EXPORT Position mostBackwardCaretPosition(const Position&, EditingBoundaryCrossingRule = CannotCrossEditingBoundary);
CORE_EXPORT PositionInFlatTree mostBackwardCaretPosition(const PositionInFlatTree&, EditingBoundaryCrossingRule = CannotCrossEditingBoundary);
CORE_EXPORT Position mostForwardCaretPosition(const Position&, EditingBoundaryCrossingRule = CannotCrossEditingBoundary);
CORE_EXPORT PositionInFlatTree mostForwardCaretPosition(const PositionInFlatTree&, EditingBoundaryCrossingRule = CannotCrossEditingBoundary);

CORE_EXPORT bool isVisuallyEquivalentCandidate(const Position&);
CORE_EXPORT bool isVisuallyEquivalentCandidate(const PositionInFlatTree&);

// Whether or not [node, 0] and [node, lastOffsetForEditing(node)] are their own VisiblePositions.
// If true, adjacent candidates are visually distinct.
CORE_EXPORT bool endsOfNodeAreVisuallyDistinctPositions(const Node*);

CORE_EXPORT Position canonicalPositionOf(const Position&);
CORE_EXPORT PositionInFlatTree canonicalPositionOf(const PositionInFlatTree&);

// Bounds of (possibly transformed) caret in absolute coords
CORE_EXPORT IntRect absoluteCaretBoundsOf(const VisiblePosition&);
CORE_EXPORT IntRect absoluteCaretBoundsOf(const VisiblePositionInFlatTree&);

CORE_EXPORT UChar32 characterAfter(const VisiblePosition&);
CORE_EXPORT UChar32 characterAfter(const VisiblePositionInFlatTree&);
CORE_EXPORT UChar32 characterBefore(const VisiblePosition&);
CORE_EXPORT UChar32 characterBefore(const VisiblePositionInFlatTree&);

// TODO(yosin) Since return value of |leftPositionOf()| with |VisiblePosition|
// isn't defined well on flat tree, we should not use it for a position in
// flat tree.
CORE_EXPORT VisiblePosition leftPositionOf(const VisiblePosition&);
CORE_EXPORT VisiblePositionInFlatTree leftPositionOf(const VisiblePositionInFlatTree&);
// TODO(yosin) Since return value of |rightPositionOf()| with |VisiblePosition|
// isn't defined well on flat tree, we should not use it for a position in
// flat tree.
CORE_EXPORT VisiblePosition rightPositionOf(const VisiblePosition&);
CORE_EXPORT VisiblePositionInFlatTree rightPositionOf(const VisiblePositionInFlatTree&);

CORE_EXPORT VisiblePosition nextPositionOf(const VisiblePosition&, EditingBoundaryCrossingRule = CanCrossEditingBoundary);
CORE_EXPORT VisiblePositionInFlatTree nextPositionOf(const VisiblePositionInFlatTree&, EditingBoundaryCrossingRule = CanCrossEditingBoundary);
CORE_EXPORT VisiblePosition previousPositionOf(const VisiblePosition&, EditingBoundaryCrossingRule = CanCrossEditingBoundary);
CORE_EXPORT VisiblePositionInFlatTree previousPositionOf(const VisiblePositionInFlatTree&, EditingBoundaryCrossingRule = CanCrossEditingBoundary);

// words
CORE_EXPORT VisiblePosition startOfWord(const VisiblePosition&, EWordSide = RightWordIfOnBoundary);
CORE_EXPORT VisiblePositionInFlatTree startOfWord(const VisiblePositionInFlatTree&, EWordSide = RightWordIfOnBoundary);
CORE_EXPORT VisiblePosition endOfWord(const VisiblePosition&, EWordSide = RightWordIfOnBoundary);
CORE_EXPORT VisiblePositionInFlatTree endOfWord(const VisiblePositionInFlatTree&, EWordSide = RightWordIfOnBoundary);
VisiblePosition previousWordPosition(const VisiblePosition&);
VisiblePosition nextWordPosition(const VisiblePosition&);
VisiblePosition rightWordPosition(const VisiblePosition&, bool skipsSpaceWhenMovingRight);
VisiblePosition leftWordPosition(const VisiblePosition&, bool skipsSpaceWhenMovingRight);

// sentences
CORE_EXPORT VisiblePosition startOfSentence(const VisiblePosition&);
CORE_EXPORT VisiblePositionInFlatTree startOfSentence(const VisiblePositionInFlatTree&);
CORE_EXPORT VisiblePosition endOfSentence(const VisiblePosition&);
CORE_EXPORT VisiblePositionInFlatTree endOfSentence(const VisiblePositionInFlatTree&);
VisiblePosition previousSentencePosition(const VisiblePosition&);
VisiblePosition nextSentencePosition(const VisiblePosition&);

// lines
// TODO(yosin) Return values of |VisiblePosition| version of |startOfLine()|
// with shadow tree isn't defined well. We should not use it for shadow tree.
CORE_EXPORT VisiblePosition startOfLine(const VisiblePosition&);
CORE_EXPORT VisiblePositionInFlatTree startOfLine(const VisiblePositionInFlatTree&);
// TODO(yosin) Return values of |VisiblePosition| version of |endOfLine()| with
// shadow tree isn't defined well. We should not use it for shadow tree.
CORE_EXPORT VisiblePosition endOfLine(const VisiblePosition&);
CORE_EXPORT VisiblePositionInFlatTree endOfLine(const VisiblePositionInFlatTree&);
CORE_EXPORT VisiblePosition previousLinePosition(const VisiblePosition&, LayoutUnit lineDirectionPoint, EditableType = ContentIsEditable);
CORE_EXPORT VisiblePosition nextLinePosition(const VisiblePosition&, LayoutUnit lineDirectionPoint, EditableType = ContentIsEditable);
CORE_EXPORT bool inSameLine(const VisiblePosition&, const VisiblePosition&);
CORE_EXPORT bool inSameLine(const VisiblePositionInFlatTree&, const VisiblePositionInFlatTree&);
CORE_EXPORT bool inSameLine(const PositionWithAffinity&, const PositionWithAffinity&);
CORE_EXPORT bool inSameLine(const PositionInFlatTreeWithAffinity&, const PositionInFlatTreeWithAffinity&);
CORE_EXPORT bool isStartOfLine(const VisiblePosition&);
CORE_EXPORT bool isStartOfLine(const VisiblePositionInFlatTree&);
CORE_EXPORT bool isEndOfLine(const VisiblePosition&);
CORE_EXPORT bool isEndOfLine(const VisiblePositionInFlatTree&);
// TODO(yosin) Return values of |VisiblePosition| version of
// |logicalStartOfLine()| with shadow tree isn't defined well. We should not use
// it for shadow tree.
CORE_EXPORT VisiblePosition logicalStartOfLine(const VisiblePosition&);
CORE_EXPORT VisiblePositionInFlatTree logicalStartOfLine(const VisiblePositionInFlatTree&);
// TODO(yosin) Return values of |VisiblePosition| version of
// |logicalEndOfLine()| with shadow tree isn't defined well. We should not use
// it for shadow tree.
CORE_EXPORT VisiblePosition logicalEndOfLine(const VisiblePosition&);
CORE_EXPORT VisiblePositionInFlatTree logicalEndOfLine(const VisiblePositionInFlatTree&);
CORE_EXPORT bool isLogicalEndOfLine(const VisiblePosition&);
CORE_EXPORT bool isLogicalEndOfLine(const VisiblePositionInFlatTree&);
VisiblePosition leftBoundaryOfLine(const VisiblePosition&, TextDirection);
VisiblePosition rightBoundaryOfLine(const VisiblePosition&, TextDirection);

// paragraphs (perhaps a misnomer, can be divided by line break elements)
// TODO(yosin) Since return value of |startOfParagraph()| with |VisiblePosition|
// isn't defined well on flat tree, we should not use it for a position in
// flat tree.
CORE_EXPORT VisiblePosition startOfParagraph(const VisiblePosition&, EditingBoundaryCrossingRule = CannotCrossEditingBoundary);
CORE_EXPORT VisiblePositionInFlatTree startOfParagraph(const VisiblePositionInFlatTree&, EditingBoundaryCrossingRule = CannotCrossEditingBoundary);
CORE_EXPORT VisiblePosition endOfParagraph(const VisiblePosition&, EditingBoundaryCrossingRule = CannotCrossEditingBoundary);
CORE_EXPORT VisiblePositionInFlatTree endOfParagraph(const VisiblePositionInFlatTree&, EditingBoundaryCrossingRule = CannotCrossEditingBoundary);
VisiblePosition startOfNextParagraph(const VisiblePosition&);
VisiblePosition previousParagraphPosition(const VisiblePosition&, LayoutUnit x);
VisiblePosition nextParagraphPosition(const VisiblePosition&, LayoutUnit x);
CORE_EXPORT bool isStartOfParagraph(const VisiblePosition&, EditingBoundaryCrossingRule = CannotCrossEditingBoundary);
CORE_EXPORT bool isStartOfParagraph(const VisiblePositionInFlatTree&, EditingBoundaryCrossingRule = CannotCrossEditingBoundary);
CORE_EXPORT bool isEndOfParagraph(const VisiblePosition&, EditingBoundaryCrossingRule = CannotCrossEditingBoundary);
CORE_EXPORT bool isEndOfParagraph(const VisiblePositionInFlatTree&, EditingBoundaryCrossingRule = CannotCrossEditingBoundary);
bool inSameParagraph(const VisiblePosition&, const VisiblePosition&, EditingBoundaryCrossingRule = CannotCrossEditingBoundary);

// blocks (true paragraphs; line break elements don't break blocks)
VisiblePosition startOfBlock(const VisiblePosition&, EditingBoundaryCrossingRule = CannotCrossEditingBoundary);
VisiblePosition endOfBlock(const VisiblePosition&, EditingBoundaryCrossingRule = CannotCrossEditingBoundary);
bool inSameBlock(const VisiblePosition&, const VisiblePosition&);
bool isStartOfBlock(const VisiblePosition&);
bool isEndOfBlock(const VisiblePosition&);

// document
CORE_EXPORT VisiblePosition startOfDocument(const VisiblePosition&);
CORE_EXPORT VisiblePositionInFlatTree startOfDocument(const VisiblePositionInFlatTree&);
CORE_EXPORT VisiblePosition endOfDocument(const VisiblePosition&);
CORE_EXPORT VisiblePositionInFlatTree endOfDocument(const VisiblePositionInFlatTree&);
bool isStartOfDocument(const VisiblePosition&);
bool isEndOfDocument(const VisiblePosition&);

// editable content
VisiblePosition startOfEditableContent(const VisiblePosition&);
VisiblePosition endOfEditableContent(const VisiblePosition&);
CORE_EXPORT bool isEndOfEditableOrNonEditableContent(const VisiblePosition&);
CORE_EXPORT bool isEndOfEditableOrNonEditableContent(const VisiblePositionInFlatTree&);

CORE_EXPORT InlineBoxPosition computeInlineBoxPosition(const Position&, TextAffinity);
CORE_EXPORT InlineBoxPosition computeInlineBoxPosition(const Position&, TextAffinity, TextDirection primaryDirection);
CORE_EXPORT InlineBoxPosition computeInlineBoxPosition(const PositionInFlatTree&, TextAffinity);
CORE_EXPORT InlineBoxPosition computeInlineBoxPosition(const PositionInFlatTree&, TextAffinity, TextDirection primaryDirection);
CORE_EXPORT InlineBoxPosition computeInlineBoxPosition(const VisiblePosition&);
CORE_EXPORT InlineBoxPosition computeInlineBoxPosition(const VisiblePositionInFlatTree&);

// Rect is local to the returned layoutObject
CORE_EXPORT LayoutRect localCaretRectOfPosition(const PositionWithAffinity&, LayoutObject*&);
CORE_EXPORT LayoutRect localCaretRectOfPosition(const PositionInFlatTreeWithAffinity&, LayoutObject*&);
bool hasRenderedNonAnonymousDescendantsWithHeight(LayoutObject*);

// Returns a hit-tested VisiblePosition for the given point in contents-space
// coordinates.
CORE_EXPORT VisiblePosition visiblePositionForContentsPoint(const IntPoint&, LocalFrame*);

CORE_EXPORT bool rendersInDifferentPosition(const Position&, const Position&);

} // namespace blink

#endif // VisibleUnits_h
