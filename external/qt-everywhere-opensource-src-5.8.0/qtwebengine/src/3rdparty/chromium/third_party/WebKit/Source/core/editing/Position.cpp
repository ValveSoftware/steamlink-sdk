/*
 * Copyright (C) 2004, 2005, 2006, 2009 Apple Inc. All rights reserved.
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

#include "core/editing/Position.h"

#include "core/dom/shadow/ElementShadow.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/TextAffinity.h"
#include "wtf/text/CString.h"
#include <stdio.h>
#include <ostream> // NOLINT

namespace blink {

#if DCHECK_IS_ON()
static bool canBeAnchorNode(Node* node)
{
    return !node || !node->isPseudoElement();
}
#endif

template <typename Strategy>
const TreeScope* PositionTemplate<Strategy>::commonAncestorTreeScope(const PositionTemplate<Strategy>& a, const PositionTemplate<Strategy>& b)
{
    if (!a.computeContainerNode() || !b.computeContainerNode())
        return nullptr;
    return a.computeContainerNode()->treeScope().commonAncestorTreeScope(b.computeContainerNode()->treeScope());
}


template <typename Strategy>
PositionTemplate<Strategy> PositionTemplate<Strategy>::editingPositionOf(Node* anchorNode, int offset)
{
    if (!anchorNode || anchorNode->isTextNode())
        return PositionTemplate<Strategy>(anchorNode, offset);

    if (!Strategy::editingIgnoresContent(anchorNode))
        return PositionTemplate<Strategy>(anchorNode, offset);

    if (offset == 0)
        return PositionTemplate<Strategy>(anchorNode, PositionAnchorType::BeforeAnchor);

    // Note: |offset| can be >= 1, if |anchorNode| have child nodes, e.g.
    // using Node.appendChild() to add a child node TEXTAREA.
    DCHECK_GE(offset, 1);
    return PositionTemplate<Strategy>(anchorNode, PositionAnchorType::AfterAnchor);
}

template <typename Strategy>
PositionTemplate<Strategy>::PositionTemplate(Node* anchorNode, PositionAnchorType anchorType)
    : m_anchorNode(anchorNode)
    , m_offset(0)
    , m_anchorType(anchorType)
{
    if (!m_anchorNode) {
        m_anchorType = PositionAnchorType::OffsetInAnchor;
        return;
    }
    if (m_anchorNode->isTextNode()) {
        DCHECK(m_anchorType == PositionAnchorType::BeforeAnchor || m_anchorType == PositionAnchorType::AfterAnchor);
        return;
    }
    if (m_anchorNode->isDocumentNode()) {
        // Since |RangeBoundaryPoint| can't represent before/after Document, we
        // should not use them.
        DCHECK(isBeforeChildren() || isAfterChildren()) << m_anchorType;
        return;
    }
#if DCHECK_IS_ON()
    DCHECK(canBeAnchorNode(m_anchorNode.get()));
#endif
    DCHECK_NE(m_anchorType, PositionAnchorType::OffsetInAnchor);
}

template <typename Strategy>
PositionTemplate<Strategy>::PositionTemplate(Node* anchorNode, int offset)
    : m_anchorNode(anchorNode)
    , m_offset(offset)
    , m_anchorType(PositionAnchorType::OffsetInAnchor)
{
    if (m_anchorNode)
        DCHECK_GE(offset, 0);
    else
        DCHECK_EQ(offset, 0);
#if DCHECK_IS_ON()
    DCHECK(canBeAnchorNode(m_anchorNode.get()));
#endif
}

template <typename Strategy>
PositionTemplate<Strategy>::PositionTemplate(const PositionTemplate& other)
    : m_anchorNode(other.m_anchorNode)
    , m_offset(other.m_offset)
    , m_anchorType(other.m_anchorType)
{
}

// --

template <typename Strategy>
Node* PositionTemplate<Strategy>::computeContainerNode() const
{
    if (!m_anchorNode)
        return 0;

    switch (anchorType()) {
    case PositionAnchorType::BeforeChildren:
    case PositionAnchorType::AfterChildren:
    case PositionAnchorType::OffsetInAnchor:
        return m_anchorNode.get();
    case PositionAnchorType::BeforeAnchor:
    case PositionAnchorType::AfterAnchor:
        return Strategy::parent(*m_anchorNode);
    }
    NOTREACHED();
    return 0;
}

template <typename Strategy>
int PositionTemplate<Strategy>::computeOffsetInContainerNode() const
{
    if (!m_anchorNode)
        return 0;

    switch (anchorType()) {
    case PositionAnchorType::BeforeChildren:
        return 0;
    case PositionAnchorType::AfterChildren:
        return lastOffsetInNode(m_anchorNode.get());
    case PositionAnchorType::OffsetInAnchor:
        return minOffsetForNode(m_anchorNode.get(), m_offset);
    case PositionAnchorType::BeforeAnchor:
        return Strategy::index(*m_anchorNode);
    case PositionAnchorType::AfterAnchor:
        return Strategy::index(*m_anchorNode) + 1;
    }
    NOTREACHED();
    return 0;
}

// Neighbor-anchored positions are invalid DOM positions, so they need to be
// fixed up before handing them off to the Range object.
template <typename Strategy>
PositionTemplate<Strategy> PositionTemplate<Strategy>::parentAnchoredEquivalent() const
{
    if (!m_anchorNode)
        return PositionTemplate<Strategy>();

    // FIXME: This should only be necessary for legacy positions, but is also needed for positions before and after Tables
    if (m_offset == 0 && !isAfterAnchorOrAfterChildren()) {
        if (Strategy::parent(*m_anchorNode) && (Strategy::editingIgnoresContent(m_anchorNode.get()) || isDisplayInsideTable(m_anchorNode.get())))
            return inParentBeforeNode(*m_anchorNode);
        return PositionTemplate<Strategy>(m_anchorNode.get(), 0);
    }
    if (!m_anchorNode->offsetInCharacters()
        && (isAfterAnchorOrAfterChildren() || static_cast<unsigned>(m_offset) == m_anchorNode->countChildren())
        && (Strategy::editingIgnoresContent(m_anchorNode.get()) || isDisplayInsideTable(m_anchorNode.get()))
        && computeContainerNode()) {
        return inParentAfterNode(*m_anchorNode);
    }

    return PositionTemplate<Strategy>(computeContainerNode(), computeOffsetInContainerNode());
}

template <typename Strategy>
PositionTemplate<Strategy> PositionTemplate<Strategy>::toOffsetInAnchor() const
{
    if (isNull())
        return PositionTemplate<Strategy>();

    return PositionTemplate<Strategy>(computeContainerNode(), computeOffsetInContainerNode());
}

template <typename Strategy>
int PositionTemplate<Strategy>::computeEditingOffset() const
{
    if (isAfterAnchorOrAfterChildren())
        return Strategy::lastOffsetForEditing(m_anchorNode.get());
    return m_offset;
}

template <typename Strategy>
Node* PositionTemplate<Strategy>::computeNodeBeforePosition() const
{
    if (!m_anchorNode)
        return 0;
    switch (anchorType()) {
    case PositionAnchorType::BeforeChildren:
        return 0;
    case PositionAnchorType::AfterChildren:
        return Strategy::lastChild(*m_anchorNode);
    case PositionAnchorType::OffsetInAnchor:
        return m_offset ? Strategy::childAt(*m_anchorNode, m_offset - 1) : 0;
    case PositionAnchorType::BeforeAnchor:
        return Strategy::previousSibling(*m_anchorNode);
    case PositionAnchorType::AfterAnchor:
        return m_anchorNode.get();
    }
    NOTREACHED();
    return 0;
}

template <typename Strategy>
Node* PositionTemplate<Strategy>::computeNodeAfterPosition() const
{
    if (!m_anchorNode)
        return 0;

    switch (anchorType()) {
    case PositionAnchorType::BeforeChildren:
        return Strategy::firstChild(*m_anchorNode);
    case PositionAnchorType::AfterChildren:
        return 0;
    case PositionAnchorType::OffsetInAnchor:
        return Strategy::childAt(*m_anchorNode, m_offset);
    case PositionAnchorType::BeforeAnchor:
        return m_anchorNode.get();
    case PositionAnchorType::AfterAnchor:
        return Strategy::nextSibling(*m_anchorNode);
    }
    NOTREACHED();
    return 0;
}

// An implementation of |Range::firstNode()|.
template <typename Strategy>
Node* PositionTemplate<Strategy>::nodeAsRangeFirstNode() const
{
    if (!m_anchorNode)
        return nullptr;
    if (!isOffsetInAnchor())
        return toOffsetInAnchor().nodeAsRangeFirstNode();
    if (m_anchorNode->offsetInCharacters())
        return m_anchorNode.get();
    if (Node* child = Strategy::childAt(*m_anchorNode, m_offset))
        return child;
    if (!m_offset)
        return m_anchorNode.get();
    return Strategy::nextSkippingChildren(*m_anchorNode);
}

template <typename Strategy>
Node* PositionTemplate<Strategy>::nodeAsRangeLastNode() const
{
    if (isNull())
        return nullptr;
    if (Node* pastLastNode = nodeAsRangePastLastNode())
        return Strategy::previous(*pastLastNode);
    return &Strategy::lastWithinOrSelf(*computeContainerNode());
}

// An implementation of |Range::pastLastNode()|.
template <typename Strategy>
Node* PositionTemplate<Strategy>::nodeAsRangePastLastNode() const
{
    if (!m_anchorNode)
        return nullptr;
    if (!isOffsetInAnchor())
        return toOffsetInAnchor().nodeAsRangePastLastNode();
    if (m_anchorNode->offsetInCharacters())
        return Strategy::nextSkippingChildren(*m_anchorNode);
    if (Node* child = Strategy::childAt(*m_anchorNode, m_offset))
        return child;
    return Strategy::nextSkippingChildren(*m_anchorNode);
}

template <typename Strategy>
Node* PositionTemplate<Strategy>::commonAncestorContainer(const PositionTemplate<Strategy>& other) const
{
    return Strategy::commonAncestor(*computeContainerNode(), *other.computeContainerNode());
}

int comparePositions(const PositionInFlatTree& positionA, const PositionInFlatTree& positionB)
{
    DCHECK(positionA.isNotNull());
    DCHECK(positionB.isNotNull());

    positionA.anchorNode()->updateDistribution();
    Node* containerA = positionA.computeContainerNode();
    positionB.anchorNode()->updateDistribution();
    Node* containerB = positionB.computeContainerNode();
    int offsetA = positionA.computeOffsetInContainerNode();
    int offsetB = positionB.computeOffsetInContainerNode();
    return comparePositionsInFlatTree(containerA, offsetA, containerB, offsetB);
}

template <typename Strategy>
int PositionTemplate<Strategy>::compareTo(const PositionTemplate<Strategy>& other) const
{
    return comparePositions(*this, other);
}

template <typename Strategy>
bool PositionTemplate<Strategy>::operator<(const PositionTemplate<Strategy>& other) const
{
    return comparePositions(*this, other) < 0;
}

template <typename Strategy>
bool PositionTemplate<Strategy>::operator<=(const PositionTemplate<Strategy>& other) const
{
    return comparePositions(*this, other) <= 0;
}

template <typename Strategy>
bool PositionTemplate<Strategy>::operator>(const PositionTemplate<Strategy>& other) const
{
    return comparePositions(*this, other) > 0;
}

template <typename Strategy>
bool PositionTemplate<Strategy>::operator>=(const PositionTemplate<Strategy>& other) const
{
    return comparePositions(*this, other) >= 0;
}

template <typename Strategy>
bool PositionTemplate<Strategy>::atFirstEditingPositionForNode() const
{
    if (isNull())
        return true;
    // FIXME: Position before anchor shouldn't be considered as at the first editing position for node
    // since that position resides outside of the node.
    switch (m_anchorType) {
    case PositionAnchorType::OffsetInAnchor:
        return m_offset == 0;
    case PositionAnchorType::BeforeChildren:
    case PositionAnchorType::BeforeAnchor:
        return true;
    case PositionAnchorType::AfterChildren:
    case PositionAnchorType::AfterAnchor:
        // TODO(yosin) We should use |Strategy::lastOffsetForEditing()| instead
        // of DOM tree version.
        return !EditingStrategy::lastOffsetForEditing(anchorNode());
    }
    NOTREACHED();
    return false;
}

template <typename Strategy>
bool PositionTemplate<Strategy>::atLastEditingPositionForNode() const
{
    if (isNull())
        return true;
    // TODO(yosin): Position after anchor shouldn't be considered as at the
    // first editing position for node since that position resides outside of
    // the node.
    // TODO(yosin) We should use |Strategy::lastOffsetForEditing()| instead of
    // DOM tree version.
    return isAfterAnchorOrAfterChildren() || m_offset >= EditingStrategy::lastOffsetForEditing(anchorNode());
}

template <typename Strategy>
bool PositionTemplate<Strategy>::atStartOfTree() const
{
    if (isNull())
        return true;
    return !Strategy::parent(*anchorNode()) && m_offset == 0;
}

template <typename Strategy>
bool PositionTemplate<Strategy>::atEndOfTree() const
{
    if (isNull())
        return true;
    // TODO(yosin) We should use |Strategy::lastOffsetForEditing()| instead of
    // DOM tree version.
    return !Strategy::parent(*anchorNode()) && m_offset >= EditingStrategy::lastOffsetForEditing(anchorNode());
}

// static
template <typename Strategy>
PositionTemplate<Strategy> PositionTemplate<Strategy>::inParentBeforeNode(const Node& node)
{
    // FIXME: This should DCHECK(node.parentNode())
    // At least one caller currently hits this ASSERT though, which indicates
    // that the caller is trying to make a position relative to a disconnected node (which is likely an error)
    // Specifically, editing/deleting/delete-ligature-001.html crashes with DCHECK(node->parentNode())
    return PositionTemplate<Strategy>(Strategy::parent(node), Strategy::index(node));
}

// static
template <typename Strategy>
PositionTemplate<Strategy> PositionTemplate<Strategy>::inParentAfterNode(const Node& node)
{
    DCHECK(node.parentNode()) << node;
    return PositionTemplate<Strategy>(Strategy::parent(node), Strategy::index(node) + 1);
}

// static
template <typename Strategy>
PositionTemplate<Strategy> PositionTemplate<Strategy>::beforeNode(Node* anchorNode)
{
    DCHECK(anchorNode);
    return PositionTemplate<Strategy>(anchorNode, PositionAnchorType::BeforeAnchor);
}

// static
template <typename Strategy>
PositionTemplate<Strategy> PositionTemplate<Strategy>::afterNode(Node* anchorNode)
{
    DCHECK(anchorNode);
    return PositionTemplate<Strategy>(anchorNode, PositionAnchorType::AfterAnchor);
}

// static
template <typename Strategy>
int PositionTemplate<Strategy>::lastOffsetInNode(Node* node)
{
    return node->offsetInCharacters() ? node->maxCharacterOffset() : static_cast<int>(Strategy::countChildren(*node));
}

// static
template <typename Strategy>
PositionTemplate<Strategy> PositionTemplate<Strategy>::firstPositionInNode(Node* anchorNode)
{
    if (anchorNode->isTextNode())
        return PositionTemplate<Strategy>(anchorNode, 0);
    return PositionTemplate<Strategy>(anchorNode, PositionAnchorType::BeforeChildren);
}

// static
template <typename Strategy>
PositionTemplate<Strategy> PositionTemplate<Strategy>::lastPositionInNode(Node* anchorNode)
{
    if (anchorNode->isTextNode())
        return PositionTemplate<Strategy>(anchorNode, lastOffsetInNode(anchorNode));
    return PositionTemplate<Strategy>(anchorNode, PositionAnchorType::AfterChildren);
}

// static
template <typename Strategy>
int PositionTemplate<Strategy>::minOffsetForNode(Node* anchorNode, int offset)
{
    if (anchorNode->offsetInCharacters())
        return std::min(offset, anchorNode->maxCharacterOffset());

    int newOffset = 0;
    for (Node* node = Strategy::firstChild(*anchorNode); node && newOffset < offset; node = Strategy::nextSibling(*node))
        newOffset++;

    return newOffset;
}

// static
template <typename Strategy>
PositionTemplate<Strategy> PositionTemplate<Strategy>::firstPositionInOrBeforeNode(Node* node)
{
    if (!node)
        return PositionTemplate<Strategy>();
    return Strategy::editingIgnoresContent(node) ? beforeNode(node) : firstPositionInNode(node);
}

// static
template <typename Strategy>
PositionTemplate<Strategy> PositionTemplate<Strategy>::lastPositionInOrAfterNode(Node* node)
{
    if (!node)
        return PositionTemplate<Strategy>();
    return Strategy::editingIgnoresContent(node) ? afterNode(node) : lastPositionInNode(node);
}

template <typename Strategy>
void PositionTemplate<Strategy>::debugPosition(const char* msg) const
{
    static const char* const anchorTypes[] = {
        "OffsetInAnchor",
        "BeforeAnchor",
        "AfterAnchor",
        "BeforeChildren",
        "AfterChildren",
        "Invalid",
    };

    if (isNull()) {
        fprintf(stderr, "Position [%s]: null\n", msg);
        return;
    }

    const char* anchorType = anchorTypes[std::min(static_cast<size_t>(m_anchorType), WTF_ARRAY_LENGTH(anchorTypes) - 1)];
    if (m_anchorNode->isTextNode()) {
        fprintf(stderr, "Position [%s]: %s [%p] %s, (%s) at %d\n", msg, anchorNode()->nodeName().utf8().data(), anchorNode(), anchorType, m_anchorNode->nodeValue().utf8().data(), m_offset);
        return;
    }

    fprintf(stderr, "Position [%s]: %s [%p] %s at %d\n", msg, anchorNode()->nodeName().utf8().data(), anchorNode(), anchorType, m_offset);
}

PositionInFlatTree toPositionInFlatTree(const Position& pos)
{
    if (pos.isNull())
        return PositionInFlatTree();

    if (pos.isOffsetInAnchor()) {
        Node* anchor = pos.anchorNode();
        if (anchor->offsetInCharacters())
            return PositionInFlatTree(anchor, pos.computeOffsetInContainerNode());
        DCHECK(!anchor->isSlotOrActiveInsertionPoint());
        int offset = pos.computeOffsetInContainerNode();
        Node* child = NodeTraversal::childAt(*anchor, offset);
        if (!child) {
            if (anchor->isShadowRoot())
                return PositionInFlatTree(anchor->shadowHost(), PositionAnchorType::AfterChildren);
            return PositionInFlatTree(anchor, PositionAnchorType::AfterChildren);
        }
        child->updateDistribution();
        if (child->isSlotOrActiveInsertionPoint()) {
            if (anchor->isShadowRoot())
                return PositionInFlatTree(anchor->shadowHost(), offset);
            return PositionInFlatTree(anchor, offset);
        }
        if (Node* parent = FlatTreeTraversal::parent(*child))
            return PositionInFlatTree(parent, FlatTreeTraversal::index(*child));
        // When |pos| isn't appeared in flat tree, we map |pos| to after
        // children of shadow host.
        // e.g. "foo",0 in <progress>foo</progress>
        if (anchor->isShadowRoot())
            return PositionInFlatTree(anchor->shadowHost(), PositionAnchorType::AfterChildren);
        return PositionInFlatTree(anchor, PositionAnchorType::AfterChildren);
    }

    return PositionInFlatTree(pos.anchorNode(), pos.anchorType());
}

Position toPositionInDOMTree(const Position& position)
{
    return position;
}

Position toPositionInDOMTree(const PositionInFlatTree& position)
{
    if (position.isNull())
        return Position();

    Node* anchorNode = position.anchorNode();

    switch (position.anchorType()) {
    case PositionAnchorType::AfterChildren:
        // FIXME: When anchorNode is <img>, assertion fails in the constructor.
        return Position(anchorNode, PositionAnchorType::AfterChildren);
    case PositionAnchorType::AfterAnchor:
        return Position::afterNode(anchorNode);
    case PositionAnchorType::BeforeChildren:
        return Position(anchorNode, PositionAnchorType::BeforeChildren);
    case PositionAnchorType::BeforeAnchor:
        return Position::beforeNode(anchorNode);
    case PositionAnchorType::OffsetInAnchor: {
        int offset = position.offsetInContainerNode();
        if (anchorNode->offsetInCharacters())
            return Position(anchorNode, offset);
        Node* child = FlatTreeTraversal::childAt(*anchorNode, offset);
        if (child)
            return Position(child->parentNode(), child->nodeIndex());
        if (!position.offsetInContainerNode())
            return Position(anchorNode, PositionAnchorType::BeforeChildren);

        // |child| is null when the position is at the end of the children.
        // <div>foo|</div>
        return Position(anchorNode, PositionAnchorType::AfterChildren);
    }
    default:
        NOTREACHED();
        return Position();
    }
}

#ifndef NDEBUG

template <typename Strategy>
void PositionTemplate<Strategy>::formatForDebugger(char* buffer, unsigned length) const
{
    StringBuilder result;

    if (isNull()) {
        result.append("<null>");
    } else {
        char s[1024];
        result.append("offset ");
        result.appendNumber(m_offset);
        result.append(" of ");
        anchorNode()->formatForDebugger(s, sizeof(s));
        result.append(s);
    }

    strncpy(buffer, result.toString().utf8().data(), length - 1);
}

template <typename Strategy>
void PositionTemplate<Strategy>::showAnchorTypeAndOffset() const
{
    switch (anchorType()) {
    case PositionAnchorType::OffsetInAnchor:
        fputs("offset", stderr);
        break;
    case PositionAnchorType::BeforeChildren:
        fputs("beforeChildren", stderr);
        break;
    case PositionAnchorType::AfterChildren:
        fputs("afterChildren", stderr);
        break;
    case PositionAnchorType::BeforeAnchor:
        fputs("before", stderr);
        break;
    case PositionAnchorType::AfterAnchor:
        fputs("after", stderr);
        break;
    }
    fprintf(stderr, ", offset:%d\n", m_offset);
}

template <typename Strategy>
void PositionTemplate<Strategy>::showTreeForThis() const
{
    if (!anchorNode())
        return;
    anchorNode()->showTreeForThis();
    showAnchorTypeAndOffset();
}

template <typename Strategy>
void PositionTemplate<Strategy>::showTreeForThisInFlatTree() const
{
    if (!anchorNode())
        return;
    anchorNode()->showTreeForThisInFlatTree();
    showAnchorTypeAndOffset();
}

#endif

template <typename PositionType>
static std::ostream& printPosition(std::ostream& ostream, const PositionType& position)
{
    if (position.isNull())
        return ostream << "null";
    ostream << position.anchorNode() << "@";
    if (position.isOffsetInAnchor())
        return ostream << position.offsetInContainerNode();
    return ostream << position.anchorType();
}

std::ostream& operator<<(std::ostream& ostream, PositionAnchorType anchorType)
{
    switch (anchorType) {
    case PositionAnchorType::AfterAnchor:
        return ostream << "afterAnchor";
    case PositionAnchorType::AfterChildren:
        return ostream << "afterChildren";
    case PositionAnchorType::BeforeAnchor:
        return ostream << "beforeAnchor";
    case PositionAnchorType::BeforeChildren:
        return ostream << "beforeChildren";
    case PositionAnchorType::OffsetInAnchor:
        return ostream << "offsetInAnchor";
    }
    NOTREACHED();
    return ostream << "anchorType=" << static_cast<int>(anchorType);
}

std::ostream& operator<<(std::ostream& ostream, const Position& position)
{
    return printPosition(ostream, position);
}

std::ostream& operator<<(std::ostream& ostream, const PositionInFlatTree& position)
{
    return printPosition(ostream, position);
}

template class CORE_TEMPLATE_EXPORT PositionTemplate<EditingStrategy>;
template class CORE_TEMPLATE_EXPORT PositionTemplate<EditingInFlatTreeStrategy>;

} // namespace blink

#ifndef NDEBUG

void showTree(const blink::Position& pos)
{
    pos.showTreeForThis();
}

void showTree(const blink::Position* pos)
{
    if (pos)
        pos->showTreeForThis();
    else
        fprintf(stderr, "Cannot showTree for (nil)\n");
}

#endif
