/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
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

#ifndef PaintLayerStackingNodeIterator_h
#define PaintLayerStackingNodeIterator_h

#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"

namespace blink {

enum ChildrenIteration {
    NegativeZOrderChildren = 1,
    // Normal flow children are not mandated by CSS 2.1 but are an artifact of
    // our implementation: we allocate PaintLayers for elements that
    // are not treated as stacking contexts and thus we need to walk them
    // during painting and hit-testing.
    NormalFlowChildren = 1 << 1,
    PositiveZOrderChildren = 1 << 2,
    AllChildren = NegativeZOrderChildren | NormalFlowChildren | PositiveZOrderChildren
};

class PaintLayerStackingNode;
class PaintLayer;

// This iterator walks the PaintLayerStackingNode lists in the following order:
// NegativeZOrderChildren -> NormalFlowChildren -> PositiveZOrderChildren.
class PaintLayerStackingNodeIterator {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(PaintLayerStackingNodeIterator);
public:
    PaintLayerStackingNodeIterator(const PaintLayerStackingNode& root, unsigned whichChildren);

    PaintLayerStackingNode* next();

private:
    const PaintLayerStackingNode& m_root;
    unsigned m_remainingChildren;
    unsigned m_index;
    PaintLayer* m_currentNormalFlowChild;
};

// This iterator is similar to PaintLayerStackingNodeIterator but it walks the lists in reverse order
// (from the last item to the first one).
class PaintLayerStackingNodeReverseIterator {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(PaintLayerStackingNodeReverseIterator);
public:
    PaintLayerStackingNodeReverseIterator(const PaintLayerStackingNode& root, unsigned whichChildren)
        : m_root(root)
        , m_remainingChildren(whichChildren)
    {
        setIndexToLastItem();
    }

    PaintLayerStackingNode* next();

private:
    void setIndexToLastItem();

    const PaintLayerStackingNode& m_root;
    unsigned m_remainingChildren;
    int m_index;
    PaintLayer* m_currentNormalFlowChild;
};

} // namespace blink

#endif // PaintLayerStackingNodeIterator_h
