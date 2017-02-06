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

#include "core/editing/SelectionAdjuster.h"

#include "core/editing/EditingUtilities.h"

namespace blink {

namespace {

Node* enclosingShadowHost(Node* node)
{
    for (Node* runner = node; runner; runner = FlatTreeTraversal::parent(*runner)) {
        if (isShadowHost(runner))
            return runner;
    }
    return nullptr;
}

bool isEnclosedBy(const PositionInFlatTree& position, const Node& node)
{
    DCHECK(position.isNotNull());
    Node* anchorNode = position.anchorNode();
    if (anchorNode == node)
        return !position.isAfterAnchor() && !position.isBeforeAnchor();

    return FlatTreeTraversal::isDescendantOf(*anchorNode, node);
}

bool isSelectionBoundary(const Node& node)
{
    return isHTMLTextAreaElement(node) || isHTMLInputElement(node) || isHTMLSelectElement(node);
}

Node* enclosingShadowHostForStart(const PositionInFlatTree& position)
{
    Node* node = position.nodeAsRangeFirstNode();
    if (!node)
        return nullptr;
    Node* shadowHost = enclosingShadowHost(node);
    if (!shadowHost)
        return nullptr;
    if (!isEnclosedBy(position, *shadowHost))
        return nullptr;
    return isSelectionBoundary(*shadowHost) ? shadowHost : nullptr;
}

Node* enclosingShadowHostForEnd(const PositionInFlatTree& position)
{
    Node* node = position.nodeAsRangeLastNode();
    if (!node)
        return nullptr;
    Node* shadowHost = enclosingShadowHost(node);
    if (!shadowHost)
        return nullptr;
    if (!isEnclosedBy(position, *shadowHost))
        return nullptr;
    return isSelectionBoundary(*shadowHost) ? shadowHost : nullptr;
}

PositionInFlatTree adjustPositionInFlatTreeForStart(const PositionInFlatTree& position, Node* shadowHost)
{
    if (isEnclosedBy(position, *shadowHost)) {
        if (position.isBeforeChildren())
            return PositionInFlatTree::beforeNode(shadowHost);
        return PositionInFlatTree::afterNode(shadowHost);
    }

    // We use |firstChild|'s after instead of beforeAllChildren for backward
    // compatibility. The positions are same but the anchors would be different,
    // and selection painting uses anchor nodes.
    if (Node* firstChild = FlatTreeTraversal::firstChild(*shadowHost))
        return PositionInFlatTree::beforeNode(firstChild);
    return PositionInFlatTree();
}

Position adjustPositionForEnd(const Position& currentPosition, Node* startContainerNode)
{
    TreeScope& treeScope = startContainerNode->treeScope();

    DCHECK(currentPosition.computeContainerNode()->treeScope() != treeScope);

    if (Node* ancestor = treeScope.ancestorInThisScope(currentPosition.computeContainerNode())) {
        if (ancestor->contains(startContainerNode))
            return Position::afterNode(ancestor);
        return Position::beforeNode(ancestor);
    }

    if (Node* lastChild = treeScope.rootNode().lastChild())
        return Position::afterNode(lastChild);

    return Position();
}

PositionInFlatTree adjustPositionInFlatTreeForEnd(const PositionInFlatTree& position, Node* shadowHost)
{
    if (isEnclosedBy(position, *shadowHost)) {
        if (position.isAfterChildren())
            return PositionInFlatTree::afterNode(shadowHost);
        return PositionInFlatTree::beforeNode(shadowHost);
    }

    // We use |lastChild|'s after instead of afterAllChildren for backward
    // compatibility. The positions are same but the anchors would be different,
    // and selection painting uses anchor nodes.
    if (Node* lastChild = FlatTreeTraversal::lastChild(*shadowHost))
        return PositionInFlatTree::afterNode(lastChild);
    return PositionInFlatTree();
}

Position adjustPositionForStart(const Position& currentPosition, Node* endContainerNode)
{
    TreeScope& treeScope = endContainerNode->treeScope();

    DCHECK(currentPosition.computeContainerNode()->treeScope() != treeScope);

    if (Node* ancestor = treeScope.ancestorInThisScope(currentPosition.computeContainerNode())) {
        if (ancestor->contains(endContainerNode))
            return Position::beforeNode(ancestor);
        return Position::afterNode(ancestor);
    }

    if (Node* firstChild = treeScope.rootNode().firstChild())
        return Position::beforeNode(firstChild);

    return Position();
}

} // namespace

// Updates |selectionInFlatTree| to match with |selection|.
void SelectionAdjuster::adjustSelectionInFlatTree(VisibleSelectionInFlatTree* selectionInFlatTree, const VisibleSelection& selection)
{
    if (selection.isNone()) {
        *selectionInFlatTree = VisibleSelectionInFlatTree();
        return;
    }

    const PositionInFlatTree& base = toPositionInFlatTree(selection.base());
    const PositionInFlatTree& extent = toPositionInFlatTree(selection.extent());
    const PositionInFlatTree& position1 = toPositionInFlatTree(selection.start());
    const PositionInFlatTree& position2 = toPositionInFlatTree(selection.end());
    position1.anchorNode()->updateDistribution();
    position2.anchorNode()->updateDistribution();
    selectionInFlatTree->m_base = base;
    selectionInFlatTree->m_extent = extent;
    selectionInFlatTree->m_affinity = selection.m_affinity;
    selectionInFlatTree->m_isDirectional = selection.m_isDirectional;
    selectionInFlatTree->m_granularity = selection.m_granularity;
    selectionInFlatTree->m_hasTrailingWhitespace = selection.m_hasTrailingWhitespace;
    selectionInFlatTree->m_baseIsFirst = base.isNull() || base.compareTo(extent) <= 0;
    if (position1.compareTo(position2) <= 0) {
        selectionInFlatTree->m_start = position1;
        selectionInFlatTree->m_end = position2;
    } else {
        selectionInFlatTree->m_start = position2;
        selectionInFlatTree->m_end = position1;
    }
    selectionInFlatTree->updateSelectionType();
}

static bool isCrossingShadowBoundaries(const VisibleSelectionInFlatTree& selection)
{
    if (!selection.isRange())
        return false;
    TreeScope& treeScope = selection.base().anchorNode()->treeScope();
    return selection.extent().anchorNode()->treeScope() != treeScope
        || selection.start().anchorNode()->treeScope() != treeScope
        || selection.end().anchorNode()->treeScope() != treeScope;
}

void SelectionAdjuster::adjustSelectionInDOMTree(VisibleSelection* selection, const VisibleSelectionInFlatTree& selectionInFlatTree)
{
    if (selectionInFlatTree.isNone()) {
        *selection = VisibleSelection();
        return;
    }

    const Position& base = toPositionInDOMTree(selectionInFlatTree.base());
    const Position& extent = toPositionInDOMTree(selectionInFlatTree.extent());

    if (isCrossingShadowBoundaries(selectionInFlatTree)) {
        *selection = VisibleSelection(base, extent);
        return;
    }

    const Position& position1 = toPositionInDOMTree(selectionInFlatTree.start());
    const Position& position2 = toPositionInDOMTree(selectionInFlatTree.end());
    selection->m_base = base;
    selection->m_extent = extent;
    selection->m_affinity = selectionInFlatTree.m_affinity;
    selection->m_isDirectional = selectionInFlatTree.m_isDirectional;
    selection->m_granularity = selectionInFlatTree.m_granularity;
    selection->m_hasTrailingWhitespace = selectionInFlatTree.m_hasTrailingWhitespace;
    selection->m_baseIsFirst = base.isNull() || base.compareTo(extent) <= 0;
    if (position1.compareTo(position2) <= 0) {
        selection->m_start = position1;
        selection->m_end = position2;
    } else {
        selection->m_start = position2;
        selection->m_end = position1;
    }
    selection->updateSelectionType();
}

void SelectionAdjuster::adjustSelectionToAvoidCrossingShadowBoundaries(VisibleSelection* selection)
{
    // Note: |m_selectionType| isn't computed yet.
    DCHECK(selection->base().isNotNull());
    DCHECK(selection->extent().isNotNull());
    DCHECK(selection->start().isNotNull());
    DCHECK(selection->end().isNotNull());

    // TODO(hajimehoshi): Checking treeScope is wrong when a node is
    // distributed, but we leave it as it is for backward compatibility.
    if (selection->start().anchorNode()->treeScope() == selection->end().anchorNode()->treeScope())
        return;

    if (selection->isBaseFirst()) {
        const Position& newEnd = adjustPositionForEnd(selection->end(), selection->start().computeContainerNode());
        selection->m_extent = newEnd;
        selection->m_end = newEnd;
        return;
    }

    const Position& newStart = adjustPositionForStart(selection->start(), selection->end().computeContainerNode());
    selection->m_extent = newStart;
    selection->m_start = newStart;
}

// This function is called twice. The first is called when |m_start| and |m_end|
// or |m_extent| are same, and the second when |m_start| and |m_end| are changed
// after downstream/upstream.
void SelectionAdjuster::adjustSelectionToAvoidCrossingShadowBoundaries(VisibleSelectionInFlatTree* selection)
{
    Node* const shadowHostStart = enclosingShadowHostForStart(selection->start());
    Node* const shadowHostEnd = enclosingShadowHostForEnd(selection->end());
    if (shadowHostStart == shadowHostEnd)
        return;

    if (selection->isBaseFirst()) {
        Node* const shadowHost = shadowHostStart ? shadowHostStart : shadowHostEnd;
        const PositionInFlatTree& newEnd = adjustPositionInFlatTreeForEnd(selection->end(), shadowHost);
        selection->m_extent = newEnd;
        selection->m_end = newEnd;
        return;
    }
    Node* const shadowHost = shadowHostEnd ? shadowHostEnd : shadowHostStart;
    const PositionInFlatTree& newStart = adjustPositionInFlatTreeForStart(selection->start(), shadowHost);
    selection->m_extent = newStart;
    selection->m_start = newStart;
}

} // namespace blink
