/*
 * Copyright (C) 2012 Apple Inc.  All rights reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef LayoutMultiColumnSet_h
#define LayoutMultiColumnSet_h

#include "core/CoreExport.h"
#include "core/layout/LayoutMultiColumnFlowThread.h"
#include "core/layout/MultiColumnFragmentainerGroup.h"
#include "wtf/Vector.h"

namespace blink {

// A set of columns in a multicol container. A column set is inserted as an anonymous child of the
// actual multicol container (i.e. the layoutObject whose style computes to non-auto column-count and/or
// column-width), next to the flow thread. There'll be one column set for each contiguous run of
// column content. The only thing that can interrupt a contiguous run of column content is a column
// spanner, which means that if there are no spanners, there'll only be one column set.
//
// Since a spanner interrupts an otherwise contiguous run of column content, inserting one may
// result in the creation of additional new column sets. A placeholder for the spanning layoutObject has
// to be placed in between the column sets that come before and after the spanner, if there's
// actually column content both before and after the spanner.
//
// A column set has no children on its own, but is merely used to slice a portion of the tall
// "single-column" flow thread into actual columns visually, to convert from flow thread coordinates
// to visual ones. It is in charge of both positioning columns correctly relatively to the parent
// multicol container, and to calculate the correct translation for each column's contents, and to
// paint any rules between them. LayoutMultiColumnSet objects are used for painting, hit testing,
// and any other type of operation that requires mapping from flow thread coordinates to visual
// coordinates.
//
// Columns are normally laid out in the inline progression direction, but if the multicol container
// is inside another fragmentation context (e.g. paged media, or an another multicol container), we
// may need to group the columns, so that we get one MultiColumnFragmentainerGroup for each outer
// fragmentainer (page / column) that the inner multicol container lives in. Each fragmentainer
// group has its own column height, but the column height is uniform within a group.
class CORE_EXPORT LayoutMultiColumnSet : public LayoutBlockFlow {
public:
    static LayoutMultiColumnSet* createAnonymous(LayoutFlowThread&, const ComputedStyle& parentStyle);

    const MultiColumnFragmentainerGroup& firstFragmentainerGroup() const { return m_fragmentainerGroups.first(); }
    const MultiColumnFragmentainerGroup& lastFragmentainerGroup() const { return m_fragmentainerGroups.last(); }
    MultiColumnFragmentainerGroup& fragmentainerGroupAtFlowThreadOffset(LayoutUnit flowThreadOffset)
    {
        return m_fragmentainerGroups[fragmentainerGroupIndexAtFlowThreadOffset(flowThreadOffset)];
    }
    const MultiColumnFragmentainerGroup& fragmentainerGroupAtFlowThreadOffset(LayoutUnit flowThreadOffset) const
    {
        return m_fragmentainerGroups[fragmentainerGroupIndexAtFlowThreadOffset(flowThreadOffset)];
    }
    const MultiColumnFragmentainerGroup& fragmentainerGroupAtVisualPoint(const LayoutPoint&) const;
    const MultiColumnFragmentainerGroupList& fragmentainerGroups() const { return m_fragmentainerGroups; }

    bool isOfType(LayoutObjectType type) const override { return type == LayoutObjectLayoutMultiColumnSet || LayoutBlockFlow::isOfType(type); }
    bool canHaveChildren() const final { return false; }

    // Return the width and height of a single column or page in the set.
    LayoutUnit pageLogicalWidth() const { return flowThread()->logicalWidth(); }
    LayoutUnit pageLogicalHeightForOffset(LayoutUnit) const;
    LayoutUnit pageRemainingLogicalHeightForOffset(LayoutUnit, PageBoundaryRule) const;
    bool isPageLogicalHeightKnown() const;
    LayoutUnit tallestUnbreakableLogicalHeight() const { return m_tallestUnbreakableLogicalHeight; }
    void propagateTallestUnbreakableLogicalHeight(LayoutUnit value) { m_tallestUnbreakableLogicalHeight = std::max(value, m_tallestUnbreakableLogicalHeight); }

    LayoutUnit nextLogicalTopForUnbreakableContent(LayoutUnit flowThreadOffset, LayoutUnit contentLogicalHeight) const;

    LayoutFlowThread* flowThread() const { return m_flowThread; }

    LayoutBlockFlow* multiColumnBlockFlow() const { return toLayoutBlockFlow(parent()); }
    LayoutMultiColumnFlowThread* multiColumnFlowThread() const { return toLayoutMultiColumnFlowThread(flowThread()); }

    LayoutMultiColumnSet* nextSiblingMultiColumnSet() const;
    LayoutMultiColumnSet* previousSiblingMultiColumnSet() const;

    // Return true if we have a fragmentainer group that can hold a column at the specified flow thread block offset.
    bool hasFragmentainerGroupForColumnAt(LayoutUnit bottomOffsetInFlowThread, PageBoundaryRule) const;

    MultiColumnFragmentainerGroup& appendNewFragmentainerGroup();

    // Logical top relative to the content edge of the multicol container.
    LayoutUnit logicalTopFromMulticolContentEdge() const;

    LayoutUnit logicalTopInFlowThread() const;
    LayoutUnit logicalBottomInFlowThread() const;
    LayoutUnit logicalHeightInFlowThread() const { return logicalBottomInFlowThread() - logicalTopInFlowThread(); }

    // Return the amount of flow thread contents that the specified fragmentainer group can hold
    // without overflowing.
    LayoutUnit fragmentainerGroupCapacity(const MultiColumnFragmentainerGroup &group) const { return group.logicalHeight() * usedColumnCount(); }

    LayoutRect flowThreadPortionRect() const;
    LayoutRect flowThreadPortionOverflowRect() const;
    LayoutRect overflowRectForFlowThreadPortion(const LayoutRect& flowThreadPortionRect, bool isFirstPortion, bool isLastPortion) const;

    // The used CSS value of column-count, i.e. how many columns there are room for without overflowing.
    unsigned usedColumnCount() const { return multiColumnFlowThread()->columnCount(); }

    bool heightIsAuto() const;

    // Find the column that contains the given block offset, and return the translation needed to
    // get from flow thread coordinates to visual coordinates.
    LayoutSize flowThreadTranslationAtOffset(LayoutUnit, CoordinateSpaceConversion) const;

    LayoutPoint visualPointToFlowThreadPoint(const LayoutPoint& visualPoint) const;

    // (Re-)calculate the column height if it's auto. This is first and foremost needed by sets that
    // are to balance the column height, but even when it isn't to be balanced, this is necessary if
    // the multicol container's height is constrained.
    bool recalculateColumnHeight();

    // Reset previously calculated column height. Will mark for layout if needed.
    void resetColumnHeight();

    void storeOldPosition() { m_oldLogicalTop = logicalTop(); }
    bool isInitialHeightCalculated() const { return m_initialHeightCalculated; }

    // Layout of flow thread content that's to be rendered inside this column set begins. This
    // happens at the beginning of flow thread layout, and when advancing from a previous column set
    // or spanner to this one.
    void beginFlow(LayoutUnit offsetInFlowThread);

    // Layout of flow thread content that was to be rendered inside this column set has
    // finished. This happens at end of flow thread layout, and when advancing to the next column
    // set or spanner.
    void endFlow(LayoutUnit offsetInFlowThread);

    void styleDidChange(StyleDifference, const ComputedStyle* oldStyle) override;
    void layout() override;

    void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const final;

    void attachToFlowThread();
    void detachFromFlowThread();

    // The top of the page nearest to the specified block offset. All in flowthread coordinates.
    LayoutUnit pageLogicalTopForOffset(LayoutUnit offset) const;

    LayoutRect fragmentsBoundingBox(const LayoutRect& boundingBoxInFlowThread) const;

    void collectLayerFragments(PaintLayerFragments&, const LayoutRect& layerBoundingBox, const LayoutRect& dirtyRect);

    LayoutUnit columnGap() const;

    // The "CSS actual" value of column-count. This includes overflowing columns, if any.
    unsigned actualColumnCount() const;

    const char* name() const override { return "LayoutMultiColumnSet"; }

protected:
    LayoutMultiColumnSet(LayoutFlowThread*);

private:
    unsigned fragmentainerGroupIndexAtFlowThreadOffset(LayoutUnit) const;

    void insertedIntoTree() final;
    void willBeRemovedFromTree() final;

    bool isSelfCollapsingBlock() const override { return false; }

    void computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues&) const override;
    PositionWithAffinity positionForPoint(const LayoutPoint&) override;

    void paintObject(const PaintInfo&, const LayoutPoint& paintOffset) const override;

    void addOverflowFromChildren() override;

    MultiColumnFragmentainerGroupList m_fragmentainerGroups;
    LayoutFlowThread* m_flowThread;

    // Height of the tallest piece of unbreakable content. This is the minimum column logical height
    // required to avoid fragmentation where it shouldn't occur (inside unbreakable content, between
    // orphans and widows, etc.). We only store this so that outer fragmentation contexts (if any)
    // can query this when calculating their own minimum. Note that we don't store this value in
    // every fragmentainer group (but rather here, in the column set), since we only need the
    // largest one among them.
    LayoutUnit m_tallestUnbreakableLogicalHeight;

    // Logical top in previous layout pass.
    LayoutUnit m_oldLogicalTop;

    bool m_initialHeightCalculated;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutMultiColumnSet, isLayoutMultiColumnSet());

} // namespace blink

#endif // LayoutMultiColumnSet_h

