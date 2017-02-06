// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutMultiColumnSet.h"

namespace blink {

// A column balancer traverses a portion of the subtree of a flow thread that belongs to one or
// more fragmentainer groups within one column set, in order to collect certain data to be used for
// column balancing. This is an abstract class that just walks the subtree and leaves it to
// subclasses to actually collect data.
class ColumnBalancer {
protected:
    ColumnBalancer(const LayoutMultiColumnSet&, LayoutUnit logicalTopInFlowThread, LayoutUnit logicalBottomInFlowThread);

    const LayoutMultiColumnSet& columnSet() const { return m_columnSet; }

    // The flow thread portion we're examining. It may be that of the entire column set, or just of
    // a fragmentainer group.
    const LayoutUnit logicalTopInFlowThread() const { return m_logicalTopInFlowThread; }
    const LayoutUnit logicalBottomInFlowThread() const { return m_logicalBottomInFlowThread; }

    const MultiColumnFragmentainerGroup& groupAtOffset(LayoutUnit offsetInFlowThread) const
    {
        return m_columnSet.fragmentainerGroupAtFlowThreadOffset(offsetInFlowThread);
    }

    LayoutUnit offsetFromColumnLogicalTop(LayoutUnit offsetInFlowThread) const
    {
        return offsetInFlowThread - groupAtOffset(offsetInFlowThread).columnLogicalTopForOffset(offsetInFlowThread);
    }

    // Flow thread offset for the layout object that we're currently examining.
    LayoutUnit flowThreadOffset() const { return m_flowThreadOffset; }

    // Return true if the specified offset is at the top of a column, as long as it's not the first
    // column in the flow thread portion.
    bool isFirstAfterBreak(LayoutUnit flowThreadOffset) const
    {
        if (flowThreadOffset <= m_logicalTopInFlowThread) {
            // The first column is either not after any break at all, or after a break in a
            // previous fragmentainer group.
            return false;
        }
        return flowThreadOffset == groupAtOffset(flowThreadOffset).columnLogicalTopForOffset(flowThreadOffset);
    }

    bool isLogicalTopWithinBounds(LayoutUnit logicalTopInFlowThread) const
    {
        return logicalTopInFlowThread >= m_logicalTopInFlowThread
            && logicalTopInFlowThread < m_logicalBottomInFlowThread;
    }

    bool isLogicalBottomWithinBounds(LayoutUnit logicalBottomInFlowThread) const
    {
        return logicalBottomInFlowThread > m_logicalTopInFlowThread
            && logicalBottomInFlowThread <= m_logicalBottomInFlowThread;
    }

    // Examine and collect column balancing data from a layout box that has been found to intersect
    // with the flow thread portion we're examining. Does not recurse into
    // children. flowThreadOffset() will return the offset from |box| to the flow thread. Two hooks
    // are provided here. The first one is called right after entering and before traversing the
    // subtree of the box, and the second one right after having traversed the subtree.
    virtual void examineBoxAfterEntering(const LayoutBox&, EBreak previousBreakAfterValue) = 0;
    virtual void examineBoxBeforeLeaving(const LayoutBox&) = 0;

    // Examine and collect column balancing data from a line that has been found to intersect with
    // the flow thread portion. Does not recurse into layout objects on that line.
    virtual void examineLine(const RootInlineBox&) = 0;

    // Examine and collect column balancing data for everything in the flow thread portion. Will
    // trigger calls to examineBoxAfterEntering(), examineBoxBeforeLeaving() and examineLine() for
    // interesting boxes and lines.
    void traverse();

private:
    void traverseSubtree(const LayoutBox&);

    const LayoutMultiColumnSet& m_columnSet;
    const LayoutUnit m_logicalTopInFlowThread;
    const LayoutUnit m_logicalBottomInFlowThread;

    LayoutUnit m_flowThreadOffset;
};

// After an initial layout pass, we know the height of the contents of a flow thread. Based on
// this, we can estimate an initial minimal column height. This class will collect the necessary
// information from the layout objects to make this estimate. This estimate may be used to perform
// another layout iteration. If we after such a layout iteration cannot fit the contents with the
// given column height without creating overflowing columns, we will have to stretch the columns by
// some amount and lay out again. We may need to do this several times (but typically not more
// times than the number of columns that we have). The amount to stretch is provided by the sister
// of this class, named MinimumSpaceShortageFinder.
class InitialColumnHeightFinder final : public ColumnBalancer {
public:
    InitialColumnHeightFinder(const LayoutMultiColumnSet&, LayoutUnit logicalTopInFlowThread, LayoutUnit logicalBottomInFlowThread);

    LayoutUnit initialMinimalBalancedHeight() const;

    // Height of the tallest piece of unbreakable content. This is the minimum column logical height
    // required to avoid fragmentation where it shouldn't occur (inside unbreakable content, between
    // orphans and widows, etc.). This will be used as a hint to the column balancer to help set a
    // good initial column height.
    LayoutUnit tallestUnbreakableLogicalHeight() const { return m_tallestUnbreakableLogicalHeight; }

private:
    void examineBoxAfterEntering(const LayoutBox&, EBreak previousBreakAfterValue);
    void examineBoxBeforeLeaving(const LayoutBox&);
    void examineLine(const RootInlineBox&);

    // Record that there's a pagination strut that ends at the specified |offsetInFlowThread|, which
    // is an offset exactly at the top of some column.
    void recordStrutBeforeOffset(LayoutUnit offsetInFlowThread, LayoutUnit strut);

    // Return the accumulated space used by struts at all column boundaries preceding the specified
    // flowthread offset.
    LayoutUnit spaceUsedByStrutsAt(LayoutUnit offsetInFlowThread) const;

    // Add a content run, specified by its end position. A content run is appended at every
    // forced/explicit break and at the end of the column set. The content runs are used to
    // determine where implicit/soft breaks will occur, in order to calculate an initial column
    // height.
    void addContentRun(LayoutUnit endOffsetInFlowThread);

    // Return the index of the content run with the currently tallest columns, taking all implicit
    // breaks assumed so far into account.
    unsigned contentRunIndexWithTallestColumns() const;

    // Given the current list of content runs, make assumptions about where we need to insert
    // implicit breaks (if there's room for any at all; depending on the number of explicit breaks),
    // and store the results. This is needed in order to balance the columns.
    void distributeImplicitBreaks();

    // A run of content without explicit (forced) breaks; i.e. a flow thread portion between two
    // explicit breaks, between flow thread start and an explicit break, between an explicit break
    // and flow thread end, or, in cases when there are no explicit breaks at all: between flow
    // thread portion start and flow thread portion end. We need to know where the explicit breaks
    // are, in order to figure out where the implicit breaks will end up, so that we get the columns
    // properly balanced. A content run starts out as representing one single column, and will
    // represent one additional column for each implicit break "inserted" there.
    class ContentRun {
    public:
        ContentRun(LayoutUnit breakOffset)
            : m_breakOffset(breakOffset)
            , m_assumedImplicitBreaks(0) { }

        unsigned assumedImplicitBreaks() const { return m_assumedImplicitBreaks; }
        void assumeAnotherImplicitBreak() { m_assumedImplicitBreaks++; }
        LayoutUnit breakOffset() const { return m_breakOffset; }

        // Return the column height that this content run would require, considering the implicit
        // breaks assumed so far.
        LayoutUnit columnLogicalHeight(LayoutUnit startOffset) const
        {
            return LayoutUnit::fromFloatCeil(float(m_breakOffset - startOffset) / float(m_assumedImplicitBreaks + 1));
        }

    private:
        LayoutUnit m_breakOffset; // Flow thread offset where this run ends.
        unsigned m_assumedImplicitBreaks; // Number of implicit breaks in this run assumed so far.
    };
    Vector<ContentRun, 32> m_contentRuns;

    // Shortest strut found at each column boundary (index 0 being the boundary between the first
    // and the second column, index 1 being the one between the second and the third boundary, and
    // so on). There may be several objects that cross the same column boundary, and we're only
    // interested in the shortest one. For example, when having a float beside regular in-flow
    // content, we end up with two parallel fragmentation flows [1]. The shortest strut found at a
    // column boundary is the amount of space that we wasted at said column boundary, and it needs
    // to be deducted when estimating the initial balanced column height, or we risk making the
    // column row too tall. An entry set to LayoutUnit::max() means that we didn't detect any object
    // crossing that boundary.
    //
    // [1] http://www.w3.org/TR/css3-break/#parallel-flows
    Vector<LayoutUnit, 32> m_shortestStruts;

    LayoutUnit m_tallestUnbreakableLogicalHeight;
};

// If we have previously used InitialColumnHeightFinder to estimate an initial column height, and
// that didn't result in tall enough columns, we need subsequent layout passes where we increase
// the column height by the minimum space shortage at column breaks. This class finds the minimum
// space shortage after having laid out with the current column height.
class MinimumSpaceShortageFinder final : public ColumnBalancer {
public:
    MinimumSpaceShortageFinder(const LayoutMultiColumnSet&, LayoutUnit logicalTopInFlowThread, LayoutUnit logicalBottomInFlowThread);

    LayoutUnit minimumSpaceShortage() const { return m_minimumSpaceShortage; }
    unsigned forcedBreaksCount() const { return m_forcedBreaksCount; }

private:
    void examineBoxAfterEntering(const LayoutBox&, EBreak previousBreakAfterValue);
    void examineBoxBeforeLeaving(const LayoutBox&);
    void examineLine(const RootInlineBox&);

    void recordSpaceShortage(LayoutUnit shortage)
    {
        // Only positive values are interesting (and allowed) here. Zero space shortage may
        // be reported when we're at the top of a column and the element has zero
        // height.
        if (shortage > 0)
            m_minimumSpaceShortage = std::min(m_minimumSpaceShortage, shortage);
    }

    // The smallest amout of space shortage that caused a column break.
    LayoutUnit m_minimumSpaceShortage;

    // Set when breaking before a block, and we're looking for the first unbreakable descendant, in
    // order to report correct space shortage for that one.
    LayoutUnit m_pendingStrut;

    unsigned m_forcedBreaksCount;
};

} // namespace blink
