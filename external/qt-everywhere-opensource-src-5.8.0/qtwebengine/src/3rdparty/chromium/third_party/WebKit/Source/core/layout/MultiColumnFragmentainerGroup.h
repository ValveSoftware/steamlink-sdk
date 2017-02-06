// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MultiColumnFragmentainerGroup_h
#define MultiColumnFragmentainerGroup_h

#include "core/layout/LayoutMultiColumnFlowThread.h"
#include "wtf/Allocator.h"

namespace blink {

// A group of columns, that are laid out in the inline progression direction, all with the same
// column height.
//
// When a multicol container is inside another fragmentation context, and said multicol container
// lives in multiple outer fragmentainers (pages / columns), we need to put these inner columns into
// separate groups, with one group per outer fragmentainer. Such a group of columns is what
// comprises a "row of column boxes" in spec lingo.
//
// Column balancing, when enabled, takes place within a column fragmentainer group.
//
// Each fragmentainer group may have its own actual column count (if there are unused columns
// because of forced breaks, for example). If there are multiple fragmentainer groups, the actual
// column count must not exceed the used column count (the one calculated based on column-count and
// column-width from CSS), or they'd overflow the outer fragmentainer in the inline direction. If we
// need more columns than what a group has room for, we'll create another group and put them there
// (and make them appear in the next outer fragmentainer).
class MultiColumnFragmentainerGroup {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
public:
    MultiColumnFragmentainerGroup(const LayoutMultiColumnSet&);

    const LayoutMultiColumnSet& columnSet() const { return m_columnSet; }

    bool isFirstGroup() const;
    bool isLastGroup() const;

    // Position within the LayoutMultiColumnSet.
    LayoutUnit logicalTop() const { return m_logicalTop; }
    void setLogicalTop(LayoutUnit logicalTop) { m_logicalTop = logicalTop; }

    LayoutUnit logicalHeight() const { return m_columnHeight; }

    LayoutSize offsetFromColumnSet() const;

    // Return the block offset from the enclosing fragmentation context, if nested. In the
    // coordinate space of the enclosing fragmentation context.
    LayoutUnit blockOffsetInEnclosingFragmentationContext() const;

    // The top of our flow thread portion
    LayoutUnit logicalTopInFlowThread() const { return m_logicalTopInFlowThread; }
    void setLogicalTopInFlowThread(LayoutUnit logicalTopInFlowThread) { m_logicalTopInFlowThread = logicalTopInFlowThread; }

    // The bottom of our flow thread portion
    LayoutUnit logicalBottomInFlowThread() const { return m_logicalBottomInFlowThread; }
    void setLogicalBottomInFlowThread(LayoutUnit logicalBottomInFlowThread) { ASSERT(logicalBottomInFlowThread >= m_logicalTopInFlowThread); m_logicalBottomInFlowThread = logicalBottomInFlowThread; }

    // The height of our flow thread portion
    LayoutUnit logicalHeightInFlowThread() const { return m_logicalBottomInFlowThread - m_logicalTopInFlowThread; }

    void resetColumnHeight();
    bool recalculateColumnHeight(LayoutMultiColumnSet&);

    LayoutSize flowThreadTranslationAtOffset(LayoutUnit, CoordinateSpaceConversion) const;
    LayoutUnit columnLogicalTopForOffset(LayoutUnit offsetInFlowThread) const;
    LayoutPoint visualPointToFlowThreadPoint(const LayoutPoint& visualPoint) const;
    LayoutRect fragmentsBoundingBox(const LayoutRect& boundingBoxInFlowThread) const;

    void collectLayerFragments(PaintLayerFragments&, const LayoutRect& layerBoundingBox, const LayoutRect& dirtyRect) const;
    LayoutRect calculateOverflow() const;

    enum ColumnIndexCalculationMode {
        ClampToExistingColumns, // Stay within the range of already existing columns.
        AssumeNewColumns // Allow column indices outside the range of already existing columns.
    };
    unsigned columnIndexAtOffset(LayoutUnit offsetInFlowThread, ColumnIndexCalculationMode = ClampToExistingColumns) const;

    // The "CSS actual" value of column-count. This includes overflowing columns, if any.
    unsigned actualColumnCount() const;

private:
    LayoutUnit heightAdjustedForRowOffset(LayoutUnit height) const;
    LayoutUnit calculateMaxColumnHeight() const;
    void setAndConstrainColumnHeight(LayoutUnit);

    LayoutUnit rebalanceColumnHeightIfNeeded() const;

    LayoutRect columnRectAt(unsigned columnIndex) const;
    LayoutUnit logicalTopInFlowThreadAt(unsigned columnIndex) const { return m_logicalTopInFlowThread + columnIndex * m_columnHeight; }
    LayoutRect flowThreadPortionRectAt(unsigned columnIndex) const;
    LayoutRect flowThreadPortionOverflowRectAt(unsigned columnIndex) const;

    // Return the column that the specified visual point belongs to. Only the coordinate on the
    // column progression axis is relevant. Every point belongs to a column, even if said point is
    // not inside any of the columns.
    unsigned columnIndexAtVisualPoint(const LayoutPoint& visualPoint) const;

    // Get the first and the last column intersecting the specified block range.
    // Note that |logicalBottomInFlowThread| is an exclusive endpoint.
    void columnIntervalForBlockRangeInFlowThread(LayoutUnit logicalTopInFlowThread, LayoutUnit logicalBottomInFlowThread, unsigned& firstColumn, unsigned& lastColumn) const;

    // Get the first and the last column intersecting the specified visual rectangle.
    void columnIntervalForVisualRect(const LayoutRect&, unsigned& firstColumn, unsigned& lastColumn) const;

    const LayoutMultiColumnSet& m_columnSet;

    LayoutUnit m_logicalTop;
    LayoutUnit m_logicalTopInFlowThread;
    LayoutUnit m_logicalBottomInFlowThread;

    LayoutUnit m_columnHeight;

    LayoutUnit m_maxColumnHeight; // Maximum column height allowed.
};

// List of all fragmentainer groups within a column set. There will always be at least one
// group. Deleting the one group is not allowed (or possible). There will be more than one group if
// the owning column set lives in multiple outer fragmentainers (e.g. multicol inside paged media).
class CORE_EXPORT MultiColumnFragmentainerGroupList {
    DISALLOW_NEW();
public:
    MultiColumnFragmentainerGroupList(LayoutMultiColumnSet&);
    ~MultiColumnFragmentainerGroupList();

    // Add an additional fragmentainer group to the end of the list, and return it.
    MultiColumnFragmentainerGroup& addExtraGroup();

    // Remove all fragmentainer groups but the first one.
    void deleteExtraGroups();

    MultiColumnFragmentainerGroup& first() { return m_groups.first(); }
    const MultiColumnFragmentainerGroup& first() const { return m_groups.first(); }
    MultiColumnFragmentainerGroup& last() { return m_groups.last(); }
    const MultiColumnFragmentainerGroup& last() const { return m_groups.last(); }

    typedef Vector<MultiColumnFragmentainerGroup, 1>::iterator iterator;
    typedef Vector<MultiColumnFragmentainerGroup, 1>::const_iterator const_iterator;

    iterator begin() { return m_groups.begin(); }
    const_iterator begin() const { return m_groups.begin(); }
    iterator end() { return m_groups.end(); }
    const_iterator end() const { return m_groups.end(); }

    size_t size() const { return m_groups.size(); }
    MultiColumnFragmentainerGroup& operator[](size_t i) { return m_groups.at(i); }
    const MultiColumnFragmentainerGroup& operator[](size_t i) const { return m_groups.at(i); }

    void append(const MultiColumnFragmentainerGroup& group) { m_groups.append(group); }
    void shrink(size_t size) { m_groups.shrink(size); }

private:
    LayoutMultiColumnSet& m_columnSet;

    Vector<MultiColumnFragmentainerGroup, 1> m_groups;
};

} // namespace blink

#endif // MultiColumnFragmentainerGroup_h
