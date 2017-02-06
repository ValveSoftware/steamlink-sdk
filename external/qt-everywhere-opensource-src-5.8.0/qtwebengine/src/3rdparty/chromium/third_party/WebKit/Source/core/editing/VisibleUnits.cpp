/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#include "core/editing/VisibleUnits.h"

#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/FirstLetterPseudoElement.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/Text.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/Position.h"
#include "core/editing/PositionIterator.h"
#include "core/editing/RenderedPosition.h"
#include "core/editing/TextAffinity.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/iterators/BackwardsCharacterIterator.h"
#include "core/editing/iterators/BackwardsTextBuffer.h"
#include "core/editing/iterators/CharacterIterator.h"
#include "core/editing/iterators/ForwardsTextBuffer.h"
#include "core/editing/iterators/SimplifiedBackwardsTextIterator.h"
#include "core/editing/iterators/TextIterator.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLBRElement.h"
#include "core/html/HTMLTextFormControlElement.h"
#include "core/layout/HitTestRequest.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutTextFragment.h"
#include "core/layout/LayoutView.h"
#include "core/layout/api/LayoutItem.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/layout/api/LineLayoutAPIShim.h"
#include "core/layout/api/LineLayoutItem.h"
#include "core/layout/line/InlineIterator.h"
#include "core/layout/line/InlineTextBox.h"
#include "platform/Logging.h"
#include "platform/heap/Handle.h"
#include "platform/text/TextBoundaries.h"
#include "platform/text/TextBreakIterator.h"

namespace blink {

template <typename PositionType>
static PositionType canonicalizeCandidate(const PositionType& candidate)
{
    if (candidate.isNull())
        return PositionType();
    DCHECK(isVisuallyEquivalentCandidate(candidate));
    PositionType upstream = mostBackwardCaretPosition(candidate);
    if (isVisuallyEquivalentCandidate(upstream))
        return upstream;
    return candidate;
}

template <typename PositionType>
static PositionType canonicalPosition(const PositionType& passedPosition)
{
    // Sometimes updating selection positions can be extremely expensive and
    // occur frequently.  Often calling preventDefault on mousedown events can
    // avoid doing unnecessary text selection work.  http://crbug.com/472258.
    TRACE_EVENT0("input", "VisibleUnits::canonicalPosition");

    // The updateLayout call below can do so much that even the position passed
    // in to us might get changed as a side effect. Specifically, there are code
    // paths that pass selection endpoints, and updateLayout can change the
    // selection.
    PositionType position = passedPosition;

    // FIXME (9535):  Canonicalizing to the leftmost candidate means that if
    // we're at a line wrap, we will ask layoutObjects to paint downstream
    // carets for other layoutObjects. To fix this, we need to either a) add
    // code to all paintCarets to pass the responsibility off to the appropriate
    // layoutObject for VisiblePosition's like these, or b) canonicalize to the
    // rightmost candidate unless the affinity is upstream.
    if (position.isNull())
        return PositionType();

    DCHECK(position.document());
    position.document()->updateStyleAndLayoutIgnorePendingStylesheets();

    Node* node = position.computeContainerNode();

    PositionType candidate = mostBackwardCaretPosition(position);
    if (isVisuallyEquivalentCandidate(candidate))
        return candidate;
    candidate = mostForwardCaretPosition(position);
    if (isVisuallyEquivalentCandidate(candidate))
        return candidate;

    // When neither upstream or downstream gets us to a candidate
    // (upstream/downstream won't leave blocks or enter new ones), we search
    // forward and backward until we find one.
    PositionType next = canonicalizeCandidate(nextCandidate(position));
    PositionType prev = canonicalizeCandidate(previousCandidate(position));
    Node* nextNode = next.anchorNode();
    Node* prevNode = prev.anchorNode();

    // The new position must be in the same editable element. Enforce that
    // first. Unless the descent is from a non-editable html element to an
    // editable body.
    if (node && node->document().documentElement() == node && !node->hasEditableStyle() && node->document().body() && node->document().body()->hasEditableStyle())
        return next.isNotNull() ? next : prev;

    Element* editingRoot = rootEditableElementOf(position);

    // If the html element is editable, descending into its body will look like
    // a descent from non-editable to editable content since
    // |rootEditableElementOf()| always stops at the body.
    if ((editingRoot && editingRoot->document().documentElement() == editingRoot) || position.anchorNode()->isDocumentNode())
        return next.isNotNull() ? next : prev;

    bool prevIsInSameEditableElement = prevNode && rootEditableElementOf(prev) == editingRoot;
    bool nextIsInSameEditableElement = nextNode && rootEditableElementOf(next) == editingRoot;
    if (prevIsInSameEditableElement && !nextIsInSameEditableElement)
        return prev;

    if (nextIsInSameEditableElement && !prevIsInSameEditableElement)
        return next;

    if (!nextIsInSameEditableElement && !prevIsInSameEditableElement)
        return PositionType();

    // The new position should be in the same block flow element. Favor that.
    Element* originalBlock = node ? enclosingBlockFlowElement(*node) : 0;
    bool nextIsOutsideOriginalBlock = !nextNode->isDescendantOf(originalBlock) && nextNode != originalBlock;
    bool prevIsOutsideOriginalBlock = !prevNode->isDescendantOf(originalBlock) && prevNode != originalBlock;
    if (nextIsOutsideOriginalBlock && !prevIsOutsideOriginalBlock)
        return prev;

    return next;
}

Position canonicalPositionOf(const Position& position)
{
    return canonicalPosition(position);
}

PositionInFlatTree canonicalPositionOf(const PositionInFlatTree& position)
{
    return canonicalPosition(position);
}

template <typename Strategy>
static PositionWithAffinityTemplate<Strategy> honorEditingBoundaryAtOrBefore(const PositionWithAffinityTemplate<Strategy>& pos, const PositionTemplate<Strategy>& anchor)
{
    if (pos.isNull())
        return pos;

    ContainerNode* highestRoot = highestEditableRoot(anchor);

    // Return empty position if |pos| is not somewhere inside the editable
    // region containing this position
    if (highestRoot && !pos.position().anchorNode()->isDescendantOf(highestRoot))
        return PositionWithAffinityTemplate<Strategy>();

    // Return |pos| itself if the two are from the very same editable region, or
    // both are non-editable
    // TODO(yosin) In the non-editable case, just because the new position is
    // non-editable doesn't mean movement to it is allowed.
    // |VisibleSelection::adjustForEditableContent()| has this problem too.
    if (highestEditableRoot(pos.position()) == highestRoot)
        return pos;

    // Return empty position if this position is non-editable, but |pos| is
    // editable.
    // TODO(yosin) Move to the previous non-editable region.
    if (!highestRoot)
        return PositionWithAffinityTemplate<Strategy>();

    // Return the last position before |pos| that is in the same editable region
    // as this position
    return lastEditablePositionBeforePositionInRoot(pos.position(), *highestRoot);
}

template <typename Strategy>
static VisiblePositionTemplate<Strategy> honorEditingBoundaryAtOrBefore(const VisiblePositionTemplate<Strategy>& pos, const PositionTemplate<Strategy>& anchor)
{
    return createVisiblePosition(honorEditingBoundaryAtOrBefore(pos.toPositionWithAffinity(), anchor));
}

template <typename Strategy>
static VisiblePositionTemplate<Strategy> honorEditingBoundaryAtOrAfter(const VisiblePositionTemplate<Strategy>& pos, const PositionTemplate<Strategy>& anchor)
{
    if (pos.isNull())
        return pos;

    ContainerNode* highestRoot = highestEditableRoot(anchor);

    // Return empty position if |pos| is not somewhere inside the editable
    // region containing this position
    if (highestRoot && !pos.deepEquivalent().anchorNode()->isDescendantOf(highestRoot))
        return VisiblePositionTemplate<Strategy>();

    // Return |pos| itself if the two are from the very same editable region, or
    // both are non-editable
    // TODO(yosin) In the non-editable case, just because the new position is
    // non-editable doesn't mean movement to it is allowed.
    // |VisibleSelection::adjustForEditableContent()| has this problem too.
    if (highestEditableRoot(pos.deepEquivalent()) == highestRoot)
        return pos;

    // Return empty position if this position is non-editable, but |pos| is
    // editable.
    // TODO(yosin) Move to the next non-editable region.
    if (!highestRoot)
        return VisiblePositionTemplate<Strategy>();

    // Return the next position after |pos| that is in the same editable region
    // as this position
    return firstEditableVisiblePositionAfterPositionInRoot(pos.deepEquivalent(), *highestRoot);
}

static Node* previousLeafWithSameEditability(Node* node, EditableType editableType)
{
    bool editable = node->hasEditableStyle(editableType);
    node = previousAtomicLeafNode(*node);
    while (node) {
        if (editable == node->hasEditableStyle(editableType))
            return node;
        node = previousAtomicLeafNode(*node);
    }
    return 0;
}

static Node* nextLeafWithSameEditability(Node* node, EditableType editableType = ContentIsEditable)
{
    if (!node)
        return 0;

    bool editable = node->hasEditableStyle(editableType);
    node = nextAtomicLeafNode(*node);
    while (node) {
        if (editable == node->hasEditableStyle(editableType))
            return node;
        node = nextAtomicLeafNode(*node);
    }
    return 0;
}

// FIXME: consolidate with code in previousLinePosition.
static Position previousRootInlineBoxCandidatePosition(Node* node, const VisiblePosition& visiblePosition, EditableType editableType)
{
    ContainerNode* highestRoot = highestEditableRoot(visiblePosition.deepEquivalent(), editableType);
    Node* previousNode = previousLeafWithSameEditability(node, editableType);

    while (previousNode && (!previousNode->layoutObject() || inSameLine(createVisiblePosition(firstPositionInOrBeforeNode(previousNode)), visiblePosition)))
        previousNode = previousLeafWithSameEditability(previousNode, editableType);

    while (previousNode && !previousNode->isShadowRoot()) {
        if (highestEditableRoot(firstPositionInOrBeforeNode(previousNode), editableType) != highestRoot)
            break;

        Position pos = isHTMLBRElement(*previousNode) ? Position::beforeNode(previousNode) :
            Position::editingPositionOf(previousNode, caretMaxOffset(previousNode));

        if (isVisuallyEquivalentCandidate(pos))
            return pos;

        previousNode = previousLeafWithSameEditability(previousNode, editableType);
    }
    return Position();
}

static Position nextRootInlineBoxCandidatePosition(Node* node, const VisiblePosition& visiblePosition, EditableType editableType)
{
    ContainerNode* highestRoot = highestEditableRoot(visiblePosition.deepEquivalent(), editableType);
    Node* nextNode = nextLeafWithSameEditability(node, editableType);
    while (nextNode && (!nextNode->layoutObject() || inSameLine(createVisiblePosition(firstPositionInOrBeforeNode(nextNode)), visiblePosition)))
        nextNode = nextLeafWithSameEditability(nextNode, ContentIsEditable);

    while (nextNode && !nextNode->isShadowRoot()) {
        if (highestEditableRoot(firstPositionInOrBeforeNode(nextNode), editableType) != highestRoot)
            break;

        Position pos;
        pos = Position::editingPositionOf(nextNode, caretMinOffset(nextNode));

        if (isVisuallyEquivalentCandidate(pos))
            return pos;

        nextNode = nextLeafWithSameEditability(nextNode, editableType);
    }
    return Position();
}

class CachedLogicallyOrderedLeafBoxes {
public:
    CachedLogicallyOrderedLeafBoxes();

    const InlineTextBox* previousTextBox(const RootInlineBox*, const InlineTextBox*);
    const InlineTextBox* nextTextBox(const RootInlineBox*, const InlineTextBox*);

    size_t size() const { return m_leafBoxes.size(); }
    const InlineBox* firstBox() const { return m_leafBoxes[0]; }

private:
    const Vector<InlineBox*>& collectBoxes(const RootInlineBox*);
    int boxIndexInLeaves(const InlineTextBox*) const;

    const RootInlineBox* m_rootInlineBox;
    Vector<InlineBox*> m_leafBoxes;
};

CachedLogicallyOrderedLeafBoxes::CachedLogicallyOrderedLeafBoxes() : m_rootInlineBox(0) { }

const InlineTextBox* CachedLogicallyOrderedLeafBoxes::previousTextBox(const RootInlineBox* root, const InlineTextBox* box)
{
    if (!root)
        return 0;

    collectBoxes(root);

    // If box is null, root is box's previous RootInlineBox, and previousBox is the last logical box in root.
    int boxIndex = m_leafBoxes.size() - 1;
    if (box)
        boxIndex = boxIndexInLeaves(box) - 1;

    for (int i = boxIndex; i >= 0; --i) {
        if (m_leafBoxes[i]->isInlineTextBox())
            return toInlineTextBox(m_leafBoxes[i]);
    }

    return 0;
}

const InlineTextBox* CachedLogicallyOrderedLeafBoxes::nextTextBox(const RootInlineBox* root, const InlineTextBox* box)
{
    if (!root)
        return 0;

    collectBoxes(root);

    // If box is null, root is box's next RootInlineBox, and nextBox is the first logical box in root.
    // Otherwise, root is box's RootInlineBox, and nextBox is the next logical box in the same line.
    size_t nextBoxIndex = 0;
    if (box)
        nextBoxIndex = boxIndexInLeaves(box) + 1;

    for (size_t i = nextBoxIndex; i < m_leafBoxes.size(); ++i) {
        if (m_leafBoxes[i]->isInlineTextBox())
            return toInlineTextBox(m_leafBoxes[i]);
    }

    return 0;
}

const Vector<InlineBox*>& CachedLogicallyOrderedLeafBoxes::collectBoxes(const RootInlineBox* root)
{
    if (m_rootInlineBox != root) {
        m_rootInlineBox = root;
        m_leafBoxes.clear();
        root->collectLeafBoxesInLogicalOrder(m_leafBoxes);
    }
    return m_leafBoxes;
}

int CachedLogicallyOrderedLeafBoxes::boxIndexInLeaves(const InlineTextBox* box) const
{
    for (size_t i = 0; i < m_leafBoxes.size(); ++i) {
        if (box == m_leafBoxes[i])
            return i;
    }
    return 0;
}

static const InlineTextBox* logicallyPreviousBox(const VisiblePosition& visiblePosition, const InlineTextBox* textBox,
    bool& previousBoxInDifferentBlock, CachedLogicallyOrderedLeafBoxes& leafBoxes)
{
    const InlineBox* startBox = textBox;

    const InlineTextBox* previousBox = leafBoxes.previousTextBox(&startBox->root(), textBox);
    if (previousBox)
        return previousBox;

    previousBox = leafBoxes.previousTextBox(startBox->root().prevRootBox(), 0);
    if (previousBox)
        return previousBox;

    while (1) {
        Node* startNode = startBox->getLineLayoutItem().nonPseudoNode();
        if (!startNode)
            break;

        Position position = previousRootInlineBoxCandidatePosition(startNode, visiblePosition, ContentIsEditable);
        if (position.isNull())
            break;

        RenderedPosition renderedPosition(position, TextAffinity::Downstream);
        RootInlineBox* previousRoot = renderedPosition.rootBox();
        if (!previousRoot)
            break;

        previousBox = leafBoxes.previousTextBox(previousRoot, 0);
        if (previousBox) {
            previousBoxInDifferentBlock = true;
            return previousBox;
        }

        if (!leafBoxes.size())
            break;
        startBox = leafBoxes.firstBox();
    }
    return 0;
}


static const InlineTextBox* logicallyNextBox(const VisiblePosition& visiblePosition, const InlineTextBox* textBox,
    bool& nextBoxInDifferentBlock, CachedLogicallyOrderedLeafBoxes& leafBoxes)
{
    const InlineBox* startBox = textBox;

    const InlineTextBox* nextBox = leafBoxes.nextTextBox(&startBox->root(), textBox);
    if (nextBox)
        return nextBox;

    nextBox = leafBoxes.nextTextBox(startBox->root().nextRootBox(), 0);
    if (nextBox)
        return nextBox;

    while (1) {
        Node* startNode =startBox->getLineLayoutItem().nonPseudoNode();
        if (!startNode)
            break;

        Position position = nextRootInlineBoxCandidatePosition(startNode, visiblePosition, ContentIsEditable);
        if (position.isNull())
            break;

        RenderedPosition renderedPosition(position, TextAffinity::Downstream);
        RootInlineBox* nextRoot = renderedPosition.rootBox();
        if (!nextRoot)
            break;

        nextBox = leafBoxes.nextTextBox(nextRoot, 0);
        if (nextBox) {
            nextBoxInDifferentBlock = true;
            return nextBox;
        }

        if (!leafBoxes.size())
            break;
        startBox = leafBoxes.firstBox();
    }
    return 0;
}

static TextBreakIterator* wordBreakIteratorForMinOffsetBoundary(const VisiblePosition& visiblePosition, const InlineTextBox* textBox,
    int& previousBoxLength, bool& previousBoxInDifferentBlock, Vector<UChar, 1024>& string, CachedLogicallyOrderedLeafBoxes& leafBoxes)
{
    previousBoxInDifferentBlock = false;

    // FIXME: Handle the case when we don't have an inline text box.
    const InlineTextBox* previousBox = logicallyPreviousBox(visiblePosition, textBox, previousBoxInDifferentBlock, leafBoxes);

    int len = 0;
    string.clear();
    if (previousBox) {
        previousBoxLength = previousBox->len();
        previousBox->getLineLayoutItem().text().appendTo(string, previousBox->start(), previousBoxLength);
        len += previousBoxLength;
    }
    textBox->getLineLayoutItem().text().appendTo(string, textBox->start(), textBox->len());
    len += textBox->len();

    return wordBreakIterator(string.data(), len);
}

static TextBreakIterator* wordBreakIteratorForMaxOffsetBoundary(const VisiblePosition& visiblePosition, const InlineTextBox* textBox,
    bool& nextBoxInDifferentBlock, Vector<UChar, 1024>& string, CachedLogicallyOrderedLeafBoxes& leafBoxes)
{
    nextBoxInDifferentBlock = false;

    // FIXME: Handle the case when we don't have an inline text box.
    const InlineTextBox* nextBox = logicallyNextBox(visiblePosition, textBox, nextBoxInDifferentBlock, leafBoxes);

    int len = 0;
    string.clear();
    textBox->getLineLayoutItem().text().appendTo(string, textBox->start(), textBox->len());
    len += textBox->len();
    if (nextBox) {
        nextBox->getLineLayoutItem().text().appendTo(string, nextBox->start(), nextBox->len());
        len += nextBox->len();
    }

    return wordBreakIterator(string.data(), len);
}

static bool isLogicalStartOfWord(TextBreakIterator* iter, int position, bool hardLineBreak)
{
    bool boundary = hardLineBreak ? true : iter->isBoundary(position);
    if (!boundary)
        return false;

    iter->following(position);
    // isWordTextBreak returns true after moving across a word and false after moving across a punctuation/space.
    return isWordTextBreak(iter);
}

static bool islogicalEndOfWord(TextBreakIterator* iter, int position, bool hardLineBreak)
{
    bool boundary = iter->isBoundary(position);
    return (hardLineBreak || boundary) && isWordTextBreak(iter);
}

enum CursorMovementDirection { MoveLeft, MoveRight };

static VisiblePosition visualWordPosition(const VisiblePosition& visiblePosition, CursorMovementDirection direction,
    bool skipsSpaceWhenMovingRight)
{
    if (visiblePosition.isNull())
        return VisiblePosition();

    TextDirection blockDirection = directionOfEnclosingBlock(visiblePosition.deepEquivalent());
    InlineBox* previouslyVisitedBox = 0;
    VisiblePosition current = visiblePosition;
    TextBreakIterator* iter = 0;

    CachedLogicallyOrderedLeafBoxes leafBoxes;
    Vector<UChar, 1024> string;

    while (1) {
        VisiblePosition adjacentCharacterPosition = direction == MoveRight ? rightPositionOf(current) : leftPositionOf(current);
        if (adjacentCharacterPosition.deepEquivalent() == current.deepEquivalent() || adjacentCharacterPosition.isNull())
            return VisiblePosition();

        InlineBoxPosition boxPosition = computeInlineBoxPosition(adjacentCharacterPosition.deepEquivalent(), TextAffinity::Upstream);
        InlineBox* box = boxPosition.inlineBox;
        int offsetInBox = boxPosition.offsetInBox;

        if (!box)
            break;
        if (!box->isInlineTextBox()) {
            current = adjacentCharacterPosition;
            continue;
        }

        InlineTextBox* textBox = toInlineTextBox(box);
        int previousBoxLength = 0;
        bool previousBoxInDifferentBlock = false;
        bool nextBoxInDifferentBlock = false;
        bool movingIntoNewBox = previouslyVisitedBox != box;

        if (offsetInBox == box->caretMinOffset()) {
            iter = wordBreakIteratorForMinOffsetBoundary(visiblePosition, textBox, previousBoxLength, previousBoxInDifferentBlock, string, leafBoxes);
        } else if (offsetInBox == box->caretMaxOffset()) {
            iter = wordBreakIteratorForMaxOffsetBoundary(visiblePosition, textBox, nextBoxInDifferentBlock, string, leafBoxes);
        } else if (movingIntoNewBox) {
            iter = wordBreakIterator(textBox->getLineLayoutItem().text(), textBox->start(), textBox->len());
            previouslyVisitedBox = box;
        }

        if (!iter)
            break;

        iter->first();
        int offsetInIterator = offsetInBox - textBox->start() + previousBoxLength;

        bool isWordBreak;
        bool boxHasSameDirectionalityAsBlock = box->direction() == blockDirection;
        bool movingBackward = (direction == MoveLeft && box->direction() == LTR) || (direction == MoveRight && box->direction() == RTL);
        if ((skipsSpaceWhenMovingRight && boxHasSameDirectionalityAsBlock)
            || (!skipsSpaceWhenMovingRight && movingBackward)) {
            bool logicalStartInLayoutObject = offsetInBox == static_cast<int>(textBox->start()) && previousBoxInDifferentBlock;
            isWordBreak = isLogicalStartOfWord(iter, offsetInIterator, logicalStartInLayoutObject);
        } else {
            bool logicalEndInLayoutObject = offsetInBox == static_cast<int>(textBox->start() + textBox->len()) && nextBoxInDifferentBlock;
            isWordBreak = islogicalEndOfWord(iter, offsetInIterator, logicalEndInLayoutObject);
        }

        if (isWordBreak)
            return adjacentCharacterPosition;

        current = adjacentCharacterPosition;
    }
    return VisiblePosition();
}

VisiblePosition leftWordPosition(const VisiblePosition& visiblePosition, bool skipsSpaceWhenMovingRight)
{
    VisiblePosition leftWordBreak = visualWordPosition(visiblePosition, MoveLeft, skipsSpaceWhenMovingRight);
    leftWordBreak = honorEditingBoundaryAtOrBefore(leftWordBreak, visiblePosition.deepEquivalent());

    // FIXME: How should we handle a non-editable position?
    if (leftWordBreak.isNull() && isEditablePosition(visiblePosition.deepEquivalent())) {
        TextDirection blockDirection = directionOfEnclosingBlock(visiblePosition.deepEquivalent());
        leftWordBreak = blockDirection == LTR ? startOfEditableContent(visiblePosition) : endOfEditableContent(visiblePosition);
    }
    return leftWordBreak;
}

VisiblePosition rightWordPosition(const VisiblePosition& visiblePosition, bool skipsSpaceWhenMovingRight)
{
    VisiblePosition rightWordBreak = visualWordPosition(visiblePosition, MoveRight, skipsSpaceWhenMovingRight);
    rightWordBreak = honorEditingBoundaryAtOrBefore(rightWordBreak, visiblePosition.deepEquivalent());

    // FIXME: How should we handle a non-editable position?
    if (rightWordBreak.isNull() && isEditablePosition(visiblePosition.deepEquivalent())) {
        TextDirection blockDirection = directionOfEnclosingBlock(visiblePosition.deepEquivalent());
        rightWordBreak = blockDirection == LTR ? endOfEditableContent(visiblePosition) : startOfEditableContent(visiblePosition);
    }
    return rightWordBreak;
}

template <typename Strategy>
static ContainerNode* nonShadowBoundaryParentNode(Node* node)
{
    ContainerNode* parent = Strategy::parent(*node);
    return parent && !parent->isShadowRoot() ? parent : nullptr;
}

template <typename Strategy>
static Node* parentEditingBoundary(const PositionTemplate<Strategy>& position)
{
    Node* const anchorNode = position.anchorNode();
    if (!anchorNode)
        return nullptr;

    Node* documentElement = anchorNode->document().documentElement();
    if (!documentElement)
        return nullptr;

    Node* boundary = position.computeContainerNode();
    while (boundary != documentElement && nonShadowBoundaryParentNode<Strategy>(boundary) && anchorNode->hasEditableStyle() == Strategy::parent(*boundary)->hasEditableStyle())
        boundary = nonShadowBoundaryParentNode<Strategy>(boundary);

    return boundary;
}

enum BoundarySearchContextAvailability { DontHaveMoreContext, MayHaveMoreContext };

typedef unsigned (*BoundarySearchFunction)(const UChar*, unsigned length, unsigned offset, BoundarySearchContextAvailability, bool& needMoreContext);

template <typename Strategy>
static VisiblePositionTemplate<Strategy> previousBoundary(const VisiblePositionTemplate<Strategy>& c, BoundarySearchFunction searchFunction)
{
    const PositionTemplate<Strategy> pos = c.deepEquivalent();
    Node* boundary = parentEditingBoundary(pos);
    if (!boundary)
        return VisiblePositionTemplate<Strategy>();

    const PositionTemplate<Strategy> start = PositionTemplate<Strategy>::editingPositionOf(boundary, 0).parentAnchoredEquivalent();
    const PositionTemplate<Strategy> end = pos.parentAnchoredEquivalent();

    ForwardsTextBuffer suffixString;
    if (requiresContextForWordBoundary(characterBefore(c))) {
        TextIteratorAlgorithm<Strategy> forwardsIterator(end, PositionTemplate<Strategy>::afterNode(boundary));
        while (!forwardsIterator.atEnd()) {
            forwardsIterator.copyTextTo(&suffixString);
            int contextEndIndex = endOfFirstWordBoundaryContext(suffixString.data() + suffixString.size() - forwardsIterator.length(), forwardsIterator.length());
            if (contextEndIndex < forwardsIterator.length()) {
                suffixString.shrink(forwardsIterator.length() - contextEndIndex);
                break;
            }
            forwardsIterator.advance();
        }
    }

    unsigned suffixLength = suffixString.size();
    BackwardsTextBuffer string;
    string.pushRange(suffixString.data(), suffixString.size());

    SimplifiedBackwardsTextIteratorAlgorithm<Strategy> it(start, end);
    int remainingLength = 0;
    unsigned next = 0;
    bool needMoreContext = false;
    while (!it.atEnd()) {
        bool inTextSecurityMode = it.isInTextSecurityMode();
        // iterate to get chunks until the searchFunction returns a non-zero
        // value.
        if (!inTextSecurityMode) {
            int runOffset = 0;
            do {
                runOffset += it.copyTextTo(&string, runOffset, string.capacity());
                // TODO(xiaochengh): The following line takes O(string.size()) time,
                // which makes quadratic overall running time in the worst case.
                // Should improve it in some way.
                next = searchFunction(string.data(), string.size(), string.size() - suffixLength, MayHaveMoreContext, needMoreContext);
            } while (!next && runOffset < it.length());
            if (next) {
                remainingLength = it.length() - runOffset;
                break;
            }
        } else {
            // Treat bullets used in the text security mode as regular
            // characters when looking for boundaries
            string.pushCharacters('x', it.length());
            next = 0;
        }
        it.advance();
    }
    if (needMoreContext) {
        // The last search returned the beginning of the buffer and asked for
        // more context, but there is no earlier text. Force a search with
        // what's available.
        // TODO(xiaochengh): Do we have to search the whole string?
        next = searchFunction(string.data(), string.size(), string.size() - suffixLength, DontHaveMoreContext, needMoreContext);
        DCHECK(!needMoreContext);
    }

    if (!next)
        return createVisiblePosition(it.atEnd() ? it.startPosition() : pos);

    Node* node = it.startContainer();
    int boundaryOffset = remainingLength + next;
    if (node->isTextNode() && boundaryOffset <= node->maxCharacterOffset()) {
        // The next variable contains a usable index into a text node
        return createVisiblePosition(PositionTemplate<Strategy>(node, boundaryOffset));
    }

    // Use the character iterator to translate the next value into a DOM
    // position.
    BackwardsCharacterIteratorAlgorithm<Strategy> charIt(start, end);
    charIt.advance(string.size() - suffixLength - next);
    // TODO(yosin) charIt can get out of shadow host.
    return createVisiblePosition(charIt.endPosition());
}

template <typename Strategy>
static VisiblePositionTemplate<Strategy> nextBoundary(const VisiblePositionTemplate<Strategy>& c, BoundarySearchFunction searchFunction)
{
    PositionTemplate<Strategy> pos = c.deepEquivalent();
    Node* boundary = parentEditingBoundary(pos);
    if (!boundary)
        return VisiblePositionTemplate<Strategy>();

    Document& d = boundary->document();
    const PositionTemplate<Strategy> start(pos.parentAnchoredEquivalent());

    BackwardsTextBuffer prefixString;
    if (requiresContextForWordBoundary(characterAfter(c))) {
        SimplifiedBackwardsTextIteratorAlgorithm<Strategy> backwardsIterator(PositionTemplate<Strategy>::firstPositionInNode(&d), start);
        while (!backwardsIterator.atEnd()) {
            backwardsIterator.copyTextTo(&prefixString);
            int contextStartIndex = startOfLastWordBoundaryContext(prefixString.data(), backwardsIterator.length());
            if (contextStartIndex > 0) {
                prefixString.shrink(contextStartIndex);
                break;
            }
            backwardsIterator.advance();
        }
    }

    unsigned prefixLength = prefixString.size();
    ForwardsTextBuffer string;
    string.pushRange(prefixString.data(), prefixString.size());

    const PositionTemplate<Strategy> searchStart = PositionTemplate<Strategy>::editingPositionOf(start.anchorNode(), start.offsetInContainerNode());
    const PositionTemplate<Strategy> searchEnd = PositionTemplate<Strategy>::lastPositionInNode(boundary);
    TextIteratorAlgorithm<Strategy> it(searchStart, searchEnd, TextIteratorEmitsCharactersBetweenAllVisiblePositions);
    const unsigned invalidOffset = static_cast<unsigned>(-1);
    unsigned next = invalidOffset;
    unsigned offset = prefixLength;
    bool needMoreContext = false;
    while (!it.atEnd()) {
        // Keep asking the iterator for chunks until the search function
        // returns an end value not equal to the length of the string passed to
        // it.
        bool inTextSecurityMode = it.isInTextSecurityMode();
        if (!inTextSecurityMode) {
            int runOffset = 0;
            do {
                runOffset += it.copyTextTo(&string, runOffset, string.capacity());
                next = searchFunction(string.data(), string.size(), offset, MayHaveMoreContext, needMoreContext);
                if (!needMoreContext) {
                    // When the search does not need more context, skip all examined
                    // characters except the last one, in case it is a boundary.
                    offset = string.size();
                    U16_BACK_1(string.data(), 0, offset);
                }
            } while (next == string.size() && runOffset < it.length());
            if (next != string.size())
                break;
        } else {
            // Treat bullets used in the text security mode as regular
            // characters when looking for boundaries
            string.pushCharacters('x', it.length());
            next = string.size();
        }
        it.advance();
    }
    if (needMoreContext) {
        // The last search returned the end of the buffer and asked for more
        // context, but there is no further text. Force a search with what's
        // available.
        // TODO(xiaochengh): Do we still have to search the whole string?
        next = searchFunction(string.data(), string.size(), prefixLength, DontHaveMoreContext, needMoreContext);
        DCHECK(!needMoreContext);
    }

    if (it.atEnd() && next == string.size()) {
        pos = it.startPositionInCurrentContainer();
    } else if (next != invalidOffset && next != prefixLength) {
        // TODO(dglazkov): The use of updateStyleAndLayoutIgnorePendingStylesheets needs to be audited.
        // see http://crbug.com/590369 for more details.
        searchStart.document()->updateStyleAndLayoutIgnorePendingStylesheets();
        // Use the character iterator to translate the next value into a DOM
        // position.
        CharacterIteratorAlgorithm<Strategy> charIt(searchStart, searchEnd, TextIteratorEmitsCharactersBetweenAllVisiblePositions);
        charIt.advance(next - prefixLength - 1);
        pos = charIt.endPosition();

        if (charIt.characterAt(0) == '\n') {
            // TODO(yosin) workaround for collapsed range (where only start
            // position is correct) emitted for some emitted newlines
            // (see rdar://5192593)
            const VisiblePositionTemplate<Strategy> visPos = createVisiblePosition(pos);
            if (visPos.deepEquivalent() == createVisiblePosition(charIt.startPosition()).deepEquivalent()) {
                charIt.advance(1);
                pos = charIt.startPosition();
            }
        }
    }

    // generate VisiblePosition, use TextAffinity::Upstream affinity if possible
    return createVisiblePosition(pos, VP_UPSTREAM_IF_POSSIBLE);
}

// ---------

static unsigned startWordBoundary(const UChar* characters, unsigned length, unsigned offset, BoundarySearchContextAvailability mayHaveMoreContext, bool& needMoreContext)
{
    TRACE_EVENT0("blink", "startWordBoundary");
    DCHECK(offset);
    if (mayHaveMoreContext && !startOfLastWordBoundaryContext(characters, offset)) {
        needMoreContext = true;
        return 0;
    }
    needMoreContext = false;
    int start, end;
    U16_BACK_1(characters, 0, offset);
    findWordBoundary(characters, length, offset, &start, &end);
    return start;
}

template <typename Strategy>
static VisiblePositionTemplate<Strategy> startOfWordAlgorithm(const VisiblePositionTemplate<Strategy>& c, EWordSide side)
{
    // TODO(yosin) This returns a null VP for c at the start of the document
    // and |side| == |LeftWordIfOnBoundary|
    VisiblePositionTemplate<Strategy> p = c;
    if (side == RightWordIfOnBoundary) {
        // at paragraph end, the startofWord is the current position
        if (isEndOfParagraph(c))
            return c;

        p = nextPositionOf(c);
        if (p.isNull())
            return c;
    }
    return previousBoundary(p, startWordBoundary);
}

VisiblePosition startOfWord(const VisiblePosition& c, EWordSide side)
{
    return startOfWordAlgorithm<EditingStrategy>(c, side);
}

VisiblePositionInFlatTree startOfWord(const VisiblePositionInFlatTree& c, EWordSide side)
{
    return startOfWordAlgorithm<EditingInFlatTreeStrategy>(c, side);
}

static unsigned endWordBoundary(const UChar* characters, unsigned length, unsigned offset, BoundarySearchContextAvailability mayHaveMoreContext, bool& needMoreContext)
{
    DCHECK_LE(offset, length);
    if (mayHaveMoreContext && endOfFirstWordBoundaryContext(characters + offset, length - offset) == static_cast<int>(length - offset)) {
        needMoreContext = true;
        return length;
    }
    needMoreContext = false;
    return findWordEndBoundary(characters, length, offset);
}

template <typename Strategy>
static VisiblePositionTemplate<Strategy> endOfWordAlgorithm(const VisiblePositionTemplate<Strategy>& c, EWordSide side)
{
    VisiblePositionTemplate<Strategy> p = c;
    if (side == LeftWordIfOnBoundary) {
        if (isStartOfParagraph(c))
            return c;

        p = previousPositionOf(c);
        if (p.isNull())
            return c;
    } else if (isEndOfParagraph(c)) {
        return c;
    }

    return nextBoundary(p, endWordBoundary);
}

VisiblePosition endOfWord(const VisiblePosition& c, EWordSide side)
{
    return endOfWordAlgorithm<EditingStrategy>(c, side);
}

VisiblePositionInFlatTree endOfWord(const VisiblePositionInFlatTree& c, EWordSide side)
{
    return endOfWordAlgorithm<EditingInFlatTreeStrategy>(c, side);
}

static unsigned previousWordPositionBoundary(const UChar* characters, unsigned length, unsigned offset, BoundarySearchContextAvailability mayHaveMoreContext, bool& needMoreContext)
{
    if (mayHaveMoreContext && !startOfLastWordBoundaryContext(characters, offset)) {
        needMoreContext = true;
        return 0;
    }
    needMoreContext = false;
    return findNextWordFromIndex(characters, length, offset, false);
}

VisiblePosition previousWordPosition(const VisiblePosition& c)
{
    VisiblePosition prev = previousBoundary(c, previousWordPositionBoundary);
    return honorEditingBoundaryAtOrBefore(prev, c.deepEquivalent());
}

static unsigned nextWordPositionBoundary(const UChar* characters, unsigned length, unsigned offset, BoundarySearchContextAvailability mayHaveMoreContext, bool& needMoreContext)
{
    if (mayHaveMoreContext && endOfFirstWordBoundaryContext(characters + offset, length - offset) == static_cast<int>(length - offset)) {
        needMoreContext = true;
        return length;
    }
    needMoreContext = false;
    return findNextWordFromIndex(characters, length, offset, true);
}

VisiblePosition nextWordPosition(const VisiblePosition& c)
{
    VisiblePosition next = nextBoundary(c, nextWordPositionBoundary);
    return honorEditingBoundaryAtOrAfter(next, c.deepEquivalent());
}

// ---------

enum LineEndpointComputationMode { UseLogicalOrdering, UseInlineBoxOrdering };
template <typename Strategy>
static PositionWithAffinityTemplate<Strategy> startPositionForLine(const PositionWithAffinityTemplate<Strategy>& c, LineEndpointComputationMode mode)
{
    if (c.isNull())
        return PositionWithAffinityTemplate<Strategy>();

    RootInlineBox* rootBox = RenderedPosition(c.position(), c.affinity()).rootBox();
    if (!rootBox) {
        // There are VisiblePositions at offset 0 in blocks without
        // RootInlineBoxes, like empty editable blocks and bordered blocks.
        PositionTemplate<Strategy> p = c.position();
        if (p.anchorNode()->layoutObject() && p.anchorNode()->layoutObject()->isLayoutBlock() && !p.computeEditingOffset())
            return c;

        return PositionWithAffinityTemplate<Strategy>();
    }

    Node* startNode;
    InlineBox* startBox;
    if (mode == UseLogicalOrdering) {
        startNode = rootBox->getLogicalStartBoxWithNode(startBox);
        if (!startNode)
            return PositionWithAffinityTemplate<Strategy>();
    } else {
        // Generated content (e.g. list markers and CSS :before and :after pseudoelements) have no corresponding DOM element,
        // and so cannot be represented by a VisiblePosition. Use whatever follows instead.
        startBox = rootBox->firstLeafChild();
        while (true) {
            if (!startBox)
                return PositionWithAffinityTemplate<Strategy>();

            startNode = startBox->getLineLayoutItem().nonPseudoNode();
            if (startNode)
                break;

            startBox = startBox->nextLeafChild();
        }
    }

    return PositionWithAffinityTemplate<Strategy>(startNode->isTextNode() ? PositionTemplate<Strategy>(toText(startNode), toInlineTextBox(startBox)->start()) : PositionTemplate<Strategy>::beforeNode(startNode));
}

template <typename Strategy>
static PositionWithAffinityTemplate<Strategy> startOfLineAlgorithm(const PositionWithAffinityTemplate<Strategy>& c)
{
    // TODO: this is the current behavior that might need to be fixed.
    // Please refer to https://bugs.webkit.org/show_bug.cgi?id=49107 for detail.
    PositionWithAffinityTemplate<Strategy> visPos = startPositionForLine(c, UseInlineBoxOrdering);
    return honorEditingBoundaryAtOrBefore(visPos, c.position());
}

static PositionWithAffinity startOfLine(const PositionWithAffinity& currentPosition)
{
    return startOfLineAlgorithm<EditingStrategy>(currentPosition);
}

static PositionInFlatTreeWithAffinity startOfLine(const PositionInFlatTreeWithAffinity& currentPosition)
{
    return startOfLineAlgorithm<EditingInFlatTreeStrategy>(currentPosition);
}

// FIXME: Rename this function to reflect the fact it ignores bidi levels.
VisiblePosition startOfLine(const VisiblePosition& currentPosition)
{
    return createVisiblePosition(startOfLine(currentPosition.toPositionWithAffinity()));
}

VisiblePositionInFlatTree startOfLine(const VisiblePositionInFlatTree& currentPosition)
{
    return createVisiblePosition(startOfLine(currentPosition.toPositionWithAffinity()));
}

template <typename Strategy>
static PositionWithAffinityTemplate<Strategy> logicalStartOfLineAlgorithm(const PositionWithAffinityTemplate<Strategy>& c)
{
    // TODO: this is the current behavior that might need to be fixed.
    // Please refer to https://bugs.webkit.org/show_bug.cgi?id=49107 for detail.
    PositionWithAffinityTemplate<Strategy> visPos = startPositionForLine(c, UseLogicalOrdering);

    if (ContainerNode* editableRoot = highestEditableRoot(c.position())) {
        if (!editableRoot->contains(visPos.position().computeContainerNode()))
            return PositionWithAffinityTemplate<Strategy>(PositionTemplate<Strategy>::firstPositionInNode(editableRoot));
    }

    return honorEditingBoundaryAtOrBefore(visPos, c.position());
}

VisiblePosition logicalStartOfLine(const VisiblePosition& currentPosition)
{
    return createVisiblePosition(logicalStartOfLineAlgorithm<EditingStrategy>(currentPosition.toPositionWithAffinity()));
}

VisiblePositionInFlatTree logicalStartOfLine(const VisiblePositionInFlatTree& currentPosition)
{
    return createVisiblePosition(logicalStartOfLineAlgorithm<EditingInFlatTreeStrategy>(currentPosition.toPositionWithAffinity()));
}

template <typename Strategy>
static VisiblePositionTemplate<Strategy> endPositionForLine(const VisiblePositionTemplate<Strategy>& c, LineEndpointComputationMode mode)
{
    if (c.isNull())
        return VisiblePositionTemplate<Strategy>();

    RootInlineBox* rootBox = RenderedPosition(c).rootBox();
    if (!rootBox) {
        // There are VisiblePositions at offset 0 in blocks without
        // RootInlineBoxes, like empty editable blocks and bordered blocks.
        const PositionTemplate<Strategy> p = c.deepEquivalent();
        if (p.anchorNode()->layoutObject() && p.anchorNode()->layoutObject()->isLayoutBlock() && !p.computeEditingOffset())
            return c;
        return VisiblePositionTemplate<Strategy>();
    }

    Node* endNode;
    InlineBox* endBox;
    if (mode == UseLogicalOrdering) {
        endNode = rootBox->getLogicalEndBoxWithNode(endBox);
        if (!endNode)
            return VisiblePositionTemplate<Strategy>();
    } else {
        // Generated content (e.g. list markers and CSS :before and :after
        // pseudo elements) have no corresponding DOM element, and so cannot be
        // represented by a VisiblePosition. Use whatever precedes instead.
        endBox = rootBox->lastLeafChild();
        while (true) {
            if (!endBox)
                return VisiblePositionTemplate<Strategy>();

            endNode = endBox->getLineLayoutItem().nonPseudoNode();
            if (endNode)
                break;

            endBox = endBox->prevLeafChild();
        }
    }

    PositionTemplate<Strategy> pos;
    if (isHTMLBRElement(*endNode)) {
        pos = PositionTemplate<Strategy>::beforeNode(endNode);
    } else if (endBox->isInlineTextBox() && endNode->isTextNode()) {
        InlineTextBox* endTextBox = toInlineTextBox(endBox);
        int endOffset = endTextBox->start();
        if (!endTextBox->isLineBreak())
            endOffset += endTextBox->len();
        pos = PositionTemplate<Strategy>(toText(endNode), endOffset);
    } else {
        pos = PositionTemplate<Strategy>::afterNode(endNode);
    }

    return createVisiblePosition(pos, VP_UPSTREAM_IF_POSSIBLE);
}

// TODO(yosin) Rename this function to reflect the fact it ignores bidi levels.
template <typename Strategy>
static VisiblePositionTemplate<Strategy> endOfLineAlgorithm(const VisiblePositionTemplate<Strategy>& currentPosition)
{
    // TODO(yosin) this is the current behavior that might need to be fixed.
    // Please refer to https://bugs.webkit.org/show_bug.cgi?id=49107 for detail.
    VisiblePositionTemplate<Strategy> visPos = endPositionForLine(currentPosition, UseInlineBoxOrdering);

    // Make sure the end of line is at the same line as the given input
    // position. Else use the previous position to obtain end of line. This
    // condition happens when the input position is before the space character
    // at the end of a soft-wrapped non-editable line. In this scenario,
    // |endPositionForLine()| would incorrectly hand back a position in the next
    // line instead. This fix is to account for the discrepancy between lines
    // with "webkit-line-break:after-white-space" style versus lines without
    // that style, which would break before a space by default.
    if (!inSameLine(currentPosition, visPos)) {
        visPos = previousPositionOf(currentPosition);
        if (visPos.isNull())
            return VisiblePositionTemplate<Strategy>();
        visPos = endPositionForLine(visPos, UseInlineBoxOrdering);
    }

    return honorEditingBoundaryAtOrAfter(visPos, currentPosition.deepEquivalent());
}

// TODO(yosin) Rename this function to reflect the fact it ignores bidi levels.
VisiblePosition endOfLine(const VisiblePosition& currentPosition)
{
    return endOfLineAlgorithm<EditingStrategy>(currentPosition);
}

VisiblePositionInFlatTree endOfLine(const VisiblePositionInFlatTree& currentPosition)
{
    return endOfLineAlgorithm<EditingInFlatTreeStrategy>(currentPosition);
}

template <typename Strategy>
static bool inSameLogicalLine(const VisiblePositionTemplate<Strategy>& a, const VisiblePositionTemplate<Strategy>& b)
{
    return a.isNotNull() && logicalStartOfLine(a).deepEquivalent() == logicalStartOfLine(b).deepEquivalent();
}

template <typename Strategy>
VisiblePositionTemplate<Strategy> logicalEndOfLineAlgorithm(const VisiblePositionTemplate<Strategy>& currentPosition)
{
    // TODO(yosin) this is the current behavior that might need to be fixed.
    // Please refer to https://bugs.webkit.org/show_bug.cgi?id=49107 for detail.
    VisiblePositionTemplate<Strategy> visPos = endPositionForLine(currentPosition, UseLogicalOrdering);

    // Make sure the end of line is at the same line as the given input
    // position. For a wrapping line, the logical end position for the
    // not-last-2-lines might incorrectly hand back the logical beginning of the
    // next line. For example,
    // <div contenteditable dir="rtl" style="line-break:before-white-space">xyz
    // a xyz xyz xyz xyz xyz xyz xyz xyz xyz xyz </div>
    // In this case, use the previous position of the computed logical end
    // position.
    if (!inSameLogicalLine(currentPosition, visPos))
        visPos = previousPositionOf(visPos);

    if (ContainerNode* editableRoot = highestEditableRoot(currentPosition.deepEquivalent())) {
        if (!editableRoot->contains(visPos.deepEquivalent().computeContainerNode()))
            return createVisiblePosition(PositionTemplate<Strategy>::lastPositionInNode(editableRoot));
    }

    return honorEditingBoundaryAtOrAfter(visPos, currentPosition.deepEquivalent());
}

VisiblePosition logicalEndOfLine(const VisiblePosition& currentPosition)
{
    return logicalEndOfLineAlgorithm<EditingStrategy>(currentPosition);
}

VisiblePositionInFlatTree logicalEndOfLine(const VisiblePositionInFlatTree& currentPosition)
{
    return logicalEndOfLineAlgorithm<EditingInFlatTreeStrategy>(currentPosition);
}

template <typename Strategy>
bool inSameLineAlgorithm(const PositionWithAffinityTemplate<Strategy>& position1, const PositionWithAffinityTemplate<Strategy>& position2)
{
    if (position1.isNull() || position2.isNull())
        return false;
    PositionWithAffinityTemplate<Strategy> startOfLine1 = startOfLine(position1);
    PositionWithAffinityTemplate<Strategy> startOfLine2 = startOfLine(position2);
    if (startOfLine1 == startOfLine2)
        return true;
    PositionTemplate<Strategy> canonicalized1 = canonicalPositionOf(startOfLine1.position());
    if (canonicalized1 == startOfLine2.position())
        return true;
    return canonicalized1 == canonicalPositionOf(startOfLine2.position());
}

bool inSameLine(const PositionWithAffinity& a, const PositionWithAffinity& b)
{
    return inSameLineAlgorithm<EditingStrategy>(a, b);
}

bool inSameLine(const PositionInFlatTreeWithAffinity& position1, const PositionInFlatTreeWithAffinity& position2)
{
    return inSameLineAlgorithm<EditingInFlatTreeStrategy>(position1, position2);
}

bool inSameLine(const VisiblePosition& position1, const VisiblePosition& position2)
{
    return inSameLine(position1.toPositionWithAffinity(), position2.toPositionWithAffinity());
}

bool inSameLine(const VisiblePositionInFlatTree& position1, const VisiblePositionInFlatTree& position2)
{
    return inSameLine(position1.toPositionWithAffinity(), position2.toPositionWithAffinity());
}

template <typename Strategy>
bool isStartOfLineAlgorithm(const VisiblePositionTemplate<Strategy>& p)
{
    return p.isNotNull() && p.deepEquivalent() == startOfLine(p).deepEquivalent();
}

bool isStartOfLine(const VisiblePosition& p)
{
    return isStartOfLineAlgorithm<EditingStrategy>(p);
}

bool isStartOfLine(const VisiblePositionInFlatTree& p)
{
    return isStartOfLineAlgorithm<EditingInFlatTreeStrategy>(p);
}

template <typename Strategy>
bool isEndOfLineAlgorithm(const VisiblePositionTemplate<Strategy>& p)
{
    return p.isNotNull() && p.deepEquivalent() == endOfLine(p).deepEquivalent();
}

bool isEndOfLine(const VisiblePosition& p)
{
    return isEndOfLineAlgorithm<EditingStrategy>(p);
}

bool isEndOfLine(const VisiblePositionInFlatTree& p)
{
    return isEndOfLineAlgorithm<EditingInFlatTreeStrategy>(p);
}

template <typename Strategy>
static bool isLogicalEndOfLineAlgorithm(const VisiblePositionTemplate<Strategy>& p)
{
    return p.isNotNull() && p.deepEquivalent() == logicalEndOfLine(p).deepEquivalent();
}

bool isLogicalEndOfLine(const VisiblePosition& p)
{
    return isLogicalEndOfLineAlgorithm<EditingStrategy>(p);
}

bool isLogicalEndOfLine(const VisiblePositionInFlatTree& p)
{
    return isLogicalEndOfLineAlgorithm<EditingInFlatTreeStrategy>(p);
}

static inline LayoutPoint absoluteLineDirectionPointToLocalPointInBlock(RootInlineBox* root, LayoutUnit lineDirectionPoint)
{
    DCHECK(root);
    LineLayoutBlockFlow containingBlock = root->block();
    FloatPoint absoluteBlockPoint = containingBlock.localToAbsolute(FloatPoint());
    if (containingBlock.hasOverflowClip())
        absoluteBlockPoint -= FloatSize(containingBlock.scrolledContentOffset());

    if (root->block().isHorizontalWritingMode())
        return LayoutPoint(LayoutUnit(lineDirectionPoint - absoluteBlockPoint.x()), root->blockDirectionPointInLine());

    return LayoutPoint(root->blockDirectionPointInLine(), LayoutUnit(lineDirectionPoint - absoluteBlockPoint.y()));
}

VisiblePosition previousLinePosition(const VisiblePosition& visiblePosition, LayoutUnit lineDirectionPoint, EditableType editableType)
{
    Position p = visiblePosition.deepEquivalent();
    Node* node = p.anchorNode();

    if (!node)
        return VisiblePosition();

    node->document().updateStyleAndLayoutIgnorePendingStylesheets();

    LayoutObject* layoutObject = node->layoutObject();
    if (!layoutObject)
        return VisiblePosition();

    RootInlineBox* root = 0;
    InlineBox* box = computeInlineBoxPosition(visiblePosition).inlineBox;
    if (box) {
        root = box->root().prevRootBox();
        // We want to skip zero height boxes.
        // This could happen in case it is a TrailingFloatsRootInlineBox.
        if (!root || !root->logicalHeight() || !root->firstLeafChild())
            root = 0;
    }

    if (!root) {
        Position position = previousRootInlineBoxCandidatePosition(node, visiblePosition, editableType);
        if (position.isNotNull()) {
            RenderedPosition renderedPosition((createVisiblePosition(position)));
            root = renderedPosition.rootBox();
            if (!root)
                return createVisiblePosition(position);
        }
    }

    if (root) {
        // FIXME: Can be wrong for multi-column layout and with transforms.
        LayoutPoint pointInLine = absoluteLineDirectionPointToLocalPointInBlock(root, lineDirectionPoint);
        LineLayoutItem lineLayoutItem = root->closestLeafChildForPoint(pointInLine, isEditablePosition(p, ContentIsEditable))->getLineLayoutItem();
        Node* node = lineLayoutItem.node();
        if (node && editingIgnoresContent(node))
            return VisiblePosition::inParentBeforeNode(*node);
        return createVisiblePosition(lineLayoutItem.positionForPoint(pointInLine));
    }

    // Could not find a previous line. This means we must already be on the first line.
    // Move to the start of the content in this block, which effectively moves us
    // to the start of the line we're on.
    Element* rootElement = node->hasEditableStyle(editableType) ? node->rootEditableElement(editableType) : node->document().documentElement();
    if (!rootElement)
        return VisiblePosition();
    return VisiblePosition::firstPositionInNode(rootElement);
}

VisiblePosition nextLinePosition(const VisiblePosition& visiblePosition, LayoutUnit lineDirectionPoint, EditableType editableType)
{
    Position p = visiblePosition.deepEquivalent();
    Node* node = p.anchorNode();

    if (!node)
        return VisiblePosition();

    node->document().updateStyleAndLayoutIgnorePendingStylesheets();

    LayoutObject* layoutObject = node->layoutObject();
    if (!layoutObject)
        return VisiblePosition();

    RootInlineBox* root = 0;
    InlineBox* box = computeInlineBoxPosition(visiblePosition).inlineBox;
    if (box) {
        root = box->root().nextRootBox();
        // We want to skip zero height boxes.
        // This could happen in case it is a TrailingFloatsRootInlineBox.
        if (!root || !root->logicalHeight() || !root->firstLeafChild())
            root = 0;
    }

    if (!root) {
        // FIXME: We need do the same in previousLinePosition.
        Node* child = NodeTraversal::childAt(*node, p.computeEditingOffset());
        node = child ? child : &NodeTraversal::lastWithinOrSelf(*node);
        Position position = nextRootInlineBoxCandidatePosition(node, visiblePosition, editableType);
        if (position.isNotNull()) {
            RenderedPosition renderedPosition((createVisiblePosition(position)));
            root = renderedPosition.rootBox();
            if (!root)
                return createVisiblePosition(position);
        }
    }

    if (root) {
        // FIXME: Can be wrong for multi-column layout and with transforms.
        LayoutPoint pointInLine = absoluteLineDirectionPointToLocalPointInBlock(root, lineDirectionPoint);
        LineLayoutItem lineLayoutItem = root->closestLeafChildForPoint(pointInLine, isEditablePosition(p))->getLineLayoutItem();
        Node* node = lineLayoutItem.node();
        if (node && editingIgnoresContent(node))
            return VisiblePosition::inParentBeforeNode(*node);
        return createVisiblePosition(lineLayoutItem.positionForPoint(pointInLine));
    }

    // Could not find a next line. This means we must already be on the last line.
    // Move to the end of the content in this block, which effectively moves us
    // to the end of the line we're on.
    Element* rootElement = node->hasEditableStyle(editableType) ? node->rootEditableElement(editableType) : node->document().documentElement();
    if (!rootElement)
        return VisiblePosition();
    return VisiblePosition::lastPositionInNode(rootElement);
}

// ---------

static unsigned startSentenceBoundary(const UChar* characters, unsigned length, unsigned, BoundarySearchContextAvailability, bool&)
{
    TextBreakIterator* iterator = sentenceBreakIterator(characters, length);
    // FIXME: The following function can return -1; we don't handle that.
    return iterator->preceding(length);
}

template <typename Strategy>
static VisiblePositionTemplate<Strategy> startOfSentenceAlgorithm(const VisiblePositionTemplate<Strategy>& c)
{
    return previousBoundary(c, startSentenceBoundary);
}

VisiblePosition startOfSentence(const VisiblePosition& c)
{
    return startOfSentenceAlgorithm<EditingStrategy>(c);
}

VisiblePositionInFlatTree startOfSentence(const VisiblePositionInFlatTree& c)
{
    return startOfSentenceAlgorithm<EditingInFlatTreeStrategy>(c);
}

static unsigned endSentenceBoundary(const UChar* characters, unsigned length, unsigned, BoundarySearchContextAvailability, bool&)
{
    TextBreakIterator* iterator = sentenceBreakIterator(characters, length);
    return iterator->next();
}

// TODO(yosin) This includes the space after the punctuation that marks the end
// of the sentence.
template <typename Strategy>
static VisiblePositionTemplate<Strategy> endOfSentenceAlgorithm(const VisiblePositionTemplate<Strategy>& c)
{
    return nextBoundary(c, endSentenceBoundary);
}

VisiblePosition endOfSentence(const VisiblePosition& c)
{
    return endOfSentenceAlgorithm<EditingStrategy>(c);
}

VisiblePositionInFlatTree endOfSentence(const VisiblePositionInFlatTree& c)
{
    return endOfSentenceAlgorithm<EditingInFlatTreeStrategy>(c);
}

static unsigned previousSentencePositionBoundary(const UChar* characters, unsigned length, unsigned, BoundarySearchContextAvailability, bool&)
{
    // FIXME: This is identical to startSentenceBoundary. I'm pretty sure that's not right.
    TextBreakIterator* iterator = sentenceBreakIterator(characters, length);
    // FIXME: The following function can return -1; we don't handle that.
    return iterator->preceding(length);
}

VisiblePosition previousSentencePosition(const VisiblePosition& c)
{
    VisiblePosition prev = previousBoundary(c, previousSentencePositionBoundary);
    return honorEditingBoundaryAtOrBefore(prev, c.deepEquivalent());
}

static unsigned nextSentencePositionBoundary(const UChar* characters, unsigned length, unsigned, BoundarySearchContextAvailability, bool&)
{
    // FIXME: This is identical to endSentenceBoundary. This isn't right, it needs to
    // move to the equivlant position in the following sentence.
    TextBreakIterator* iterator = sentenceBreakIterator(characters, length);
    return iterator->following(0);
}

VisiblePosition nextSentencePosition(const VisiblePosition& c)
{
    VisiblePosition next = nextBoundary(c, nextSentencePositionBoundary);
    return honorEditingBoundaryAtOrAfter(next, c.deepEquivalent());
}

template <typename Strategy>
VisiblePositionTemplate<Strategy> startOfParagraphAlgorithm(const VisiblePositionTemplate<Strategy>& c, EditingBoundaryCrossingRule boundaryCrossingRule)
{
    const PositionTemplate<Strategy> p = c.deepEquivalent();
    Node* startNode = p.anchorNode();

    if (!startNode)
        return VisiblePositionTemplate<Strategy>();

    if (isRenderedAsNonInlineTableImageOrHR(startNode))
        return createVisiblePosition(PositionTemplate<Strategy>::beforeNode(startNode));

    Element* startBlock = enclosingBlock(PositionTemplate<Strategy>::firstPositionInOrBeforeNode(startNode), CannotCrossEditingBoundary);

    Node* node = startNode;
    ContainerNode* highestRoot = highestEditableRoot(p);
    int offset = p.computeEditingOffset();
    PositionAnchorType type = p.anchorType();

    Node* n = startNode;
    bool startNodeIsEditable = startNode->hasEditableStyle();
    while (n) {
        if (boundaryCrossingRule == CannotCrossEditingBoundary && !nodeIsUserSelectAll(n) && n->hasEditableStyle() != startNodeIsEditable)
            break;
        if (boundaryCrossingRule == CanSkipOverEditingBoundary) {
            while (n && n->hasEditableStyle() != startNodeIsEditable)
                n = Strategy::previousPostOrder(*n, startBlock);
            if (!n || !n->isDescendantOf(highestRoot))
                break;
        }
        LayoutObject* r = n->layoutObject();
        LayoutItem ri = LayoutItem(r);
        if (ri.isNull()) {
            n = Strategy::previousPostOrder(*n, startBlock);
            continue;
        }
        const ComputedStyle& style = ri.styleRef();
        if (style.visibility() != VISIBLE) {
            n = Strategy::previousPostOrder(*n, startBlock);
            continue;
        }

        if (ri.isBR() || isEnclosingBlock(n))
            break;

        if (ri.isText() && toLayoutText(r)->resolvedTextLength()) {
            ASSERT_WITH_SECURITY_IMPLICATION(n->isTextNode());
            type = PositionAnchorType::OffsetInAnchor;
            if (style.preserveNewline()) {
                LayoutText* text = toLayoutText(r);
                int i = text->textLength();
                int o = offset;
                if (n == startNode && o < i)
                    i = max(0, o);
                while (--i >= 0) {
                    if ((*text)[i] == '\n')
                        return createVisiblePosition(PositionTemplate<Strategy>(toText(n), i + 1));
                }
            }
            node = n;
            offset = 0;
            n = Strategy::previousPostOrder(*n, startBlock);
        } else if (editingIgnoresContent(n) || isDisplayInsideTable(n)) {
            node = n;
            type = PositionAnchorType::BeforeAnchor;
            n = n->previousSibling() ? n->previousSibling() : Strategy::previousPostOrder(*n, startBlock);
        } else {
            n = Strategy::previousPostOrder(*n, startBlock);
        }
    }

    if (type == PositionAnchorType::OffsetInAnchor)
        return createVisiblePosition(PositionTemplate<Strategy>(node, offset));

    return createVisiblePosition(PositionTemplate<Strategy>(node, type));
}

VisiblePosition startOfParagraph(const VisiblePosition& c, EditingBoundaryCrossingRule boundaryCrossingRule)
{
    return startOfParagraphAlgorithm<EditingStrategy>(c, boundaryCrossingRule);
}

VisiblePositionInFlatTree startOfParagraph(const VisiblePositionInFlatTree& c, EditingBoundaryCrossingRule boundaryCrossingRule)
{
    return startOfParagraphAlgorithm<EditingInFlatTreeStrategy>(c, boundaryCrossingRule);
}

template <typename Strategy>
static VisiblePositionTemplate<Strategy> endOfParagraphAlgorithm(const VisiblePositionTemplate<Strategy>& c, EditingBoundaryCrossingRule boundaryCrossingRule)
{
    if (c.isNull())
        return VisiblePositionTemplate<Strategy>();

    const PositionTemplate<Strategy> p = c.deepEquivalent();
    Node* startNode = p.anchorNode();

    if (isRenderedAsNonInlineTableImageOrHR(startNode))
        return createVisiblePosition(PositionTemplate<Strategy>::afterNode(startNode));

    Element* startBlock = enclosingBlock(PositionTemplate<Strategy>::firstPositionInOrBeforeNode(startNode), CannotCrossEditingBoundary);
    Element* stayInsideBlock = startBlock;

    Node* node = startNode;
    ContainerNode* highestRoot = highestEditableRoot(p);
    int offset = p.computeEditingOffset();
    PositionAnchorType type = p.anchorType();

    Node* n = startNode;
    bool startNodeIsEditable = startNode->hasEditableStyle();
    while (n) {
        if (boundaryCrossingRule == CannotCrossEditingBoundary && !nodeIsUserSelectAll(n) && n->hasEditableStyle() != startNodeIsEditable)
            break;
        if (boundaryCrossingRule == CanSkipOverEditingBoundary) {
            while (n && n->hasEditableStyle() != startNodeIsEditable)
                n = Strategy::next(*n, stayInsideBlock);
            if (!n || !n->isDescendantOf(highestRoot))
                break;
        }

        LayoutObject* r = n->layoutObject();
        if (!r) {
            n = Strategy::next(*n, stayInsideBlock);
            continue;
        }
        const ComputedStyle& style = r->styleRef();
        if (style.visibility() != VISIBLE) {
            n = Strategy::next(*n, stayInsideBlock);
            continue;
        }

        if (r->isBR() || isEnclosingBlock(n))
            break;

        // FIXME: We avoid returning a position where the layoutObject can't accept the caret.
        if (r->isText() && toLayoutText(r)->resolvedTextLength()) {
            ASSERT_WITH_SECURITY_IMPLICATION(n->isTextNode());
            int length = toLayoutText(r)->textLength();
            type = PositionAnchorType::OffsetInAnchor;
            LayoutText* text = toLayoutText(r);
            if (style.preserveNewline()) {
                int o = n == startNode ? offset : 0;
                for (int i = o; i < length; ++i) {
                    if ((*text)[i] == '\n')
                        return createVisiblePosition(PositionTemplate<Strategy>(toText(n), i + text->textStartOffset()));
                }
            }
            node = n;
            offset = r->caretMaxOffset() + text->textStartOffset();
            n = Strategy::next(*n, stayInsideBlock);
        } else if (Strategy::editingIgnoresContent(n) || isDisplayInsideTable(n)) {
            node = n;
            type = PositionAnchorType::AfterAnchor;
            n = Strategy::nextSkippingChildren(*n, stayInsideBlock);
        } else {
            n = Strategy::next(*n, stayInsideBlock);
        }
    }

    if (type == PositionAnchorType::OffsetInAnchor)
        return createVisiblePosition(PositionTemplate<Strategy>(node, offset));

    return createVisiblePosition(PositionTemplate<Strategy>(node, type));
}

VisiblePosition endOfParagraph(const VisiblePosition& c, EditingBoundaryCrossingRule boundaryCrossingRule)
{
    return endOfParagraphAlgorithm<EditingStrategy>(c, boundaryCrossingRule);
}

VisiblePositionInFlatTree endOfParagraph(const VisiblePositionInFlatTree& c, EditingBoundaryCrossingRule boundaryCrossingRule)
{
    return endOfParagraphAlgorithm<EditingInFlatTreeStrategy>(c, boundaryCrossingRule);
}

// FIXME: isStartOfParagraph(startOfNextParagraph(pos)) is not always true
VisiblePosition startOfNextParagraph(const VisiblePosition& visiblePosition)
{
    VisiblePosition paragraphEnd(endOfParagraph(visiblePosition, CanSkipOverEditingBoundary));
    VisiblePosition afterParagraphEnd(nextPositionOf(paragraphEnd, CannotCrossEditingBoundary));
    // The position after the last position in the last cell of a table
    // is not the start of the next paragraph.
    if (tableElementJustBefore(afterParagraphEnd))
        return nextPositionOf(afterParagraphEnd, CannotCrossEditingBoundary);
    return afterParagraphEnd;
}

bool inSameParagraph(const VisiblePosition& a, const VisiblePosition& b, EditingBoundaryCrossingRule boundaryCrossingRule)
{
    return a.isNotNull() && startOfParagraph(a, boundaryCrossingRule).deepEquivalent() == startOfParagraph(b, boundaryCrossingRule).deepEquivalent();
}

template <typename Strategy>
static bool isStartOfParagraphAlgorithm(const VisiblePositionTemplate<Strategy>& pos, EditingBoundaryCrossingRule boundaryCrossingRule)
{
    return pos.isNotNull() && pos.deepEquivalent() == startOfParagraph(pos, boundaryCrossingRule).deepEquivalent();
}

bool isStartOfParagraph(const VisiblePosition& pos, EditingBoundaryCrossingRule boundaryCrossingRule)
{
    return isStartOfParagraphAlgorithm<EditingStrategy>(pos, boundaryCrossingRule);
}

bool isStartOfParagraph(const VisiblePositionInFlatTree& pos, EditingBoundaryCrossingRule boundaryCrossingRule)
{
    return isStartOfParagraphAlgorithm<EditingInFlatTreeStrategy>(pos, boundaryCrossingRule);
}

template <typename Strategy>
static bool isEndOfParagraphAlgorithm(const VisiblePositionTemplate<Strategy>& pos, EditingBoundaryCrossingRule boundaryCrossingRule)
{
    return pos.isNotNull() && pos.deepEquivalent() == endOfParagraph(pos, boundaryCrossingRule).deepEquivalent();
}

bool isEndOfParagraph(const VisiblePosition& pos, EditingBoundaryCrossingRule boundaryCrossingRule)
{
    return isEndOfParagraphAlgorithm<EditingStrategy>(pos, boundaryCrossingRule);
}

bool isEndOfParagraph(const VisiblePositionInFlatTree& pos, EditingBoundaryCrossingRule boundaryCrossingRule)
{
    return isEndOfParagraphAlgorithm<EditingInFlatTreeStrategy>(pos, boundaryCrossingRule);
}

VisiblePosition previousParagraphPosition(const VisiblePosition& p, LayoutUnit x)
{
    VisiblePosition pos = p;
    do {
        VisiblePosition n = previousLinePosition(pos, x);
        if (n.isNull() || n.deepEquivalent() == pos.deepEquivalent())
            break;
        pos = n;
    } while (inSameParagraph(p, pos));
    return pos;
}

VisiblePosition nextParagraphPosition(const VisiblePosition& p, LayoutUnit x)
{
    VisiblePosition pos = p;
    do {
        VisiblePosition n = nextLinePosition(pos, x);
        if (n.isNull() || n.deepEquivalent() == pos.deepEquivalent())
            break;
        pos = n;
    } while (inSameParagraph(p, pos));
    return pos;
}

// ---------

VisiblePosition startOfBlock(const VisiblePosition& visiblePosition, EditingBoundaryCrossingRule rule)
{
    Position position = visiblePosition.deepEquivalent();
    Element* startBlock = position.computeContainerNode() ? enclosingBlock(position.computeContainerNode(), rule) : 0;
    return startBlock ? VisiblePosition::firstPositionInNode(startBlock) : VisiblePosition();
}

VisiblePosition endOfBlock(const VisiblePosition& visiblePosition, EditingBoundaryCrossingRule rule)
{
    Position position = visiblePosition.deepEquivalent();
    Element* endBlock = position.computeContainerNode() ? enclosingBlock(position.computeContainerNode(), rule) : 0;
    return endBlock ? VisiblePosition::lastPositionInNode(endBlock) : VisiblePosition();
}

bool inSameBlock(const VisiblePosition& a, const VisiblePosition& b)
{
    return !a.isNull() && enclosingBlock(a.deepEquivalent().computeContainerNode()) == enclosingBlock(b.deepEquivalent().computeContainerNode());
}

bool isStartOfBlock(const VisiblePosition& pos)
{
    return pos.isNotNull() && pos.deepEquivalent() == startOfBlock(pos, CanCrossEditingBoundary).deepEquivalent();
}

bool isEndOfBlock(const VisiblePosition& pos)
{
    return pos.isNotNull() && pos.deepEquivalent() == endOfBlock(pos, CanCrossEditingBoundary).deepEquivalent();
}

// ---------

template <typename Strategy>
static VisiblePositionTemplate<Strategy> startOfDocumentAlgorithm(const VisiblePositionTemplate<Strategy>& visiblePosition)
{
    Node* node = visiblePosition.deepEquivalent().anchorNode();
    if (!node || !node->document().documentElement())
        return VisiblePositionTemplate<Strategy>();

    return createVisiblePosition(PositionTemplate<Strategy>::firstPositionInNode(node->document().documentElement()));
}

VisiblePosition startOfDocument(const VisiblePosition& c)
{
    return startOfDocumentAlgorithm<EditingStrategy>(c);
}

VisiblePositionInFlatTree startOfDocument(const VisiblePositionInFlatTree& c)
{
    return startOfDocumentAlgorithm<EditingInFlatTreeStrategy>(c);
}

template <typename Strategy>
static VisiblePositionTemplate<Strategy> endOfDocumentAlgorithm(const VisiblePositionTemplate<Strategy>& visiblePosition)
{
    Node* node = visiblePosition.deepEquivalent().anchorNode();
    if (!node || !node->document().documentElement())
        return VisiblePositionTemplate<Strategy>();

    Element* doc = node->document().documentElement();
    return createVisiblePosition(PositionTemplate<Strategy>::lastPositionInNode(doc));
}

VisiblePosition endOfDocument(const VisiblePosition& c)
{
    return endOfDocumentAlgorithm<EditingStrategy>(c);
}

VisiblePositionInFlatTree endOfDocument(const VisiblePositionInFlatTree& c)
{
    return endOfDocumentAlgorithm<EditingInFlatTreeStrategy>(c);
}

bool isStartOfDocument(const VisiblePosition& p)
{
    return p.isNotNull() && previousPositionOf(p, CanCrossEditingBoundary).isNull();
}

bool isEndOfDocument(const VisiblePosition& p)
{
    return p.isNotNull() && nextPositionOf(p, CanCrossEditingBoundary).isNull();
}

// ---------

VisiblePosition startOfEditableContent(const VisiblePosition& visiblePosition)
{
    ContainerNode* highestRoot = highestEditableRoot(visiblePosition.deepEquivalent());
    if (!highestRoot)
        return VisiblePosition();

    return VisiblePosition::firstPositionInNode(highestRoot);
}

VisiblePosition endOfEditableContent(const VisiblePosition& visiblePosition)
{
    ContainerNode* highestRoot = highestEditableRoot(visiblePosition.deepEquivalent());
    if (!highestRoot)
        return VisiblePosition();

    return VisiblePosition::lastPositionInNode(highestRoot);
}

bool isEndOfEditableOrNonEditableContent(const VisiblePosition& position)
{
    return position.isNotNull() && nextPositionOf(position).isNull();
}

// TODO(yosin) We should rename |isEndOfEditableOrNonEditableContent()| what
// this function does, e.g. |isLastVisiblePositionOrEndOfInnerEditor()|.
bool isEndOfEditableOrNonEditableContent(const VisiblePositionInFlatTree& position)
{
    if (position.isNull())
        return false;
    const VisiblePositionInFlatTree nextPosition = nextPositionOf(position);
    if (nextPosition.isNull())
        return true;
    // In DOM version, following condition, the last position of inner editor
    // of INPUT/TEXTAREA element, by |nextPosition().isNull()|, because of
    // an inner editor is an only leaf node.
    if (!nextPosition.deepEquivalent().isAfterAnchor())
        return false;
    return isHTMLTextFormControlElement(nextPosition.deepEquivalent().anchorNode());
}

VisiblePosition leftBoundaryOfLine(const VisiblePosition& c, TextDirection direction)
{
    return direction == LTR ? logicalStartOfLine(c) : logicalEndOfLine(c);
}

VisiblePosition rightBoundaryOfLine(const VisiblePosition& c, TextDirection direction)
{
    return direction == LTR ? logicalEndOfLine(c) : logicalStartOfLine(c);
}

static bool isNonTextLeafChild(LayoutObject* object)
{
    if (object->slowFirstChild())
        return false;
    if (object->isText())
        return false;
    return true;
}

static InlineTextBox* searchAheadForBetterMatch(LayoutObject* layoutObject)
{
    LayoutBlock* container = layoutObject->containingBlock();
    for (LayoutObject* next = layoutObject->nextInPreOrder(container); next; next = next->nextInPreOrder(container)) {
        if (next->isLayoutBlock())
            return 0;
        if (next->isBR())
            return 0;
        if (isNonTextLeafChild(next))
            return 0;
        if (next->isText()) {
            InlineTextBox* match = 0;
            int minOffset = INT_MAX;
            for (InlineTextBox* box = toLayoutText(next)->firstTextBox(); box; box = box->nextTextBox()) {
                int caretMinOffset = box->caretMinOffset();
                if (caretMinOffset < minOffset) {
                    match = box;
                    minOffset = caretMinOffset;
                }
            }
            if (match)
                return match;
        }
    }
    return 0;
}

template <typename Strategy>
PositionTemplate<Strategy> downstreamIgnoringEditingBoundaries(PositionTemplate<Strategy> position)
{
    PositionTemplate<Strategy> lastPosition;
    while (position != lastPosition) {
        lastPosition = position;
        position = mostForwardCaretPosition(position, CanCrossEditingBoundary);
    }
    return position;
}

template <typename Strategy>
PositionTemplate<Strategy> upstreamIgnoringEditingBoundaries(PositionTemplate<Strategy> position)
{
    PositionTemplate<Strategy> lastPosition;
    while (position != lastPosition) {
        lastPosition = position;
        position = mostBackwardCaretPosition(position, CanCrossEditingBoundary);
    }
    return position;
}

template <typename Strategy>
static InlineBoxPosition computeInlineBoxPositionTemplate(const PositionTemplate<Strategy>& position, TextAffinity affinity, TextDirection primaryDirection)
{
    InlineBox* inlineBox = nullptr;
    int caretOffset = position.computeEditingOffset();
    Node* const anchorNode = position.anchorNode();
    LayoutObject* layoutObject = anchorNode->isShadowRoot() ? toShadowRoot(anchorNode)->host().layoutObject() : anchorNode->layoutObject();

    DCHECK(layoutObject) << position;

    if (!layoutObject->isText()) {
        inlineBox = 0;
        if (canHaveChildrenForEditing(anchorNode) && layoutObject->isLayoutBlockFlow() && hasRenderedNonAnonymousDescendantsWithHeight(layoutObject)) {
            // Try a visually equivalent position with possibly opposite
            // editability. This helps in case |this| is in an editable block
            // but surrounded by non-editable positions. It acts to negate the
            // logic at the beginning of
            // |LayoutObject::createPositionWithAffinity()|.
            PositionTemplate<Strategy> equivalent = downstreamIgnoringEditingBoundaries(position);
            if (equivalent == position) {
                equivalent = upstreamIgnoringEditingBoundaries(position);
                if (equivalent == position || downstreamIgnoringEditingBoundaries(equivalent) == position)
                    return InlineBoxPosition(inlineBox, caretOffset);
            }

            return computeInlineBoxPosition(equivalent, TextAffinity::Upstream, primaryDirection);
        }
        if (layoutObject->isBox()) {
            inlineBox = toLayoutBox(layoutObject)->inlineBoxWrapper();
            if (!inlineBox || (caretOffset > inlineBox->caretMinOffset() && caretOffset < inlineBox->caretMaxOffset()))
                return InlineBoxPosition(inlineBox, caretOffset);
        }
    } else {
        LayoutText* textLayoutObject = toLayoutText(layoutObject);

        InlineTextBox* box;
        InlineTextBox* candidate = 0;

        for (box = textLayoutObject->firstTextBox(); box; box = box->nextTextBox()) {
            int caretMinOffset = box->caretMinOffset();
            int caretMaxOffset = box->caretMaxOffset();

            if (caretOffset < caretMinOffset || caretOffset > caretMaxOffset || (caretOffset == caretMaxOffset && box->isLineBreak()))
                continue;

            if (caretOffset > caretMinOffset && caretOffset < caretMaxOffset)
                return InlineBoxPosition(box, caretOffset);

            if (((caretOffset == caretMaxOffset) ^ (affinity == TextAffinity::Downstream))
                || ((caretOffset == caretMinOffset) ^ (affinity == TextAffinity::Upstream))
                || (caretOffset == caretMaxOffset && box->nextLeafChild() && box->nextLeafChild()->isLineBreak()))
                break;

            candidate = box;
        }
        if (candidate && candidate == textLayoutObject->lastTextBox() && affinity == TextAffinity::Downstream) {
            box = searchAheadForBetterMatch(textLayoutObject);
            if (box)
                caretOffset = box->caretMinOffset();
        }
        inlineBox = box ? box : candidate;
    }

    if (!inlineBox)
        return InlineBoxPosition(inlineBox, caretOffset);

    unsigned char level = inlineBox->bidiLevel();

    if (inlineBox->direction() == primaryDirection) {
        if (caretOffset == inlineBox->caretRightmostOffset()) {
            InlineBox* nextBox = inlineBox->nextLeafChild();
            if (!nextBox || nextBox->bidiLevel() >= level)
                return InlineBoxPosition(inlineBox, caretOffset);

            level = nextBox->bidiLevel();
            InlineBox* prevBox = inlineBox;
            do {
                prevBox = prevBox->prevLeafChild();
            } while (prevBox && prevBox->bidiLevel() > level);

            // For example, abc FED 123 ^ CBA
            if (prevBox && prevBox->bidiLevel() == level)
                return InlineBoxPosition(inlineBox, caretOffset);

            // For example, abc 123 ^ CBA
            while (InlineBox* nextBox = inlineBox->nextLeafChild()) {
                if (nextBox->bidiLevel() < level)
                    break;
                inlineBox = nextBox;
            }
            return InlineBoxPosition(inlineBox, inlineBox->caretRightmostOffset());
        }

        InlineBox* prevBox = inlineBox->prevLeafChild();
        if (!prevBox || prevBox->bidiLevel() >= level)
            return InlineBoxPosition(inlineBox, caretOffset);

        level = prevBox->bidiLevel();
        InlineBox* nextBox = inlineBox;
        do {
            nextBox = nextBox->nextLeafChild();
        } while (nextBox && nextBox->bidiLevel() > level);

        if (nextBox && nextBox->bidiLevel() == level)
            return InlineBoxPosition(inlineBox, caretOffset);

        while (InlineBox* prevBox = inlineBox->prevLeafChild()) {
            if (prevBox->bidiLevel() < level)
                break;
            inlineBox = prevBox;
        }
        return InlineBoxPosition(inlineBox, inlineBox->caretLeftmostOffset());
    }

    if (caretOffset == inlineBox->caretLeftmostOffset()) {
        InlineBox* prevBox = inlineBox->prevLeafChildIgnoringLineBreak();
        if (!prevBox || prevBox->bidiLevel() < level) {
            // Left edge of a secondary run. Set to the right edge of the entire
            // run.
            while (InlineBox* nextBox = inlineBox->nextLeafChildIgnoringLineBreak()) {
                if (nextBox->bidiLevel() < level)
                    break;
                inlineBox = nextBox;
            }
            return InlineBoxPosition(inlineBox, inlineBox->caretRightmostOffset());
        }

        if (prevBox->bidiLevel() > level) {
            // Right edge of a "tertiary" run. Set to the left edge of that run.
            while (InlineBox* tertiaryBox = inlineBox->prevLeafChildIgnoringLineBreak()) {
                if (tertiaryBox->bidiLevel() <= level)
                    break;
                inlineBox = tertiaryBox;
            }
            return InlineBoxPosition(inlineBox, inlineBox->caretLeftmostOffset());
        }
        return InlineBoxPosition(inlineBox, caretOffset);
    }

    if (layoutObject && layoutObject->style()->unicodeBidi() == Plaintext) {
        if (inlineBox->bidiLevel() < level)
            return InlineBoxPosition(inlineBox, inlineBox->caretLeftmostOffset());
        return InlineBoxPosition(inlineBox, inlineBox->caretRightmostOffset());
    }

    InlineBox* nextBox = inlineBox->nextLeafChildIgnoringLineBreak();
    if (!nextBox || nextBox->bidiLevel() < level) {
        // Right edge of a secondary run. Set to the left edge of the entire
        // run.
        while (InlineBox* prevBox = inlineBox->prevLeafChildIgnoringLineBreak()) {
            if (prevBox->bidiLevel() < level)
                break;
            inlineBox = prevBox;
        }
        return InlineBoxPosition(inlineBox, inlineBox->caretLeftmostOffset());
    }

    if (nextBox->bidiLevel() <= level)
        return InlineBoxPosition(inlineBox, caretOffset);

    // Left edge of a "tertiary" run. Set to the right edge of that run.
    while (InlineBox* tertiaryBox = inlineBox->nextLeafChildIgnoringLineBreak()) {
        if (tertiaryBox->bidiLevel() <= level)
            break;
        inlineBox = tertiaryBox;
    }
    return InlineBoxPosition(inlineBox, inlineBox->caretRightmostOffset());
}

template <typename Strategy>
static InlineBoxPosition computeInlineBoxPositionTemplate(const PositionTemplate<Strategy>& position, TextAffinity affinity)
{
    return computeInlineBoxPositionTemplate<Strategy>(position, affinity, primaryDirectionOf(*position.anchorNode()));
}

InlineBoxPosition computeInlineBoxPosition(const Position& position, TextAffinity affinity)
{
    return computeInlineBoxPositionTemplate<EditingStrategy>(position, affinity);
}

InlineBoxPosition computeInlineBoxPosition(const PositionInFlatTree& position, TextAffinity affinity)
{
    return computeInlineBoxPositionTemplate<EditingInFlatTreeStrategy>(position, affinity);
}

InlineBoxPosition computeInlineBoxPosition(const VisiblePosition& position)
{
    return computeInlineBoxPosition(position.deepEquivalent(), position.affinity());
}

InlineBoxPosition computeInlineBoxPosition(const VisiblePositionInFlatTree& position)
{
    return computeInlineBoxPosition(position.deepEquivalent(), position.affinity());
}

InlineBoxPosition computeInlineBoxPosition(const Position& position, TextAffinity affinity, TextDirection primaryDirection)
{
    return computeInlineBoxPositionTemplate<EditingStrategy>(position, affinity, primaryDirection);
}

InlineBoxPosition computeInlineBoxPosition(const PositionInFlatTree& position, TextAffinity affinity, TextDirection primaryDirection)
{
    return computeInlineBoxPositionTemplate<EditingInFlatTreeStrategy>(position, affinity, primaryDirection);
}

template <typename Strategy>
LayoutRect localCaretRectOfPositionTemplate(const PositionWithAffinityTemplate<Strategy>& position, LayoutObject*& layoutObject)
{
    if (position.position().isNull()) {
        layoutObject = nullptr;
        return LayoutRect();
    }
    Node* node = position.position().anchorNode();

    layoutObject = node->layoutObject();
    if (!layoutObject)
        return LayoutRect();

    InlineBoxPosition boxPosition = computeInlineBoxPosition(position.position(), position.affinity());

    if (boxPosition.inlineBox)
        layoutObject = LineLayoutAPIShim::layoutObjectFrom(boxPosition.inlineBox->getLineLayoutItem());

    return layoutObject->localCaretRect(boxPosition.inlineBox, boxPosition.offsetInBox);
}

LayoutRect localCaretRectOfPosition(const PositionWithAffinity& position, LayoutObject*& layoutObject)
{
    return localCaretRectOfPositionTemplate<EditingStrategy>(position, layoutObject);
}

LayoutRect localCaretRectOfPosition(const PositionInFlatTreeWithAffinity& position, LayoutObject*& layoutObject)
{
    return localCaretRectOfPositionTemplate<EditingInFlatTreeStrategy>(position, layoutObject);
}

static LayoutUnit boundingBoxLogicalHeight(LayoutObject *o, const LayoutRect& rect)
{
    return o->style()->isHorizontalWritingMode() ? rect.height() : rect.width();
}

bool hasRenderedNonAnonymousDescendantsWithHeight(LayoutObject* layoutObject)
{
    LayoutObject* stop = layoutObject->nextInPreOrderAfterChildren();
    for (LayoutObject *o = layoutObject->slowFirstChild(); o && o != stop; o = o->nextInPreOrder()) {
        if (o->nonPseudoNode()) {
            if ((o->isText() && boundingBoxLogicalHeight(o, toLayoutText(o)->linesBoundingBox()))
                || (o->isBox() && toLayoutBox(o)->pixelSnappedLogicalHeight())
                || (o->isLayoutInline() && isEmptyInline(LineLayoutItem(o)) && boundingBoxLogicalHeight(o, toLayoutInline(o)->linesBoundingBox())))
                return true;
        }
    }
    return false;
}

VisiblePosition visiblePositionForContentsPoint(const IntPoint& contentsPoint, LocalFrame* frame)
{
    HitTestRequest request = HitTestRequest::Move | HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::IgnoreClipping;
    HitTestResult result(request, contentsPoint);
    frame->document()->layoutViewItem().hitTest(result);

    if (Node* node = result.innerNode())
        return createVisiblePosition(positionRespectingEditingBoundary(frame->selection().selection().start(), result.localPoint(), node));
    return VisiblePosition();
}

// TODO(yosin): We should use |associatedLayoutObjectOf()| in "VisibleUnits.cpp"
// where it takes |LayoutObject| from |Position|.
// Note about ::first-letter pseudo-element:
//   When an element has ::first-letter pseudo-element, first letter characters
//   are taken from |Text| node and first letter characters are considered
//   as content of <pseudo:first-letter>.
//   For following HTML,
//      <style>div::first-letter {color: red}</style>
//      <div>abc</div>
//   we have following layout tree:
//      LayoutBlockFlow {DIV} at (0,0) size 784x55
//        LayoutInline {<pseudo:first-letter>} at (0,0) size 22x53
//          LayoutTextFragment (anonymous) at (0,1) size 22x53
//            text run at (0,1) width 22: "a"
//        LayoutTextFragment {#text} at (21,30) size 16x17
//          text run at (21,30) width 16: "bc"
//  In this case, |Text::layoutObject()| for "abc" returns |LayoutTextFragment|
//  containing "bc", and it is called remaining part.
//
//  Even if |Text| node contains only first-letter characters, e.g. just "a",
//  remaining part of |LayoutTextFragment|, with |fragmentLength()| == 0, is
//  appeared in layout tree.
//
//  When |Text| node contains only first-letter characters and whitespaces, e.g.
//  "B\n", associated |LayoutTextFragment| is first-letter part instead of
//  remaining part.
//
//  Punctuation characters are considered as first-letter. For "(1)ab",
//  "(1)" are first-letter part and "ab" are remaining part.
LayoutObject* associatedLayoutObjectOf(const Node& node, int offsetInNode)
{
    DCHECK_GE(offsetInNode, 0);
    LayoutObject* layoutObject = node.layoutObject();
    if (!node.isTextNode() || !layoutObject || !toLayoutText(layoutObject)->isTextFragment())
        return layoutObject;
    LayoutTextFragment* layoutTextFragment = toLayoutTextFragment(layoutObject);
    if (!layoutTextFragment->isRemainingTextLayoutObject()) {
        DCHECK_LE(static_cast<unsigned>(offsetInNode), layoutTextFragment->start() + layoutTextFragment->fragmentLength());
        return layoutTextFragment;
    }
    if (layoutTextFragment->fragmentLength() && static_cast<unsigned>(offsetInNode) >= layoutTextFragment->start())
        return layoutObject;
    LayoutObject* firstLetterLayoutObject = layoutTextFragment->firstLetterPseudoElement()->layoutObject();
    // TODO(yosin): We're not sure when |firstLetterLayoutObject| has
    // multiple child layout object.
    DCHECK_EQ(firstLetterLayoutObject->slowFirstChild(), firstLetterLayoutObject->slowLastChild());
    return firstLetterLayoutObject->slowFirstChild();
}

int caretMinOffset(const Node* node)
{
    LayoutObject* layoutObject = associatedLayoutObjectOf(*node, 0);
    return layoutObject ? layoutObject->caretMinOffset() : 0;
}

int caretMaxOffset(const Node* n)
{
    return EditingStrategy::caretMaxOffset(*n);
}

template <typename Strategy>
static bool inRenderedText(const PositionTemplate<Strategy>& position)
{
    Node* const anchorNode = position.anchorNode();
    if (!anchorNode || !anchorNode->isTextNode())
        return false;

    const int offsetInNode = position.computeEditingOffset();
    LayoutObject* layoutObject = associatedLayoutObjectOf(*anchorNode, offsetInNode);
    if (!layoutObject)
        return false;

    LayoutText* textLayoutObject = toLayoutText(layoutObject);
    const int textOffset = offsetInNode - textLayoutObject->textStartOffset();
    for (InlineTextBox *box = textLayoutObject->firstTextBox(); box; box = box->nextTextBox()) {
        if (textOffset < static_cast<int>(box->start()) && !textLayoutObject->containsReversedText()) {
            // The offset we're looking for is before this node
            // this means the offset must be in content that is
            // not laid out. Return false.
            return false;
        }
        if (box->containsCaretOffset(textOffset)) {
            // Return false for offsets inside composed characters.
            return textOffset == 0 || textOffset == nextGraphemeBoundaryOf(anchorNode, previousGraphemeBoundaryOf(anchorNode, textOffset));
        }
    }

    return false;
}

bool rendersInDifferentPosition(const Position& position1, const Position& position2)
{
    if (position1.isNull() || position2.isNull())
        return false;
    LayoutObject* layoutObject1;
    const LayoutRect& rect1 = localCaretRectOfPosition(PositionWithAffinity(position1), layoutObject1);
    LayoutObject* layoutObject2;
    const LayoutRect& rect2 = localCaretRectOfPosition(PositionWithAffinity(position2), layoutObject2);
    if (!layoutObject1 || !layoutObject2)
        return layoutObject1 != layoutObject2;
    return layoutObject1->localToAbsoluteQuad(FloatRect(rect1)) != layoutObject2->localToAbsoluteQuad(FloatRect(rect2));
}

static bool isVisuallyEmpty(const LayoutObject* layout)
{
    for (LayoutObject* child = layout->slowFirstChild(); child; child = child->nextSibling()) {
        // TODO(xiaochengh): Replace type-based conditioning by virtual function.
        if (child->isBox()) {
            if (!toLayoutBox(child)->size().isEmpty())
                return false;
        } else if (child->isLayoutInline()) {
            if (toLayoutInline(child)->firstLineBoxIncludingCulling())
                return false;
        } else if (child->isText()) {
            if (toLayoutText(child)->hasTextBoxes())
                return false;
        } else {
            return false;
        }
    }
    return true;
}

// FIXME: Share code with isCandidate, if possible.
bool endsOfNodeAreVisuallyDistinctPositions(const Node* node)
{
    if (!node || !node->layoutObject())
        return false;

    if (!node->layoutObject()->isInline())
        return true;

    // Don't include inline tables.
    if (isHTMLTableElement(*node))
        return false;

    // A Marquee elements are moving so we should assume their ends are always
    // visibily distinct.
    if (isHTMLMarqueeElement(*node))
        return true;

    // There is a VisiblePosition inside an empty inline-block container.
    return node->layoutObject()->isAtomicInlineLevel() && canHaveChildrenForEditing(node) && !toLayoutBox(node->layoutObject())->size().isEmpty() && isVisuallyEmpty(node->layoutObject());
}

template <typename Strategy>
static Node* enclosingVisualBoundary(Node* node)
{
    while (node && !endsOfNodeAreVisuallyDistinctPositions(node))
        node = Strategy::parent(*node);

    return node;
}

// upstream() and downstream() want to return positions that are either in a
// text node or at just before a non-text node.  This method checks for that.
template <typename Strategy>
static bool isStreamer(const PositionIteratorAlgorithm<Strategy>& pos)
{
    if (!pos.node())
        return true;

    if (isAtomicNode(pos.node()))
        return true;

    return pos.atStartOfNode();
}

template <typename Strategy>
static PositionTemplate<Strategy> mostBackwardCaretPosition(const PositionTemplate<Strategy>& position, EditingBoundaryCrossingRule rule)
{
    TRACE_EVENT0("input", "VisibleUnits::mostBackwardCaretPosition");

    Node* startNode = position.anchorNode();
    if (!startNode)
        return PositionTemplate<Strategy>();

    // iterate backward from there, looking for a qualified position
    Node* boundary = enclosingVisualBoundary<Strategy>(startNode);
    // FIXME: PositionIterator should respect Before and After positions.
    PositionIteratorAlgorithm<Strategy> lastVisible(position.isAfterAnchor() ? PositionTemplate<Strategy>::editingPositionOf(position.anchorNode(), Strategy::caretMaxOffset(*position.anchorNode())) : position);
    PositionIteratorAlgorithm<Strategy> currentPos = lastVisible;
    bool startEditable = startNode->hasEditableStyle();
    Node* lastNode = startNode;
    bool boundaryCrossed = false;
    for (; !currentPos.atStart(); currentPos.decrement()) {
        Node* currentNode = currentPos.node();
        // Don't check for an editability change if we haven't moved to a different node,
        // to avoid the expense of computing hasEditableStyle().
        if (currentNode != lastNode) {
            // Don't change editability.
            bool currentEditable = currentNode->hasEditableStyle();
            if (startEditable != currentEditable) {
                if (rule == CannotCrossEditingBoundary)
                    break;
                boundaryCrossed = true;
            }
            lastNode = currentNode;
        }

        // If we've moved to a position that is visually distinct, return the last saved position. There
        // is code below that terminates early if we're *about* to move to a visually distinct position.
        if (endsOfNodeAreVisuallyDistinctPositions(currentNode) && currentNode != boundary)
            return lastVisible.deprecatedComputePosition();

        // skip position in non-laid out or invisible node
        LayoutObject* layoutObject = associatedLayoutObjectOf(*currentNode, currentPos.offsetInLeafNode());
        if (!layoutObject || layoutObject->style()->visibility() != VISIBLE)
            continue;

        if (rule == CanCrossEditingBoundary && boundaryCrossed) {
            lastVisible = currentPos;
            break;
        }

        // track last visible streamer position
        if (isStreamer<Strategy>(currentPos))
            lastVisible = currentPos;

        // Don't move past a position that is visually distinct.  We could rely on code above to terminate and
        // return lastVisible on the next iteration, but we terminate early to avoid doing a nodeIndex() call.
        if (endsOfNodeAreVisuallyDistinctPositions(currentNode) && currentPos.atStartOfNode())
            return lastVisible.deprecatedComputePosition();

        // Return position after tables and nodes which have content that can be ignored.
        if (Strategy::editingIgnoresContent(currentNode) || isDisplayInsideTable(currentNode)) {
            if (currentPos.atEndOfNode())
                return PositionTemplate<Strategy>::afterNode(currentNode);
            continue;
        }

        // return current position if it is in laid out text
        if (layoutObject->isText() && toLayoutText(layoutObject)->firstTextBox()) {
            LayoutText* const textLayoutObject = toLayoutText(layoutObject);
            const unsigned textStartOffset = textLayoutObject->textStartOffset();
            if (currentNode != startNode) {
                // This assertion fires in layout tests in the case-transform.html test because
                // of a mix-up between offsets in the text in the DOM tree with text in the
                // layout tree which can have a different length due to case transformation.
                // Until we resolve that, disable this so we can run the layout tests!
                // DCHECK_GE(currentOffset, layoutObject->caretMaxOffset());
                return PositionTemplate<Strategy>(currentNode, layoutObject->caretMaxOffset() + textStartOffset);
            }

            // Map offset in DOM node to offset in InlineBox.
            DCHECK_GE(currentPos.offsetInLeafNode(), static_cast<int>(textStartOffset));
            const unsigned textOffset = currentPos.offsetInLeafNode() - textStartOffset;
            InlineTextBox* lastTextBox = textLayoutObject->lastTextBox();
            for (InlineTextBox* box = textLayoutObject->firstTextBox(); box; box = box->nextTextBox()) {
                if (textOffset == box->start()) {
                    if (textLayoutObject->isTextFragment() && toLayoutTextFragment(layoutObject)->isRemainingTextLayoutObject()) {
                        // |currentPos| is at start of remaining text of
                        // |Text| node with :first-letter.
                        DCHECK_GE(currentPos.offsetInLeafNode(), 1);
                        LayoutObject* firstLetterLayoutObject = toLayoutTextFragment(layoutObject)->firstLetterPseudoElement()->layoutObject();
                        if (firstLetterLayoutObject && firstLetterLayoutObject->style()->visibility() == VISIBLE)
                            return currentPos.computePosition();
                    }
                    continue;
                }
                if (textOffset <= box->start() + box->len()) {
                    if (textOffset > box->start())
                        return currentPos.computePosition();
                    continue;
                }

                if (box == lastTextBox || textOffset != box->start() + box->len() + 1)
                    continue;

                // The text continues on the next line only if the last text box is not on this line and
                // none of the boxes on this line have a larger start offset.

                bool continuesOnNextLine = true;
                InlineBox* otherBox = box;
                while (continuesOnNextLine) {
                    otherBox = otherBox->nextLeafChild();
                    if (!otherBox)
                        break;
                    if (otherBox == lastTextBox || (LineLayoutAPIShim::layoutObjectFrom(otherBox->getLineLayoutItem()) == textLayoutObject && toInlineTextBox(otherBox)->start() > textOffset))
                        continuesOnNextLine = false;
                }

                otherBox = box;
                while (continuesOnNextLine) {
                    otherBox = otherBox->prevLeafChild();
                    if (!otherBox)
                        break;
                    if (otherBox == lastTextBox || (LineLayoutAPIShim::layoutObjectFrom(otherBox->getLineLayoutItem()) == textLayoutObject && toInlineTextBox(otherBox)->start() > textOffset))
                        continuesOnNextLine = false;
                }

                if (continuesOnNextLine)
                    return currentPos.computePosition();
            }
        }
    }
    return lastVisible.deprecatedComputePosition();
}

Position mostBackwardCaretPosition(const Position& position, EditingBoundaryCrossingRule rule)
{
    return mostBackwardCaretPosition<EditingStrategy>(position, rule);
}

PositionInFlatTree mostBackwardCaretPosition(const PositionInFlatTree& position, EditingBoundaryCrossingRule rule)
{
    return mostBackwardCaretPosition<EditingInFlatTreeStrategy>(position, rule);
}

template <typename Strategy>
PositionTemplate<Strategy> mostForwardCaretPosition(const PositionTemplate<Strategy>& position, EditingBoundaryCrossingRule rule)
{
    TRACE_EVENT0("input", "VisibleUnits::mostForwardCaretPosition");

    Node* startNode = position.anchorNode();
    if (!startNode)
        return PositionTemplate<Strategy>();

    // iterate forward from there, looking for a qualified position
    Node* boundary = enclosingVisualBoundary<Strategy>(startNode);
    // FIXME: PositionIterator should respect Before and After positions.
    PositionIteratorAlgorithm<Strategy> lastVisible(position.isAfterAnchor() ? PositionTemplate<Strategy>::editingPositionOf(position.anchorNode(), Strategy::caretMaxOffset(*position.anchorNode())) : position);
    PositionIteratorAlgorithm<Strategy> currentPos = lastVisible;
    bool startEditable = startNode->hasEditableStyle();
    Node* lastNode = startNode;
    bool boundaryCrossed = false;
    for (; !currentPos.atEnd(); currentPos.increment()) {
        Node* currentNode = currentPos.node();
        // Don't check for an editability change if we haven't moved to a different node,
        // to avoid the expense of computing hasEditableStyle().
        if (currentNode != lastNode) {
            // Don't change editability.
            bool currentEditable = currentNode->hasEditableStyle();
            if (startEditable != currentEditable) {
                if (rule == CannotCrossEditingBoundary)
                    break;
                boundaryCrossed = true;
            }

            lastNode = currentNode;
        }

        // stop before going above the body, up into the head
        // return the last visible streamer position
        if (isHTMLBodyElement(*currentNode) && currentPos.atEndOfNode())
            break;

        // Do not move to a visually distinct position.
        if (endsOfNodeAreVisuallyDistinctPositions(currentNode) && currentNode != boundary)
            return lastVisible.deprecatedComputePosition();
        // Do not move past a visually disinct position.
        // Note: The first position after the last in a node whose ends are visually distinct
        // positions will be [boundary->parentNode(), originalBlock->nodeIndex() + 1].
        if (boundary && Strategy::parent(*boundary) == currentNode)
            return lastVisible.deprecatedComputePosition();

        // skip position in non-laid out or invisible node
        LayoutObject* layoutObject = associatedLayoutObjectOf(*currentNode, currentPos.offsetInLeafNode());
        if (!layoutObject || layoutObject->style()->visibility() != VISIBLE)
            continue;

        if (rule == CanCrossEditingBoundary && boundaryCrossed) {
            lastVisible = currentPos;
            break;
        }

        // track last visible streamer position
        if (isStreamer<Strategy>(currentPos))
            lastVisible = currentPos;

        // Return position before tables and nodes which have content that can be ignored.
        if (Strategy::editingIgnoresContent(currentNode) || isDisplayInsideTable(currentNode)) {
            if (currentPos.offsetInLeafNode() <= layoutObject->caretMinOffset())
                return PositionTemplate<Strategy>::editingPositionOf(currentNode, layoutObject->caretMinOffset());
            continue;
        }

        // return current position if it is in laid out text
        if (layoutObject->isText() && toLayoutText(layoutObject)->firstTextBox()) {
            LayoutText* const textLayoutObject = toLayoutText(layoutObject);
            const unsigned textStartOffset = textLayoutObject->textStartOffset();
            if (currentNode != startNode) {
                DCHECK(currentPos.atStartOfNode());
                return PositionTemplate<Strategy>(currentNode, layoutObject->caretMinOffset() + textStartOffset);
            }

            // Map offset in DOM node to offset in InlineBox.
            DCHECK_GE(currentPos.offsetInLeafNode(), static_cast<int>(textStartOffset));
            const unsigned textOffset = currentPos.offsetInLeafNode() - textStartOffset;
            InlineTextBox* lastTextBox = textLayoutObject->lastTextBox();
            for (InlineTextBox* box = textLayoutObject->firstTextBox(); box; box = box->nextTextBox()) {
                if (textOffset <= box->end()) {
                    if (textOffset >= box->start())
                        return currentPos.computePosition();
                    continue;
                }

                if (box == lastTextBox || textOffset != box->start() + box->len())
                    continue;

                // The text continues on the next line only if the last text box is not on this line and
                // none of the boxes on this line have a larger start offset.

                bool continuesOnNextLine = true;
                InlineBox* otherBox = box;
                while (continuesOnNextLine) {
                    otherBox = otherBox->nextLeafChild();
                    if (!otherBox)
                        break;
                    if (otherBox == lastTextBox || (LineLayoutAPIShim::layoutObjectFrom(otherBox->getLineLayoutItem()) == textLayoutObject && toInlineTextBox(otherBox)->start() >= textOffset))
                        continuesOnNextLine = false;
                }

                otherBox = box;
                while (continuesOnNextLine) {
                    otherBox = otherBox->prevLeafChild();
                    if (!otherBox)
                        break;
                    if (otherBox == lastTextBox || (LineLayoutAPIShim::layoutObjectFrom(otherBox->getLineLayoutItem()) == textLayoutObject && toInlineTextBox(otherBox)->start() >= textOffset))
                        continuesOnNextLine = false;
                }

                if (continuesOnNextLine)
                    return currentPos.computePosition();
            }
        }
    }

    return lastVisible.deprecatedComputePosition();
}

Position mostForwardCaretPosition(const Position& position, EditingBoundaryCrossingRule rule)
{
    return mostForwardCaretPosition<EditingStrategy>(position, rule);
}

PositionInFlatTree mostForwardCaretPosition(const PositionInFlatTree& position, EditingBoundaryCrossingRule rule)
{
    return mostForwardCaretPosition<EditingInFlatTreeStrategy>(position, rule);
}

// Returns true if the visually equivalent positions around have different
// editability. A position is considered at editing boundary if one of the
// following is true:
// 1. It is the first position in the node and the next visually equivalent
//    position is non editable.
// 2. It is the last position in the node and the previous visually equivalent
//    position is non editable.
// 3. It is an editable position and both the next and previous visually
//    equivalent positions are both non editable.
template <typename Strategy>
static bool atEditingBoundary(const PositionTemplate<Strategy> positions)
{
    PositionTemplate<Strategy> nextPosition = mostForwardCaretPosition(positions, CanCrossEditingBoundary);
    if (positions.atFirstEditingPositionForNode() && nextPosition.isNotNull() && !nextPosition.anchorNode()->hasEditableStyle())
        return true;

    PositionTemplate<Strategy> prevPosition = mostBackwardCaretPosition(positions, CanCrossEditingBoundary);
    if (positions.atLastEditingPositionForNode() && prevPosition.isNotNull() && !prevPosition.anchorNode()->hasEditableStyle())
        return true;

    return nextPosition.isNotNull() && !nextPosition.anchorNode()->hasEditableStyle()
        && prevPosition.isNotNull() && !prevPosition.anchorNode()->hasEditableStyle();
}

template <typename Strategy>
static bool isVisuallyEquivalentCandidateAlgorithm(const PositionTemplate<Strategy>& position)
{
    Node* const anchorNode = position.anchorNode();
    if (!anchorNode)
        return false;

    LayoutObject* layoutObject = anchorNode->layoutObject();
    if (!layoutObject)
        return false;

    if (layoutObject->style()->visibility() != VISIBLE)
        return false;

    if (layoutObject->isBR()) {
        // TODO(leviw) The condition should be
        // m_anchorType == PositionAnchorType::BeforeAnchor, but for now we
        // still need to support legacy positions.
        if (position.isAfterAnchor())
            return false;
        if (position.computeEditingOffset())
            return false;
        const Node* parent = Strategy::parent(*anchorNode);
        return parent->layoutObject() && parent->layoutObject()->isSelectable();
    }

    if (layoutObject->isText())
        return layoutObject->isSelectable() && inRenderedText(position);

    if (layoutObject->isSVG()) {
        // We don't consider SVG elements are contenteditable except for
        // associated |layoutObject| returns |isText()| true,
        // e.g. |LayoutSVGInlineText|.
        return false;
    }

    if (isDisplayInsideTable(anchorNode) || Strategy::editingIgnoresContent(anchorNode)) {
        if (!position.atFirstEditingPositionForNode() && !position.atLastEditingPositionForNode())
            return false;
        const Node* parent = Strategy::parent(*anchorNode);
        return parent->layoutObject() && parent->layoutObject()->isSelectable();
    }

    if (anchorNode->document().documentElement() == anchorNode || anchorNode->isDocumentNode())
        return false;

    if (!layoutObject->isSelectable())
        return false;

    if (layoutObject->isLayoutBlockFlow() || layoutObject->isFlexibleBox() || layoutObject->isLayoutGrid()) {
        if (toLayoutBlock(layoutObject)->logicalHeight() || isHTMLBodyElement(*anchorNode)) {
            if (!hasRenderedNonAnonymousDescendantsWithHeight(layoutObject))
                return position.atFirstEditingPositionForNode();
            return anchorNode->hasEditableStyle() && atEditingBoundary(position);
        }
    } else {
        LocalFrame* frame = anchorNode->document().frame();
        bool caretBrowsing = frame->settings() && frame->settings()->caretBrowsingEnabled();
        return (caretBrowsing || anchorNode->hasEditableStyle()) && atEditingBoundary(position);
    }

    return false;
}

bool isVisuallyEquivalentCandidate(const Position& position)
{
    return isVisuallyEquivalentCandidateAlgorithm<EditingStrategy>(position);
}

bool isVisuallyEquivalentCandidate(const PositionInFlatTree& position)
{
    return isVisuallyEquivalentCandidateAlgorithm<EditingInFlatTreeStrategy>(position);
}

template <typename Strategy>
static IntRect absoluteCaretBoundsOfAlgorithm(const VisiblePositionTemplate<Strategy>& visiblePosition)
{
    LayoutObject* layoutObject;
    LayoutRect localRect = localCaretRectOfPosition(visiblePosition.toPositionWithAffinity(), layoutObject);
    if (localRect.isEmpty() || !layoutObject)
        return IntRect();

    return layoutObject->localToAbsoluteQuad(FloatRect(localRect)).enclosingBoundingBox();
}

IntRect absoluteCaretBoundsOf(const VisiblePosition& visiblePosition)
{
    return absoluteCaretBoundsOfAlgorithm<EditingStrategy>(visiblePosition);
}

IntRect absoluteCaretBoundsOf(const VisiblePositionInFlatTree& visiblePosition)
{
    return absoluteCaretBoundsOfAlgorithm<EditingInFlatTreeStrategy>(visiblePosition);
}

template <typename Strategy>
static VisiblePositionTemplate<Strategy> skipToEndOfEditingBoundary(const VisiblePositionTemplate<Strategy>& pos, const PositionTemplate<Strategy>& anchor)
{
    if (pos.isNull())
        return pos;

    ContainerNode* highestRoot = highestEditableRoot(anchor);
    ContainerNode* highestRootOfPos = highestEditableRoot(pos.deepEquivalent());

    // Return |pos| itself if the two are from the very same editable region,
    // or both are non-editable.
    if (highestRootOfPos == highestRoot)
        return pos;

    // If this is not editable but |pos| has an editable root, skip to the end
    if (!highestRoot && highestRootOfPos)
        return createVisiblePosition(PositionTemplate<Strategy>(highestRootOfPos, PositionAnchorType::AfterAnchor).parentAnchoredEquivalent());

    // That must mean that |pos| is not editable. Return the next position after
    // |pos| that is in the same editable region as this position
    DCHECK(highestRoot);
    return firstEditableVisiblePositionAfterPositionInRoot(pos.deepEquivalent(), *highestRoot);
}

template <typename Strategy>
static UChar32 characterAfterAlgorithm(const VisiblePositionTemplate<Strategy>& visiblePosition)
{
    // We canonicalize to the first of two equivalent candidates, but the second
    // of the two candidates is the one that will be inside the text node
    // containing the character after this visible position.
    const PositionTemplate<Strategy> pos = mostForwardCaretPosition(visiblePosition.deepEquivalent());
    if (!pos.isOffsetInAnchor())
        return 0;
    Node* containerNode = pos.computeContainerNode();
    if (!containerNode || !containerNode->isTextNode())
        return 0;
    unsigned offset = static_cast<unsigned>(pos.offsetInContainerNode());
    Text* textNode = toText(containerNode);
    unsigned length = textNode->length();
    if (offset >= length)
        return 0;

    return textNode->data().characterStartingAt(offset);
}

UChar32 characterAfter(const VisiblePosition& visiblePosition)
{
    return characterAfterAlgorithm<EditingStrategy>(visiblePosition);
}

UChar32 characterAfter(const VisiblePositionInFlatTree& visiblePosition)
{
    return characterAfterAlgorithm<EditingInFlatTreeStrategy>(visiblePosition);
}

template <typename Strategy>
static UChar32 characterBeforeAlgorithm(const VisiblePositionTemplate<Strategy>& visiblePosition)
{
    return characterAfter(previousPositionOf(visiblePosition));
}

UChar32 characterBefore(const VisiblePosition& visiblePosition)
{
    return characterBeforeAlgorithm<EditingStrategy>(visiblePosition);
}

UChar32 characterBefore(const VisiblePositionInFlatTree& visiblePosition)
{
    return characterBeforeAlgorithm<EditingInFlatTreeStrategy>(visiblePosition);
}

template <typename Strategy>
static PositionTemplate<Strategy> leftVisuallyDistinctCandidate(const VisiblePositionTemplate<Strategy>& visiblePosition)
{
    const PositionTemplate<Strategy> deepPosition = visiblePosition.deepEquivalent();
    PositionTemplate<Strategy> p = deepPosition;

    if (p.isNull())
        return PositionTemplate<Strategy>();

    const PositionTemplate<Strategy> downstreamStart = mostForwardCaretPosition(p);
    TextDirection primaryDirection = primaryDirectionOf(*p.anchorNode());
    const TextAffinity affinity = visiblePosition.affinity();

    while (true) {
        InlineBoxPosition boxPosition = computeInlineBoxPosition(p, affinity, primaryDirection);
        InlineBox* box = boxPosition.inlineBox;
        int offset = boxPosition.offsetInBox;
        if (!box)
            return primaryDirection == LTR ? previousVisuallyDistinctCandidate(deepPosition) : nextVisuallyDistinctCandidate(deepPosition);

        LineLayoutItem lineLayoutItem = box->getLineLayoutItem();

        while (true) {
            if ((lineLayoutItem.isAtomicInlineLevel() || lineLayoutItem.isBR()) && offset == box->caretRightmostOffset())
                return box->isLeftToRightDirection() ? previousVisuallyDistinctCandidate(deepPosition) : nextVisuallyDistinctCandidate(deepPosition);

            if (!lineLayoutItem.node()) {
                box = box->prevLeafChild();
                if (!box)
                    return primaryDirection == LTR ? previousVisuallyDistinctCandidate(deepPosition) : nextVisuallyDistinctCandidate(deepPosition);
                lineLayoutItem = box->getLineLayoutItem();
                offset = box->caretRightmostOffset();
                continue;
            }

            offset = box->isLeftToRightDirection() ? previousGraphemeBoundaryOf(lineLayoutItem.node(), offset) : nextGraphemeBoundaryOf(lineLayoutItem.node(), offset);

            int caretMinOffset = box->caretMinOffset();
            int caretMaxOffset = box->caretMaxOffset();

            if (offset > caretMinOffset && offset < caretMaxOffset)
                break;

            if (box->isLeftToRightDirection() ? offset < caretMinOffset : offset > caretMaxOffset) {
                // Overshot to the left.
                InlineBox* prevBox = box->prevLeafChildIgnoringLineBreak();
                if (!prevBox) {
                    PositionTemplate<Strategy> positionOnLeft = primaryDirection == LTR ? previousVisuallyDistinctCandidate(visiblePosition.deepEquivalent()) : nextVisuallyDistinctCandidate(visiblePosition.deepEquivalent());
                    if (positionOnLeft.isNull())
                        return PositionTemplate<Strategy>();

                    InlineBox* boxOnLeft = computeInlineBoxPosition(positionOnLeft, affinity, primaryDirection).inlineBox;
                    if (boxOnLeft && boxOnLeft->root() == box->root())
                        return PositionTemplate<Strategy>();
                    return positionOnLeft;
                }

                // Reposition at the other logical position corresponding to our
                // edge's visual position and go for another round.
                box = prevBox;
                lineLayoutItem = box->getLineLayoutItem();
                offset = prevBox->caretRightmostOffset();
                continue;
            }

            DCHECK_EQ(offset, box->caretLeftmostOffset());

            unsigned char level = box->bidiLevel();
            InlineBox* prevBox = box->prevLeafChild();

            if (box->direction() == primaryDirection) {
                if (!prevBox) {
                    InlineBox* logicalStart = 0;
                    if (primaryDirection == LTR ? box->root().getLogicalStartBoxWithNode(logicalStart) : box->root().getLogicalEndBoxWithNode(logicalStart)) {
                        box = logicalStart;
                        lineLayoutItem = box->getLineLayoutItem();
                        offset = primaryDirection == LTR ? box->caretMinOffset() : box->caretMaxOffset();
                    }
                    break;
                }
                if (prevBox->bidiLevel() >= level)
                    break;

                level = prevBox->bidiLevel();

                InlineBox* nextBox = box;
                do {
                    nextBox = nextBox->nextLeafChild();
                } while (nextBox && nextBox->bidiLevel() > level);

                if (nextBox && nextBox->bidiLevel() == level)
                    break;

                box = prevBox;
                lineLayoutItem = box->getLineLayoutItem();
                offset = box->caretRightmostOffset();
                if (box->direction() == primaryDirection)
                    break;
                continue;
            }

            while (prevBox && !prevBox->getLineLayoutItem().node())
                prevBox = prevBox->prevLeafChild();

            if (prevBox) {
                box = prevBox;
                lineLayoutItem = box->getLineLayoutItem();
                offset = box->caretRightmostOffset();
                if (box->bidiLevel() > level) {
                    do {
                        prevBox = prevBox->prevLeafChild();
                    } while (prevBox && prevBox->bidiLevel() > level);

                    if (!prevBox || prevBox->bidiLevel() < level)
                        continue;
                }
            } else {
                // Trailing edge of a secondary run. Set to the leading edge of
                // the entire run.
                while (true) {
                    while (InlineBox* nextBox = box->nextLeafChild()) {
                        if (nextBox->bidiLevel() < level)
                            break;
                        box = nextBox;
                    }
                    if (box->bidiLevel() == level)
                        break;
                    level = box->bidiLevel();
                    while (InlineBox* prevBox = box->prevLeafChild()) {
                        if (prevBox->bidiLevel() < level)
                            break;
                        box = prevBox;
                    }
                    if (box->bidiLevel() == level)
                        break;
                    level = box->bidiLevel();
                }
                lineLayoutItem = box->getLineLayoutItem();
                offset = primaryDirection == LTR ? box->caretMinOffset() : box->caretMaxOffset();
            }
            break;
        }

        p = PositionTemplate<Strategy>::editingPositionOf(lineLayoutItem.node(), offset);

        if ((isVisuallyEquivalentCandidate(p) && mostForwardCaretPosition(p) != downstreamStart) || p.atStartOfTree() || p.atEndOfTree())
            return p;

        DCHECK_NE(p, deepPosition);
    }
}

template <typename Strategy>
VisiblePositionTemplate<Strategy> leftPositionOfAlgorithm(const VisiblePositionTemplate<Strategy>& visiblePosition)
{
    const PositionTemplate<Strategy> pos = leftVisuallyDistinctCandidate(visiblePosition);
    // TODO(yosin) Why can't we move left from the last position in a tree?
    if (pos.atStartOfTree() || pos.atEndOfTree())
        return VisiblePositionTemplate<Strategy>();

    const VisiblePositionTemplate<Strategy> left = createVisiblePosition(pos);
    DCHECK_NE(left.deepEquivalent(), visiblePosition.deepEquivalent());

    return directionOfEnclosingBlock(left.deepEquivalent()) == LTR ? honorEditingBoundaryAtOrBefore(left, visiblePosition.deepEquivalent()) : honorEditingBoundaryAtOrAfter(left, visiblePosition.deepEquivalent());
}

VisiblePosition leftPositionOf(const VisiblePosition& visiblePosition)
{
    return leftPositionOfAlgorithm<EditingStrategy>(visiblePosition);
}

VisiblePositionInFlatTree leftPositionOf(const VisiblePositionInFlatTree& visiblePosition)
{
    return leftPositionOfAlgorithm<EditingInFlatTreeStrategy>(visiblePosition);
}

template <typename Strategy>
static PositionTemplate<Strategy> rightVisuallyDistinctCandidate(const VisiblePositionTemplate<Strategy>& visiblePosition)
{
    const PositionTemplate<Strategy> deepPosition = visiblePosition.deepEquivalent();
    PositionTemplate<Strategy> p = deepPosition;
    if (p.isNull())
        return PositionTemplate<Strategy>();

    const PositionTemplate<Strategy> downstreamStart = mostForwardCaretPosition(p);
    TextDirection primaryDirection = primaryDirectionOf(*p.anchorNode());
    const TextAffinity affinity = visiblePosition.affinity();

    while (true) {
        InlineBoxPosition boxPosition = computeInlineBoxPosition(p, affinity, primaryDirection);
        InlineBox* box = boxPosition.inlineBox;
        int offset = boxPosition.offsetInBox;
        if (!box)
            return primaryDirection == LTR ? nextVisuallyDistinctCandidate(deepPosition) : previousVisuallyDistinctCandidate(deepPosition);

        LayoutObject* layoutObject = LineLayoutAPIShim::layoutObjectFrom(box->getLineLayoutItem());

        while (true) {
            if ((layoutObject->isAtomicInlineLevel() || layoutObject->isBR()) && offset == box->caretLeftmostOffset())
                return box->isLeftToRightDirection() ? nextVisuallyDistinctCandidate(deepPosition) : previousVisuallyDistinctCandidate(deepPosition);

            if (!layoutObject->node()) {
                box = box->nextLeafChild();
                if (!box)
                    return primaryDirection == LTR ? nextVisuallyDistinctCandidate(deepPosition) : previousVisuallyDistinctCandidate(deepPosition);
                layoutObject = LineLayoutAPIShim::layoutObjectFrom(box->getLineLayoutItem());
                offset = box->caretLeftmostOffset();
                continue;
            }

            offset = box->isLeftToRightDirection() ? nextGraphemeBoundaryOf(layoutObject->node(), offset) : previousGraphemeBoundaryOf(layoutObject->node(), offset);

            int caretMinOffset = box->caretMinOffset();
            int caretMaxOffset = box->caretMaxOffset();

            if (offset > caretMinOffset && offset < caretMaxOffset)
                break;

            if (box->isLeftToRightDirection() ? offset > caretMaxOffset : offset < caretMinOffset) {
                // Overshot to the right.
                InlineBox* nextBox = box->nextLeafChildIgnoringLineBreak();
                if (!nextBox) {
                    PositionTemplate<Strategy> positionOnRight = primaryDirection == LTR ? nextVisuallyDistinctCandidate(deepPosition) : previousVisuallyDistinctCandidate(deepPosition);
                    if (positionOnRight.isNull())
                        return PositionTemplate<Strategy>();

                    InlineBox* boxOnRight = computeInlineBoxPosition(positionOnRight, affinity, primaryDirection).inlineBox;
                    if (boxOnRight && boxOnRight->root() == box->root())
                        return PositionTemplate<Strategy>();
                    return positionOnRight;
                }

                // Reposition at the other logical position corresponding to our
                // edge's visual position and go for another round.
                box = nextBox;
                layoutObject = LineLayoutAPIShim::layoutObjectFrom(box->getLineLayoutItem());
                offset = nextBox->caretLeftmostOffset();
                continue;
            }

            DCHECK_EQ(offset, box->caretRightmostOffset());

            unsigned char level = box->bidiLevel();
            InlineBox* nextBox = box->nextLeafChild();

            if (box->direction() == primaryDirection) {
                if (!nextBox) {
                    InlineBox* logicalEnd = 0;
                    if (primaryDirection == LTR ? box->root().getLogicalEndBoxWithNode(logicalEnd) : box->root().getLogicalStartBoxWithNode(logicalEnd)) {
                        box = logicalEnd;
                        layoutObject = LineLayoutAPIShim::layoutObjectFrom(box->getLineLayoutItem());
                        offset = primaryDirection == LTR ? box->caretMaxOffset() : box->caretMinOffset();
                    }
                    break;
                }

                if (nextBox->bidiLevel() >= level)
                    break;

                level = nextBox->bidiLevel();

                InlineBox* prevBox = box;
                do {
                    prevBox = prevBox->prevLeafChild();
                } while (prevBox && prevBox->bidiLevel() > level);

                // For example, abc FED 123 ^ CBA
                if (prevBox && prevBox->bidiLevel() == level)
                    break;

                // For example, abc 123 ^ CBA or 123 ^ CBA abc
                box = nextBox;
                layoutObject = LineLayoutAPIShim::layoutObjectFrom(box->getLineLayoutItem());
                offset = box->caretLeftmostOffset();
                if (box->direction() == primaryDirection)
                    break;
                continue;
            }

            while (nextBox && !nextBox->getLineLayoutItem().node())
                nextBox = nextBox->nextLeafChild();

            if (nextBox) {
                box = nextBox;
                layoutObject = LineLayoutAPIShim::layoutObjectFrom(box->getLineLayoutItem());
                offset = box->caretLeftmostOffset();

                if (box->bidiLevel() > level) {
                    do {
                        nextBox = nextBox->nextLeafChild();
                    } while (nextBox && nextBox->bidiLevel() > level);

                    if (!nextBox || nextBox->bidiLevel() < level)
                        continue;
                }
            } else {
                // Trailing edge of a secondary run. Set to the leading edge of the entire run.
                while (true) {
                    while (InlineBox* prevBox = box->prevLeafChild()) {
                        if (prevBox->bidiLevel() < level)
                            break;
                        box = prevBox;
                    }
                    if (box->bidiLevel() == level)
                        break;
                    level = box->bidiLevel();
                    while (InlineBox* nextBox = box->nextLeafChild()) {
                        if (nextBox->bidiLevel() < level)
                            break;
                        box = nextBox;
                    }
                    if (box->bidiLevel() == level)
                        break;
                    level = box->bidiLevel();
                }
                layoutObject = LineLayoutAPIShim::layoutObjectFrom(box->getLineLayoutItem());
                offset = primaryDirection == LTR ? box->caretMaxOffset() : box->caretMinOffset();
            }
            break;
        }

        p = PositionTemplate<Strategy>::editingPositionOf(layoutObject->node(), offset);

        if ((isVisuallyEquivalentCandidate(p) && mostForwardCaretPosition(p) != downstreamStart) || p.atStartOfTree() || p.atEndOfTree())
            return p;

        DCHECK_NE(p, deepPosition);
    }
}

template <typename Strategy>
static VisiblePositionTemplate<Strategy> rightPositionOfAlgorithm(const VisiblePositionTemplate<Strategy>& visiblePosition)
{
    const PositionTemplate<Strategy> pos = rightVisuallyDistinctCandidate(visiblePosition);
    // FIXME: Why can't we move left from the last position in a tree?
    if (pos.atStartOfTree() || pos.atEndOfTree())
        return VisiblePositionTemplate<Strategy>();

    const VisiblePositionTemplate<Strategy> right = createVisiblePosition(pos);
    DCHECK_NE(right.deepEquivalent(), visiblePosition.deepEquivalent());

    return directionOfEnclosingBlock(right.deepEquivalent()) == LTR ? honorEditingBoundaryAtOrAfter(right, visiblePosition.deepEquivalent()) : honorEditingBoundaryAtOrBefore(right, visiblePosition.deepEquivalent());
}

VisiblePosition rightPositionOf(const VisiblePosition& visiblePosition)
{
    return rightPositionOfAlgorithm<EditingStrategy>(visiblePosition);
}

VisiblePositionInFlatTree rightPositionOf(const VisiblePositionInFlatTree& visiblePosition)
{
    return rightPositionOfAlgorithm<EditingInFlatTreeStrategy>(visiblePosition);
}

template <typename Strategy>
static VisiblePositionTemplate<Strategy> nextPositionOfAlgorithm(const VisiblePositionTemplate<Strategy>& visiblePosition, EditingBoundaryCrossingRule rule)
{
    const VisiblePositionTemplate<Strategy> next = createVisiblePosition(nextVisuallyDistinctCandidate(visiblePosition.deepEquivalent()), visiblePosition.affinity());

    switch (rule) {
    case CanCrossEditingBoundary:
        return next;
    case CannotCrossEditingBoundary:
        return honorEditingBoundaryAtOrAfter(next, visiblePosition.deepEquivalent());
    case CanSkipOverEditingBoundary:
        return skipToEndOfEditingBoundary(next, visiblePosition.deepEquivalent());
    }
    NOTREACHED();
    return honorEditingBoundaryAtOrAfter(next, visiblePosition.deepEquivalent());
}

VisiblePosition nextPositionOf(const VisiblePosition& visiblePosition, EditingBoundaryCrossingRule rule)
{
    return nextPositionOfAlgorithm<EditingStrategy>(visiblePosition, rule);
}

VisiblePositionInFlatTree nextPositionOf(const VisiblePositionInFlatTree& visiblePosition, EditingBoundaryCrossingRule rule)
{
    return nextPositionOfAlgorithm<EditingInFlatTreeStrategy>(visiblePosition, rule);
}

template <typename Strategy>
static VisiblePositionTemplate<Strategy> skipToStartOfEditingBoundary(const VisiblePositionTemplate<Strategy>& pos, const PositionTemplate<Strategy>& anchor)
{
    if (pos.isNull())
        return pos;

    ContainerNode* highestRoot = highestEditableRoot(anchor);
    ContainerNode* highestRootOfPos = highestEditableRoot(pos.deepEquivalent());

    // Return |pos| itself if the two are from the very same editable region, or
    // both are non-editable.
    if (highestRootOfPos == highestRoot)
        return pos;

    // If this is not editable but |pos| has an editable root, skip to the start
    if (!highestRoot && highestRootOfPos)
        return createVisiblePosition(previousVisuallyDistinctCandidate(PositionTemplate<Strategy>(highestRootOfPos, PositionAnchorType::BeforeAnchor).parentAnchoredEquivalent()));

    // That must mean that |pos| is not editable. Return the last position
    // before |pos| that is in the same editable region as this position
    DCHECK(highestRoot);
    return lastEditableVisiblePositionBeforePositionInRoot(pos.deepEquivalent(), *highestRoot);
}

template <typename Strategy>
static VisiblePositionTemplate<Strategy> previousPositionOfAlgorithm(const VisiblePositionTemplate<Strategy>& visiblePosition, EditingBoundaryCrossingRule rule)
{
    const PositionTemplate<Strategy> pos = previousVisuallyDistinctCandidate(visiblePosition.deepEquivalent());

    // return null visible position if there is no previous visible position
    if (pos.atStartOfTree())
        return VisiblePositionTemplate<Strategy>();

    // we should always be able to make the affinity |TextAffinity::Downstream|,
    // because going previous from an |TextAffinity::Upstream| position can
    // never yield another |TextAffinity::Upstream position| (unless line wrap
    // length is 0!).
    const VisiblePositionTemplate<Strategy> prev = createVisiblePosition(pos);
    DCHECK_NE(prev.deepEquivalent(), visiblePosition.deepEquivalent());

    switch (rule) {
    case CanCrossEditingBoundary:
        return prev;
    case CannotCrossEditingBoundary:
        return honorEditingBoundaryAtOrBefore(prev, visiblePosition.deepEquivalent());
    case CanSkipOverEditingBoundary:
        return skipToStartOfEditingBoundary(prev, visiblePosition.deepEquivalent());
    }

    NOTREACHED();
    return honorEditingBoundaryAtOrBefore(prev, visiblePosition.deepEquivalent());
}

VisiblePosition previousPositionOf(const VisiblePosition& visiblePosition, EditingBoundaryCrossingRule rule)
{
    return previousPositionOfAlgorithm<EditingStrategy>(visiblePosition, rule);
}

VisiblePositionInFlatTree previousPositionOf(const VisiblePositionInFlatTree& visiblePosition, EditingBoundaryCrossingRule rule)
{
    return previousPositionOfAlgorithm<EditingInFlatTreeStrategy>(visiblePosition, rule);
}

} // namespace blink
