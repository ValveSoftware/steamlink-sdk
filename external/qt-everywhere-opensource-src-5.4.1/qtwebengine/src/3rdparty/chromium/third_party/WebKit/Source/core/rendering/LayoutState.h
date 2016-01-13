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

#include "core/rendering/ColumnInfo.h"
#include "platform/geometry/LayoutRect.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"

namespace WebCore {

class ForceHorriblySlowRectMapping;
class RenderBox;
class RenderObject;
class RenderView;

class LayoutState {
    WTF_MAKE_NONCOPYABLE(LayoutState);
public:
    // Constructor for root LayoutState created by RenderView
    LayoutState(LayoutUnit pageLogicalHeight, bool pageLogicalHeightChanged, RenderView&);
    // Constructor for sub-tree Layout and RenderTableSections
    explicit LayoutState(RenderObject& root);

    LayoutState(RenderBox&, const LayoutSize& offset, LayoutUnit pageLogicalHeight = 0, bool pageHeightLogicalChanged = false, ColumnInfo* = 0);

    ~LayoutState();

    void clearPaginationInformation();
    bool isPaginatingColumns() const { return m_columnInfo; }
    bool isPaginated() const { return m_isPaginated; }
    bool isClipped() const { return m_clipped; }

    // The page logical offset is the object's offset from the top of the page in the page progression
    // direction (so an x-offset in vertical text and a y-offset for horizontal text).
    LayoutUnit pageLogicalOffset(const RenderBox&, const LayoutUnit& childLogicalOffset) const;

    void addForcedColumnBreak(const RenderBox&, const LayoutUnit& childLogicalOffset);

    void addLayoutDelta(const LayoutSize& delta)
    {
        m_layoutDelta += delta;
#if ASSERT_ENABLED
            m_layoutDeltaXSaturated |= m_layoutDelta.width() == LayoutUnit::max() || m_layoutDelta.width() == LayoutUnit::min();
            m_layoutDeltaYSaturated |= m_layoutDelta.height() == LayoutUnit::max() || m_layoutDelta.height() == LayoutUnit::min();
#endif
    }

    void setColumnInfo(ColumnInfo* columnInfo) { m_columnInfo = columnInfo; }

    const LayoutSize& layoutOffset() const { return m_layoutOffset; }
    const LayoutSize& layoutDelta() const { return m_layoutDelta; }
    const LayoutSize& pageOffset() const { return m_pageOffset; }
    LayoutUnit pageLogicalHeight() const { return m_pageLogicalHeight; }
    bool pageLogicalHeightChanged() const { return m_pageLogicalHeightChanged; }

    LayoutState* next() const { return m_next; }

    bool needsBlockDirectionLocationSetBeforeLayout() const { return m_isPaginated && m_pageLogicalHeight; }

    ColumnInfo* columnInfo() const { return m_columnInfo; }

    bool cachedOffsetsEnabled() const { return m_cachedOffsetsEnabled; }

    const LayoutRect& clipRect() const
    {
        ASSERT(m_cachedOffsetsEnabled);
        return m_clipRect;
    }
    const LayoutSize& paintOffset() const
    {
        ASSERT(m_cachedOffsetsEnabled);
        return m_paintOffset;
    }


    RenderObject& renderer() const { return m_renderer; }

#if ASSERT_ENABLED
    bool layoutDeltaXSaturated() const { return m_layoutDeltaXSaturated; }
    bool layoutDeltaYSaturated() const { return m_layoutDeltaYSaturated; }
#endif

private:
    friend class ForceHorriblySlowRectMapping;

    // Do not add anything apart from bitfields until after m_columnInfo. See https://bugs.webkit.org/show_bug.cgi?id=100173
    bool m_clipped:1;
    bool m_isPaginated:1;
    // If our page height has changed, this will force all blocks to relayout.
    bool m_pageLogicalHeightChanged:1;

    bool m_cachedOffsetsEnabled:1;
#if ASSERT_ENABLED
    bool m_layoutDeltaXSaturated:1;
    bool m_layoutDeltaYSaturated:1;
#endif
    // If the enclosing pagination model is a column model, then this will store column information for easy retrieval/manipulation.
    ColumnInfo* m_columnInfo;
    LayoutState* m_next;

    // FIXME: Distinguish between the layout clip rect and the paint clip rect which may be larger,
    // e.g., because of composited scrolling.
    LayoutRect m_clipRect;

    // x/y offset from container. Includes relative positioning and scroll offsets.
    LayoutSize m_paintOffset;
    // x/y offset from container. Does not include relative positioning or scroll offsets.
    LayoutSize m_layoutOffset;
    // Transient offset from the final position of the object
    // used to ensure that repaints happen in the correct place.
    // This is a total delta accumulated from the root.
    LayoutSize m_layoutDelta;

    // The current page height for the pagination model that encloses us.
    LayoutUnit m_pageLogicalHeight;
    // The offset of the start of the first page in the nearest enclosing pagination model.
    LayoutSize m_pageOffset;

    RenderObject& m_renderer;
};

} // namespace WebCore

#endif // LayoutState_h
