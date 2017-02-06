/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/dom/shadow/FlatTreeTraversal.h"

#include "core/dom/Element.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/html/HTMLShadowElement.h"
#include "core/html/HTMLSlotElement.h"

namespace blink {

static inline ElementShadow* shadowFor(const Node& node)
{
    return node.isElementNode() ? toElement(node).shadow() : nullptr;
}

static inline bool canBeDistributedToInsertionPoint(const Node& node)
{
    return node.isInV0ShadowTree() || node.isChildOfV0ShadowHost();
}

Node* FlatTreeTraversal::traverseChild(const Node& node, TraversalDirection direction)
{
    ElementShadow* shadow = shadowFor(node);
    if (shadow) {
        ShadowRoot& shadowRoot = shadow->youngestShadowRoot();
        return resolveDistributionStartingAt(direction == TraversalDirectionForward ? shadowRoot.firstChild() : shadowRoot.lastChild(), direction);
    }
    return resolveDistributionStartingAt(direction == TraversalDirectionForward ? node.firstChild() : node.lastChild(), direction);
}

Node* FlatTreeTraversal::resolveDistributionStartingAt(const Node* node, TraversalDirection direction)
{
    if (!node)
        return nullptr;
    for (const Node* sibling = node; sibling; sibling = (direction == TraversalDirectionForward ? sibling->nextSibling() : sibling->previousSibling())) {
        if (isHTMLSlotElement(*sibling)) {
            const HTMLSlotElement& slot = toHTMLSlotElement(*sibling);
            if (Node* found = (direction == TraversalDirectionForward ? slot.firstDistributedNode() : slot.lastDistributedNode()))
                return found;
            continue;
        }
        if (node->isInV0ShadowTree())
            return v0ResolveDistributionStartingAt(*sibling, direction);
        return const_cast<Node*>(sibling);
    }
    return nullptr;
}

Node* FlatTreeTraversal::v0ResolveDistributionStartingAt(const Node& node, TraversalDirection direction)
{
    DCHECK(!isHTMLSlotElement(node));
    for (const Node* sibling = &node; sibling; sibling = (direction == TraversalDirectionForward ? sibling->nextSibling() : sibling->previousSibling())) {
        if (!isActiveInsertionPoint(*sibling))
            return const_cast<Node*>(sibling);
        const InsertionPoint& insertionPoint = toInsertionPoint(*sibling);
        if (Node* found = (direction == TraversalDirectionForward ? insertionPoint.firstDistributedNode() : insertionPoint.lastDistributedNode()))
            return found;
        DCHECK(isHTMLShadowElement(insertionPoint) || (isHTMLContentElement(insertionPoint) && !insertionPoint.hasChildren()));
    }
    return nullptr;
}

static HTMLSlotElement* finalDestinationSlotFor(const Node& node)
{
    HTMLSlotElement* slot = node.assignedSlot();
    if (!slot)
        return nullptr;
    for (HTMLSlotElement* next = slot->assignedSlot(); next; next = next->assignedSlot()) {
        slot = next;
    }
    return slot;
}

// TODO(hayato): This may return a wrong result for a node which is not in a
// document flat tree.  See FlatTreeTraversalTest's redistribution test for details.
Node* FlatTreeTraversal::traverseSiblings(const Node& node, TraversalDirection direction)
{
    if (node.isChildOfV1ShadowHost())
        return traverseSiblingsForV1HostChild(node, direction);

    if (shadowWhereNodeCanBeDistributed(node))
        return traverseSiblingsForV0Distribution(node, direction);

    if (Node* found = resolveDistributionStartingAt(direction == TraversalDirectionForward ? node.nextSibling() : node.previousSibling(), direction))
        return found;

    if (!node.isInV0ShadowTree())
        return nullptr;

    // For v0 older shadow tree
    if (node.parentNode() && node.parentNode()->isShadowRoot()) {
        ShadowRoot* parentShadowRoot = toShadowRoot(node.parentNode());
        if (!parentShadowRoot->isYoungest()) {
            HTMLShadowElement* assignedInsertionPoint = parentShadowRoot->shadowInsertionPointOfYoungerShadowRoot();
            DCHECK(assignedInsertionPoint);
            return traverseSiblings(*assignedInsertionPoint, direction);
        }
    }
    return nullptr;
}

Node* FlatTreeTraversal::traverseSiblingsForV1HostChild(const Node& node, TraversalDirection direction)
{
    HTMLSlotElement* slot = finalDestinationSlotFor(node);
    if (!slot)
        return nullptr;
    if (Node* siblingInDistributedNodes = (direction == TraversalDirectionForward ? slot->distributedNodeNextTo(node) : slot->distributedNodePreviousTo(node)))
        return siblingInDistributedNodes;
    return traverseSiblings(*slot, direction);
}

Node* FlatTreeTraversal::traverseSiblingsForV0Distribution(const Node& node, TraversalDirection direction)
{
    const InsertionPoint* finalDestination = resolveReprojection(&node);
    if (!finalDestination)
        return nullptr;
    if (Node* found = (direction == TraversalDirectionForward ? finalDestination->distributedNodeNextTo(&node) : finalDestination->distributedNodePreviousTo(&node)))
        return found;
    return traverseSiblings(*finalDestination, direction);

}

ContainerNode* FlatTreeTraversal::traverseParent(const Node& node, ParentTraversalDetails* details)
{
    // TODO(hayato): Stop this hack for a pseudo element because a pseudo element is not a child of its parentOrShadowHostNode() in a flat tree.
    if (node.isPseudoElement())
        return node.parentOrShadowHostNode();

    if (node.isChildOfV1ShadowHost()) {
        HTMLSlotElement* slot = finalDestinationSlotFor(node);
        if (!slot)
            return nullptr;
        return traverseParent(*slot);
    }

    Element* parent = node.parentElement();
    if (parent && isHTMLSlotElement(parent)) {
        HTMLSlotElement& slot = toHTMLSlotElement(*parent);
        if (!slot.assignedNodes().isEmpty())
            return nullptr;
        return traverseParent(slot, details);
    }

    if (canBeDistributedToInsertionPoint(node))
        return traverseParentForV0(node, details);

    DCHECK(!shadowWhereNodeCanBeDistributed(node));
    return traverseParentOrHost(node);
}

ContainerNode* FlatTreeTraversal::traverseParentForV0(const Node& node, ParentTraversalDetails* details)
{
    if (shadowWhereNodeCanBeDistributed(node)) {
        if (const InsertionPoint* insertionPoint = resolveReprojection(&node)) {
            if (details)
                details->didTraverseInsertionPoint(insertionPoint);
            // The node is distributed. But the distribution was stopped at this insertion point.
            if (shadowWhereNodeCanBeDistributed(*insertionPoint))
                return nullptr;
            return traverseParent(*insertionPoint);
        }
        return nullptr;
    }
    ContainerNode* parent = traverseParentOrHost(node);
    if (isActiveInsertionPoint(*parent))
        return nullptr;
    return parent;
}

ContainerNode* FlatTreeTraversal::traverseParentOrHost(const Node& node)
{
    ContainerNode* parent = node.parentNode();
    if (!parent)
        return nullptr;
    if (!parent->isShadowRoot())
        return parent;
    ShadowRoot* shadowRoot = toShadowRoot(parent);
    DCHECK(!shadowRoot->shadowInsertionPointOfYoungerShadowRoot());
    if (!shadowRoot->isYoungest())
        return nullptr;
    return &shadowRoot->host();
}

Node* FlatTreeTraversal::childAt(const Node& node, unsigned index)
{
    assertPrecondition(node);
    Node* child = traverseFirstChild(node);
    while (child && index--)
        child = nextSibling(*child);
    assertPostcondition(child);
    return child;
}

Node* FlatTreeTraversal::nextSkippingChildren(const Node& node)
{
    if (Node* nextSibling = traverseNextSibling(node))
        return nextSibling;
    return traverseNextAncestorSibling(node);
}

bool FlatTreeTraversal::containsIncludingPseudoElement(const ContainerNode& container, const Node& node)
{
    assertPrecondition(container);
    assertPrecondition(node);
    // This can be slower than FlatTreeTraversal::contains() because we
    // can't early exit even when container doesn't have children.
    for (const Node* current = &node; current; current = traverseParent(*current)) {
        if (current == &container)
            return true;
    }
    return false;
}

Node* FlatTreeTraversal::previousSkippingChildren(const Node& node)
{
    if (Node* previousSibling = traversePreviousSibling(node))
        return previousSibling;
    return traversePreviousAncestorSibling(node);
}

static Node* previousAncestorSiblingPostOrder(const Node& current, const Node* stayWithin)
{
    DCHECK(!FlatTreeTraversal::previousSibling(current));
    for (Node* parent = FlatTreeTraversal::parent(current); parent; parent = FlatTreeTraversal::parent(*parent)) {
        if (parent == stayWithin)
            return nullptr;
        if (Node* previousSibling = FlatTreeTraversal::previousSibling(*parent))
            return previousSibling;
    }
    return nullptr;
}

// TODO(yosin) We should consider introducing template class to share code
// between DOM tree traversal and flat tree tarversal.
Node* FlatTreeTraversal::previousPostOrder(const Node& current, const Node* stayWithin)
{
    assertPrecondition(current);
    if (stayWithin)
        assertPrecondition(*stayWithin);
    if (Node* lastChild = traverseLastChild(current)) {
        assertPostcondition(lastChild);
        return lastChild;
    }
    if (current == stayWithin)
        return nullptr;
    if (Node* previousSibling = traversePreviousSibling(current)) {
        assertPostcondition(previousSibling);
        return previousSibling;
    }
    return previousAncestorSiblingPostOrder(current, stayWithin);
}

bool FlatTreeTraversal::isDescendantOf(const Node& node, const Node& other)
{
    assertPrecondition(node);
    assertPrecondition(other);
    if (!hasChildren(other) || node.inShadowIncludingDocument() != other.inShadowIncludingDocument())
        return false;
    for (const ContainerNode* n = traverseParent(node); n; n = traverseParent(*n)) {
        if (n == other)
            return true;
    }
    return false;
}

Node* FlatTreeTraversal::commonAncestor(const Node& nodeA, const Node& nodeB)
{
    assertPrecondition(nodeA);
    assertPrecondition(nodeB);
    Node* result = nodeA.commonAncestor(nodeB,
        [](const Node& node)
        {
            return FlatTreeTraversal::parent(node);
        });
    assertPostcondition(result);
    return result;
}

Node* FlatTreeTraversal::traverseNextAncestorSibling(const Node& node)
{
    DCHECK(!traverseNextSibling(node));
    for (Node* parent = traverseParent(node); parent; parent = traverseParent(*parent)) {
        if (Node* nextSibling = traverseNextSibling(*parent))
            return nextSibling;
    }
    return nullptr;
}

Node* FlatTreeTraversal::traversePreviousAncestorSibling(const Node& node)
{
    DCHECK(!traversePreviousSibling(node));
    for (Node* parent = traverseParent(node); parent; parent = traverseParent(*parent)) {
        if (Node* previousSibling = traversePreviousSibling(*parent))
            return previousSibling;
    }
    return nullptr;
}

unsigned FlatTreeTraversal::index(const Node& node)
{
    assertPrecondition(node);
    unsigned count = 0;
    for (Node* runner = traversePreviousSibling(node); runner; runner = previousSibling(*runner))
        ++count;
    return count;
}

unsigned FlatTreeTraversal::countChildren(const Node& node)
{
    assertPrecondition(node);
    unsigned count = 0;
    for (Node* runner = traverseFirstChild(node); runner; runner = traverseNextSibling(*runner))
        ++count;
    return count;
}

Node* FlatTreeTraversal::lastWithin(const Node& node)
{
    assertPrecondition(node);
    Node* descendant = traverseLastChild(node);
    for (Node* child = descendant; child; child = lastChild(*child))
        descendant = child;
    assertPostcondition(descendant);
    return descendant;
}

Node& FlatTreeTraversal::lastWithinOrSelf(const Node& node)
{
    assertPrecondition(node);
    Node* lastDescendant = lastWithin(node);
    Node& result = lastDescendant ? *lastDescendant : const_cast<Node&>(node);
    assertPostcondition(&result);
    return result;
}

} // namespace blink
