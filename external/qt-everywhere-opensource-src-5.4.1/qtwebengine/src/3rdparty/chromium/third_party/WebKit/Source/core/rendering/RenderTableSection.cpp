/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008, 2009, 2010, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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

#include "config.h"
#include "core/rendering/RenderTableSection.h"

#include <limits>
#include "core/rendering/GraphicsContextAnnotator.h"
#include "core/rendering/HitTestResult.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderTableCell.h"
#include "core/rendering/RenderTableCol.h"
#include "core/rendering/RenderTableRow.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/SubtreeLayoutScope.h"
#include "wtf/HashSet.h"

using namespace std;

namespace WebCore {

using namespace HTMLNames;

// Those 2 variables are used to balance the memory consumption vs the repaint time on big tables.
static unsigned gMinTableSizeToUseFastPaintPathWithOverflowingCell = 75 * 75;
static float gMaxAllowedOverflowingCellRatioForFastPaintPath = 0.1f;

static inline void setRowLogicalHeightToRowStyleLogicalHeight(RenderTableSection::RowStruct& row)
{
    ASSERT(row.rowRenderer);
    row.logicalHeight = row.rowRenderer->style()->logicalHeight();
}

static inline void updateLogicalHeightForCell(RenderTableSection::RowStruct& row, const RenderTableCell* cell)
{
    // We ignore height settings on rowspan cells.
    if (cell->rowSpan() != 1)
        return;

    Length logicalHeight = cell->style()->logicalHeight();
    if (logicalHeight.isPositive()) {
        Length cRowLogicalHeight = row.logicalHeight;
        switch (logicalHeight.type()) {
        case Percent:
            if (!(cRowLogicalHeight.isPercent())
                || (cRowLogicalHeight.isPercent() && cRowLogicalHeight.percent() < logicalHeight.percent()))
                row.logicalHeight = logicalHeight;
            break;
        case Fixed:
            if (cRowLogicalHeight.type() < Percent
                || (cRowLogicalHeight.isFixed() && cRowLogicalHeight.value() < logicalHeight.value()))
                row.logicalHeight = logicalHeight;
            break;
        default:
            break;
        }
    }
}


RenderTableSection::RenderTableSection(Element* element)
    : RenderBox(element)
    , m_cCol(0)
    , m_cRow(0)
    , m_outerBorderStart(0)
    , m_outerBorderEnd(0)
    , m_outerBorderBefore(0)
    , m_outerBorderAfter(0)
    , m_needsCellRecalc(false)
    , m_hasMultipleCellLevels(false)
{
    // init RenderObject attributes
    setInline(false); // our object is not Inline
}

RenderTableSection::~RenderTableSection()
{
}

void RenderTableSection::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBox::styleDidChange(diff, oldStyle);
    propagateStyleToAnonymousChildren();

    // If border was changed, notify table.
    RenderTable* table = this->table();
    if (table && !table->selfNeedsLayout() && !table->normalChildNeedsLayout() && oldStyle && oldStyle->border() != style()->border())
        table->invalidateCollapsedBorders();
}

void RenderTableSection::willBeRemovedFromTree()
{
    RenderBox::willBeRemovedFromTree();

    // Preventively invalidate our cells as we may be re-inserted into
    // a new table which would require us to rebuild our structure.
    setNeedsCellRecalc();
}

void RenderTableSection::addChild(RenderObject* child, RenderObject* beforeChild)
{
    if (!child->isTableRow()) {
        RenderObject* last = beforeChild;
        if (!last)
            last = lastRow();
        if (last && last->isAnonymous() && !last->isBeforeOrAfterContent()) {
            if (beforeChild == last)
                beforeChild = last->slowFirstChild();
            last->addChild(child, beforeChild);
            return;
        }

        if (beforeChild && !beforeChild->isAnonymous() && beforeChild->parent() == this) {
            RenderObject* row = beforeChild->previousSibling();
            if (row && row->isTableRow() && row->isAnonymous()) {
                row->addChild(child);
                return;
            }
        }

        // If beforeChild is inside an anonymous cell/row, insert into the cell or into
        // the anonymous row containing it, if there is one.
        RenderObject* lastBox = last;
        while (lastBox && lastBox->parent()->isAnonymous() && !lastBox->isTableRow())
            lastBox = lastBox->parent();
        if (lastBox && lastBox->isAnonymous() && !lastBox->isBeforeOrAfterContent()) {
            lastBox->addChild(child, beforeChild);
            return;
        }

        RenderObject* row = RenderTableRow::createAnonymousWithParentRenderer(this);
        addChild(row, beforeChild);
        row->addChild(child);
        return;
    }

    if (beforeChild)
        setNeedsCellRecalc();

    unsigned insertionRow = m_cRow;
    ++m_cRow;
    m_cCol = 0;

    ensureRows(m_cRow);

    RenderTableRow* row = toRenderTableRow(child);
    m_grid[insertionRow].rowRenderer = row;
    row->setRowIndex(insertionRow);

    if (!beforeChild)
        setRowLogicalHeightToRowStyleLogicalHeight(m_grid[insertionRow]);

    if (beforeChild && beforeChild->parent() != this)
        beforeChild = splitAnonymousBoxesAroundChild(beforeChild);

    ASSERT(!beforeChild || beforeChild->isTableRow());
    RenderBox::addChild(child, beforeChild);
}

void RenderTableSection::ensureRows(unsigned numRows)
{
    if (numRows <= m_grid.size())
        return;

    unsigned oldSize = m_grid.size();
    m_grid.grow(numRows);

    unsigned effectiveColumnCount = max(1u, table()->numEffCols());
    for (unsigned row = oldSize; row < m_grid.size(); ++row)
        m_grid[row].row.grow(effectiveColumnCount);
}

void RenderTableSection::addCell(RenderTableCell* cell, RenderTableRow* row)
{
    // We don't insert the cell if we need cell recalc as our internal columns' representation
    // will have drifted from the table's representation. Also recalcCells will call addCell
    // at a later time after sync'ing our columns' with the table's.
    if (needsCellRecalc())
        return;

    unsigned rSpan = cell->rowSpan();
    unsigned cSpan = cell->colSpan();
    const Vector<RenderTable::ColumnStruct>& columns = table()->columns();
    unsigned nCols = columns.size();
    unsigned insertionRow = row->rowIndex();

    // ### mozilla still seems to do the old HTML way, even for strict DTD
    // (see the annotation on table cell layouting in the CSS specs and the testcase below:
    // <TABLE border>
    // <TR><TD>1 <TD rowspan="2">2 <TD>3 <TD>4
    // <TR><TD colspan="2">5
    // </TABLE>
    while (m_cCol < nCols && (cellAt(insertionRow, m_cCol).hasCells() || cellAt(insertionRow, m_cCol).inColSpan))
        m_cCol++;

    updateLogicalHeightForCell(m_grid[insertionRow], cell);

    ensureRows(insertionRow + rSpan);

    m_grid[insertionRow].rowRenderer = row;

    unsigned col = m_cCol;
    // tell the cell where it is
    bool inColSpan = false;
    while (cSpan) {
        unsigned currentSpan;
        if (m_cCol >= nCols) {
            table()->appendColumn(cSpan);
            currentSpan = cSpan;
        } else {
            if (cSpan < columns[m_cCol].span)
                table()->splitColumn(m_cCol, cSpan);
            currentSpan = columns[m_cCol].span;
        }
        for (unsigned r = 0; r < rSpan; r++) {
            CellStruct& c = cellAt(insertionRow + r, m_cCol);
            ASSERT(cell);
            c.cells.append(cell);
            // If cells overlap then we take the slow path for painting.
            if (c.cells.size() > 1)
                m_hasMultipleCellLevels = true;
            if (inColSpan)
                c.inColSpan = true;
        }
        m_cCol++;
        cSpan -= currentSpan;
        inColSpan = true;
    }
    cell->setCol(table()->effColToCol(col));
}

bool RenderTableSection::rowHasOnlySpanningCells(unsigned row)
{
    unsigned totalCols = m_grid[row].row.size();

    if (!totalCols)
        return false;

    for (unsigned col = 0; col < totalCols; col++) {
        const CellStruct& rowSpanCell = cellAt(row, col);

        // Empty cell is not a valid cell so it is not a rowspan cell.
        if (rowSpanCell.cells.isEmpty())
            return false;

        if (rowSpanCell.cells[0]->rowSpan() == 1)
            return false;
    }

    return true;
}

void RenderTableSection::populateSpanningRowsHeightFromCell(RenderTableCell* cell, struct SpanningRowsHeight& spanningRowsHeight)
{
    const unsigned rowSpan = cell->rowSpan();
    const unsigned rowIndex = cell->rowIndex();

    spanningRowsHeight.spanningCellHeightIgnoringBorderSpacing = cell->logicalHeightForRowSizing();

    spanningRowsHeight.rowHeight.resize(rowSpan);
    spanningRowsHeight.totalRowsHeight = 0;
    for (unsigned row = 0; row < rowSpan; row++) {
        unsigned actualRow = row + rowIndex;

        spanningRowsHeight.rowHeight[row] = m_rowPos[actualRow + 1] - m_rowPos[actualRow] - borderSpacingForRow(actualRow);
        if (!spanningRowsHeight.rowHeight[row])
            spanningRowsHeight.isAnyRowWithOnlySpanningCells |= rowHasOnlySpanningCells(actualRow);

        spanningRowsHeight.totalRowsHeight += spanningRowsHeight.rowHeight[row];
        spanningRowsHeight.spanningCellHeightIgnoringBorderSpacing -= borderSpacingForRow(actualRow);
    }
    // We don't span the following row so its border-spacing (if any) should be included.
    spanningRowsHeight.spanningCellHeightIgnoringBorderSpacing += borderSpacingForRow(rowIndex + rowSpan - 1);
}

void RenderTableSection::distributeExtraRowSpanHeightToPercentRows(RenderTableCell* cell, int totalPercent, int& extraRowSpanningHeight, Vector<int>& rowsHeight)
{
    if (!extraRowSpanningHeight || !totalPercent)
        return;

    const unsigned rowSpan = cell->rowSpan();
    const unsigned rowIndex = cell->rowIndex();
    int percent = min(totalPercent, 100);
    const int tableHeight = m_rowPos[m_grid.size()] + extraRowSpanningHeight;

    // Our algorithm matches Firefox. Extra spanning height would be distributed Only in first percent height rows
    // those total percent is 100. Other percent rows would be uneffected even extra spanning height is remain.
    int accumulatedPositionIncrease = 0;
    for (unsigned row = rowIndex; row < (rowIndex + rowSpan); row++) {
        if (percent > 0 && extraRowSpanningHeight > 0) {
            if (m_grid[row].logicalHeight.isPercent()) {
                int toAdd = (tableHeight * m_grid[row].logicalHeight.percent() / 100) - rowsHeight[row - rowIndex];
                // FIXME: Note that this is wrong if we have a percentage above 100% and may make us grow
                // above the available space.

                toAdd = min(toAdd, extraRowSpanningHeight);
                accumulatedPositionIncrease += toAdd;
                extraRowSpanningHeight -= toAdd;
                percent -= m_grid[row].logicalHeight.percent();
            }
        }
        m_rowPos[row + 1] += accumulatedPositionIncrease;
    }
}

// Sometimes the multiplication of the 2 values below will overflow an integer.
// So we convert the parameters to 'long long' instead of 'int' to avoid the
// problem in this function.
static void updatePositionIncreasedWithRowHeight(long long extraHeight, long long rowHeight, long long totalHeight, int& accumulatedPositionIncrease, int& remainder)
{
    COMPILE_ASSERT(sizeof(long long int) > sizeof(int), int_should_be_less_than_longlong);

    accumulatedPositionIncrease += (extraHeight * rowHeight) / totalHeight;
    remainder += (extraHeight * rowHeight) % totalHeight;
}

void RenderTableSection::distributeExtraRowSpanHeightToAutoRows(RenderTableCell* cell, int totalAutoRowsHeight, int& extraRowSpanningHeight, Vector<int>& rowsHeight)
{
    if (!extraRowSpanningHeight || !totalAutoRowsHeight)
        return;

    const unsigned rowSpan = cell->rowSpan();
    const unsigned rowIndex = cell->rowIndex();
    int accumulatedPositionIncrease = 0;
    int remainder = 0;

    // Aspect ratios of auto rows should not change otherwise table may look different than user expected.
    // So extra height distributed in auto spanning rows based on their weight in spanning cell.
    for (unsigned row = rowIndex; row < (rowIndex + rowSpan); row++) {
        if (m_grid[row].logicalHeight.isAuto()) {
            updatePositionIncreasedWithRowHeight(extraRowSpanningHeight, rowsHeight[row - rowIndex], totalAutoRowsHeight, accumulatedPositionIncrease, remainder);

            // While whole extra spanning height is distributing in auto spanning rows, rational parts remains
            // in every integer division. So accumulating all remainder part in integer division and when total remainder
            // is equvalent to divisor then 1 unit increased in row position.
            // Note that this algorithm is biased towards adding more space towards the lower rows.
            if (remainder >= totalAutoRowsHeight) {
                remainder -= totalAutoRowsHeight;
                accumulatedPositionIncrease++;
            }
        }
        m_rowPos[row + 1] += accumulatedPositionIncrease;
    }

    ASSERT(!remainder);

    extraRowSpanningHeight -= accumulatedPositionIncrease;
}

void RenderTableSection::distributeExtraRowSpanHeightToRemainingRows(RenderTableCell* cell, int totalRemainingRowsHeight, int& extraRowSpanningHeight, Vector<int>& rowsHeight)
{
    if (!extraRowSpanningHeight || !totalRemainingRowsHeight)
        return;

    const unsigned rowSpan = cell->rowSpan();
    const unsigned rowIndex = cell->rowIndex();
    int accumulatedPositionIncrease = 0;
    int remainder = 0;

    // Aspect ratios of the rows should not change otherwise table may look different than user expected.
    // So extra height distribution in remaining spanning rows based on their weight in spanning cell.
    for (unsigned row = rowIndex; row < (rowIndex + rowSpan); row++) {
        if (!m_grid[row].logicalHeight.isPercent()) {
            updatePositionIncreasedWithRowHeight(extraRowSpanningHeight, rowsHeight[row - rowIndex], totalRemainingRowsHeight, accumulatedPositionIncrease, remainder);

            // While whole extra spanning height is distributing in remaining spanning rows, rational parts remains
            // in every integer division. So accumulating all remainder part in integer division and when total remainder
            // is equvalent to divisor then 1 unit increased in row position.
            // Note that this algorithm is biased towards adding more space towards the lower rows.
            if (remainder >= totalRemainingRowsHeight) {
                remainder -= totalRemainingRowsHeight;
                accumulatedPositionIncrease++;
            }
        }
        m_rowPos[row + 1] += accumulatedPositionIncrease;
    }

    ASSERT(!remainder);

    extraRowSpanningHeight -= accumulatedPositionIncrease;
}

static bool cellIsFullyIncludedInOtherCell(const RenderTableCell* cell1, const RenderTableCell* cell2)
{
    return (cell1->rowIndex() >= cell2->rowIndex() && (cell1->rowIndex() + cell1->rowSpan()) <= (cell2->rowIndex() + cell2->rowSpan()));
}

// To avoid unneeded extra height distributions, we apply the following sorting algorithm:
static bool compareRowSpanCellsInHeightDistributionOrder(const RenderTableCell* cell1, const RenderTableCell* cell2)
{
    // Sorting bigger height cell first if cells are at same index with same span because we will skip smaller
    // height cell to distribute it's extra height.
    if (cell1->rowIndex() == cell2->rowIndex() && cell1->rowSpan() == cell2->rowSpan())
        return (cell1->logicalHeightForRowSizing() > cell2->logicalHeightForRowSizing());
    // Sorting inner most cell first because if inner spanning cell'e extra height is distributed then outer
    // spanning cell's extra height will adjust accordingly. In reverse order, there is more chances that outer
    // spanning cell's height will exceed than defined by user.
    if (cellIsFullyIncludedInOtherCell(cell1, cell2))
        return true;
    // Sorting lower row index first because first we need to apply the extra height of spanning cell which
    // comes first in the table so lower rows's position would increment in sequence.
    if (!cellIsFullyIncludedInOtherCell(cell2, cell1))
        return (cell1->rowIndex() < cell2->rowIndex());

    return false;
}

bool RenderTableSection::isHeightNeededForRowHavingOnlySpanningCells(unsigned row)
{
    unsigned totalCols = m_grid[row].row.size();

    if (!totalCols)
        return false;

    for (unsigned col = 0; col < totalCols; col++) {
        const CellStruct& rowSpanCell = cellAt(row, col);

        if (rowSpanCell.cells.size()) {
            RenderTableCell* cell = rowSpanCell.cells[0];
            const unsigned rowIndex = cell->rowIndex();
            const unsigned rowSpan = cell->rowSpan();
            int totalRowSpanCellHeight = 0;

            for (unsigned row = 0; row < rowSpan; row++) {
                unsigned actualRow = row + rowIndex;
                totalRowSpanCellHeight += m_rowPos[actualRow + 1] - m_rowPos[actualRow];
            }
            totalRowSpanCellHeight -= borderSpacingForRow(rowIndex + rowSpan - 1);

            if (totalRowSpanCellHeight < cell->logicalHeightForRowSizing())
                return true;
        }
    }

    return false;
}

unsigned RenderTableSection::calcRowHeightHavingOnlySpanningCells(unsigned row)
{
    ASSERT(rowHasOnlySpanningCells(row));

    unsigned totalCols = m_grid[row].row.size();

    if (!totalCols)
        return 0;

    unsigned rowHeight = 0;

    for (unsigned col = 0; col < totalCols; col++) {
        const CellStruct& rowSpanCell = cellAt(row, col);
        if (rowSpanCell.cells.size() && rowSpanCell.cells[0]->rowSpan() > 1)
            rowHeight = max(rowHeight, rowSpanCell.cells[0]->logicalHeightForRowSizing() / rowSpanCell.cells[0]->rowSpan());
    }

    return rowHeight;
}

void RenderTableSection::updateRowsHeightHavingOnlySpanningCells(RenderTableCell* cell, struct SpanningRowsHeight& spanningRowsHeight)
{
    ASSERT(spanningRowsHeight.rowHeight.size());

    int accumulatedPositionIncrease = 0;
    const unsigned rowSpan = cell->rowSpan();
    const unsigned rowIndex = cell->rowIndex();

    ASSERT_UNUSED(rowSpan, rowSpan == spanningRowsHeight.rowHeight.size());

    for (unsigned row = 0; row < spanningRowsHeight.rowHeight.size(); row++) {
        unsigned actualRow = row + rowIndex;
        if (!spanningRowsHeight.rowHeight[row] && rowHasOnlySpanningCells(actualRow) && isHeightNeededForRowHavingOnlySpanningCells(actualRow)) {
            spanningRowsHeight.rowHeight[row] = calcRowHeightHavingOnlySpanningCells(actualRow);
            accumulatedPositionIncrease += spanningRowsHeight.rowHeight[row];
        }
        m_rowPos[actualRow + 1] += accumulatedPositionIncrease;
    }

    spanningRowsHeight.totalRowsHeight += accumulatedPositionIncrease;
}

// Distribute rowSpan cell height in rows those comes in rowSpan cell based on the ratio of row's height if
// 1. RowSpan cell height is greater then the total height of rows in rowSpan cell
void RenderTableSection::distributeRowSpanHeightToRows(SpanningRenderTableCells& rowSpanCells)
{
    ASSERT(rowSpanCells.size());

    // 'rowSpanCells' list is already sorted based on the cells rowIndex in ascending order
    // Arrange row spanning cell in the order in which we need to process first.
    std::sort(rowSpanCells.begin(), rowSpanCells.end(), compareRowSpanCellsInHeightDistributionOrder);

    unsigned extraHeightToPropagate = 0;
    unsigned lastRowIndex = 0;
    unsigned lastRowSpan = 0;

    for (unsigned i = 0; i < rowSpanCells.size(); i++) {
        RenderTableCell* cell = rowSpanCells[i];

        unsigned rowIndex = cell->rowIndex();

        unsigned rowSpan = cell->rowSpan();

        unsigned spanningCellEndIndex = rowIndex + rowSpan;
        unsigned lastSpanningCellEndIndex = lastRowIndex + lastRowSpan;

        // Only heightest spanning cell will distribute it's extra height in row if more then one spanning cells
        // present at same level.
        if (rowIndex == lastRowIndex && rowSpan == lastRowSpan)
            continue;

        int originalBeforePosition = m_rowPos[spanningCellEndIndex];

        // When 2 spanning cells are ending at same row index then while extra height distribution of first spanning
        // cell updates position of the last row so getting the original position of the last row in second spanning
        // cell need to reduce the height changed by first spanning cell.
        if (spanningCellEndIndex == lastSpanningCellEndIndex)
            originalBeforePosition -= extraHeightToPropagate;

        if (extraHeightToPropagate) {
            for (unsigned row = lastSpanningCellEndIndex + 1; row <= spanningCellEndIndex; row++)
                m_rowPos[row] += extraHeightToPropagate;
        }

        lastRowIndex = rowIndex;
        lastRowSpan = rowSpan;

        struct SpanningRowsHeight spanningRowsHeight;

        populateSpanningRowsHeightFromCell(cell, spanningRowsHeight);

        // Here we are handling only row(s) who have only rowspanning cells and do not have any empty cell.
        if (spanningRowsHeight.isAnyRowWithOnlySpanningCells)
            updateRowsHeightHavingOnlySpanningCells(cell, spanningRowsHeight);

        // This code handle row(s) that have rowspanning cell(s) and at least one empty cell.
        // Such rows are not handled below and end up having a height of 0. That would mean
        // content overlapping if one of their cells has any content. To avoid the problem, we
        // add all the remaining spanning cells' height to the last spanned row.
        // This means that we could grow a row past its 'height' or break percentage spreading
        // however this is better than overlapping content.
        // FIXME: Is there a better algorithm?
        if (!spanningRowsHeight.totalRowsHeight) {
            if (spanningRowsHeight.spanningCellHeightIgnoringBorderSpacing)
                m_rowPos[spanningCellEndIndex] += spanningRowsHeight.spanningCellHeightIgnoringBorderSpacing + borderSpacingForRow(spanningCellEndIndex - 1);

            extraHeightToPropagate = m_rowPos[spanningCellEndIndex] - originalBeforePosition;
            continue;
        }

        if (spanningRowsHeight.spanningCellHeightIgnoringBorderSpacing <= spanningRowsHeight.totalRowsHeight) {
            extraHeightToPropagate = m_rowPos[rowIndex + rowSpan] - originalBeforePosition;
            continue;
        }

        // Below we are handling only row(s) who have at least one visible cell without rowspan value.
        int totalPercent = 0;
        int totalAutoRowsHeight = 0;
        int totalRemainingRowsHeight = spanningRowsHeight.totalRowsHeight;

        // FIXME: Inner spanning cell height should not change if it have fixed height when it's parent spanning cell
        // is distributing it's extra height in rows.

        // Calculate total percentage, total auto rows height and total rows height except percent rows.
        for (unsigned row = rowIndex; row < spanningCellEndIndex; row++) {
            if (m_grid[row].logicalHeight.isPercent()) {
                totalPercent += m_grid[row].logicalHeight.percent();
                totalRemainingRowsHeight -= spanningRowsHeight.rowHeight[row - rowIndex];
            } else if (m_grid[row].logicalHeight.isAuto()) {
                totalAutoRowsHeight += spanningRowsHeight.rowHeight[row - rowIndex];
            }
        }

        int extraRowSpanningHeight = spanningRowsHeight.spanningCellHeightIgnoringBorderSpacing - spanningRowsHeight.totalRowsHeight;

        distributeExtraRowSpanHeightToPercentRows(cell, totalPercent, extraRowSpanningHeight, spanningRowsHeight.rowHeight);
        distributeExtraRowSpanHeightToAutoRows(cell, totalAutoRowsHeight, extraRowSpanningHeight, spanningRowsHeight.rowHeight);
        distributeExtraRowSpanHeightToRemainingRows(cell, totalRemainingRowsHeight, extraRowSpanningHeight, spanningRowsHeight.rowHeight);

        ASSERT(!extraRowSpanningHeight);

        // Getting total changed height in the table
        extraHeightToPropagate = m_rowPos[spanningCellEndIndex] - originalBeforePosition;
    }

    if (extraHeightToPropagate) {
        // Apply changed height by rowSpan cells to rows present at the end of the table
        for (unsigned row = lastRowIndex + lastRowSpan + 1; row <= m_grid.size(); row++)
            m_rowPos[row] += extraHeightToPropagate;
    }
}

// Find out the baseline of the cell
// If the cell's baseline is more then the row's baseline then the cell's baseline become the row's baseline
// and if the row's baseline goes out of the row's boundries then adjust row height accordingly.
void RenderTableSection::updateBaselineForCell(RenderTableCell* cell, unsigned row, LayoutUnit& baselineDescent)
{
    if (!cell->isBaselineAligned())
        return;

    // Ignoring the intrinsic padding as it depends on knowing the row's baseline, which won't be accurate
    // until the end of this function.
    LayoutUnit baselinePosition = cell->cellBaselinePosition() - cell->intrinsicPaddingBefore();
    if (baselinePosition > cell->borderBefore() + (cell->paddingBefore() - cell->intrinsicPaddingBefore())) {
        m_grid[row].baseline = max(m_grid[row].baseline, baselinePosition);

        int cellStartRowBaselineDescent = 0;
        if (cell->rowSpan() == 1) {
            baselineDescent = max(baselineDescent, cell->logicalHeightForRowSizing() - baselinePosition);
            cellStartRowBaselineDescent = baselineDescent;
        }
        m_rowPos[row + 1] = max<int>(m_rowPos[row + 1], m_rowPos[row] + m_grid[row].baseline + cellStartRowBaselineDescent);
    }
}

int RenderTableSection::calcRowLogicalHeight()
{
#ifndef NDEBUG
    SetLayoutNeededForbiddenScope layoutForbiddenScope(*this);
#endif

    ASSERT(!needsLayout());

    RenderTableCell* cell;

    // FIXME: This shouldn't use the same constructor as RenderView.
    LayoutState state(*this);

    m_rowPos.resize(m_grid.size() + 1);

    // We ignore the border-spacing on any non-top section as it is already included in the previous section's last row position.
    if (this == table()->topSection())
        m_rowPos[0] = table()->vBorderSpacing();
    else
        m_rowPos[0] = 0;

    SpanningRenderTableCells rowSpanCells;
#ifndef NDEBUG
    HashSet<const RenderTableCell*> uniqueCells;
#endif

    for (unsigned r = 0; r < m_grid.size(); r++) {
        m_grid[r].baseline = 0;
        LayoutUnit baselineDescent = 0;

        // Our base size is the biggest logical height from our cells' styles (excluding row spanning cells).
        m_rowPos[r + 1] = max(m_rowPos[r] + minimumValueForLength(m_grid[r].logicalHeight, 0).round(), 0);

        Row& row = m_grid[r].row;
        unsigned totalCols = row.size();
        RenderTableCell* lastRowSpanCell = 0;

        for (unsigned c = 0; c < totalCols; c++) {
            CellStruct& current = cellAt(r, c);
            for (unsigned i = 0; i < current.cells.size(); i++) {
                cell = current.cells[i];
                if (current.inColSpan && cell->rowSpan() == 1)
                    continue;

                if (cell->rowSpan() > 1) {
                    // For row spanning cells, we only handle them for the first row they span. This ensures we take their baseline into account.
                    if (lastRowSpanCell != cell && cell->rowIndex() == r) {
#ifndef NDEBUG
                        ASSERT(!uniqueCells.contains(cell));
                        uniqueCells.add(cell);
#endif

                        rowSpanCells.append(cell);
                        lastRowSpanCell = cell;

                        // Find out the baseline. The baseline is set on the first row in a rowSpan.
                        updateBaselineForCell(cell, r, baselineDescent);
                    }
                    continue;
                }

                ASSERT(cell->rowSpan() == 1);

                if (cell->hasOverrideHeight()) {
                    cell->clearIntrinsicPadding();
                    cell->clearOverrideSize();
                    cell->forceChildLayout();
                }

                m_rowPos[r + 1] = max(m_rowPos[r + 1], m_rowPos[r] + cell->logicalHeightForRowSizing());

                // Find out the baseline.
                updateBaselineForCell(cell, r, baselineDescent);
            }
        }

        // Add the border-spacing to our final position.
        m_rowPos[r + 1] += borderSpacingForRow(r);
        m_rowPos[r + 1] = max(m_rowPos[r + 1], m_rowPos[r]);
    }

    if (!rowSpanCells.isEmpty())
        distributeRowSpanHeightToRows(rowSpanCells);

    ASSERT(!needsLayout());

    return m_rowPos[m_grid.size()];
}

void RenderTableSection::layout()
{
    ASSERT(needsLayout());
    ASSERT(!needsCellRecalc());
    ASSERT(!table()->needsSectionRecalc());

    // addChild may over-grow m_grid but we don't want to throw away the memory too early as addChild
    // can be called in a loop (e.g during parsing). Doing it now ensures we have a stable-enough structure.
    m_grid.shrinkToFit();

    LayoutState state(*this, locationOffset());

    const Vector<int>& columnPos = table()->columnPositions();

    SubtreeLayoutScope layouter(*this);
    for (unsigned r = 0; r < m_grid.size(); ++r) {
        Row& row = m_grid[r].row;
        unsigned cols = row.size();
        // First, propagate our table layout's information to the cells. This will mark the row as needing layout
        // if there was a column logical width change.
        for (unsigned startColumn = 0; startColumn < cols; ++startColumn) {
            CellStruct& current = row[startColumn];
            RenderTableCell* cell = current.primaryCell();
            if (!cell || current.inColSpan)
                continue;

            unsigned endCol = startColumn;
            unsigned cspan = cell->colSpan();
            while (cspan && endCol < cols) {
                ASSERT(endCol < table()->columns().size());
                cspan -= table()->columns()[endCol].span;
                endCol++;
            }
            int tableLayoutLogicalWidth = columnPos[endCol] - columnPos[startColumn] - table()->hBorderSpacing();
            cell->setCellLogicalWidth(tableLayoutLogicalWidth, layouter);
        }

        if (RenderTableRow* rowRenderer = m_grid[r].rowRenderer) {
            if (!rowRenderer->needsLayout())
                rowRenderer->markForPaginationRelayoutIfNeeded(layouter);
            rowRenderer->layoutIfNeeded();
        }
    }

    clearNeedsLayout();
}

void RenderTableSection::distributeExtraLogicalHeightToPercentRows(int& extraLogicalHeight, int totalPercent)
{
    if (!totalPercent)
        return;

    unsigned totalRows = m_grid.size();
    int totalHeight = m_rowPos[totalRows] + extraLogicalHeight;
    int totalLogicalHeightAdded = 0;
    totalPercent = min(totalPercent, 100);
    int rowHeight = m_rowPos[1] - m_rowPos[0];
    for (unsigned r = 0; r < totalRows; ++r) {
        if (totalPercent > 0 && m_grid[r].logicalHeight.isPercent()) {
            int toAdd = min<int>(extraLogicalHeight, (totalHeight * m_grid[r].logicalHeight.percent() / 100) - rowHeight);
            // If toAdd is negative, then we don't want to shrink the row (this bug
            // affected Outlook Web Access).
            toAdd = max(0, toAdd);
            totalLogicalHeightAdded += toAdd;
            extraLogicalHeight -= toAdd;
            totalPercent -= m_grid[r].logicalHeight.percent();
        }
        ASSERT(totalRows >= 1);
        if (r < totalRows - 1)
            rowHeight = m_rowPos[r + 2] - m_rowPos[r + 1];
        m_rowPos[r + 1] += totalLogicalHeightAdded;
    }
}

void RenderTableSection::distributeExtraLogicalHeightToAutoRows(int& extraLogicalHeight, unsigned autoRowsCount)
{
    if (!autoRowsCount)
        return;

    int totalLogicalHeightAdded = 0;
    for (unsigned r = 0; r < m_grid.size(); ++r) {
        if (autoRowsCount > 0 && m_grid[r].logicalHeight.isAuto()) {
            // Recomputing |extraLogicalHeightForRow| guarantees that we properly ditribute round |extraLogicalHeight|.
            int extraLogicalHeightForRow = extraLogicalHeight / autoRowsCount;
            totalLogicalHeightAdded += extraLogicalHeightForRow;
            extraLogicalHeight -= extraLogicalHeightForRow;
            --autoRowsCount;
        }
        m_rowPos[r + 1] += totalLogicalHeightAdded;
    }
}

void RenderTableSection::distributeRemainingExtraLogicalHeight(int& extraLogicalHeight)
{
    unsigned totalRows = m_grid.size();

    if (extraLogicalHeight <= 0 || !m_rowPos[totalRows])
        return;

    // FIXME: m_rowPos[totalRows] - m_rowPos[0] is the total rows' size.
    int totalRowSize = m_rowPos[totalRows];
    int totalLogicalHeightAdded = 0;
    int previousRowPosition = m_rowPos[0];
    for (unsigned r = 0; r < totalRows; r++) {
        // weight with the original height
        totalLogicalHeightAdded += extraLogicalHeight * (m_rowPos[r + 1] - previousRowPosition) / totalRowSize;
        previousRowPosition = m_rowPos[r + 1];
        m_rowPos[r + 1] += totalLogicalHeightAdded;
    }

    extraLogicalHeight -= totalLogicalHeightAdded;
}

int RenderTableSection::distributeExtraLogicalHeightToRows(int extraLogicalHeight)
{
    if (!extraLogicalHeight)
        return extraLogicalHeight;

    unsigned totalRows = m_grid.size();
    if (!totalRows)
        return extraLogicalHeight;

    if (!m_rowPos[totalRows] && nextSibling())
        return extraLogicalHeight;

    unsigned autoRowsCount = 0;
    int totalPercent = 0;
    for (unsigned r = 0; r < totalRows; r++) {
        if (m_grid[r].logicalHeight.isAuto())
            ++autoRowsCount;
        else if (m_grid[r].logicalHeight.isPercent())
            totalPercent += m_grid[r].logicalHeight.percent();
    }

    int remainingExtraLogicalHeight = extraLogicalHeight;
    distributeExtraLogicalHeightToPercentRows(remainingExtraLogicalHeight, totalPercent);
    distributeExtraLogicalHeightToAutoRows(remainingExtraLogicalHeight, autoRowsCount);
    distributeRemainingExtraLogicalHeight(remainingExtraLogicalHeight);
    return extraLogicalHeight - remainingExtraLogicalHeight;
}

static bool shouldFlexCellChild(RenderObject* cellDescendant)
{
    return cellDescendant->isReplaced() || (cellDescendant->isBox() && toRenderBox(cellDescendant)->scrollsOverflow());
}

void RenderTableSection::layoutRows()
{
#ifndef NDEBUG
    SetLayoutNeededForbiddenScope layoutForbiddenScope(*this);
#endif

    ASSERT(!needsLayout());

    // FIXME: Changing the height without a layout can change the overflow so it seems wrong.

    unsigned totalRows = m_grid.size();

    // Set the width of our section now.  The rows will also be this width.
    setLogicalWidth(table()->contentLogicalWidth());
    m_overflow.clear();
    m_overflowingCells.clear();
    m_forceSlowPaintPathWithOverflowingCell = false;

    int vspacing = table()->vBorderSpacing();
    unsigned nEffCols = table()->numEffCols();

    LayoutState state(*this, locationOffset());

    for (unsigned r = 0; r < totalRows; r++) {
        // Set the row's x/y position and width/height.
        if (RenderTableRow* rowRenderer = m_grid[r].rowRenderer) {
            rowRenderer->setLocation(LayoutPoint(0, m_rowPos[r]));
            rowRenderer->setLogicalWidth(logicalWidth());
            rowRenderer->setLogicalHeight(m_rowPos[r + 1] - m_rowPos[r] - vspacing);
            rowRenderer->updateLayerTransformAfterLayout();
            rowRenderer->clearAllOverflows();
            rowRenderer->addVisualEffectOverflow();
        }

        int rowHeightIncreaseForPagination = 0;

        for (unsigned c = 0; c < nEffCols; c++) {
            CellStruct& cs = cellAt(r, c);
            RenderTableCell* cell = cs.primaryCell();

            if (!cell || cs.inColSpan)
                continue;

            int rowIndex = cell->rowIndex();
            int rHeight = m_rowPos[rowIndex + cell->rowSpan()] - m_rowPos[rowIndex] - vspacing;

            // Force percent height children to lay themselves out again.
            // This will cause these children to grow to fill the cell.
            // FIXME: There is still more work to do here to fully match WinIE (should
            // it become necessary to do so).  In quirks mode, WinIE behaves like we
            // do, but it will clip the cells that spill out of the table section.  In
            // strict mode, Mozilla and WinIE both regrow the table to accommodate the
            // new height of the cell (thus letting the percentages cause growth one
            // time only).  We may also not be handling row-spanning cells correctly.
            //
            // Note also the oddity where replaced elements always flex, and yet blocks/tables do
            // not necessarily flex.  WinIE is crazy and inconsistent, and we can't hope to
            // match the behavior perfectly, but we'll continue to refine it as we discover new
            // bugs. :)
            bool cellChildrenFlex = false;
            bool flexAllChildren = cell->style()->logicalHeight().isFixed()
                || (!table()->style()->logicalHeight().isAuto() && rHeight != cell->logicalHeight());

            for (RenderObject* child = cell->firstChild(); child; child = child->nextSibling()) {
                if (!child->isText() && child->style()->logicalHeight().isPercent()
                    && (flexAllChildren || shouldFlexCellChild(child))
                    && (!child->isTable() || toRenderTable(child)->hasSections())) {
                    cellChildrenFlex = true;
                    break;
                }
            }

            if (!cellChildrenFlex) {
                if (TrackedRendererListHashSet* percentHeightDescendants = cell->percentHeightDescendants()) {
                    TrackedRendererListHashSet::iterator end = percentHeightDescendants->end();
                    for (TrackedRendererListHashSet::iterator it = percentHeightDescendants->begin(); it != end; ++it) {
                        if (flexAllChildren || shouldFlexCellChild(*it)) {
                            cellChildrenFlex = true;
                            break;
                        }
                    }
                }
            }

            if (cellChildrenFlex) {
                // Alignment within a cell is based off the calculated
                // height, which becomes irrelevant once the cell has
                // been resized based off its percentage.
                cell->setOverrideLogicalContentHeightFromRowHeight(rHeight);
                cell->forceChildLayout();

                // If the baseline moved, we may have to update the data for our row. Find out the new baseline.
                if (cell->isBaselineAligned()) {
                    LayoutUnit baseline = cell->cellBaselinePosition();
                    if (baseline > cell->borderBefore() + cell->paddingBefore())
                        m_grid[r].baseline = max(m_grid[r].baseline, baseline);
                }
            }

            SubtreeLayoutScope layouter(*cell);
            cell->computeIntrinsicPadding(rHeight, layouter);

            LayoutRect oldCellRect = cell->frameRect();

            setLogicalPositionForCell(cell, c);

            if (!cell->needsLayout())
                cell->markForPaginationRelayoutIfNeeded(layouter);

            cell->layoutIfNeeded();

            // FIXME: Make pagination work with vertical tables.
            if (view()->layoutState()->pageLogicalHeight() && cell->logicalHeight() != rHeight) {
                // FIXME: Pagination might have made us change size. For now just shrink or grow the cell to fit without doing a relayout.
                // We'll also do a basic increase of the row height to accommodate the cell if it's bigger, but this isn't quite right
                // either. It's at least stable though and won't result in an infinite # of relayouts that may never stabilize.
                LayoutUnit oldLogicalHeight = cell->logicalHeight();
                if (oldLogicalHeight > rHeight)
                    rowHeightIncreaseForPagination = max<int>(rowHeightIncreaseForPagination, oldLogicalHeight - rHeight);
                cell->setLogicalHeight(rHeight);
                cell->computeOverflow(oldLogicalHeight, false);
            }

            LayoutSize childOffset(cell->location() - oldCellRect.location());
            if (childOffset.width() || childOffset.height()) {
                if (!RuntimeEnabledFeatures::repaintAfterLayoutEnabled())
                    view()->addLayoutDelta(childOffset);

                // If the child moved, we have to repaint it as well as any floating/positioned
                // descendants. An exception is if we need a layout. In this case, we know we're going to
                // repaint ourselves (and the child) anyway.
                if (!table()->selfNeedsLayout() && cell->checkForPaintInvalidation()) {
                    if (RuntimeEnabledFeatures::repaintAfterLayoutEnabled())
                        cell->setMayNeedPaintInvalidation(true);
                    else
                        cell->repaintDuringLayoutIfMoved(oldCellRect);
                }
            }
        }
        if (rowHeightIncreaseForPagination) {
            for (unsigned rowIndex = r + 1; rowIndex <= totalRows; rowIndex++)
                m_rowPos[rowIndex] += rowHeightIncreaseForPagination;
            for (unsigned c = 0; c < nEffCols; ++c) {
                Vector<RenderTableCell*, 1>& cells = cellAt(r, c).cells;
                for (size_t i = 0; i < cells.size(); ++i) {
                    LayoutUnit oldLogicalHeight = cells[i]->logicalHeight();
                    cells[i]->setLogicalHeight(oldLogicalHeight + rowHeightIncreaseForPagination);
                    cells[i]->computeOverflow(oldLogicalHeight, false);
                }
            }
        }
    }

    ASSERT(!needsLayout());

    setLogicalHeight(m_rowPos[totalRows]);

    computeOverflowFromCells(totalRows, nEffCols);
}

void RenderTableSection::computeOverflowFromCells()
{
    unsigned totalRows = m_grid.size();
    unsigned nEffCols = table()->numEffCols();
    computeOverflowFromCells(totalRows, nEffCols);
}

void RenderTableSection::computeOverflowFromCells(unsigned totalRows, unsigned nEffCols)
{
    unsigned totalCellsCount = nEffCols * totalRows;
    unsigned maxAllowedOverflowingCellsCount = totalCellsCount < gMinTableSizeToUseFastPaintPathWithOverflowingCell ? 0 : gMaxAllowedOverflowingCellRatioForFastPaintPath * totalCellsCount;

#ifndef NDEBUG
    bool hasOverflowingCell = false;
#endif
    // Now that our height has been determined, add in overflow from cells.
    for (unsigned r = 0; r < totalRows; r++) {
        for (unsigned c = 0; c < nEffCols; c++) {
            CellStruct& cs = cellAt(r, c);
            RenderTableCell* cell = cs.primaryCell();
            if (!cell || cs.inColSpan)
                continue;
            if (r < totalRows - 1 && cell == primaryCellAt(r + 1, c))
                continue;
            addOverflowFromChild(cell);
#ifndef NDEBUG
            hasOverflowingCell |= cell->hasVisualOverflow();
#endif
            if (cell->hasVisualOverflow() && !m_forceSlowPaintPathWithOverflowingCell) {
                m_overflowingCells.add(cell);
                if (m_overflowingCells.size() > maxAllowedOverflowingCellsCount) {
                    // We need to set m_forcesSlowPaintPath only if there is a least one overflowing cells as the hit testing code rely on this information.
                    m_forceSlowPaintPathWithOverflowingCell = true;
                    // The slow path does not make any use of the overflowing cells info, don't hold on to the memory.
                    m_overflowingCells.clear();
                }
            }
        }
    }

    ASSERT(hasOverflowingCell == this->hasOverflowingCell());
}

int RenderTableSection::calcBlockDirectionOuterBorder(BlockBorderSide side) const
{
    unsigned totalCols = table()->numEffCols();
    if (!m_grid.size() || !totalCols)
        return 0;

    unsigned borderWidth = 0;

    const BorderValue& sb = side == BorderBefore ? style()->borderBefore() : style()->borderAfter();
    if (sb.style() == BHIDDEN)
        return -1;
    if (sb.style() > BHIDDEN)
        borderWidth = sb.width();

    const BorderValue& rb = side == BorderBefore ? firstRow()->style()->borderBefore() : lastRow()->style()->borderAfter();
    if (rb.style() == BHIDDEN)
        return -1;
    if (rb.style() > BHIDDEN && rb.width() > borderWidth)
        borderWidth = rb.width();

    bool allHidden = true;
    for (unsigned c = 0; c < totalCols; c++) {
        const CellStruct& current = cellAt(side == BorderBefore ? 0 : m_grid.size() - 1, c);
        if (current.inColSpan || !current.hasCells())
            continue;
        const RenderStyle* primaryCellStyle = current.primaryCell()->style();
        const BorderValue& cb = side == BorderBefore ? primaryCellStyle->borderBefore() : primaryCellStyle->borderAfter(); // FIXME: Make this work with perpendicular and flipped cells.
        // FIXME: Don't repeat for the same col group
        RenderTableCol* colGroup = table()->colElement(c);
        if (colGroup) {
            const BorderValue& gb = side == BorderBefore ? colGroup->style()->borderBefore() : colGroup->style()->borderAfter();
            if (gb.style() == BHIDDEN || cb.style() == BHIDDEN)
                continue;
            allHidden = false;
            if (gb.style() > BHIDDEN && gb.width() > borderWidth)
                borderWidth = gb.width();
            if (cb.style() > BHIDDEN && cb.width() > borderWidth)
                borderWidth = cb.width();
        } else {
            if (cb.style() == BHIDDEN)
                continue;
            allHidden = false;
            if (cb.style() > BHIDDEN && cb.width() > borderWidth)
                borderWidth = cb.width();
        }
    }
    if (allHidden)
        return -1;

    if (side == BorderAfter)
        borderWidth++; // Distribute rounding error
    return borderWidth / 2;
}

int RenderTableSection::calcInlineDirectionOuterBorder(InlineBorderSide side) const
{
    unsigned totalCols = table()->numEffCols();
    if (!m_grid.size() || !totalCols)
        return 0;
    unsigned colIndex = side == BorderStart ? 0 : totalCols - 1;

    unsigned borderWidth = 0;

    const BorderValue& sb = side == BorderStart ? style()->borderStart() : style()->borderEnd();
    if (sb.style() == BHIDDEN)
        return -1;
    if (sb.style() > BHIDDEN)
        borderWidth = sb.width();

    if (RenderTableCol* colGroup = table()->colElement(colIndex)) {
        const BorderValue& gb = side == BorderStart ? colGroup->style()->borderStart() : colGroup->style()->borderEnd();
        if (gb.style() == BHIDDEN)
            return -1;
        if (gb.style() > BHIDDEN && gb.width() > borderWidth)
            borderWidth = gb.width();
    }

    bool allHidden = true;
    for (unsigned r = 0; r < m_grid.size(); r++) {
        const CellStruct& current = cellAt(r, colIndex);
        if (!current.hasCells())
            continue;
        // FIXME: Don't repeat for the same cell
        const RenderStyle* primaryCellStyle = current.primaryCell()->style();
        const RenderStyle* primaryCellParentStyle = current.primaryCell()->parent()->style();
        const BorderValue& cb = side == BorderStart ? primaryCellStyle->borderStart() : primaryCellStyle->borderEnd(); // FIXME: Make this work with perpendicular and flipped cells.
        const BorderValue& rb = side == BorderStart ? primaryCellParentStyle->borderStart() : primaryCellParentStyle->borderEnd();
        if (cb.style() == BHIDDEN || rb.style() == BHIDDEN)
            continue;
        allHidden = false;
        if (cb.style() > BHIDDEN && cb.width() > borderWidth)
            borderWidth = cb.width();
        if (rb.style() > BHIDDEN && rb.width() > borderWidth)
            borderWidth = rb.width();
    }
    if (allHidden)
        return -1;

    if ((side == BorderStart) != table()->style()->isLeftToRightDirection())
        borderWidth++; // Distribute rounding error
    return borderWidth / 2;
}

void RenderTableSection::recalcOuterBorder()
{
    m_outerBorderBefore = calcBlockDirectionOuterBorder(BorderBefore);
    m_outerBorderAfter = calcBlockDirectionOuterBorder(BorderAfter);
    m_outerBorderStart = calcInlineDirectionOuterBorder(BorderStart);
    m_outerBorderEnd = calcInlineDirectionOuterBorder(BorderEnd);
}

int RenderTableSection::firstLineBoxBaseline() const
{
    if (!m_grid.size())
        return -1;

    int firstLineBaseline = m_grid[0].baseline;
    if (firstLineBaseline)
        return firstLineBaseline + m_rowPos[0];

    firstLineBaseline = -1;
    const Row& firstRow = m_grid[0].row;
    for (size_t i = 0; i < firstRow.size(); ++i) {
        const CellStruct& cs = firstRow.at(i);
        const RenderTableCell* cell = cs.primaryCell();
        // Only cells with content have a baseline
        if (cell && cell->contentLogicalHeight())
            firstLineBaseline = max<int>(firstLineBaseline, cell->logicalTop() + cell->paddingBefore() + cell->borderBefore() + cell->contentLogicalHeight());
    }

    return firstLineBaseline;
}

void RenderTableSection::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ANNOTATE_GRAPHICS_CONTEXT(paintInfo, this);

    ASSERT(!needsLayout());
    // avoid crashing on bugs that cause us to paint with dirty layout
    if (needsLayout())
        return;

    unsigned totalRows = m_grid.size();
    unsigned totalCols = table()->columns().size();

    if (!totalRows || !totalCols)
        return;

    LayoutPoint adjustedPaintOffset = paintOffset + location();

    PaintPhase phase = paintInfo.phase;
    bool pushedClip = pushContentsClip(paintInfo, adjustedPaintOffset, ForceContentsClip);
    paintObject(paintInfo, adjustedPaintOffset);
    if (pushedClip)
        popContentsClip(paintInfo, phase, adjustedPaintOffset);

    if ((phase == PaintPhaseOutline || phase == PaintPhaseSelfOutline) && style()->visibility() == VISIBLE)
        paintOutline(paintInfo, LayoutRect(adjustedPaintOffset, size()));
}

static inline bool compareCellPositions(RenderTableCell* elem1, RenderTableCell* elem2)
{
    return elem1->rowIndex() < elem2->rowIndex();
}

// This comparison is used only when we have overflowing cells as we have an unsorted array to sort. We thus need
// to sort both on rows and columns to properly repaint.
static inline bool compareCellPositionsWithOverflowingCells(RenderTableCell* elem1, RenderTableCell* elem2)
{
    if (elem1->rowIndex() != elem2->rowIndex())
        return elem1->rowIndex() < elem2->rowIndex();

    return elem1->col() < elem2->col();
}

void RenderTableSection::paintCell(RenderTableCell* cell, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    LayoutPoint cellPoint = flipForWritingModeForChild(cell, paintOffset);
    PaintPhase paintPhase = paintInfo.phase;
    RenderTableRow* row = toRenderTableRow(cell->parent());

    if (paintPhase == PaintPhaseBlockBackground || paintPhase == PaintPhaseChildBlockBackground) {
        // We need to handle painting a stack of backgrounds.  This stack (from bottom to top) consists of
        // the column group, column, row group, row, and then the cell.
        RenderTableCol* column = table()->colElement(cell->col());
        RenderTableCol* columnGroup = column ? column->enclosingColumnGroup() : 0;

        // Column groups and columns first.
        // FIXME: Columns and column groups do not currently support opacity, and they are being painted "too late" in
        // the stack, since we have already opened a transparency layer (potentially) for the table row group.
        // Note that we deliberately ignore whether or not the cell has a layer, since these backgrounds paint "behind" the
        // cell.
        cell->paintBackgroundsBehindCell(paintInfo, cellPoint, columnGroup);
        cell->paintBackgroundsBehindCell(paintInfo, cellPoint, column);

        // Paint the row group next.
        cell->paintBackgroundsBehindCell(paintInfo, cellPoint, this);

        // Paint the row next, but only if it doesn't have a layer.  If a row has a layer, it will be responsible for
        // painting the row background for the cell.
        if (!row->hasSelfPaintingLayer())
            cell->paintBackgroundsBehindCell(paintInfo, cellPoint, row);
    }
    if ((!cell->hasSelfPaintingLayer() && !row->hasSelfPaintingLayer()))
        cell->paint(paintInfo, cellPoint);
}

LayoutRect RenderTableSection::logicalRectForWritingModeAndDirection(const LayoutRect& rect) const
{
    LayoutRect tableAlignedRect(rect);

    flipForWritingMode(tableAlignedRect);

    if (!style()->isHorizontalWritingMode())
        tableAlignedRect = tableAlignedRect.transposedRect();

    const Vector<int>& columnPos = table()->columnPositions();
    // FIXME: The table's direction should determine our row's direction, not the section's (see bug 96691).
    if (!style()->isLeftToRightDirection())
        tableAlignedRect.setX(columnPos[columnPos.size() - 1] - tableAlignedRect.maxX());

    return tableAlignedRect;
}

CellSpan RenderTableSection::dirtiedRows(const LayoutRect& damageRect) const
{
    if (m_forceSlowPaintPathWithOverflowingCell)
        return fullTableRowSpan();

    CellSpan coveredRows = spannedRows(damageRect);

    // To repaint the border we might need to repaint first or last row even if they are not spanned themselves.
    if (coveredRows.start() >= m_rowPos.size() - 1 && m_rowPos[m_rowPos.size() - 1] + table()->outerBorderAfter() >= damageRect.y())
        --coveredRows.start();

    if (!coveredRows.end() && m_rowPos[0] - table()->outerBorderBefore() <= damageRect.maxY())
        ++coveredRows.end();

    return coveredRows;
}

CellSpan RenderTableSection::dirtiedColumns(const LayoutRect& damageRect) const
{
    if (m_forceSlowPaintPathWithOverflowingCell)
        return fullTableColumnSpan();

    CellSpan coveredColumns = spannedColumns(damageRect);

    const Vector<int>& columnPos = table()->columnPositions();
    // To repaint the border we might need to repaint first or last column even if they are not spanned themselves.
    if (coveredColumns.start() >= columnPos.size() - 1 && columnPos[columnPos.size() - 1] + table()->outerBorderEnd() >= damageRect.x())
        --coveredColumns.start();

    if (!coveredColumns.end() && columnPos[0] - table()->outerBorderStart() <= damageRect.maxX())
        ++coveredColumns.end();

    return coveredColumns;
}

CellSpan RenderTableSection::spannedRows(const LayoutRect& flippedRect) const
{
    // Find the first row that starts after rect top.
    unsigned nextRow = std::upper_bound(m_rowPos.begin(), m_rowPos.end(), flippedRect.y()) - m_rowPos.begin();

    if (nextRow == m_rowPos.size())
        return CellSpan(m_rowPos.size() - 1, m_rowPos.size() - 1); // After all rows.

    unsigned startRow = nextRow > 0 ? nextRow - 1 : 0;

    // Find the first row that starts after rect bottom.
    unsigned endRow;
    if (m_rowPos[nextRow] >= flippedRect.maxY())
        endRow = nextRow;
    else {
        endRow = std::upper_bound(m_rowPos.begin() + nextRow, m_rowPos.end(), flippedRect.maxY()) - m_rowPos.begin();
        if (endRow == m_rowPos.size())
            endRow = m_rowPos.size() - 1;
    }

    return CellSpan(startRow, endRow);
}

CellSpan RenderTableSection::spannedColumns(const LayoutRect& flippedRect) const
{
    const Vector<int>& columnPos = table()->columnPositions();

    // Find the first column that starts after rect left.
    // lower_bound doesn't handle the edge between two cells properly as it would wrongly return the
    // cell on the logical top/left.
    // upper_bound on the other hand properly returns the cell on the logical bottom/right, which also
    // matches the behavior of other browsers.
    unsigned nextColumn = std::upper_bound(columnPos.begin(), columnPos.end(), flippedRect.x()) - columnPos.begin();

    if (nextColumn == columnPos.size())
        return CellSpan(columnPos.size() - 1, columnPos.size() - 1); // After all columns.

    unsigned startColumn = nextColumn > 0 ? nextColumn - 1 : 0;

    // Find the first column that starts after rect right.
    unsigned endColumn;
    if (columnPos[nextColumn] >= flippedRect.maxX())
        endColumn = nextColumn;
    else {
        endColumn = std::upper_bound(columnPos.begin() + nextColumn, columnPos.end(), flippedRect.maxX()) - columnPos.begin();
        if (endColumn == columnPos.size())
            endColumn = columnPos.size() - 1;
    }

    return CellSpan(startColumn, endColumn);
}


void RenderTableSection::paintObject(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    LayoutRect localRepaintRect = paintInfo.rect;
    localRepaintRect.moveBy(-paintOffset);

    LayoutRect tableAlignedRect = logicalRectForWritingModeAndDirection(localRepaintRect);

    CellSpan dirtiedRows = this->dirtiedRows(tableAlignedRect);
    CellSpan dirtiedColumns = this->dirtiedColumns(tableAlignedRect);

    if (dirtiedColumns.start() < dirtiedColumns.end()) {
        if (!m_hasMultipleCellLevels && !m_overflowingCells.size()) {
            if (paintInfo.phase == PaintPhaseCollapsedTableBorders) {
                // Collapsed borders are painted from the bottom right to the top left so that precedence
                // due to cell position is respected.
                for (unsigned r = dirtiedRows.end(); r > dirtiedRows.start(); r--) {
                    unsigned row = r - 1;
                    for (unsigned c = dirtiedColumns.end(); c > dirtiedColumns.start(); c--) {
                        unsigned col = c - 1;
                        CellStruct& current = cellAt(row, col);
                        RenderTableCell* cell = current.primaryCell();
                        if (!cell || (row > dirtiedRows.start() && primaryCellAt(row - 1, col) == cell) || (col > dirtiedColumns.start() && primaryCellAt(row, col - 1) == cell))
                            continue;
                        LayoutPoint cellPoint = flipForWritingModeForChild(cell, paintOffset);
                        cell->paintCollapsedBorders(paintInfo, cellPoint);
                    }
                }
            } else {
                // Draw the dirty cells in the order that they appear.
                for (unsigned r = dirtiedRows.start(); r < dirtiedRows.end(); r++) {
                    RenderTableRow* row = m_grid[r].rowRenderer;
                    if (row && !row->hasSelfPaintingLayer())
                        row->paintOutlineForRowIfNeeded(paintInfo, paintOffset);
                    for (unsigned c = dirtiedColumns.start(); c < dirtiedColumns.end(); c++) {
                        CellStruct& current = cellAt(r, c);
                        RenderTableCell* cell = current.primaryCell();
                        if (!cell || (r > dirtiedRows.start() && primaryCellAt(r - 1, c) == cell) || (c > dirtiedColumns.start() && primaryCellAt(r, c - 1) == cell))
                            continue;
                        paintCell(cell, paintInfo, paintOffset);
                    }
                }
            }
        } else {
            // The overflowing cells should be scarce to avoid adding a lot of cells to the HashSet.
#ifndef NDEBUG
            unsigned totalRows = m_grid.size();
            unsigned totalCols = table()->columns().size();
            ASSERT(m_overflowingCells.size() < totalRows * totalCols * gMaxAllowedOverflowingCellRatioForFastPaintPath);
#endif

            // To make sure we properly repaint the section, we repaint all the overflowing cells that we collected.
            Vector<RenderTableCell*> cells;
            copyToVector(m_overflowingCells, cells);

            HashSet<RenderTableCell*> spanningCells;

            for (unsigned r = dirtiedRows.start(); r < dirtiedRows.end(); r++) {
                RenderTableRow* row = m_grid[r].rowRenderer;
                if (row && !row->hasSelfPaintingLayer())
                    row->paintOutlineForRowIfNeeded(paintInfo, paintOffset);
                for (unsigned c = dirtiedColumns.start(); c < dirtiedColumns.end(); c++) {
                    CellStruct& current = cellAt(r, c);
                    if (!current.hasCells())
                        continue;
                    for (unsigned i = 0; i < current.cells.size(); ++i) {
                        if (m_overflowingCells.contains(current.cells[i]))
                            continue;

                        if (current.cells[i]->rowSpan() > 1 || current.cells[i]->colSpan() > 1) {
                            if (!spanningCells.add(current.cells[i]).isNewEntry)
                                continue;
                        }

                        cells.append(current.cells[i]);
                    }
                }
            }

            // Sort the dirty cells by paint order.
            if (!m_overflowingCells.size())
                std::stable_sort(cells.begin(), cells.end(), compareCellPositions);
            else
                std::sort(cells.begin(), cells.end(), compareCellPositionsWithOverflowingCells);

            if (paintInfo.phase == PaintPhaseCollapsedTableBorders) {
                for (unsigned i = cells.size(); i > 0; --i) {
                    LayoutPoint cellPoint = flipForWritingModeForChild(cells[i - 1], paintOffset);
                    cells[i - 1]->paintCollapsedBorders(paintInfo, cellPoint);
                }
            } else {
                for (unsigned i = 0; i < cells.size(); ++i)
                    paintCell(cells[i], paintInfo, paintOffset);
            }
        }
    }
}

void RenderTableSection::imageChanged(WrappedImagePtr, const IntRect*)
{
    // FIXME: Examine cells and repaint only the rect the image paints in.
    paintInvalidationForWholeRenderer();
}

void RenderTableSection::recalcCells()
{
    ASSERT(m_needsCellRecalc);
    // We reset the flag here to ensure that |addCell| works. This is safe to do as
    // fillRowsWithDefaultStartingAtPosition makes sure we match the table's columns
    // representation.
    m_needsCellRecalc = false;

    m_cCol = 0;
    m_cRow = 0;
    m_grid.clear();

    for (RenderTableRow* row = firstRow(); row; row = row->nextRow()) {
        unsigned insertionRow = m_cRow;
        ++m_cRow;
        m_cCol = 0;
        ensureRows(m_cRow);

        m_grid[insertionRow].rowRenderer = row;
        row->setRowIndex(insertionRow);
        setRowLogicalHeightToRowStyleLogicalHeight(m_grid[insertionRow]);

        for (RenderTableCell* cell = row->firstCell(); cell; cell = cell->nextCell())
            addCell(cell, row);
    }

    m_grid.shrinkToFit();
    setNeedsLayoutAndFullPaintInvalidation();
}

// FIXME: This function could be made O(1) in certain cases (like for the non-most-constrainive cells' case).
void RenderTableSection::rowLogicalHeightChanged(unsigned rowIndex)
{
    if (needsCellRecalc())
        return;

    setRowLogicalHeightToRowStyleLogicalHeight(m_grid[rowIndex]);

    for (RenderTableCell* cell = m_grid[rowIndex].rowRenderer->firstCell(); cell; cell = cell->nextCell())
        updateLogicalHeightForCell(m_grid[rowIndex], cell);
}

void RenderTableSection::setNeedsCellRecalc()
{
    m_needsCellRecalc = true;
    if (RenderTable* t = table())
        t->setNeedsSectionRecalc();
}

unsigned RenderTableSection::numColumns() const
{
    unsigned result = 0;

    for (unsigned r = 0; r < m_grid.size(); ++r) {
        for (unsigned c = result; c < table()->numEffCols(); ++c) {
            const CellStruct& cell = cellAt(r, c);
            if (cell.hasCells() || cell.inColSpan)
                result = c;
        }
    }

    return result + 1;
}

const BorderValue& RenderTableSection::borderAdjoiningStartCell(const RenderTableCell* cell) const
{
    ASSERT(cell->isFirstOrLastCellInRow());
    return hasSameDirectionAs(cell) ? style()->borderStart() : style()->borderEnd();
}

const BorderValue& RenderTableSection::borderAdjoiningEndCell(const RenderTableCell* cell) const
{
    ASSERT(cell->isFirstOrLastCellInRow());
    return hasSameDirectionAs(cell) ? style()->borderEnd() : style()->borderStart();
}

const RenderTableCell* RenderTableSection::firstRowCellAdjoiningTableStart() const
{
    unsigned adjoiningStartCellColumnIndex = hasSameDirectionAs(table()) ? 0 : table()->lastColumnIndex();
    return cellAt(0, adjoiningStartCellColumnIndex).primaryCell();
}

const RenderTableCell* RenderTableSection::firstRowCellAdjoiningTableEnd() const
{
    unsigned adjoiningEndCellColumnIndex = hasSameDirectionAs(table()) ? table()->lastColumnIndex() : 0;
    return cellAt(0, adjoiningEndCellColumnIndex).primaryCell();
}

void RenderTableSection::appendColumn(unsigned pos)
{
    ASSERT(!m_needsCellRecalc);

    for (unsigned row = 0; row < m_grid.size(); ++row)
        m_grid[row].row.resize(pos + 1);
}

void RenderTableSection::splitColumn(unsigned pos, unsigned first)
{
    ASSERT(!m_needsCellRecalc);

    if (m_cCol > pos)
        m_cCol++;
    for (unsigned row = 0; row < m_grid.size(); ++row) {
        Row& r = m_grid[row].row;
        r.insert(pos + 1, CellStruct());
        if (r[pos].hasCells()) {
            r[pos + 1].cells.appendVector(r[pos].cells);
            RenderTableCell* cell = r[pos].primaryCell();
            ASSERT(cell);
            ASSERT(cell->colSpan() >= (r[pos].inColSpan ? 1u : 0));
            unsigned colleft = cell->colSpan() - r[pos].inColSpan;
            if (first > colleft)
              r[pos + 1].inColSpan = 0;
            else
              r[pos + 1].inColSpan = first + r[pos].inColSpan;
        } else {
            r[pos + 1].inColSpan = 0;
        }
    }
}

// Hit Testing
bool RenderTableSection::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction action)
{
    // If we have no children then we have nothing to do.
    if (!firstRow())
        return false;

    // Table sections cannot ever be hit tested.  Effectively they do not exist.
    // Just forward to our children always.
    LayoutPoint adjustedLocation = accumulatedOffset + location();

    if (hasOverflowClip() && !locationInContainer.intersects(overflowClipRect(adjustedLocation)))
        return false;

    if (hasOverflowingCell()) {
        for (RenderTableRow* row = lastRow(); row; row = row->previousRow()) {
            // FIXME: We have to skip over inline flows, since they can show up inside table rows
            // at the moment (a demoted inline <form> for example). If we ever implement a
            // table-specific hit-test method (which we should do for performance reasons anyway),
            // then we can remove this check.
            if (!row->hasSelfPaintingLayer()) {
                LayoutPoint childPoint = flipForWritingModeForChild(row, adjustedLocation);
                if (row->nodeAtPoint(request, result, locationInContainer, childPoint, action)) {
                    updateHitTestResult(result, toLayoutPoint(locationInContainer.point() - childPoint));
                    return true;
                }
            }
        }
        return false;
    }

    recalcCellsIfNeeded();

    LayoutRect hitTestRect = locationInContainer.boundingBox();
    hitTestRect.moveBy(-adjustedLocation);

    LayoutRect tableAlignedRect = logicalRectForWritingModeAndDirection(hitTestRect);
    CellSpan rowSpan = spannedRows(tableAlignedRect);
    CellSpan columnSpan = spannedColumns(tableAlignedRect);

    // Now iterate over the spanned rows and columns.
    for (unsigned hitRow = rowSpan.start(); hitRow < rowSpan.end(); ++hitRow) {
        for (unsigned hitColumn = columnSpan.start(); hitColumn < columnSpan.end(); ++hitColumn) {
            CellStruct& current = cellAt(hitRow, hitColumn);

            // If the cell is empty, there's nothing to do
            if (!current.hasCells())
                continue;

            for (unsigned i = current.cells.size() ; i; ) {
                --i;
                RenderTableCell* cell = current.cells[i];
                LayoutPoint cellPoint = flipForWritingModeForChild(cell, adjustedLocation);
                if (static_cast<RenderObject*>(cell)->nodeAtPoint(request, result, locationInContainer, cellPoint, action)) {
                    updateHitTestResult(result, locationInContainer.point() - toLayoutSize(cellPoint));
                    return true;
                }
            }
            if (!result.isRectBasedTest())
                break;
        }
        if (!result.isRectBasedTest())
            break;
    }

    return false;
}

void RenderTableSection::removeCachedCollapsedBorders(const RenderTableCell* cell)
{
    if (!table()->collapseBorders())
        return;

    for (int side = CBSBefore; side <= CBSEnd; ++side)
        m_cellsCollapsedBorders.remove(make_pair(cell, side));
}

void RenderTableSection::setCachedCollapsedBorder(const RenderTableCell* cell, CollapsedBorderSide side, CollapsedBorderValue border)
{
    ASSERT(table()->collapseBorders());
    m_cellsCollapsedBorders.set(make_pair(cell, side), border);
}

CollapsedBorderValue& RenderTableSection::cachedCollapsedBorder(const RenderTableCell* cell, CollapsedBorderSide side)
{
    ASSERT(table()->collapseBorders());
    HashMap<pair<const RenderTableCell*, int>, CollapsedBorderValue>::iterator it = m_cellsCollapsedBorders.find(make_pair(cell, side));
    ASSERT_WITH_SECURITY_IMPLICATION(it != m_cellsCollapsedBorders.end());
    return it->value;
}

RenderTableSection* RenderTableSection::createAnonymousWithParentRenderer(const RenderObject* parent)
{
    RefPtr<RenderStyle> newStyle = RenderStyle::createAnonymousStyleWithDisplay(parent->style(), TABLE_ROW_GROUP);
    RenderTableSection* newSection = new RenderTableSection(0);
    newSection->setDocumentForAnonymous(&parent->document());
    newSection->setStyle(newStyle.release());
    return newSection;
}

void RenderTableSection::setLogicalPositionForCell(RenderTableCell* cell, unsigned effectiveColumn) const
{
    LayoutPoint oldCellLocation = cell->location();

    LayoutPoint cellLocation(0, m_rowPos[cell->rowIndex()]);
    int horizontalBorderSpacing = table()->hBorderSpacing();

    // FIXME: The table's direction should determine our row's direction, not the section's (see bug 96691).
    if (!style()->isLeftToRightDirection())
        cellLocation.setX(table()->columnPositions()[table()->numEffCols()] - table()->columnPositions()[table()->colToEffCol(cell->col() + cell->colSpan())] + horizontalBorderSpacing);
    else
        cellLocation.setX(table()->columnPositions()[effectiveColumn] + horizontalBorderSpacing);

    cell->setLogicalLocation(cellLocation);

    if (!RuntimeEnabledFeatures::repaintAfterLayoutEnabled())
        view()->addLayoutDelta(oldCellLocation - cell->location());
}

} // namespace WebCore
