/*
 * Copyright (C) 2012 Apple Computer, Inc.  All rights reserved.
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

#include "core/editing/commands/SimplifyMarkupCommand.h"

#include "core/dom/NodeComputedStyle.h"
#include "core/dom/NodeTraversal.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutObject.h"
#include "core/style/ComputedStyle.h"

namespace blink {

SimplifyMarkupCommand::SimplifyMarkupCommand(Document& document, Node* firstNode, Node* nodeAfterLast)
    : CompositeEditCommand(document), m_firstNode(firstNode), m_nodeAfterLast(nodeAfterLast)
{
}

void SimplifyMarkupCommand::doApply(EditingState* editingState)
{
    ContainerNode* rootNode = m_firstNode->parentNode();
    HeapVector<Member<ContainerNode>> nodesToRemove;

    // Walk through the inserted nodes, to see if there are elements that could be removed
    // without affecting the style. The goal is to produce leaner markup even when starting
    // from a verbose fragment.
    // We look at inline elements as well as non top level divs that don't have attributes.
    for (Node* node = m_firstNode.get(); node && node != m_nodeAfterLast; node = NodeTraversal::next(*node)) {
        if (node->hasChildren() || (node->isTextNode() && node->nextSibling()))
            continue;

        ContainerNode* const startingNode = node->parentNode();
        if (!startingNode)
            continue;
        const ComputedStyle* startingStyle = startingNode->computedStyle();
        if (!startingStyle)
            continue;
        ContainerNode* currentNode = startingNode;
        ContainerNode* topNodeWithStartingStyle = nullptr;
        while (currentNode != rootNode) {
            if (currentNode->parentNode() != rootNode && isRemovableBlock(currentNode))
                nodesToRemove.append(currentNode);

            currentNode = currentNode->parentNode();
            if (!currentNode)
                break;

            if (!currentNode->layoutObject() || !currentNode->layoutObject()->isLayoutInline() || toLayoutInline(currentNode->layoutObject())->alwaysCreateLineBoxes())
                continue;

            if (currentNode->firstChild() != currentNode->lastChild()) {
                topNodeWithStartingStyle = 0;
                break;
            }

            if (!currentNode->computedStyle()->visualInvalidationDiff(*startingStyle).hasDifference())
                topNodeWithStartingStyle = currentNode;

        }
        if (topNodeWithStartingStyle) {
            for (Node& node : NodeTraversal::inclusiveAncestorsOf(*startingNode)) {
                if (node == topNodeWithStartingStyle)
                    break;
                nodesToRemove.append(static_cast<ContainerNode*>(&node));
            }
        }
    }

    // we perform all the DOM mutations at once.
    for (size_t i = 0; i < nodesToRemove.size(); ++i) {
        // FIXME: We can do better by directly moving children from nodesToRemove[i].
        int numPrunedAncestors = pruneSubsequentAncestorsToRemove(nodesToRemove, i, editingState);
        if (editingState->isAborted())
            return;
        if (numPrunedAncestors < 0)
            continue;
        removeNodePreservingChildren(nodesToRemove[i], editingState, AssumeContentIsAlwaysEditable);
        if (editingState->isAborted())
            return;
        i += numPrunedAncestors;
    }
}

int SimplifyMarkupCommand::pruneSubsequentAncestorsToRemove(HeapVector<Member<ContainerNode>>& nodesToRemove, size_t startNodeIndex, EditingState* editingState)
{
    size_t pastLastNodeToRemove = startNodeIndex + 1;
    for (; pastLastNodeToRemove < nodesToRemove.size(); ++pastLastNodeToRemove) {
        if (nodesToRemove[pastLastNodeToRemove - 1]->parentNode() != nodesToRemove[pastLastNodeToRemove])
            break;
        DCHECK_EQ(nodesToRemove[pastLastNodeToRemove]->firstChild(), nodesToRemove[pastLastNodeToRemove]->lastChild());
    }

    ContainerNode* highestAncestorToRemove = nodesToRemove[pastLastNodeToRemove - 1].get();
    ContainerNode* parent = highestAncestorToRemove->parentNode();
    if (!parent) // Parent has already been removed.
        return -1;

    if (pastLastNodeToRemove == startNodeIndex + 1)
        return 0;

    removeNode(nodesToRemove[startNodeIndex], editingState, AssumeContentIsAlwaysEditable);
    if (editingState->isAborted())
        return -1;
    insertNodeBefore(nodesToRemove[startNodeIndex], highestAncestorToRemove, editingState, AssumeContentIsAlwaysEditable);
    if (editingState->isAborted())
        return -1;
    removeNode(highestAncestorToRemove, editingState, AssumeContentIsAlwaysEditable);
    if (editingState->isAborted())
        return -1;

    return pastLastNodeToRemove - startNodeIndex - 1;
}

DEFINE_TRACE(SimplifyMarkupCommand)
{
    visitor->trace(m_firstNode);
    visitor->trace(m_nodeAfterLast);
    CompositeEditCommand::trace(visitor);
}

} // namespace blink
