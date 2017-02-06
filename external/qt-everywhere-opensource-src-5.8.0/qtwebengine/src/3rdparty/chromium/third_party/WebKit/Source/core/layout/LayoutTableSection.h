/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2009, 2013 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef LayoutTableSection_h
#define LayoutTableSection_h

#include "core/CoreExport.h"
#include "core/layout/LayoutTable.h"
#include "core/layout/LayoutTableBoxComponent.h"
#include "wtf/Vector.h"

namespace blink {

// This variable is used to balance the memory consumption vs the paint invalidation time on big tables.
const float gMaxAllowedOverflowingCellRatioForFastPaintPath = 0.1f;

// Helper class for paintObject.
class CellSpan {
    STACK_ALLOCATED();
public:
    CellSpan(unsigned start, unsigned end)
        : m_start(start)
        , m_end(end)
    {
    }

    unsigned start() const { return m_start; }
    unsigned end() const { return m_end; }

    void decreaseStart() { --m_start; }
    void increaseEnd() { ++m_end; }

    void ensureConsistency(const unsigned);

private:
    unsigned m_start;
    unsigned m_end;
};

class LayoutTableCell;
class LayoutTableRow;

// LayoutTableSection is used to represent table row group (display:
// table-row-group), header group (display: table-header-group) and footer group
// (display: table-footer-group).
//
// The object holds the internal representation of the rows (m_grid). See
// recalcCells() below for some extra explanation.
//
// A lot of the complexity in this class is related to handling rowspan, colspan
// or just non-regular tables.
//
// Example of rowspan / colspan leading to overlapping cells (rowspan and
// colspan are overlapping):
// <table>
//   <tr>
//       <td>first row</td>
//       <td rowspan="2">rowspan</td>
//     </tr>
//    <tr>
//        <td colspan="2">colspan</td>
//     </tr>
// </table>
//
// Example of non-regular table (missing one cell in the first row):
// <!DOCTYPE html>
// <table>
//   <tr><td>First row only child.</td></tr>
//   <tr>
//     <td>Second row first child</td>
//     <td>Second row second child</td>
//   </tr>
// </table>
//
// LayoutTableSection is responsible for laying out LayoutTableRows and
// LayoutTableCells (see layoutRows()). However it is not their containing
// block, the enclosing LayoutTable (this object's parent()) is. This is why
// this class inherits from LayoutTableBoxComponent and not LayoutBlock.
class CORE_EXPORT LayoutTableSection final : public LayoutTableBoxComponent {
public:
    explicit LayoutTableSection(Element*);
    ~LayoutTableSection() override;

    LayoutTableRow* firstRow() const;
    LayoutTableRow* lastRow() const;

    void addChild(LayoutObject* child, LayoutObject* beforeChild = nullptr) override;

    int firstLineBoxBaseline() const override;

    void addCell(LayoutTableCell*, LayoutTableRow*);

    int calcRowLogicalHeight();
    void layoutRows();
    void computeOverflowFromCells();
    bool recalcChildOverflowAfterStyleChange();

    LayoutTable* table() const { return toLayoutTable(parent()); }

    typedef Vector<LayoutTableCell*, 2> SpanningLayoutTableCells;

    // CellStruct represents the cells that occupy an (N, M) position in the
    // table grid.
    struct CellStruct {
        DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    public:
        // All the cells that fills this grid "slot".
        // Due to colspan / rowpsan, it is possible to have overlapping cells
        // (see class comment about an example).
        // This Vector is sorted in DOM order.
        Vector<LayoutTableCell*, 1> cells;
        bool inColSpan; // true for columns after the first in a colspan

        CellStruct();
        ~CellStruct();

        // This is the cell in the grid "slot" that is on top of the others
        // (aka the last cell in DOM order for this slot).
        //
        // This is the cell originating from this slot if it exists.
        //
        // The concept of a primary cell is dubious at most as it doesn't
        // correspond to a DOM or rendering concept. Also callers should be
        // careful about assumptions about it. For example, even though the
        // primary cell is visibly the top most, it is not guaranteed to be
        // the only one visible for this slot due to different visual
        // overflow rectangles.
        LayoutTableCell* primaryCell()
        {
            return hasCells() ? cells[cells.size() - 1] : 0;
        }

        const LayoutTableCell* primaryCell() const
        {
            return hasCells() ? cells[cells.size() - 1] : 0;
        }

        bool hasCells() const { return cells.size() > 0; }
    };

    // The index is effective column index.
    typedef Vector<CellStruct> Row;

    struct RowStruct {
        DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    public:
        RowStruct()
            : rowLayoutObject(nullptr)
            , baseline(-1)
        {
        }

        Row row;
        LayoutTableRow* rowLayoutObject;
        int baseline;
        Length logicalHeight;
    };

    struct SpanningRowsHeight {
        STACK_ALLOCATED();
        WTF_MAKE_NONCOPYABLE(SpanningRowsHeight);

    public:
        SpanningRowsHeight()
            : totalRowsHeight(0)
            , spanningCellHeightIgnoringBorderSpacing(0)
            , isAnyRowWithOnlySpanningCells(false)
        {
        }

        Vector<int> rowHeight;
        int totalRowsHeight;
        int spanningCellHeightIgnoringBorderSpacing;
        bool isAnyRowWithOnlySpanningCells;
    };

    const BorderValue& borderAdjoiningTableStart() const
    {
        if (hasSameDirectionAs(table()))
            return style()->borderStart();

        return style()->borderEnd();
    }

    const BorderValue& borderAdjoiningTableEnd() const
    {
        if (hasSameDirectionAs(table()))
            return style()->borderEnd();

        return style()->borderStart();
    }

    const BorderValue& borderAdjoiningStartCell(const LayoutTableCell*) const;
    const BorderValue& borderAdjoiningEndCell(const LayoutTableCell*) const;

    const LayoutTableCell* firstRowCellAdjoiningTableStart() const;
    const LayoutTableCell* firstRowCellAdjoiningTableEnd() const;

    CellStruct& cellAt(unsigned row,  unsigned effectiveColumn) { return m_grid[row].row[effectiveColumn]; }
    const CellStruct& cellAt(unsigned row, unsigned effectiveColumn) const { return m_grid[row].row[effectiveColumn]; }
    LayoutTableCell* primaryCellAt(unsigned row, unsigned effectiveColumn)
    {
        CellStruct& c = m_grid[row].row[effectiveColumn];
        return c.primaryCell();
    }
    const LayoutTableCell* primaryCellAt(unsigned row, unsigned effectiveColumn) const { return const_cast<LayoutTableSection*>(this)->primaryCellAt(row, effectiveColumn); }

    LayoutTableRow* rowLayoutObjectAt(unsigned row) { return m_grid[row].rowLayoutObject; }
    const LayoutTableRow* rowLayoutObjectAt(unsigned row) const { return m_grid[row].rowLayoutObject; }

    void appendEffectiveColumn(unsigned pos);
    void splitEffectiveColumn(unsigned pos, unsigned first);

    enum BlockBorderSide { BorderBefore, BorderAfter };
    int calcBlockDirectionOuterBorder(BlockBorderSide) const;
    enum InlineBorderSide { BorderStart, BorderEnd };
    int calcInlineDirectionOuterBorder(InlineBorderSide) const;
    void recalcOuterBorder();

    int outerBorderBefore() const { return m_outerBorderBefore; }
    int outerBorderAfter() const { return m_outerBorderAfter; }
    int outerBorderStart() const { return m_outerBorderStart; }
    int outerBorderEnd() const { return m_outerBorderEnd; }

    unsigned numRows() const { return m_grid.size(); }
    unsigned numEffectiveColumns() const;

    // recalcCells() is used when we are not sure about the section's structure
    // and want to do an expensive (but safe) reconstruction of m_grid from
    // scratch.
    // An example of this is inserting a new cell in the middle of an existing
    // row or removing a row.
    //
    // Accessing m_grid when m_needsCellRecalc is set is UNSAFE as pointers can
    // be left dangling. Thus care should be taken in the code to check
    // m_needsCellRecalc before accessing m_grid.
    void recalcCells();
    void recalcCellsIfNeeded()
    {
        if (m_needsCellRecalc)
            recalcCells();
    }

    bool needsCellRecalc() const { return m_needsCellRecalc; }
    void setNeedsCellRecalc();

    int rowBaseline(unsigned row) { return m_grid[row].baseline; }

    void rowLogicalHeightChanged(LayoutTableRow*);

    // distributeExtraLogicalHeightToRows methods return the *consumed* extra logical height.
    // FIXME: We may want to introduce a structure holding the in-flux layout information.
    int distributeExtraLogicalHeightToRows(int extraLogicalHeight);

    static LayoutTableSection* createAnonymousWithParent(const LayoutObject*);
    LayoutBox* createAnonymousBoxWithSameTypeAs(const LayoutObject* parent) const override
    {
        return createAnonymousWithParent(parent);
    }

    void paint(const PaintInfo&, const LayoutPoint&) const override;

    // Flip the rect so it aligns with the coordinates used by the rowPos and columnPos vectors.
    LayoutRect logicalRectForWritingModeAndDirection(const LayoutRect&) const;

    CellSpan dirtiedRows(const LayoutRect& paintInvalidationRect) const;
    CellSpan dirtiedEffectiveColumns(const LayoutRect& paintInvalidationRect) const;
    const HashSet<LayoutTableCell*>& overflowingCells() const { return m_overflowingCells; }
    bool hasMultipleCellLevels() const { return m_hasMultipleCellLevels; }

    const char* name() const override { return "LayoutTableSection"; }

    // Whether a section has opaque background depends on many factors, e.g. border spacing,
    // border collapsing, missing cells, etc.
    // For simplicity, just conservatively assume all table sections are not opaque.
    bool foregroundIsKnownToBeOpaqueInRect(const LayoutRect&, unsigned) const override { return false; }
    bool backgroundIsKnownToBeOpaqueInRect(const LayoutRect&) const override { return false; }

    int paginationStrutForRow(LayoutTableRow*, LayoutUnit logicalOffset) const;

    void setOffsetForRepeatingHeader(LayoutUnit offset) { m_offsetForRepeatingHeader = offset; }
    LayoutUnit offsetForRepeatingHeader() const { return m_offsetForRepeatingHeader; }

    bool mapToVisualRectInAncestorSpace(const LayoutBoxModelObject* ancestor, LayoutRect&, VisualRectFlags = DefaultVisualRectFlags) const override;

    bool hasRepeatingHeaderGroup() const;

protected:
    void styleDidChange(StyleDifference, const ComputedStyle* oldStyle) override;
    bool nodeAtPoint(HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;

private:
    bool isOfType(LayoutObjectType type) const override { return type == LayoutObjectTableSection || LayoutBox::isOfType(type); }

    void willBeRemovedFromTree() override;

    void layout() override;

    int borderSpacingForRow(unsigned row) const { return m_grid[row].rowLayoutObject ? table()->vBorderSpacing() : 0; }

    void ensureRows(unsigned);

    bool rowHasOnlySpanningCells(unsigned);
    unsigned calcRowHeightHavingOnlySpanningCells(unsigned, int&, unsigned, unsigned&, Vector<int>&);
    void updateRowsHeightHavingOnlySpanningCells(LayoutTableCell*, struct SpanningRowsHeight&, unsigned&, Vector<int>&);

    void populateSpanningRowsHeightFromCell(LayoutTableCell*, struct SpanningRowsHeight&);
    void distributeExtraRowSpanHeightToPercentRows(LayoutTableCell*, float, int&, Vector<int>&);
    void distributeWholeExtraRowSpanHeightToPercentRows(LayoutTableCell*, float, int&, Vector<int>&);
    void distributeExtraRowSpanHeightToAutoRows(LayoutTableCell*, int, int&, Vector<int>&);
    void distributeExtraRowSpanHeightToRemainingRows(LayoutTableCell*, int, int&, Vector<int>&);
    void distributeRowSpanHeightToRows(SpanningLayoutTableCells& rowSpanCells);

    void distributeExtraLogicalHeightToPercentRows(int& extraLogicalHeight, int totalPercent);
    void distributeExtraLogicalHeightToAutoRows(int& extraLogicalHeight, unsigned autoRowsCount);
    void distributeRemainingExtraLogicalHeight(int& extraLogicalHeight);

    void updateBaselineForCell(LayoutTableCell*, unsigned row, int& baselineDescent);

    bool hasOverflowingCell() const { return m_overflowingCells.size() || m_forceSlowPaintPathWithOverflowingCell; }

    void computeOverflowFromCells(unsigned totalRows, unsigned nEffCols);

    CellSpan fullTableRowSpan() const { return CellSpan(0, m_grid.size()); }
    CellSpan fullTableEffectiveColumnSpan() const { return CellSpan(0, table()->numEffectiveColumns()); }

    // These two functions take a rectangle as input that has been flipped by logicalRectForWritingModeAndDirection.
    // The returned span of rows or columns is end-exclusive, and empty if start==end.
    CellSpan spannedRows(const LayoutRect& flippedRect) const;
    CellSpan spannedEffectiveColumns(const LayoutRect& flippedRect) const;

    void setLogicalPositionForCell(LayoutTableCell*, unsigned effectiveColumn) const;

    // The representation of the rows and their cells (CellStruct).
    Vector<RowStruct> m_grid;

    // The logical offset of each row from the top of the section.
    //
    // Note that this Vector has one more entry than the number of rows so that
    // we can keep track of the final size of the section. That is,
    // m_rowPos[m_grid.size()] is a valid entry.
    //
    // To know a row's height at |rowIndex|, use the formula:
    // m_rowPos[rowIndex + 1] - m_rowPos[rowIndex]
    Vector<int> m_rowPos;

    // The current insertion position in the grid.
    // The position is used when inserting a new cell into the section to
    // know where it should be inserted and expand our internal structure.
    //
    // The reason for them is that we process cells as we discover them
    // during parsing or during recalcCells (ie in DOM order). This means
    // that we can discover changes in the structure later (e.g. due to
    // colspans, extra cells, ...).
    //
    // Do not use outside of recalcCells and addChild.
    unsigned m_cCol;
    unsigned m_cRow;

    int m_outerBorderStart;
    int m_outerBorderEnd;
    int m_outerBorderBefore;
    int m_outerBorderAfter;

    bool m_needsCellRecalc;

    // This HashSet holds the overflowing cells for faster painting.
    // If we have more than gMaxAllowedOverflowingCellRatio * total cells, it will be empty
    // and m_forceSlowPaintPathWithOverflowingCell will be set to save memory.
    HashSet<LayoutTableCell*> m_overflowingCells;
    bool m_forceSlowPaintPathWithOverflowingCell;

    // This boolean tracks if we have cells overlapping due to rowspan / colspan
    // (see class comment above about when it could appear).
    //
    // The use is to disable a painting optimization where we just paint the
    // invalidated cells.
    bool m_hasMultipleCellLevels;

    LayoutUnit m_offsetForRepeatingHeader;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutTableSection, isTableSection());

} // namespace blink

#endif // LayoutTableSection_h
