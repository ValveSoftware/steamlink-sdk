/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#ifndef LayoutState_h
#define LayoutState_h

#include "platform/geometry/LayoutRect.h"
#include "wtf/Allocator.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"

namespace blink {

class LayoutBox;
class LayoutFlowThread;
class LayoutObject;
class LayoutView;

// LayoutState is an optimization used during layout.
//
// LayoutState's purpose is to cache information as we walk down the container
// block chain during layout. In particular, the absolute layout offset for the
// current LayoutObject is O(1) when using LayoutState, when it is
// O(depthOfTree) without it (thus potentially making layout O(N^2)).
// LayoutState incurs some memory overhead and is pretty intrusive (see next
// paragraphs about those downsides).
//
// To use LayoutState, the layout() functions have to allocate a new LayoutSTate
// object on the stack whenever the LayoutObject creates a new coordinate system
// (which is pretty much all objects but LayoutTableRow).
//
// LayoutStates are linked together with a single linked list, acting in
// practice like a stack that we push / pop. LayoutView holds the top-most
// pointer to this stack.
//
// See the layout() functions on how to set it up during layout.
// See e.g LayoutBox::offsetFromLogicalTopOfFirstPage on how to use LayoutState
// for computations.
class LayoutState {
    // LayoutState is always allocated on the stack.
    // The reason is that it is scoped to layout, thus we can avoid expensive
    // mallocs.
    DISALLOW_NEW();
    WTF_MAKE_NONCOPYABLE(LayoutState);
public:
    // Constructor for root LayoutState created by LayoutView
    LayoutState(LayoutUnit pageLogicalHeight, bool pageLogicalHeightChanged, LayoutView&);
    // Constructor for sub-tree layout and orthogonal writing-mode roots
    explicit LayoutState(LayoutObject& root);

    LayoutState(LayoutBox&, const LayoutSize& offset, LayoutUnit pageLogicalHeight = LayoutUnit(), bool pageHeightLogicalChanged = false, bool containingBlockLogicalWidthChanged = false);

    ~LayoutState();

    bool isPaginated() const { return m_isPaginated; }

    // The page logical offset is the object's offset from the top of the page in the page progression
    // direction (so an x-offset in vertical text and a y-offset for horizontal text).
    LayoutUnit pageLogicalOffset(const LayoutBox&, const LayoutUnit& childLogicalOffset) const;

    LayoutUnit heightOffsetForTableHeaders() const { return m_heightOffsetForTableHeaders; };
    void setHeightOffsetForTableHeaders(LayoutUnit offset) { m_heightOffsetForTableHeaders = offset; };

    const LayoutSize& layoutOffset() const { return m_layoutOffset; }
    const LayoutSize& pageOffset() const { return m_pageOffset; }
    LayoutUnit pageLogicalHeight() const { return m_pageLogicalHeight; }
    bool pageLogicalHeightChanged() const { return m_pageLogicalHeightChanged; }
    bool containingBlockLogicalWidthChanged() const { return m_containingBlockLogicalWidthChanged; }

    LayoutState* next() const { return m_next; }

    LayoutFlowThread* flowThread() const { return m_flowThread; }

    LayoutObject& layoutObject() const { return m_layoutObject; }

private:
    // Do not add anything apart from bitfields until after m_flowThread. See https://bugs.webkit.org/show_bug.cgi?id=100173
    bool m_isPaginated : 1;
    // If our page height has changed, this will force all blocks to relayout.
    bool m_pageLogicalHeightChanged : 1;
    bool m_containingBlockLogicalWidthChanged : 1;

    LayoutFlowThread* m_flowThread;

    LayoutState* m_next;

    // x/y offset from container. Does not include relative positioning or scroll offsets.
    LayoutSize m_layoutOffset;

    // The current page height for the pagination model that encloses us.
    LayoutUnit m_pageLogicalHeight;

    // The height we need to make available for repeating table headers in paginated layout.
    LayoutUnit m_heightOffsetForTableHeaders;

    // The offset of the start of the first page in the nearest enclosing pagination model.
    LayoutSize m_pageOffset;

    LayoutObject& m_layoutObject;
};

} // namespace blink

#endif // LayoutState_h
