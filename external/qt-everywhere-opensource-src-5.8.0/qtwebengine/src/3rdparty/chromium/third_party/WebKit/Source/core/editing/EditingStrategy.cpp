// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/EditingStrategy.h"

#include "core/editing/EditingUtilities.h"
#include "core/layout/LayoutObject.h"

namespace blink {

// If a node can contain candidates for VisiblePositions, return the offset of
// the last candidate, otherwise return the number of children for container
// nodes and the length for unrendered text nodes.
template <typename Traversal>
int EditingAlgorithm<Traversal>::caretMaxOffset(const Node& node)
{
    // For rendered text nodes, return the last position that a caret could
    // occupy.
    if (node.isTextNode() && node.layoutObject())
        return node.layoutObject()->caretMaxOffset();
    // For containers return the number of children. For others do the same as
    // above.
    return lastOffsetForEditing(&node);
}

template <typename Traversal>
bool EditingAlgorithm<Traversal>::isEmptyNonEditableNodeInEditable(const Node* node)
{
    // Editability is defined the DOM tree rather than the flat tree. For example:
    // DOM:
    //   <host><span>unedittable</span><shadowroot><div ce><content /></div></shadowroot></host>
    // Flat Tree:
    //   <host><div ce><span1>unedittable</span></div></host>
    // e.g. editing/shadow/breaking-editing-boundaries.html
    return !Traversal::hasChildren(*node) && !node->hasEditableStyle() && node->parentNode() && node->parentNode()->hasEditableStyle();
}

template <typename Traversal>
bool EditingAlgorithm<Traversal>::editingIgnoresContent(const Node* node)
{
    return !node->canContainRangeEndPoint() || isEmptyNonEditableNodeInEditable(node);
}

template <typename Traversal>
int EditingAlgorithm<Traversal>::lastOffsetForEditing(const Node* node)
{
    DCHECK(node);
    if (!node)
        return 0;
    if (node->offsetInCharacters())
        return node->maxCharacterOffset();

    if (Traversal::hasChildren(*node))
        return Traversal::countChildren(*node);

    // FIXME: Try return 0 here.

    if (!editingIgnoresContent(node))
        return 0;

    // editingIgnoresContent uses the same logic in
    // isEmptyNonEditableNodeInEditable (EditingUtilities.cpp). We don't
    // understand why this function returns 1 even when the node doesn't have
    // children.
    return 1;
}

template <typename Strategy>
Node* EditingAlgorithm<Strategy>::rootUserSelectAllForNode(Node* node)
{
    if (!node || !nodeIsUserSelectAll(node))
        return nullptr;
    Node* parent = Strategy::parent(*node);
    if (!parent)
        return node;

    Node* candidateRoot = node;
    while (parent) {
        if (!parent->layoutObject()) {
            parent = Strategy::parent(*parent);
            continue;
        }
        if (!nodeIsUserSelectAll(parent))
            break;
        candidateRoot = parent;
        parent = Strategy::parent(*candidateRoot);
    }
    return candidateRoot;
}

template class CORE_TEMPLATE_EXPORT EditingAlgorithm<NodeTraversal>;
template class CORE_TEMPLATE_EXPORT EditingAlgorithm<FlatTreeTraversal>;

} // namespace blink
