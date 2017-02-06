/*
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#include "core/editing/EditingUtilities.h"

#include "core/HTMLElementFactory.h"
#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/dom/Range.h"
#include "core/dom/Text.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/EditingStrategy.h"
#include "core/editing/Editor.h"
#include "core/editing/PlainTextRange.h"
#include "core/editing/PositionIterator.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/VisibleSelection.h"
#include "core/editing/VisibleUnits.h"
#include "core/editing/iterators/TextIterator.h"
#include "core/editing/serializers/HTMLInterchange.h"
#include "core/editing/state_machines/BackspaceStateMachine.h"
#include "core/editing/state_machines/BackwardGraphemeBoundaryStateMachine.h"
#include "core/editing/state_machines/ForwardGraphemeBoundaryStateMachine.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLBRElement.h"
#include "core/html/HTMLDivElement.h"
#include "core/html/HTMLLIElement.h"
#include "core/html/HTMLParagraphElement.h"
#include "core/html/HTMLSpanElement.h"
#include "core/html/HTMLTableCellElement.h"
#include "core/html/HTMLUListElement.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutTableCell.h"
#include "wtf/Assertions.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/Unicode.h"

namespace blink {

using namespace HTMLNames;

namespace {

std::ostream& operator<<(std::ostream& os, PositionMoveType type)
{
    static const char* const texts[] = {
        "CodeUnit", "BackwardDeletion", "GraphemeCluster"
    };
    const auto& it = std::begin(texts) + static_cast<size_t>(type);
    DCHECK_GE(it, std::begin(texts)) << "Unknown PositionMoveType value";
    DCHECK_LT(it, std::end(texts)) << "Unknown PositionMoveType value";
    return os << *it;
}

} // namespace

bool needsLayoutTreeUpdate(const Node& node)
{
    const Document& document = node.document();
    if (document.needsLayoutTreeUpdate())
        return true;
    // TODO(yosin): We should make |document::needsLayoutTreeUpdate()| to
    // check |LayoutView::needsLayout()|.
    return document.view() && document.view()->needsLayout();
}

bool needsLayoutTreeUpdate(const Position& position)
{
    const Node* node = position.anchorNode();
    if (!node)
        return false;
    return needsLayoutTreeUpdate(*node);
}

// Atomic means that the node has no children, or has children which are ignored for the
// purposes of editing.
bool isAtomicNode(const Node *node)
{
    return node && (!node->hasChildren() || editingIgnoresContent(node));
}

template <typename Traversal>
static int comparePositions(Node* containerA, int offsetA, Node* containerB, int offsetB, bool* disconnected)
{
    DCHECK(containerA);
    DCHECK(containerB);

    if (disconnected)
        *disconnected = false;

    if (!containerA)
        return -1;
    if (!containerB)
        return 1;

    // see DOM2 traversal & range section 2.5

    // case 1: both points have the same container
    if (containerA == containerB) {
        if (offsetA == offsetB)
            return 0; // A is equal to B
        if (offsetA < offsetB)
            return -1; // A is before B
        return 1; // A is after B
    }

    // case 2: node C (container B or an ancestor) is a child node of A
    Node* c = containerB;
    while (c && Traversal::parent(*c) != containerA)
        c = Traversal::parent(*c);
    if (c) {
        int offsetC = 0;
        Node* n = Traversal::firstChild(*containerA);
        while (n != c && offsetC < offsetA) {
            offsetC++;
            n = Traversal::nextSibling(*n);
        }

        if (offsetA <= offsetC)
            return -1; // A is before B
        return 1; // A is after B
    }

    // case 3: node C (container A or an ancestor) is a child node of B
    c = containerA;
    while (c && Traversal::parent(*c) != containerB)
        c = Traversal::parent(*c);
    if (c) {
        int offsetC = 0;
        Node* n = Traversal::firstChild(*containerB);
        while (n != c && offsetC < offsetB) {
            offsetC++;
            n = Traversal::nextSibling(*n);
        }

        if (offsetC < offsetB)
            return -1; // A is before B
        return 1; // A is after B
    }

    // case 4: containers A & B are siblings, or children of siblings
    // ### we need to do a traversal here instead
    Node* commonAncestor = Traversal::commonAncestor(*containerA, *containerB);
    if (!commonAncestor) {
        if (disconnected)
            *disconnected = true;
        return 0;
    }
    Node* childA = containerA;
    while (childA && Traversal::parent(*childA) != commonAncestor)
        childA = Traversal::parent(*childA);
    if (!childA)
        childA = commonAncestor;
    Node* childB = containerB;
    while (childB && Traversal::parent(*childB) != commonAncestor)
        childB = Traversal::parent(*childB);
    if (!childB)
        childB = commonAncestor;

    if (childA == childB)
        return 0; // A is equal to B

    Node* n = Traversal::firstChild(*commonAncestor);
    while (n) {
        if (n == childA)
            return -1; // A is before B
        if (n == childB)
            return 1; // A is after B
        n = Traversal::nextSibling(*n);
    }

    // Should never reach this point.
    NOTREACHED();
    return 0;
}

int comparePositionsInDOMTree(Node* containerA, int offsetA, Node* containerB, int offsetB, bool* disconnected)
{
    return comparePositions<NodeTraversal>(containerA, offsetA, containerB, offsetB, disconnected);
}

int comparePositionsInFlatTree(Node* containerA, int offsetA, Node* containerB, int offsetB, bool* disconnected)
{
    return comparePositions<FlatTreeTraversal>(containerA, offsetA, containerB, offsetB, disconnected);
}

// Compare two positions, taking into account the possibility that one or both
// could be inside a shadow tree. Only works for non-null values.
int comparePositions(const Position& a, const Position& b)
{
    DCHECK(a.isNotNull());
    DCHECK(b.isNotNull());
    const TreeScope* commonScope = Position::commonAncestorTreeScope(a, b);

    DCHECK(commonScope);
    if (!commonScope)
        return 0;

    Node* nodeA = commonScope->ancestorInThisScope(a.computeContainerNode());
    DCHECK(nodeA);
    bool hasDescendentA = nodeA != a.computeContainerNode();
    int offsetA = hasDescendentA ? 0 : a.computeOffsetInContainerNode();

    Node* nodeB = commonScope->ancestorInThisScope(b.computeContainerNode());
    DCHECK(nodeB);
    bool hasDescendentB = nodeB != b.computeContainerNode();
    int offsetB = hasDescendentB ? 0 : b.computeOffsetInContainerNode();

    int bias = 0;
    if (nodeA == nodeB) {
        if (hasDescendentA)
            bias = -1;
        else if (hasDescendentB)
            bias = 1;
    }

    int result = comparePositionsInDOMTree(nodeA, offsetA, nodeB, offsetB);
    return result ? result : bias;
}

int comparePositions(const PositionWithAffinity& a, const PositionWithAffinity& b)
{
    return comparePositions(a.position(), b.position());
}

int comparePositions(const VisiblePosition& a, const VisiblePosition& b)
{
    return comparePositions(a.deepEquivalent(), b.deepEquivalent());
}

ContainerNode* highestEditableRoot(const Position& position, EditableType editableType)
{
    if (position.isNull())
        return 0;

    ContainerNode* highestRoot = rootEditableElementOf(position, editableType);
    if (!highestRoot)
        return 0;

    if (isHTMLBodyElement(*highestRoot))
        return highestRoot;

    ContainerNode* node = highestRoot->parentNode();
    while (node) {
        if (node->hasEditableStyle(editableType))
            highestRoot = node;
        if (isHTMLBodyElement(*node))
            break;
        node = node->parentNode();
    }

    return highestRoot;
}

ContainerNode* highestEditableRoot(const PositionInFlatTree& position, EditableType editableType)
{
    return highestEditableRoot(toPositionInDOMTree(position), editableType);
}

bool isEditablePosition(const Position& position, EditableType editableType)
{
    Node* node = position.parentAnchoredEquivalent().anchorNode();
    if (!node)
        return false;
    DCHECK(node->document().isActive());
    if (node->document().lifecycle().state() >= DocumentLifecycle::InStyleRecalc) {
        // TODO(yosin): Once we change |LayoutObject::adjustStyleDifference()|
        // not to call |FrameSelection::hasCaret()|, we should not assume
        // calling |isEditablePosition()| in |InStyleRecalc| is safe.
    } else {
        DCHECK(!needsLayoutTreeUpdate(position)) << position;
    }

    if (isDisplayInsideTable(node))
        node = node->parentNode();

    if (node->isDocumentNode())
        return false;
    return node->hasEditableStyle(editableType);
}

bool isEditablePosition(const PositionInFlatTree& p, EditableType editableType)
{
    return isEditablePosition(toPositionInDOMTree(p), editableType);
}

bool isAtUnsplittableElement(const Position& pos)
{
    Node* node = pos.anchorNode();
    return (node == rootEditableElementOf(pos) || node == enclosingNodeOfType(pos, &isTableCell));
}


bool isRichlyEditablePosition(const Position& p, EditableType editableType)
{
    Node* node = p.anchorNode();
    if (!node)
        return false;

    if (isDisplayInsideTable(node))
        node = node->parentNode();

    return node->layoutObjectIsRichlyEditable(editableType);
}

Element* rootEditableElementOf(const Position& p, EditableType editableType)
{
    Node* node = p.computeContainerNode();
    if (!node)
        return 0;

    if (isDisplayInsideTable(node))
        node = node->parentNode();

    return node->rootEditableElement(editableType);
}

Element* rootEditableElementOf(const PositionInFlatTree& p, EditableType editableType)
{
    return rootEditableElementOf(toPositionInDOMTree(p), editableType);
}

// TODO(yosin) This does not handle [table, 0] correctly.
Element* rootEditableElementOf(const VisiblePosition& visiblePosition)
{
    Node* anchorNode = visiblePosition.deepEquivalent().anchorNode();
    return anchorNode ? anchorNode->rootEditableElement() : nullptr;
}

// Finds the enclosing element until which the tree can be split.
// When a user hits ENTER, they won't expect this element to be split into two.
// You may pass it as the second argument of splitTreeToNode.
Element* unsplittableElementForPosition(const Position& p)
{
    // Since enclosingNodeOfType won't search beyond the highest root editable node,
    // this code works even if the closest table cell was outside of the root editable node.
    Element* enclosingCell = toElement(enclosingNodeOfType(p, &isTableCell));
    if (enclosingCell)
        return enclosingCell;

    return rootEditableElementOf(p);
}

template <typename Strategy>
PositionTemplate<Strategy> nextCandidateAlgorithm(const PositionTemplate<Strategy>& position)
{
    TRACE_EVENT0("input", "EditingUtility::nextCandidateAlgorithm");
    PositionIteratorAlgorithm<Strategy> p(position);

    p.increment();
    while (!p.atEnd()) {
        PositionTemplate<Strategy> candidate = p.computePosition();
        if (isVisuallyEquivalentCandidate(candidate))
            return candidate;

        p.increment();
    }

    return PositionTemplate<Strategy>();
}

Position nextCandidate(const Position& position)
{
    return nextCandidateAlgorithm<EditingStrategy>(position);
}

PositionInFlatTree nextCandidate(const PositionInFlatTree& position)
{
    return nextCandidateAlgorithm<EditingInFlatTreeStrategy>(position);
}

// |nextVisuallyDistinctCandidate| is similar to |nextCandidate| except
// for returning position which |downstream()| not equal to initial position's
// |downstream()|.
template <typename Strategy>
static PositionTemplate<Strategy> nextVisuallyDistinctCandidateAlgorithm(const PositionTemplate<Strategy>& position)
{
    TRACE_EVENT0("input", "EditingUtility::nextVisuallyDistinctCandidateAlgorithm");
    if (position.isNull())
        return PositionTemplate<Strategy>();

    PositionIteratorAlgorithm<Strategy> p(position);
    const PositionTemplate<Strategy> downstreamStart = mostForwardCaretPosition(position);

    p.increment();
    while (!p.atEnd()) {
        PositionTemplate<Strategy> candidate = p.computePosition();
        if (isVisuallyEquivalentCandidate(candidate) && mostForwardCaretPosition(candidate) != downstreamStart)
            return candidate;

        p.increment();
    }

    return PositionTemplate<Strategy>();
}

Position nextVisuallyDistinctCandidate(const Position& position)
{
    return nextVisuallyDistinctCandidateAlgorithm<EditingStrategy>(position);
}

PositionInFlatTree nextVisuallyDistinctCandidate(const PositionInFlatTree& position)
{
    return nextVisuallyDistinctCandidateAlgorithm<EditingInFlatTreeStrategy>(position);
}

template <typename Strategy>
PositionTemplate<Strategy> previousCandidateAlgorithm(const PositionTemplate<Strategy>& position)
{
    TRACE_EVENT0("input", "EditingUtility::previousCandidateAlgorithm");
    PositionIteratorAlgorithm<Strategy> p(position);

    p.decrement();
    while (!p.atStart()) {
        PositionTemplate<Strategy> candidate = p.computePosition();
        if (isVisuallyEquivalentCandidate(candidate))
            return candidate;

        p.decrement();
    }

    return PositionTemplate<Strategy>();
}

Position previousCandidate(const Position& position)
{
    return previousCandidateAlgorithm<EditingStrategy>(position);
}

PositionInFlatTree previousCandidate(const PositionInFlatTree& position)
{
    return previousCandidateAlgorithm<EditingInFlatTreeStrategy>(position);
}

// |previousVisuallyDistinctCandidate| is similar to |previousCandidate| except
// for returning position which |downstream()| not equal to initial position's
// |downstream()|.
template <typename Strategy>
PositionTemplate<Strategy> previousVisuallyDistinctCandidateAlgorithm(const PositionTemplate<Strategy>& position)
{
    TRACE_EVENT0("input", "EditingUtility::previousVisuallyDistinctCandidateAlgorithm");
    if (position.isNull())
        return PositionTemplate<Strategy>();

    PositionIteratorAlgorithm<Strategy> p(position);
    PositionTemplate<Strategy> downstreamStart = mostForwardCaretPosition(position);

    p.decrement();
    while (!p.atStart()) {
        PositionTemplate<Strategy> candidate = p.computePosition();
        if (isVisuallyEquivalentCandidate(candidate) && mostForwardCaretPosition(candidate) != downstreamStart)
            return candidate;

        p.decrement();
    }

    return PositionTemplate<Strategy>();
}

Position previousVisuallyDistinctCandidate(const Position& position)
{
    return previousVisuallyDistinctCandidateAlgorithm<EditingStrategy>(position);
}

PositionInFlatTree previousVisuallyDistinctCandidate(const PositionInFlatTree& position)
{
    return previousVisuallyDistinctCandidateAlgorithm<EditingInFlatTreeStrategy>(position);
}

VisiblePosition firstEditableVisiblePositionAfterPositionInRoot(const Position& position, ContainerNode& highestRoot)
{
    return createVisiblePosition(firstEditablePositionAfterPositionInRoot(position, highestRoot));
}

VisiblePositionInFlatTree firstEditableVisiblePositionAfterPositionInRoot(const PositionInFlatTree& position, ContainerNode& highestRoot)
{
    return createVisiblePosition(firstEditablePositionAfterPositionInRoot(position, highestRoot));
}

template <typename Strategy>
PositionTemplate<Strategy> firstEditablePositionAfterPositionInRootAlgorithm(const PositionTemplate<Strategy>& position, Node& highestRoot)
{
    DCHECK(!needsLayoutTreeUpdate(highestRoot)) << position << ' ' << highestRoot;
    // position falls before highestRoot.
    if (position.compareTo(PositionTemplate<Strategy>::firstPositionInNode(&highestRoot)) == -1 && highestRoot.hasEditableStyle())
        return PositionTemplate<Strategy>::firstPositionInNode(&highestRoot);

    PositionTemplate<Strategy> editablePosition = position;

    if (position.anchorNode()->treeScope() != highestRoot.treeScope()) {
        Node* shadowAncestor = highestRoot.treeScope().ancestorInThisScope(editablePosition.anchorNode());
        if (!shadowAncestor)
            return PositionTemplate<Strategy>();

        editablePosition = PositionTemplate<Strategy>::afterNode(shadowAncestor);
    }

    while (editablePosition.anchorNode() && !isEditablePosition(editablePosition) && editablePosition.anchorNode()->isDescendantOf(&highestRoot))
        editablePosition = isAtomicNode(editablePosition.anchorNode()) ? PositionTemplate<Strategy>::inParentAfterNode(*editablePosition.anchorNode()) : nextVisuallyDistinctCandidate(editablePosition);

    if (editablePosition.anchorNode() && editablePosition.anchorNode() != &highestRoot && !editablePosition.anchorNode()->isDescendantOf(&highestRoot))
        return PositionTemplate<Strategy>();

    return editablePosition;
}

Position firstEditablePositionAfterPositionInRoot(const Position& position, Node& highestRoot)
{
    return firstEditablePositionAfterPositionInRootAlgorithm<EditingStrategy>(position, highestRoot);
}

PositionInFlatTree firstEditablePositionAfterPositionInRoot(const PositionInFlatTree& position, Node& highestRoot)
{
    return firstEditablePositionAfterPositionInRootAlgorithm<EditingInFlatTreeStrategy>(position, highestRoot);
}

VisiblePosition lastEditableVisiblePositionBeforePositionInRoot(const Position& position, ContainerNode& highestRoot)
{
    return createVisiblePosition(lastEditablePositionBeforePositionInRoot(position, highestRoot));
}

VisiblePositionInFlatTree lastEditableVisiblePositionBeforePositionInRoot(const PositionInFlatTree& position, ContainerNode& highestRoot)
{
    return createVisiblePosition(lastEditablePositionBeforePositionInRoot(position, highestRoot));
}

template <typename Strategy>
PositionTemplate<Strategy> lastEditablePositionBeforePositionInRootAlgorithm(const PositionTemplate<Strategy>& position, Node& highestRoot)
{
    DCHECK(!needsLayoutTreeUpdate(highestRoot)) << position << ' ' << highestRoot;
    // When position falls after highestRoot, the result is easy to compute.
    if (position.compareTo(PositionTemplate<Strategy>::lastPositionInNode(&highestRoot)) == 1)
        return PositionTemplate<Strategy>::lastPositionInNode(&highestRoot);

    PositionTemplate<Strategy> editablePosition = position;

    if (position.anchorNode()->treeScope() != highestRoot.treeScope()) {
        Node* shadowAncestor = highestRoot.treeScope().ancestorInThisScope(editablePosition.anchorNode());
        if (!shadowAncestor)
            return PositionTemplate<Strategy>();

        editablePosition = PositionTemplate<Strategy>::firstPositionInOrBeforeNode(shadowAncestor);
    }

    while (editablePosition.anchorNode() && !isEditablePosition(editablePosition) && editablePosition.anchorNode()->isDescendantOf(&highestRoot))
        editablePosition = isAtomicNode(editablePosition.anchorNode()) ? PositionTemplate<Strategy>::inParentBeforeNode(*editablePosition.anchorNode()) : previousVisuallyDistinctCandidate(editablePosition);

    if (editablePosition.anchorNode() && editablePosition.anchorNode() != &highestRoot && !editablePosition.anchorNode()->isDescendantOf(&highestRoot))
        return PositionTemplate<Strategy>();
    return editablePosition;
}

Position lastEditablePositionBeforePositionInRoot(const Position& position, Node& highestRoot)
{
    return lastEditablePositionBeforePositionInRootAlgorithm<EditingStrategy>(position, highestRoot);
}

PositionInFlatTree lastEditablePositionBeforePositionInRoot(const PositionInFlatTree& position, Node& highestRoot)
{
    return lastEditablePositionBeforePositionInRootAlgorithm<EditingInFlatTreeStrategy>(position, highestRoot);
}

template<typename StateMachine>
int findNextBoundaryOffset(const String& str, int current)
{
    StateMachine machine;
    TextSegmentationMachineState state = TextSegmentationMachineState::Invalid;

    for (int i = current - 1; i >= 0; --i) {
        state = machine.feedPrecedingCodeUnit(str[i]);
        if (state != TextSegmentationMachineState::NeedMoreCodeUnit)
            break;
    }
    if (current == 0 || state == TextSegmentationMachineState::NeedMoreCodeUnit)
        state = machine.tellEndOfPrecedingText();
    if (state == TextSegmentationMachineState::Finished)
        return current + machine.finalizeAndGetBoundaryOffset();
    const int length = str.length();
    DCHECK_EQ(TextSegmentationMachineState::NeedFollowingCodeUnit, state);
    for (int i = current; i < length; ++i) {
        state = machine.feedFollowingCodeUnit(str[i]);
        if (state != TextSegmentationMachineState::NeedMoreCodeUnit)
            break;
    }
    return current + machine.finalizeAndGetBoundaryOffset();
}

int previousGraphemeBoundaryOf(const Node* node, int current)
{
    // TODO(yosin): Need to support grapheme crossing |Node| boundary.
    DCHECK_GE(current, 0);
    if (current <= 1 || !node->isTextNode())
        return current - 1;
    const String& text = toText(node)->data();
    // TODO(yosin): Replace with DCHECK for out-of-range request.
    if (static_cast<unsigned>(current) > text.length())
        return current - 1;
    return findNextBoundaryOffset<BackwardGraphemeBoundaryStateMachine>(text, current);
}

static int previousBackwardDeletionOffsetOf(const Node* node, int current)
{
    DCHECK_GE(current, 0);
    if (current <= 1)
        return 0;
    if (!node->isTextNode())
        return current - 1;

    const String& text = toText(node)->data();
    DCHECK_LT(static_cast<unsigned>(current - 1), text.length());
    return findNextBoundaryOffset<BackspaceStateMachine>(text, current);
}

int nextGraphemeBoundaryOf(const Node* node, int current)
{
    // TODO(yosin): Need to support grapheme crossing |Node| boundary.
    if (!node->isTextNode())
        return current + 1;
    const String& text = toText(node)->data();
    const int length = text.length();
    DCHECK_LE(current, length);
    if (current >= length - 1)
        return current + 1;
    return findNextBoundaryOffset<ForwardGraphemeBoundaryStateMachine>(text, current);
}

template <typename Strategy>
PositionTemplate<Strategy> previousPositionOfAlgorithm(const PositionTemplate<Strategy>& position, PositionMoveType moveType)
{
    Node* const node = position.anchorNode();
    if (!node)
        return position;

    const int offset = position.computeEditingOffset();

    if (offset > 0) {
        if (editingIgnoresContent(node))
            return PositionTemplate<Strategy>::beforeNode(node);
        if (Node* child = Strategy::childAt(*node, offset - 1))
            return PositionTemplate<Strategy>::lastPositionInOrAfterNode(child);

        // There are two reasons child might be 0:
        //   1) The node is node like a text node that is not an element, and
        //      therefore has no children. Going backward one character at a
        //      time is correct.
        //   2) The old offset was a bogus offset like (<br>, 1), and there is
        //      no child. Going from 1 to 0 is correct.
        switch (moveType) {
        case PositionMoveType::CodeUnit:
            return PositionTemplate<Strategy>(node, offset - 1);
        case PositionMoveType::BackwardDeletion:
            return PositionTemplate<Strategy>(node, previousBackwardDeletionOffsetOf(node, offset));
        case PositionMoveType::GraphemeCluster:
            return PositionTemplate<Strategy>(node, previousGraphemeBoundaryOf(node, offset));
        default:
            NOTREACHED() << "Unhandled moveType: " << moveType;
        }
    }

    if (ContainerNode* parent = Strategy::parent(*node)) {
        if (editingIgnoresContent(parent))
            return PositionTemplate<Strategy>::beforeNode(parent);
        // TODO(yosin) We should use |Strategy::index(Node&)| instead of
        // |Node::nodeIndex()|.
        return PositionTemplate<Strategy>(parent, node->nodeIndex());
    }
    return position;
}

Position previousPositionOf(const Position& position, PositionMoveType moveType)
{
    return previousPositionOfAlgorithm<EditingStrategy>(position, moveType);
}

PositionInFlatTree previousPositionOf(const PositionInFlatTree& position, PositionMoveType moveType)
{
    return previousPositionOfAlgorithm<EditingInFlatTreeStrategy>(position, moveType);
}

template <typename Strategy>
PositionTemplate<Strategy> nextPositionOfAlgorithm(const PositionTemplate<Strategy>& position, PositionMoveType moveType)
{
    // TODO(yosin): We should have printer for PositionMoveType.
    DCHECK(moveType != PositionMoveType::BackwardDeletion);

    Node* node = position.anchorNode();
    if (!node)
        return position;

    const int offset = position.computeEditingOffset();

    if (Node* child = Strategy::childAt(*node, offset))
        return PositionTemplate<Strategy>::firstPositionInOrBeforeNode(child);

    // TODO(yosin) We should use |Strategy::lastOffsetForEditing()| instead of
    // DOM tree version.
    if (!Strategy::hasChildren(*node) && offset < EditingStrategy::lastOffsetForEditing(node)) {
        // There are two reasons child might be 0:
        //   1) The node is node like a text node that is not an element, and
        //      therefore has no children. Going forward one character at a time
        //      is correct.
        //   2) The new offset is a bogus offset like (<br>, 1), and there is no
        //      child. Going from 0 to 1 is correct.
        switch (moveType) {
        case PositionMoveType::CodeUnit:
            return PositionTemplate<Strategy>::editingPositionOf(node, offset + 1);
        case PositionMoveType::BackwardDeletion:
            NOTREACHED()
                << "BackwardDeletion is only available for prevPositionOf "
                << "functions.";
            return PositionTemplate<Strategy>::editingPositionOf(node, offset + 1);
        case PositionMoveType::GraphemeCluster:
            return PositionTemplate<Strategy>::editingPositionOf(node, nextGraphemeBoundaryOf(node, offset));
        default:
            NOTREACHED() << "Unhandled moveType: " << moveType;
        }
    }

    if (ContainerNode* parent = Strategy::parent(*node))
        return PositionTemplate<Strategy>::editingPositionOf(parent, Strategy::index(*node) + 1);
    return position;
}

Position nextPositionOf(const Position& position, PositionMoveType moveType)
{
    return nextPositionOfAlgorithm<EditingStrategy>(position, moveType);
}

PositionInFlatTree nextPositionOf(const PositionInFlatTree& position, PositionMoveType moveType)
{
    return nextPositionOfAlgorithm<EditingInFlatTreeStrategy>(position, moveType);
}

bool isEnclosingBlock(const Node* node)
{
    return node && node->layoutObject() && !node->layoutObject()->isInline() && !node->layoutObject()->isRubyText();
}

bool isInline(const Node* node)
{
    return node && node->computedStyle()->display() == INLINE;
}

// TODO(yosin) Deploy this in all of the places where |enclosingBlockFlow()| and
// |enclosingBlockFlowOrTableElement()| are used.
// TODO(yosin) Callers of |Node| version of |enclosingBlock()| should use
// |Position| version The enclosing block of [table, x] for example, should be
// the block that contains the table and not the table, and this function should
// be the only one responsible for knowing about these kinds of special cases.
Element* enclosingBlock(Node* node, EditingBoundaryCrossingRule rule)
{
    return enclosingBlock(firstPositionInOrBeforeNode(node), rule);
}

template <typename Strategy>
Element* enclosingBlockAlgorithm(const PositionTemplate<Strategy>& position, EditingBoundaryCrossingRule rule)
{
    Node* enclosingNode = enclosingNodeOfType(position, isEnclosingBlock, rule);
    return enclosingNode && enclosingNode->isElementNode() ? toElement(enclosingNode) : nullptr;
}

Element* enclosingBlock(const Position& position, EditingBoundaryCrossingRule rule)
{
    return enclosingBlockAlgorithm<EditingStrategy>(position, rule);
}

Element* enclosingBlock(const PositionInFlatTree& position, EditingBoundaryCrossingRule rule)
{
    return enclosingBlockAlgorithm<EditingInFlatTreeStrategy>(position, rule);
}

Element* enclosingBlockFlowElement(const Node& node)
{
    if (isBlockFlowElement(node))
        return const_cast<Element*>(&toElement(node));

    for (Node& runner : NodeTraversal::ancestorsOf(node)) {
        if (isBlockFlowElement(runner) || isHTMLBodyElement(runner))
            return toElement(&runner);
    }
    return nullptr;
}

bool nodeIsUserSelectAll(const Node* node)
{
    return RuntimeEnabledFeatures::userSelectAllEnabled() && node && node->layoutObject() && node->layoutObject()->style()->userSelect() == SELECT_ALL;

}

template <typename Strategy>
TextDirection directionOfEnclosingBlockAlgorithm(const PositionTemplate<Strategy>& position)
{
    Element* enclosingBlockElement = enclosingBlock(PositionTemplate<Strategy>::firstPositionInOrBeforeNode(position.computeContainerNode()), CannotCrossEditingBoundary);
    if (!enclosingBlockElement)
        return LTR;
    LayoutObject* layoutObject = enclosingBlockElement->layoutObject();
    return layoutObject ? layoutObject->style()->direction() : LTR;
}

TextDirection directionOfEnclosingBlock(const Position& position)
{
    return directionOfEnclosingBlockAlgorithm<EditingStrategy>(position);
}

TextDirection directionOfEnclosingBlock(const PositionInFlatTree& position)
{
    return directionOfEnclosingBlockAlgorithm<EditingInFlatTreeStrategy>(position);
}

TextDirection primaryDirectionOf(const Node& node)
{
    TextDirection primaryDirection = LTR;
    for (const LayoutObject* r = node.layoutObject(); r; r = r->parent()) {
        if (r->isLayoutBlockFlow()) {
            primaryDirection = r->style()->direction();
            break;
        }
    }

    return primaryDirection;
}

String stringWithRebalancedWhitespace(const String& string, bool startIsStartOfParagraph, bool endIsEndOfParagraph)
{
    unsigned length = string.length();

    StringBuilder rebalancedString;
    rebalancedString.reserveCapacity(length);

    bool previousCharacterWasSpace = false;
    for (size_t i = 0; i < length; i++) {
        UChar c = string[i];
        if (!isWhitespace(c)) {
            rebalancedString.append(c);
            previousCharacterWasSpace = false;
            continue;
        }

        if (previousCharacterWasSpace || (!i && startIsStartOfParagraph) || (i + 1 == length && endIsEndOfParagraph)) {
            rebalancedString.append(noBreakSpaceCharacter);
            previousCharacterWasSpace = false;
        } else {
            rebalancedString.append(' ');
            previousCharacterWasSpace = true;
        }
    }

    DCHECK_EQ(rebalancedString.length(), length);

    return rebalancedString.toString();
}

bool isTableStructureNode(const Node *node)
{
    LayoutObject* layoutObject = node->layoutObject();
    return (layoutObject && (layoutObject->isTableCell() || layoutObject->isTableRow() || layoutObject->isTableSection() || layoutObject->isLayoutTableCol()));
}

const String& nonBreakingSpaceString()
{
    DEFINE_STATIC_LOCAL(String, nonBreakingSpaceString, (&noBreakSpaceCharacter, 1));
    return nonBreakingSpaceString;
}

// FIXME: need to dump this
static bool isSpecialHTMLElement(const Node& n)
{
    if (!n.isHTMLElement())
        return false;

    if (n.isLink())
        return true;

    LayoutObject* layoutObject = n.layoutObject();
    if (!layoutObject)
        return false;

    if (layoutObject->style()->display() == TABLE || layoutObject->style()->display() == INLINE_TABLE)
        return true;

    if (layoutObject->style()->isFloating())
        return true;

    return false;
}

static HTMLElement* firstInSpecialElement(const Position& pos)
{
    Element* rootEditableElement = pos.computeContainerNode()->rootEditableElement();
    for (Node& runner : NodeTraversal::inclusiveAncestorsOf(*pos.anchorNode())) {
        if (runner.rootEditableElement() != rootEditableElement)
            break;
        if (isSpecialHTMLElement(runner)) {
            HTMLElement* specialElement = toHTMLElement(&runner);
            VisiblePosition vPos = createVisiblePosition(pos);
            VisiblePosition firstInElement = createVisiblePosition(firstPositionInOrBeforeNode(specialElement));
            if (isDisplayInsideTable(specialElement) && vPos.deepEquivalent() == nextPositionOf(firstInElement).deepEquivalent())
                return specialElement;
            if (vPos.deepEquivalent() == firstInElement.deepEquivalent())
                return specialElement;
        }
    }
    return 0;
}

static HTMLElement* lastInSpecialElement(const Position& pos)
{
    Element* rootEditableElement = pos.computeContainerNode()->rootEditableElement();
    for (Node& runner : NodeTraversal::inclusiveAncestorsOf(*pos.anchorNode())) {
        if (runner.rootEditableElement() != rootEditableElement)
            break;
        if (isSpecialHTMLElement(runner)) {
            HTMLElement* specialElement = toHTMLElement(&runner);
            VisiblePosition vPos = createVisiblePosition(pos);
            VisiblePosition lastInElement = createVisiblePosition(lastPositionInOrAfterNode(specialElement));
            if (isDisplayInsideTable(specialElement) && vPos.deepEquivalent() == previousPositionOf(lastInElement).deepEquivalent())
                return specialElement;
            if (vPos.deepEquivalent() == lastInElement.deepEquivalent())
                return specialElement;
        }
    }
    return 0;
}

Position positionBeforeContainingSpecialElement(const Position& pos, HTMLElement** containingSpecialElement)
{
    HTMLElement* n = firstInSpecialElement(pos);
    if (!n)
        return pos;
    Position result = Position::inParentBeforeNode(*n);
    if (result.isNull() || result.anchorNode()->rootEditableElement() != pos.anchorNode()->rootEditableElement())
        return pos;
    if (containingSpecialElement)
        *containingSpecialElement = n;
    return result;
}

Position positionAfterContainingSpecialElement(const Position& pos, HTMLElement** containingSpecialElement)
{
    HTMLElement* n = lastInSpecialElement(pos);
    if (!n)
        return pos;
    Position result = Position::inParentAfterNode(*n);
    if (result.isNull() || result.anchorNode()->rootEditableElement() != pos.anchorNode()->rootEditableElement())
        return pos;
    if (containingSpecialElement)
        *containingSpecialElement = n;
    return result;
}

template <typename Strategy>
static Element* tableElementJustBeforeAlgorithm(const VisiblePositionTemplate<Strategy>& visiblePosition)
{
    const PositionTemplate<Strategy> upstream(mostBackwardCaretPosition(visiblePosition.deepEquivalent()));
    if (isDisplayInsideTable(upstream.anchorNode()) && upstream.atLastEditingPositionForNode())
        return toElement(upstream.anchorNode());

    return nullptr;
}

Element* tableElementJustBefore(const VisiblePosition& visiblePosition)
{
    return tableElementJustBeforeAlgorithm<EditingStrategy>(visiblePosition);
}

Element* tableElementJustBefore(const VisiblePositionInFlatTree& visiblePosition)
{
    return tableElementJustBeforeAlgorithm<EditingInFlatTreeStrategy>(visiblePosition);
}

Element* tableElementJustAfter(const VisiblePosition& visiblePosition)
{
    Position downstream(mostForwardCaretPosition(visiblePosition.deepEquivalent()));
    if (isDisplayInsideTable(downstream.anchorNode()) && downstream.atFirstEditingPositionForNode())
        return toElement(downstream.anchorNode());

    return 0;
}

static Node* previousNodeConsideringAtomicNodes(const Node& start)
{
    if (start.previousSibling()) {
        Node* node = start.previousSibling();
        while (!isAtomicNode(node) && node->lastChild())
            node = node->lastChild();
        return node;
    }
    return start.parentNode();
}

static Node* nextNodeConsideringAtomicNodes(const Node& start)
{
    if (!isAtomicNode(&start) && start.hasChildren())
        return start.firstChild();
    if (start.nextSibling())
        return start.nextSibling();
    const Node* node = &start;
    while (node && !node->nextSibling())
        node = node->parentNode();
    if (node)
        return node->nextSibling();
    return nullptr;
}

Node* previousAtomicLeafNode(const Node& start)
{
    Node* node = previousNodeConsideringAtomicNodes(start);
    while (node) {
        if (isAtomicNode(node))
            return node;
        node = previousNodeConsideringAtomicNodes(*node);
    }
    return nullptr;
}

Node* nextAtomicLeafNode(const Node& start)
{
    Node* node = nextNodeConsideringAtomicNodes(start);
    while (node) {
        if (isAtomicNode(node))
            return node;
        node = nextNodeConsideringAtomicNodes(*node);
    }
    return nullptr;
}

// Returns the visible position at the beginning of a node
VisiblePosition visiblePositionBeforeNode(Node& node)
{
    if (node.hasChildren())
        return createVisiblePosition(firstPositionInOrBeforeNode(&node));
    DCHECK(node.parentNode()) << node;
    DCHECK(!node.parentNode()->isShadowRoot()) << node.parentNode();
    return VisiblePosition::inParentBeforeNode(node);
}

// Returns the visible position at the ending of a node
VisiblePosition visiblePositionAfterNode(Node& node)
{
    if (node.hasChildren())
        return createVisiblePosition(lastPositionInOrAfterNode(&node));
    DCHECK(node.parentNode()) << node.parentNode();
    DCHECK(!node.parentNode()->isShadowRoot()) << node.parentNode();
    return VisiblePosition::inParentAfterNode(node);
}

bool isHTMLListElement(Node* n)
{
    return (n && (isHTMLUListElement(*n) || isHTMLOListElement(*n) || isHTMLDListElement(*n)));
}

bool isListItem(const Node* n)
{
    return n && n->layoutObject() && n->layoutObject()->isListItem();
}

Element* associatedElementOf(const Position& position)
{
    Node* node = position.anchorNode();
    if (!node || node->isElementNode())
        return toElement(node);
    ContainerNode* parent = NodeTraversal::parent(*node);
    return parent && parent->isElementNode() ? toElement(parent) : nullptr;
}

Element* enclosingElementWithTag(const Position& p, const QualifiedName& tagName)
{
    if (p.isNull())
        return 0;

    ContainerNode* root = highestEditableRoot(p);
    Element* ancestor = p.anchorNode()->isElementNode() ? toElement(p.anchorNode()) : p.anchorNode()->parentElement();
    for (; ancestor; ancestor = ancestor->parentElement()) {
        if (root && !ancestor->hasEditableStyle())
            continue;
        if (ancestor->hasTagName(tagName))
            return ancestor;
        if (ancestor == root)
            return 0;
    }

    return 0;
}

template <typename Strategy>
static Node* enclosingNodeOfTypeAlgorithm(const PositionTemplate<Strategy>& p, bool (*nodeIsOfType)(const Node*), EditingBoundaryCrossingRule rule)
{
    // TODO(yosin) support CanSkipCrossEditingBoundary
    DCHECK(rule == CanCrossEditingBoundary || rule == CannotCrossEditingBoundary) << rule;
    if (p.isNull())
        return nullptr;

    ContainerNode* const root = rule == CannotCrossEditingBoundary ? highestEditableRoot(p) : nullptr;
    for (Node* n = p.anchorNode(); n; n = Strategy::parent(*n)) {
        // Don't return a non-editable node if the input position was editable, since
        // the callers from editing will no doubt want to perform editing inside the returned node.
        if (root && !n->hasEditableStyle())
            continue;
        if (nodeIsOfType(n))
            return n;
        if (n == root)
            return nullptr;
    }

    return nullptr;
}

Node* enclosingNodeOfType(const Position& p, bool (*nodeIsOfType)(const Node*), EditingBoundaryCrossingRule rule)
{
    return enclosingNodeOfTypeAlgorithm<EditingStrategy>(p, nodeIsOfType, rule);
}

Node* enclosingNodeOfType(const PositionInFlatTree& p, bool (*nodeIsOfType)(const Node*), EditingBoundaryCrossingRule rule)
{
    return enclosingNodeOfTypeAlgorithm<EditingInFlatTreeStrategy>(p, nodeIsOfType, rule);
}

Node* highestEnclosingNodeOfType(const Position& p, bool (*nodeIsOfType)(const Node*), EditingBoundaryCrossingRule rule, Node* stayWithin)
{
    Node* highest = nullptr;
    ContainerNode* root = rule == CannotCrossEditingBoundary ? highestEditableRoot(p) : nullptr;
    for (Node* n = p.computeContainerNode(); n && n != stayWithin; n = n->parentNode()) {
        if (root && !n->hasEditableStyle())
            continue;
        if (nodeIsOfType(n))
            highest = n;
        if (n == root)
            break;
    }

    return highest;
}

static bool hasARenderedDescendant(Node* node, Node* excludedNode)
{
    for (Node* n = node->firstChild(); n;) {
        if (n == excludedNode) {
            n = NodeTraversal::nextSkippingChildren(*n, node);
            continue;
        }
        if (n->layoutObject())
            return true;
        n = NodeTraversal::next(*n, node);
    }
    return false;
}

Node* highestNodeToRemoveInPruning(Node* node, Node* excludeNode)
{
    Node* previousNode = nullptr;
    Element* rootEditableElement = node ? node->rootEditableElement() : nullptr;
    for (; node; node = node->parentNode()) {
        if (LayoutObject* layoutObject = node->layoutObject()) {
            if (!layoutObject->canHaveChildren() || hasARenderedDescendant(node, previousNode) || rootEditableElement == node || excludeNode == node)
                return previousNode;
        }
        previousNode = node;
    }
    return 0;
}

Element* enclosingTableCell(const Position& p)
{
    return toElement(enclosingNodeOfType(p, isTableCell));
}

Element* enclosingAnchorElement(const Position& p)
{
    if (p.isNull())
        return 0;

    for (Element* ancestor = ElementTraversal::firstAncestorOrSelf(*p.anchorNode()); ancestor; ancestor = ElementTraversal::firstAncestor(*ancestor)) {
        if (ancestor->isLink())
            return ancestor;
    }
    return 0;
}

HTMLElement* enclosingList(Node* node)
{
    if (!node)
        return 0;

    ContainerNode* root = highestEditableRoot(firstPositionInOrBeforeNode(node));

    for (Node& runner : NodeTraversal::ancestorsOf(*node)) {
        if (isHTMLUListElement(runner) || isHTMLOListElement(runner))
            return toHTMLElement(&runner);
        if (runner == root)
            return 0;
    }

    return 0;
}

Node* enclosingListChild(Node *node)
{
    if (!node)
        return 0;
    // Check for a list item element, or for a node whose parent is a list element. Such a node
    // will appear visually as a list item (but without a list marker)
    ContainerNode* root = highestEditableRoot(firstPositionInOrBeforeNode(node));

    // FIXME: This function is inappropriately named if it starts with node instead of node->parentNode()
    for (Node* n = node; n && n->parentNode(); n = n->parentNode()) {
        if (isHTMLLIElement(*n) || (isHTMLListElement(n->parentNode()) && n != root))
            return n;
        if (n == root || isTableCell(n))
            return 0;
    }

    return 0;
}

// FIXME: This method should not need to call isStartOfParagraph/isEndOfParagraph
Node* enclosingEmptyListItem(const VisiblePosition& visiblePos)
{
    // Check that position is on a line by itself inside a list item
    Node* listChildNode = enclosingListChild(visiblePos.deepEquivalent().anchorNode());
    if (!listChildNode || !isStartOfParagraph(visiblePos) || !isEndOfParagraph(visiblePos))
        return 0;

    VisiblePosition firstInListChild = createVisiblePosition(firstPositionInOrBeforeNode(listChildNode));
    VisiblePosition lastInListChild = createVisiblePosition(lastPositionInOrAfterNode(listChildNode));

    if (firstInListChild.deepEquivalent() != visiblePos.deepEquivalent() || lastInListChild.deepEquivalent() != visiblePos.deepEquivalent())
        return 0;

    return listChildNode;
}

HTMLElement* outermostEnclosingList(Node* node, HTMLElement* rootList)
{
    HTMLElement* list = enclosingList(node);
    if (!list)
        return 0;

    while (HTMLElement* nextList = enclosingList(list)) {
        if (nextList == rootList)
            break;
        list = nextList;
    }

    return list;
}

// Determines whether two positions are visibly next to each other (first then second)
// while ignoring whitespaces and unrendered nodes
static bool isVisiblyAdjacent(const Position& first, const Position& second)
{
    return createVisiblePosition(first).deepEquivalent() == createVisiblePosition(mostBackwardCaretPosition(second)).deepEquivalent();
}

bool canMergeLists(Element* firstList, Element* secondList)
{
    if (!firstList || !secondList || !firstList->isHTMLElement() || !secondList->isHTMLElement())
        return false;

    return firstList->hasTagName(secondList->tagQName()) // make sure the list types match (ol vs. ul)
    && firstList->hasEditableStyle() && secondList->hasEditableStyle() // both lists are editable
    && firstList->rootEditableElement() == secondList->rootEditableElement() // don't cross editing boundaries
    && isVisiblyAdjacent(Position::inParentAfterNode(*firstList), Position::inParentBeforeNode(*secondList));
    // Make sure there is no visible content between this li and the previous list
}

bool isDisplayInsideTable(const Node* node)
{
    if (!node || !node->isElementNode())
        return false;

    LayoutObject* layoutObject = node->layoutObject();
    return (layoutObject && layoutObject->isTable());
}

bool isTableCell(const Node* node)
{
    DCHECK(node);
    LayoutObject* r = node->layoutObject();
    return r ? r->isTableCell() : isHTMLTableCellElement(*node);
}

bool isEmptyTableCell(const Node* node)
{
    // Returns true IFF the passed in node is one of:
    //   .) a table cell with no children,
    //   .) a table cell with a single BR child, and which has no other child layoutObject, including :before and :after layoutObject
    //   .) the BR child of such a table cell

    // Find rendered node
    while (node && !node->layoutObject())
        node = node->parentNode();
    if (!node)
        return false;

    // Make sure the rendered node is a table cell or <br>.
    // If it's a <br>, then the parent node has to be a table cell.
    LayoutObject* layoutObject = node->layoutObject();
    if (layoutObject->isBR()) {
        layoutObject = layoutObject->parent();
        if (!layoutObject)
            return false;
    }
    if (!layoutObject->isTableCell())
        return false;

    // Check that the table cell contains no child layoutObjects except for perhaps a single <br>.
    LayoutObject* childLayoutObject = toLayoutTableCell(layoutObject)->firstChild();
    if (!childLayoutObject)
        return true;
    if (!childLayoutObject->isBR())
        return false;
    return !childLayoutObject->nextSibling();
}

HTMLElement* createDefaultParagraphElement(Document& document)
{
    switch (document.frame()->editor().defaultParagraphSeparator()) {
    case EditorParagraphSeparatorIsDiv:
        return HTMLDivElement::create(document);
    case EditorParagraphSeparatorIsP:
        return HTMLParagraphElement::create(document);
    }

    NOTREACHED();
    return nullptr;
}

HTMLElement* createHTMLElement(Document& document, const QualifiedName& name)
{
    return HTMLElementFactory::createHTMLElement(name.localName(), document, 0, CreatedByCloneNode);
}

bool isTabHTMLSpanElement(const Node* node)
{
    if (!isHTMLSpanElement(node) || toHTMLSpanElement(node)->getAttribute(classAttr) != AppleTabSpanClass)
        return false;
    UseCounter::count(node->document(), UseCounter::EditingAppleTabSpanClass);
    return true;
}

bool isTabHTMLSpanElementTextNode(const Node* node)
{
    return node && node->isTextNode() && node->parentNode() && isTabHTMLSpanElement(node->parentNode());
}

HTMLSpanElement* tabSpanElement(const Node* node)
{
    return isTabHTMLSpanElementTextNode(node) ? toHTMLSpanElement(node->parentNode()) : 0;
}

static HTMLSpanElement* createTabSpanElement(Document& document, Text* tabTextNode)
{
    // Make the span to hold the tab.
    HTMLSpanElement* spanElement = HTMLSpanElement::create(document);
    spanElement->setAttribute(classAttr, AppleTabSpanClass);
    spanElement->setAttribute(styleAttr, "white-space:pre");

    // Add tab text to that span.
    if (!tabTextNode)
        tabTextNode = document.createEditingTextNode("\t");

    spanElement->appendChild(tabTextNode);

    return spanElement;
}

HTMLSpanElement* createTabSpanElement(Document& document, const String& tabText)
{
    return createTabSpanElement(document, document.createTextNode(tabText));
}

HTMLSpanElement* createTabSpanElement(Document& document)
{
    return createTabSpanElement(document, nullptr);
}

bool isNodeRendered(const Node& node)
{
    LayoutObject* layoutObject = node.layoutObject();
    if (!layoutObject)
        return false;

    return layoutObject->style()->visibility() == VISIBLE;
}

// return first preceding DOM position rendered at a different location, or "this"
static Position previousCharacterPosition(const Position& position, TextAffinity affinity)
{
    if (position.isNull())
        return Position();

    Element* fromRootEditableElement = position.anchorNode()->rootEditableElement();

    bool atStartOfLine = isStartOfLine(createVisiblePosition(position, affinity));
    bool rendered = isVisuallyEquivalentCandidate(position);

    Position currentPos = position;
    while (!currentPos.atStartOfTree()) {
        // TODO(yosin) When we use |previousCharacterPosition()| other than
        // finding leading whitespace, we should use |Character| instead of
        // |CodePoint|.
        currentPos = previousPositionOf(currentPos, PositionMoveType::CodeUnit);

        if (currentPos.anchorNode()->rootEditableElement() != fromRootEditableElement)
            return position;

        if (atStartOfLine || !rendered) {
            if (isVisuallyEquivalentCandidate(currentPos))
                return currentPos;
        } else if (rendersInDifferentPosition(position, currentPos)) {
            return currentPos;
        }
    }

    return position;
}

// This assumes that it starts in editable content.
Position leadingWhitespacePosition(const Position& position, TextAffinity affinity, WhitespacePositionOption option)
{
    DCHECK(isEditablePosition(position, ContentIsEditable)) << position;
    if (position.isNull())
        return Position();

    if (isHTMLBRElement(*mostBackwardCaretPosition(position).anchorNode()))
        return Position();

    const Position& prev = previousCharacterPosition(position, affinity);
    if (prev == position)
        return Position();
    const Node* const anchorNode = prev.anchorNode();
    if (!anchorNode || !anchorNode->isTextNode())
        return Position();
    if (enclosingBlockFlowElement(*anchorNode) != enclosingBlockFlowElement(*position.anchorNode()))
        return Position();
    if (option == NotConsiderNonCollapsibleWhitespace && anchorNode->layoutObject() && !anchorNode->layoutObject()->style()->collapseWhiteSpace())
        return Position();
    const String& string = toText(anchorNode)->data();
    const UChar previousCharacter = string[prev.computeOffsetInContainerNode()];
    const bool isSpace = option == ConsiderNonCollapsibleWhitespace ? (isSpaceOrNewline(previousCharacter) || previousCharacter == noBreakSpaceCharacter) : isCollapsibleWhitespace(previousCharacter);
    if (!isSpace || !isEditablePosition(prev))
        return Position();
    return prev;
}

// This assumes that it starts in editable content.
Position trailingWhitespacePosition(const Position& position, TextAffinity, WhitespacePositionOption option)
{
    DCHECK(isEditablePosition(position, ContentIsEditable)) << position;
    if (position.isNull())
        return Position();

    VisiblePosition visiblePosition = createVisiblePosition(position);
    UChar characterAfterVisiblePosition = characterAfter(visiblePosition);
    bool isSpace = option == ConsiderNonCollapsibleWhitespace ? (isSpaceOrNewline(characterAfterVisiblePosition) || characterAfterVisiblePosition == noBreakSpaceCharacter) : isCollapsibleWhitespace(characterAfterVisiblePosition);
    // The space must not be in another paragraph and it must be editable.
    if (isSpace && !isEndOfParagraph(visiblePosition) && nextPositionOf(visiblePosition, CannotCrossEditingBoundary).isNotNull())
        return position;
    return Position();
}

unsigned numEnclosingMailBlockquotes(const Position& p)
{
    unsigned num = 0;
    for (Node* n = p.anchorNode(); n; n = n->parentNode()) {
        if (isMailHTMLBlockquoteElement(n))
            num++;
    }
    return num;
}

PositionWithAffinity positionRespectingEditingBoundary(const Position& position, const LayoutPoint& localPoint, Node* targetNode)
{
    if (!targetNode->layoutObject())
        return PositionWithAffinity();

    LayoutPoint selectionEndPoint = localPoint;
    Element* editableElement = rootEditableElementOf(position);

    if (editableElement && !editableElement->contains(targetNode)) {
        if (!editableElement->layoutObject())
            return PositionWithAffinity();

        FloatPoint absolutePoint = targetNode->layoutObject()->localToAbsolute(FloatPoint(selectionEndPoint));
        selectionEndPoint = roundedLayoutPoint(editableElement->layoutObject()->absoluteToLocal(absolutePoint));
        targetNode = editableElement;
    }

    return targetNode->layoutObject()->positionForPoint(selectionEndPoint);
}

void updatePositionForNodeRemoval(Position& position, Node& node)
{
    if (position.isNull())
        return;
    switch (position.anchorType()) {
    case PositionAnchorType::BeforeChildren:
        if (node.isShadowIncludingInclusiveAncestorOf(position.computeContainerNode()))
            position = Position::inParentBeforeNode(node);
        break;
    case PositionAnchorType::AfterChildren:
        if (node.isShadowIncludingInclusiveAncestorOf(position.computeContainerNode()))
            position = Position::inParentAfterNode(node);
        break;
    case PositionAnchorType::OffsetInAnchor:
        if (position.computeContainerNode() == node.parentNode() && static_cast<unsigned>(position.offsetInContainerNode()) > node.nodeIndex())
            position = Position(position.computeContainerNode(), position.offsetInContainerNode() - 1);
        else if (node.isShadowIncludingInclusiveAncestorOf(position.computeContainerNode()))
            position = Position::inParentBeforeNode(node);
        break;
    case PositionAnchorType::AfterAnchor:
        if (node.isShadowIncludingInclusiveAncestorOf(position.anchorNode()))
            position = Position::inParentAfterNode(node);
        break;
    case PositionAnchorType::BeforeAnchor:
        if (node.isShadowIncludingInclusiveAncestorOf(position.anchorNode()))
            position = Position::inParentBeforeNode(node);
        break;
    }
}

bool isMailHTMLBlockquoteElement(const Node* node)
{
    if (!node || !node->isHTMLElement())
        return false;

    const HTMLElement& element = toHTMLElement(*node);
    return element.hasTagName(blockquoteTag) && element.getAttribute("type") == "cite";
}

bool lineBreakExistsAtVisiblePosition(const VisiblePosition& visiblePosition)
{
    return lineBreakExistsAtPosition(mostForwardCaretPosition(visiblePosition.deepEquivalent()));
}

bool lineBreakExistsAtPosition(const Position& position)
{
    if (position.isNull())
        return false;

    if (isHTMLBRElement(*position.anchorNode()) && position.atFirstEditingPositionForNode())
        return true;

    if (!position.anchorNode()->layoutObject())
        return false;

    if (!position.anchorNode()->isTextNode() || !position.anchorNode()->layoutObject()->style()->preserveNewline())
        return false;

    Text* textNode = toText(position.anchorNode());
    unsigned offset = position.offsetInContainerNode();
    return offset < textNode->length() && textNode->data()[offset] == '\n';
}

// Modifies selections that have an end point at the edge of a table
// that contains the other endpoint so that they don't confuse
// code that iterates over selected paragraphs.
VisibleSelection selectionForParagraphIteration(const VisibleSelection& original)
{
    VisibleSelection newSelection(original);
    VisiblePosition startOfSelection(newSelection.visibleStart());
    VisiblePosition endOfSelection(newSelection.visibleEnd());

    // If the end of the selection to modify is just after a table, and
    // if the start of the selection is inside that table, then the last paragraph
    // that we'll want modify is the last one inside the table, not the table itself
    // (a table is itself a paragraph).
    if (Element* table = tableElementJustBefore(endOfSelection)) {
        if (startOfSelection.deepEquivalent().anchorNode()->isDescendantOf(table))
            newSelection = VisibleSelection(startOfSelection, previousPositionOf(endOfSelection, CannotCrossEditingBoundary));
    }

    // If the start of the selection to modify is just before a table,
    // and if the end of the selection is inside that table, then the first paragraph
    // we'll want to modify is the first one inside the table, not the paragraph
    // containing the table itself.
    if (Element* table = tableElementJustAfter(startOfSelection)) {
        if (endOfSelection.deepEquivalent().anchorNode()->isDescendantOf(table))
            newSelection = VisibleSelection(nextPositionOf(startOfSelection, CannotCrossEditingBoundary), endOfSelection);
    }

    return newSelection;
}

// FIXME: indexForVisiblePosition and visiblePositionForIndex use TextIterators to convert between
// VisiblePositions and indices. But TextIterator iteration using TextIteratorEmitsCharactersBetweenAllVisiblePositions
// does not exactly match VisiblePosition iteration, so using them to preserve a selection during an editing
// opertion is unreliable. TextIterator's TextIteratorEmitsCharactersBetweenAllVisiblePositions mode needs to be fixed,
// or these functions need to be changed to iterate using actual VisiblePositions.
// FIXME: Deploy these functions everywhere that TextIterators are used to convert between VisiblePositions and indices.
int indexForVisiblePosition(const VisiblePosition& visiblePosition, ContainerNode*& scope)
{
    if (visiblePosition.isNull())
        return 0;

    Position p(visiblePosition.deepEquivalent());
    Document& document = *p.document();
    ShadowRoot* shadowRoot = p.anchorNode()->containingShadowRoot();

    if (shadowRoot)
        scope = shadowRoot;
    else
        scope = document.documentElement();

    Range* range = Range::create(document, Position::firstPositionInNode(scope), p.parentAnchoredEquivalent());

    return TextIterator::rangeLength(range->startPosition(), range->endPosition(), true);
}

EphemeralRange makeRange(const VisiblePosition &start, const VisiblePosition &end)
{
    if (start.isNull() || end.isNull())
        return EphemeralRange();

    Position s = start.deepEquivalent().parentAnchoredEquivalent();
    Position e = end.deepEquivalent().parentAnchoredEquivalent();
    if (s.isNull() || e.isNull())
        return EphemeralRange();

    return EphemeralRange(s, e);
}

template <typename Strategy>
static EphemeralRangeTemplate<Strategy> normalizeRangeAlgorithm(const EphemeralRangeTemplate<Strategy>& range)
{
    DCHECK(range.isNotNull());
    range.document().updateStyleAndLayoutIgnorePendingStylesheets();

    // TODO(yosin) We should not call |parentAnchoredEquivalent()|, it is
    // redundant.
    const PositionTemplate<Strategy> normalizedStart = mostForwardCaretPosition(range.startPosition()).parentAnchoredEquivalent();
    const PositionTemplate<Strategy> normalizedEnd = mostBackwardCaretPosition(range.endPosition()).parentAnchoredEquivalent();
    // The order of the positions of |start| and |end| can be swapped after
    // upstream/downstream. e.g. editing/pasteboard/copy-display-none.html
    if (normalizedStart.compareTo(normalizedEnd) > 0)
        return EphemeralRangeTemplate<Strategy>(normalizedEnd, normalizedStart);
    return EphemeralRangeTemplate<Strategy>(normalizedStart, normalizedEnd);
}

EphemeralRange normalizeRange(const EphemeralRange& range)
{
    return normalizeRangeAlgorithm<EditingStrategy>(range);
}

EphemeralRangeInFlatTree normalizeRange(const EphemeralRangeInFlatTree& range)
{
    return normalizeRangeAlgorithm<EditingInFlatTreeStrategy>(range);
}

VisiblePosition visiblePositionForIndex(int index, ContainerNode* scope)
{
    if (!scope)
        return VisiblePosition();
    EphemeralRange range = PlainTextRange(index).createRangeForSelection(*scope);
    // Check for an invalid index. Certain editing operations invalidate indices
    // because of problems with
    // TextIteratorEmitsCharactersBetweenAllVisiblePositions.
    if (range.isNull())
        return VisiblePosition();
    return createVisiblePosition(range.startPosition());
}

// Determines whether a node is inside a range or visibly starts and ends at the boundaries of the range.
// Call this function to determine whether a node is visibly fit inside selectedRange
bool isNodeVisiblyContainedWithin(Node& node, const Range& selectedRange)
{
    if (selectedRange.isNodeFullyContained(node))
        return true;

    bool startIsVisuallySame = visiblePositionBeforeNode(node).deepEquivalent() == createVisiblePosition(selectedRange.startPosition()).deepEquivalent();
    if (startIsVisuallySame && comparePositions(Position::inParentAfterNode(node), selectedRange.endPosition()) < 0)
        return true;

    bool endIsVisuallySame = visiblePositionAfterNode(node).deepEquivalent() == createVisiblePosition(selectedRange.endPosition()).deepEquivalent();
    if (endIsVisuallySame && comparePositions(selectedRange.startPosition(), Position::inParentBeforeNode(node)) < 0)
        return true;

    return startIsVisuallySame && endIsVisuallySame;
}

bool isRenderedAsNonInlineTableImageOrHR(const Node* node)
{
    if (!node)
        return false;
    LayoutObject* layoutObject = node->layoutObject();
    return layoutObject && ((layoutObject->isTable() && !layoutObject->isInline()) || (layoutObject->isImage() && !layoutObject->isInline()) || layoutObject->isHR());
}

bool areIdenticalElements(const Node& first, const Node& second)
{
    if (!first.isElementNode() || !second.isElementNode())
        return false;

    const Element& firstElement = toElement(first);
    const Element& secondElement = toElement(second);
    if (!firstElement.hasTagName(secondElement.tagQName()))
        return false;

    if (!firstElement.hasEquivalentAttributes(&secondElement))
        return false;

    return firstElement.hasEditableStyle() && secondElement.hasEditableStyle();
}

bool isNonTableCellHTMLBlockElement(const Node* node)
{
    if (!node->isHTMLElement())
        return false;

    const HTMLElement& element = toHTMLElement(*node);
    return element.hasTagName(listingTag)
        || element.hasTagName(olTag)
        || element.hasTagName(preTag)
        || element.hasTagName(tableTag)
        || element.hasTagName(ulTag)
        || element.hasTagName(xmpTag)
        || element.hasTagName(h1Tag)
        || element.hasTagName(h2Tag)
        || element.hasTagName(h3Tag)
        || element.hasTagName(h4Tag)
        || element.hasTagName(h5Tag);
}

bool isBlockFlowElement(const Node& node)
{
    LayoutObject* layoutObject = node.layoutObject();
    return node.isElementNode() && layoutObject && layoutObject->isLayoutBlockFlow();
}

Position adjustedSelectionStartForStyleComputation(const VisibleSelection& selection)
{
    // This function is used by range style computations to avoid bugs like:
    // <rdar://problem/4017641> REGRESSION (Mail): you can only bold/unbold a selection starting from end of line once
    // It is important to skip certain irrelevant content at the start of the selection, so we do not wind up
    // with a spurious "mixed" style.

    VisiblePosition visiblePosition = createVisiblePosition(selection.start());
    if (visiblePosition.isNull())
        return Position();

    // if the selection is a caret, just return the position, since the style
    // behind us is relevant
    if (selection.isCaret())
        return visiblePosition.deepEquivalent();

    // if the selection starts just before a paragraph break, skip over it
    if (isEndOfParagraph(visiblePosition))
        return mostForwardCaretPosition(nextPositionOf(visiblePosition).deepEquivalent());

    // otherwise, make sure to be at the start of the first selected node,
    // instead of possibly at the end of the last node before the selection
    return mostForwardCaretPosition(visiblePosition.deepEquivalent());
}

bool isTextSecurityNode(const Node* node)
{
    return node && node->layoutObject() && node->layoutObject()->style()->textSecurity() != TSNONE;
}

DispatchEventResult dispatchBeforeInputInsertText(EventTarget* target, const String& data)
{
    if (!RuntimeEnabledFeatures::inputEventEnabled())
        return DispatchEventResult::NotCanceled;
    if (!target)
        return DispatchEventResult::NotCanceled;
    // TODO(chongz): Pass appreciate |ranges| after it's defined on spec.
    // http://w3c.github.io/editing/input-events.html#dom-inputevent-inputtype
    InputEvent* beforeInputEvent = InputEvent::createBeforeInput(InputEvent::InputType::InsertText, data, InputEvent::EventCancelable::IsCancelable, InputEvent::EventIsComposing::NotComposing, nullptr);
    return target->dispatchEvent(beforeInputEvent);
}

DispatchEventResult dispatchBeforeInputFromComposition(EventTarget* target, InputEvent::InputType inputType, const String& data, InputEvent::EventCancelable cancelable)
{
    if (!RuntimeEnabledFeatures::inputEventEnabled())
        return DispatchEventResult::NotCanceled;
    if (!target)
        return DispatchEventResult::NotCanceled;
    // TODO(chongz): Pass appreciate |ranges| after it's defined on spec.
    // http://w3c.github.io/editing/input-events.html#dom-inputevent-inputtype
    InputEvent* beforeInputEvent = InputEvent::createBeforeInput(inputType, data, cancelable, InputEvent::EventIsComposing::IsComposing, nullptr);
    return target->dispatchEvent(beforeInputEvent);
}

DispatchEventResult dispatchBeforeInputEditorCommand(EventTarget* target, InputEvent::InputType inputType, const String& data, const RangeVector* ranges)
{
    if (!RuntimeEnabledFeatures::inputEventEnabled())
        return DispatchEventResult::NotCanceled;
    if (!target)
        return DispatchEventResult::NotCanceled;
    InputEvent* beforeInputEvent = InputEvent::createBeforeInput(inputType, data, InputEvent::EventCancelable::IsCancelable, InputEvent::EventIsComposing::NotComposing, ranges);
    return target->dispatchEvent(beforeInputEvent);
}

} // namespace blink
