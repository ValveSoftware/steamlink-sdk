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

#ifndef PositionIterator_h
#define PositionIterator_h

#include "core/dom/Node.h"
#include "core/editing/EditingStrategy.h"
#include "core/editing/EditingUtilities.h"
#include "core/html/HTMLHtmlElement.h"

namespace blink {

// A Position iterator with nearly constant-time
// increment, decrement, and several predicates on the Position it is at.
// Conversion from Position is O(n) in the depth.
// Conversion to Position is O(1).
// PositionIteratorAlgorithm must be used without DOM tree change.
template <typename Strategy>
class PositionIteratorAlgorithm {
    STACK_ALLOCATED();
public:
    explicit PositionIteratorAlgorithm(const PositionTemplate<Strategy>&);
    PositionIteratorAlgorithm();

    // Since |deprecatedComputePosition()| is slow, new code should use
    // |computePosition()| instead.
    PositionTemplate<Strategy> deprecatedComputePosition() const;
    PositionTemplate<Strategy> computePosition() const;

    // increment() takes O(1) other than incrementing to a element that has
    // new parent.
    // In the later case, it takes time of O(<number of childlen>) but the case
    // happens at most depth-of-the-tree times over whole tree traversal.
    void increment();
    // decrement() takes O(1) other than decrement into new node that has
    // childlen.
    // In the later case, it takes time of O(<number of childlen>).
    void decrement();

    Node* node() const { return m_anchorNode; }
    int offsetInLeafNode() const { return m_offsetInAnchor; }

    bool atStart() const;
    bool atEnd() const;
    bool atStartOfNode() const;
    bool atEndOfNode() const;

private:
    PositionIteratorAlgorithm(Node* anchorNode, int offsetInAnchorNode);

    bool isValid() const { return !m_anchorNode || m_domTreeVersion == m_anchorNode->document().domTreeVersion(); }

    Member<Node> m_anchorNode;
    Member<Node> m_nodeAfterPositionInAnchor; // If this is non-null, Strategy::parent(*m_nodeAfterPositionInAnchor) == m_anchorNode;
    int m_offsetInAnchor;
    size_t m_depthToAnchorNode;
    // If |m_nodeAfterPositionInAnchor| is not null,
    // m_offsetsInAnchorNode[m_depthToAnchorNode] ==
    //    Strategy::index(m_nodeAfterPositionInAnchor).
    Vector<int> m_offsetsInAnchorNode;
    uint64_t m_domTreeVersion;
};

extern template class PositionIteratorAlgorithm<EditingStrategy>;
extern template class PositionIteratorAlgorithm<EditingInFlatTreeStrategy>;

using PositionIterator = PositionIteratorAlgorithm<EditingStrategy>;
using PositionIteratorInFlatTree = PositionIteratorAlgorithm<EditingInFlatTreeStrategy>;

} // namespace blink

#endif // PositionIterator_h
