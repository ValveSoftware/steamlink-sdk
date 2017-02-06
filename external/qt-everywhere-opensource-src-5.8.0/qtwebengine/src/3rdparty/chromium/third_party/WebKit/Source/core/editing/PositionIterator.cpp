/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/editing/PositionIterator.h"

namespace blink {

static const int kInvalidOffset = -1;

template <typename Strategy>
PositionIteratorAlgorithm<Strategy>::PositionIteratorAlgorithm(Node* anchorNode, int offsetInAnchor)
    : m_anchorNode(anchorNode)
    , m_nodeAfterPositionInAnchor(Strategy::childAt(*anchorNode, offsetInAnchor))
    , m_offsetInAnchor(m_nodeAfterPositionInAnchor ? 0 : offsetInAnchor)
    , m_depthToAnchorNode(0)
    , m_domTreeVersion(anchorNode->document().domTreeVersion())
{
    for (Node* node = Strategy::parent(*anchorNode); node; node = Strategy::parent(*node)) {
        // Each m_offsetsInAnchorNode[offset] should be an index of node in
        // parent, but delay to calculate the index until it is needed for
        // performance.
        m_offsetsInAnchorNode.append(kInvalidOffset);
        ++m_depthToAnchorNode;
    }
    if (m_nodeAfterPositionInAnchor)
        m_offsetsInAnchorNode.append(offsetInAnchor);
}
template <typename Strategy>
PositionIteratorAlgorithm<Strategy>::PositionIteratorAlgorithm(const PositionTemplate<Strategy>& pos)
    : PositionIteratorAlgorithm(pos.anchorNode(), pos.computeEditingOffset())
{
}

template <typename Strategy>
PositionIteratorAlgorithm<Strategy>::PositionIteratorAlgorithm()
    : m_anchorNode(nullptr)
    , m_nodeAfterPositionInAnchor(nullptr)
    , m_offsetInAnchor(0)
    , m_depthToAnchorNode(0)
    , m_domTreeVersion(0)
{
}

template <typename Strategy>
PositionTemplate<Strategy> PositionIteratorAlgorithm<Strategy>::deprecatedComputePosition() const
{
    // TODO(yoichio): Share code to check domTreeVersion with EphemeralRange.
    DCHECK(isValid());
    if (m_nodeAfterPositionInAnchor) {
        DCHECK_EQ(Strategy::parent(*m_nodeAfterPositionInAnchor), m_anchorNode);
        DCHECK_NE(m_offsetsInAnchorNode[m_depthToAnchorNode], kInvalidOffset);
        // FIXME: This check is inadaquete because any ancestor could be ignored by editing
        if (Strategy::editingIgnoresContent(Strategy::parent(*m_nodeAfterPositionInAnchor)))
            return PositionTemplate<Strategy>::beforeNode(m_anchorNode);
        return PositionTemplate<Strategy>(m_anchorNode, m_offsetsInAnchorNode[m_depthToAnchorNode]);
    }
    if (Strategy::hasChildren(*m_anchorNode))
        return PositionTemplate<Strategy>::lastPositionInOrAfterNode(m_anchorNode);
    return PositionTemplate<Strategy>::editingPositionOf(m_anchorNode, m_offsetInAnchor);
}

template <typename Strategy>
PositionTemplate<Strategy> PositionIteratorAlgorithm<Strategy>::computePosition() const
{
    DCHECK(isValid());
    // Assume that we have the following DOM tree:
    // A
    // |-B
    // | |-E
    // | +-F
    // |
    // |-C
    // +-D
    //   |-G
    //   +-H
    if (m_nodeAfterPositionInAnchor) {
        // For example, position is before E, F.
        DCHECK_EQ(Strategy::parent(*m_nodeAfterPositionInAnchor), m_anchorNode);
        DCHECK_NE(m_offsetsInAnchorNode[m_depthToAnchorNode], kInvalidOffset);
        // TODO(yoichio): This should be equivalent to
        // PositionTemplate<Strategy>(m_anchorNode, PositionAnchorType::BeforeAnchor);
        return PositionTemplate<Strategy>(m_anchorNode, m_offsetsInAnchorNode[m_depthToAnchorNode]);
    }
    if (Strategy::hasChildren(*m_anchorNode))
        // For example, position is the end of B.
        return PositionTemplate<Strategy>::lastPositionInOrAfterNode(m_anchorNode);
    if (m_anchorNode->isTextNode())
        return PositionTemplate<Strategy>(m_anchorNode, m_offsetInAnchor);
    if (m_offsetInAnchor)
        // For example, position is after G.
        return PositionTemplate<Strategy>(m_anchorNode, PositionAnchorType::AfterAnchor);
    // For example, position is before G.
    return PositionTemplate<Strategy>(m_anchorNode, PositionAnchorType::BeforeAnchor);
}

template <typename Strategy>
void PositionIteratorAlgorithm<Strategy>::increment()
{
    DCHECK(isValid());
    if (!m_anchorNode)
        return;

    // Assume that we have the following DOM tree:
    // A
    // |-B
    // | |-E
    // | +-F
    // |
    // |-C
    // +-D
    //   |-G
    //   +-H
    // Let |anchor| as |m_anchorNode| and
    // |child| as |m_nodeAfterPositionInAnchor|.
    if (m_nodeAfterPositionInAnchor) {
        // Case #1: Move to position before the first child of
        // |m_nodeAfterPositionInAnchor|.
        // This is a point just before |child|.
        // Let |anchor| is A and |child| is B,
        // then next |anchor| is B and |child| is E.
        m_anchorNode = m_nodeAfterPositionInAnchor;
        m_nodeAfterPositionInAnchor = Strategy::firstChild(*m_anchorNode);
        m_offsetInAnchor = 0;
        // Increment depth intializing with 0.
        ++m_depthToAnchorNode;
        if (m_depthToAnchorNode == m_offsetsInAnchorNode.size())
            m_offsetsInAnchorNode.append(0);
        else
            m_offsetsInAnchorNode[m_depthToAnchorNode] = 0;
        return;
    }

    if (m_anchorNode->layoutObject() && !Strategy::hasChildren(*m_anchorNode) && m_offsetInAnchor < Strategy::lastOffsetForEditing(m_anchorNode)) {
        // Case #2. This is the next of Case #1 or #2 itself.
        // Position is (|anchor|, |m_offsetInAchor|).
        // In this case |anchor| is a leaf(E,F,C,G or H) and
        // |m_offsetInAnchor| is not on the end of |anchor|.
        // Then just increment |m_offsetInAnchor|.
        m_offsetInAnchor = nextGraphemeBoundaryOf(m_anchorNode, m_offsetInAnchor);
    } else {
        // Case #3. This is the next of Case #2 or #3.
        // Position is the end of |anchor|.
        // 3-a. If |anchor| has next sibling (let E),
        //      next |anchor| is B and |child| is F (next is Case #1.)
        // 3-b. If |anchor| doesn't have next sibling (let F),
        //      next |anchor| is B and |child| is null. (next is Case #3.)
        m_nodeAfterPositionInAnchor = m_anchorNode;
        m_anchorNode = Strategy::parent(*m_nodeAfterPositionInAnchor);
        if (!m_anchorNode)
            return;
        DCHECK_GT(m_depthToAnchorNode, 0u);
        --m_depthToAnchorNode;
        // Increment offset of |child| or initialize if it have never been
        // used.
        if (m_offsetsInAnchorNode[m_depthToAnchorNode] == kInvalidOffset)
            m_offsetsInAnchorNode[m_depthToAnchorNode] = Strategy::index(*m_nodeAfterPositionInAnchor) + 1;
        else
            ++m_offsetsInAnchorNode[m_depthToAnchorNode];
        m_nodeAfterPositionInAnchor = Strategy::nextSibling(*m_nodeAfterPositionInAnchor);
        m_offsetInAnchor = 0;
    }
}

template <typename Strategy>
void PositionIteratorAlgorithm<Strategy>::decrement()
{
    DCHECK(isValid());
    if (!m_anchorNode)
        return;

    // Assume that we have the following DOM tree:
    // A
    // |-B
    // | |-E
    // | +-F
    // |
    // |-C
    // +-D
    //   |-G
    //   +-H
    // Let |anchor| as |m_anchorNode| and
    // |child| as |m_nodeAfterPositionInAnchor|.
    // decrement() is complex but logically reverse of increment(), of course:)
    if (m_nodeAfterPositionInAnchor) {
        m_anchorNode = Strategy::previousSibling(*m_nodeAfterPositionInAnchor);
        if (m_anchorNode) {
            // Case #1-a. This is a revese of increment()::Case#3-a.
            // |child| has a previous sibling.
            // Let |anchor| is B and |child| is F,
            // next |anchor| is E and |child| is null.
            m_nodeAfterPositionInAnchor = nullptr;
            m_offsetInAnchor = Strategy::hasChildren(*m_anchorNode) ? 0 : Strategy::lastOffsetForEditing(m_anchorNode);
            // Decrement offset of |child| or initialize if it have never been
            // used.
            if (m_offsetsInAnchorNode[m_depthToAnchorNode] == kInvalidOffset)
                m_offsetsInAnchorNode[m_depthToAnchorNode] = Strategy::index(*m_nodeAfterPositionInAnchor);
            else
                --m_offsetsInAnchorNode[m_depthToAnchorNode];
            DCHECK_GE(m_offsetsInAnchorNode[m_depthToAnchorNode], 0);
            // Increment depth intializing with last offset.
            ++m_depthToAnchorNode;
            if (m_depthToAnchorNode >= m_offsetsInAnchorNode.size())
                m_offsetsInAnchorNode.append(m_offsetInAnchor);
            else
                m_offsetsInAnchorNode[m_depthToAnchorNode] = m_offsetInAnchor;
            return;
        } else {
            // Case #1-b. This is a revese of increment()::Case#1.
            // |child| doesn't have a previous sibling.
            // Let |anchor| is B and |child| is E,
            // next |anchor| is A and |child| is B.
            m_nodeAfterPositionInAnchor = Strategy::parent(*m_nodeAfterPositionInAnchor);
            m_anchorNode = Strategy::parent(*m_nodeAfterPositionInAnchor);
            if (!m_anchorNode)
                return;
            m_offsetInAnchor = 0;
            // Decrement depth and intialize if needs.
            DCHECK_GT(m_depthToAnchorNode, 0u);
            --m_depthToAnchorNode;
            if (m_offsetsInAnchorNode[m_depthToAnchorNode] == kInvalidOffset)
                m_offsetsInAnchorNode[m_depthToAnchorNode] = Strategy::index(*m_nodeAfterPositionInAnchor);
        }
        return;
    }

    if (Strategy::hasChildren(*m_anchorNode)) {
        // Case #2. This is a reverse of increment()::Case3-b.
        // Let |anchor| is B, next |anchor| is F.
        m_anchorNode = Strategy::lastChild(*m_anchorNode);
        m_offsetInAnchor = Strategy::hasChildren(*m_anchorNode)? 0 : Strategy::lastOffsetForEditing(m_anchorNode);
        // Decrement depth initializing with -1 because
        // |m_nodeAfterPositionInAnchor| is null so still unneeded.
        if (m_depthToAnchorNode >= m_offsetsInAnchorNode.size())
            m_offsetsInAnchorNode.append(kInvalidOffset);
        else
            m_offsetsInAnchorNode[m_depthToAnchorNode] = kInvalidOffset;
        ++m_depthToAnchorNode;
        return;
    } else {
        if (m_offsetInAnchor && m_anchorNode->layoutObject()) {
            // Case #3-a. This is a reverse of increment()::Case#2.
            // In this case |anchor| is a leaf(E,F,C,G or H) and
            // |m_offsetInAnchor| is not on the beginning of |anchor|.
            // Then just decrement |m_offsetInAnchor|.
            m_offsetInAnchor = previousGraphemeBoundaryOf(m_anchorNode, m_offsetInAnchor);
            return;
        } else {
            // Case #3-b. This is a reverse of increment()::Case#1.
            // In this case |anchor| is a leaf(E,F,C,G or H) and
            // |m_offsetInAnchor| is on the beginning of |anchor|.
            // Let |anchor| is E,
            // next |anchor| is B and |child| is E.
            m_nodeAfterPositionInAnchor = m_anchorNode;
            m_anchorNode = Strategy::parent(*m_anchorNode);
            if (!m_anchorNode)
                return;
            DCHECK_GT(m_depthToAnchorNode, 0u);
            --m_depthToAnchorNode;
            if (m_offsetsInAnchorNode[m_depthToAnchorNode] == kInvalidOffset)
                m_offsetsInAnchorNode[m_depthToAnchorNode] = Strategy::index(*m_nodeAfterPositionInAnchor);
        }
    }
}

template <typename Strategy>
bool PositionIteratorAlgorithm<Strategy>::atStart() const
{
    DCHECK(isValid());
    if (!m_anchorNode)
        return true;
    if (Strategy::parent(*m_anchorNode))
        return false;
    return (!Strategy::hasChildren(*m_anchorNode) && !m_offsetInAnchor) || (m_nodeAfterPositionInAnchor && !Strategy::previousSibling(*m_nodeAfterPositionInAnchor));
}

template <typename Strategy>
bool PositionIteratorAlgorithm<Strategy>::atEnd() const
{
    DCHECK(isValid());
    if (!m_anchorNode)
        return true;
    if (m_nodeAfterPositionInAnchor)
        return false;
    return !Strategy::parent(*m_anchorNode) && (Strategy::hasChildren(*m_anchorNode) || m_offsetInAnchor >= Strategy::lastOffsetForEditing(m_anchorNode));
}

template <typename Strategy>
bool PositionIteratorAlgorithm<Strategy>::atStartOfNode() const
{
    DCHECK(isValid());
    if (!m_anchorNode)
        return true;
    if (!m_nodeAfterPositionInAnchor)
        return !Strategy::hasChildren(*m_anchorNode) && !m_offsetInAnchor;
    return !Strategy::previousSibling(*m_nodeAfterPositionInAnchor);
}

template <typename Strategy>
bool PositionIteratorAlgorithm<Strategy>::atEndOfNode() const
{
    DCHECK(isValid());
    if (!m_anchorNode)
        return true;
    if (m_nodeAfterPositionInAnchor)
        return false;
    return Strategy::hasChildren(*m_anchorNode) || m_offsetInAnchor >= Strategy::lastOffsetForEditing(m_anchorNode);
}

template class PositionIteratorAlgorithm<EditingStrategy>;
template class PositionIteratorAlgorithm<EditingInFlatTreeStrategy>;

} // namespace blink
