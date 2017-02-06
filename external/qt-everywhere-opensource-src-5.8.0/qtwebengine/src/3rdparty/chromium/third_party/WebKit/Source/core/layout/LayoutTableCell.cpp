/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#include "core/layout/LayoutTableCell.h"

#include "core/HTMLNames.h"
#include "core/css/StylePropertySet.h"
#include "core/html/HTMLTableCellElement.h"
#include "core/layout/LayoutAnalyzer.h"
#include "core/layout/LayoutTableCol.h"
#include "core/layout/SubtreeLayoutScope.h"
#include "core/paint/TableCellPainter.h"
#include "core/style/CollapsedBorderValue.h"
#include "platform/geometry/FloatQuad.h"
#include "platform/geometry/TransformState.h"
#include "wtf/PtrUtil.h"

namespace blink {

using namespace HTMLNames;

struct SameSizeAsLayoutTableCell : public LayoutBlockFlow {
    unsigned bitfields;
    int paddings[2];
    void* pointer;
};

static_assert(sizeof(LayoutTableCell) == sizeof(SameSizeAsLayoutTableCell), "LayoutTableCell should stay small");
static_assert(sizeof(CollapsedBorderValue) == 8, "CollapsedBorderValue should stay small");

LayoutTableCell::LayoutTableCell(Element* element)
    : LayoutBlockFlow(element)
    , m_absoluteColumnIndex(unsetColumnIndex)
    , m_cellWidthChanged(false)
    , m_intrinsicPaddingBefore(0)
    , m_intrinsicPaddingAfter(0)
{
    // We only update the flags when notified of DOM changes in colSpanOrRowSpanChanged()
    // so we need to set their initial values here in case something asks for colSpan()/rowSpan() before then.
    updateColAndRowSpanFlags();
}

void LayoutTableCell::willBeRemovedFromTree()
{
    LayoutBlockFlow::willBeRemovedFromTree();

    section()->setNeedsCellRecalc();
}

unsigned LayoutTableCell::parseColSpanFromDOM() const
{
    ASSERT(node());
    if (isHTMLTableCellElement(*node()))
        return std::min<unsigned>(toHTMLTableCellElement(*node()).colSpan(), maxColumnIndex);
    return 1;
}

unsigned LayoutTableCell::parseRowSpanFromDOM() const
{
    ASSERT(node());
    if (isHTMLTableCellElement(*node()))
        return std::min<unsigned>(toHTMLTableCellElement(*node()).rowSpan(), maxRowIndex);
    return 1;
}

void LayoutTableCell::updateColAndRowSpanFlags()
{
    // The vast majority of table cells do not have a colspan or rowspan,
    // so we keep a bool to know if we need to bother reading from the DOM.
    m_hasColSpan = node() && parseColSpanFromDOM() != 1;
    m_hasRowSpan = node() && parseRowSpanFromDOM() != 1;
}

void LayoutTableCell::colSpanOrRowSpanChanged()
{
    ASSERT(node());
    ASSERT(isHTMLTableCellElement(*node()));

    updateColAndRowSpanFlags();

    setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(LayoutInvalidationReason::AttributeChanged);
    if (parent() && section())
        section()->setNeedsCellRecalc();
}

Length LayoutTableCell::logicalWidthFromColumns(LayoutTableCol* firstColForThisCell, Length widthFromStyle) const
{
    ASSERT(firstColForThisCell && firstColForThisCell == table()->colElementAtAbsoluteColumn(absoluteColumnIndex()).innermostColOrColGroup());
    LayoutTableCol* tableCol = firstColForThisCell;

    unsigned colSpanCount = colSpan();
    int colWidthSum = 0;
    for (unsigned i = 1; i <= colSpanCount; i++) {
        Length colWidth = tableCol->style()->logicalWidth();

        // Percentage value should be returned only for colSpan == 1.
        // Otherwise we return original width for the cell.
        if (!colWidth.isFixed()) {
            if (colSpanCount > 1)
                return widthFromStyle;
            return colWidth;
        }

        colWidthSum += colWidth.value();
        tableCol = tableCol->nextColumn();
        // If no next <col> tag found for the span we just return what we have for now.
        if (!tableCol)
            break;
    }

    // Column widths specified on <col> apply to the border box of the cell, see bug 8126.
    // FIXME: Why is border/padding ignored in the negative width case?
    if (colWidthSum > 0)
        return Length(std::max(0, colWidthSum - borderAndPaddingLogicalWidth().ceil()), Fixed);
    return Length(colWidthSum, Fixed);
}

void LayoutTableCell::computePreferredLogicalWidths()
{
    // The child cells rely on the grids up in the sections to do their computePreferredLogicalWidths work.  Normally the sections are set up early, as table
    // cells are added, but relayout can cause the cells to be freed, leaving stale pointers in the sections'
    // grids.  We must refresh those grids before the child cells try to use them.
    table()->recalcSectionsIfNeeded();

    LayoutBlockFlow::computePreferredLogicalWidths();
    if (node() && style()->autoWrap()) {
        // See if nowrap was set.
        Length w = styleOrColLogicalWidth();
        const AtomicString& nowrap = toElement(node())->getAttribute(nowrapAttr);
        if (!nowrap.isNull() && w.isFixed()) {
            // Nowrap is set, but we didn't actually use it because of the
            // fixed width set on the cell.  Even so, it is a WinIE/Moz trait
            // to make the minwidth of the cell into the fixed width.  They do this
            // even in strict mode, so do not make this a quirk.  Affected the top
            // of hiptop.com.
            m_minPreferredLogicalWidth = std::max(LayoutUnit(w.value()), m_minPreferredLogicalWidth);
        }
    }
}

void LayoutTableCell::addLayerHitTestRects(LayerHitTestRects& layerRects, const PaintLayer* currentLayer, const LayoutPoint& layerOffset, const LayoutRect& containerRect) const
{
    LayoutPoint adjustedLayerOffset = layerOffset;
    // LayoutTableCell's location includes the offset of it's containing LayoutTableRow, so
    // we need to subtract that again here (as for LayoutTableCell::offsetFromContainer.
    if (parent())
        adjustedLayerOffset -= parentBox()->locationOffset();
    LayoutBox::addLayerHitTestRects(layerRects, currentLayer, adjustedLayerOffset, containerRect);
}

void LayoutTableCell::computeIntrinsicPadding(int rowHeight, SubtreeLayoutScope& layouter)
{
    int oldIntrinsicPaddingBefore = intrinsicPaddingBefore();
    int oldIntrinsicPaddingAfter = intrinsicPaddingAfter();
    int logicalHeightWithoutIntrinsicPadding = pixelSnappedLogicalHeight() - oldIntrinsicPaddingBefore - oldIntrinsicPaddingAfter;

    int intrinsicPaddingBefore = 0;
    switch (style()->verticalAlign()) {
    case VerticalAlignSub:
    case VerticalAlignSuper:
    case VerticalAlignTextTop:
    case VerticalAlignTextBottom:
    case VerticalAlignLength:
    case VerticalAlignBaseline: {
        int baseline = cellBaselinePosition();
        if (baseline > borderBefore() + paddingBefore())
            intrinsicPaddingBefore = section()->rowBaseline(rowIndex()) - (baseline - oldIntrinsicPaddingBefore);
        break;
    }
    case VerticalAlignTop:
        break;
    case VerticalAlignMiddle:
        intrinsicPaddingBefore = (rowHeight - logicalHeightWithoutIntrinsicPadding) / 2;
        break;
    case VerticalAlignBottom:
        intrinsicPaddingBefore = rowHeight - logicalHeightWithoutIntrinsicPadding;
        break;
    case VerticalAlignBaselineMiddle:
        break;
    }

    int intrinsicPaddingAfter = rowHeight - logicalHeightWithoutIntrinsicPadding - intrinsicPaddingBefore;
    setIntrinsicPaddingBefore(intrinsicPaddingBefore);
    setIntrinsicPaddingAfter(intrinsicPaddingAfter);

    // FIXME: Changing an intrinsic padding shouldn't trigger a relayout as it only shifts the cell inside the row but
    // doesn't change the logical height.
    if (intrinsicPaddingBefore != oldIntrinsicPaddingBefore || intrinsicPaddingAfter != oldIntrinsicPaddingAfter)
        layouter.setNeedsLayout(this, LayoutInvalidationReason::PaddingChanged);
}

void LayoutTableCell::updateLogicalWidth()
{
}

void LayoutTableCell::setCellLogicalWidth(int tableLayoutLogicalWidth, SubtreeLayoutScope& layouter)
{
    if (tableLayoutLogicalWidth == logicalWidth())
        return;

    layouter.setNeedsLayout(this, LayoutInvalidationReason::SizeChanged);

    setLogicalWidth(LayoutUnit(tableLayoutLogicalWidth));
    setCellWidthChanged(true);
}

void LayoutTableCell::layout()
{
    ASSERT(needsLayout());
    LayoutAnalyzer::Scope analyzer(*this);

    int oldCellBaseline = cellBaselinePosition();
    layoutBlock(cellWidthChanged());

    // If we have replaced content, the intrinsic height of our content may have changed since the last time we laid out. If that's the case the intrinsic padding we used
    // for layout (the padding required to push the contents of the cell down to the row's baseline) is included in our new height and baseline and makes both
    // of them wrong. So if our content's intrinsic height has changed push the new content up into the intrinsic padding and relayout so that the rest of
    // table and row layout can use the correct baseline and height for this cell.
    if (isBaselineAligned() && section()->rowBaseline(rowIndex()) && cellBaselinePosition() > section()->rowBaseline(rowIndex())) {
        int newIntrinsicPaddingBefore = std::max(intrinsicPaddingBefore() - std::max(cellBaselinePosition() - oldCellBaseline, 0), 0);
        setIntrinsicPaddingBefore(newIntrinsicPaddingBefore);
        SubtreeLayoutScope layouter(*this);
        layouter.setNeedsLayout(this, LayoutInvalidationReason::TableChanged);
        layoutBlock(cellWidthChanged());
    }

    // FIXME: This value isn't the intrinsic content logical height, but we need
    // to update the value as its used by flexbox layout. crbug.com/367324
    setIntrinsicContentLogicalHeight(contentLogicalHeight());

    setCellWidthChanged(false);
}

LayoutUnit LayoutTableCell::paddingTop() const
{
    LayoutUnit result = computedCSSPaddingTop();
    if (isHorizontalWritingMode())
        result += (style()->getWritingMode() == TopToBottomWritingMode ? intrinsicPaddingBefore() : intrinsicPaddingAfter());
    // TODO(leviw): The floor call should be removed when Table is sub-pixel aware. crbug.com/377847
    return LayoutUnit(result.floor());
}

LayoutUnit LayoutTableCell::paddingBottom() const
{
    LayoutUnit result = computedCSSPaddingBottom();
    if (isHorizontalWritingMode())
        result += (style()->getWritingMode() == TopToBottomWritingMode ? intrinsicPaddingAfter() : intrinsicPaddingBefore());
    // TODO(leviw): The floor call should be removed when Table is sub-pixel aware. crbug.com/377847
    return LayoutUnit(result.floor());
}

LayoutUnit LayoutTableCell::paddingLeft() const
{
    LayoutUnit result = computedCSSPaddingLeft();
    if (!isHorizontalWritingMode())
        result += (style()->getWritingMode() == LeftToRightWritingMode ? intrinsicPaddingBefore() : intrinsicPaddingAfter());
    // TODO(leviw): The floor call should be removed when Table is sub-pixel aware. crbug.com/377847
    return LayoutUnit(result.floor());
}

LayoutUnit LayoutTableCell::paddingRight() const
{
    LayoutUnit result = computedCSSPaddingRight();
    if (!isHorizontalWritingMode())
        result += (style()->getWritingMode() == LeftToRightWritingMode ? intrinsicPaddingAfter() : intrinsicPaddingBefore());
    // TODO(leviw): The floor call should be removed when Table is sub-pixel aware. crbug.com/377847
    return LayoutUnit(result.floor());
}

LayoutUnit LayoutTableCell::paddingBefore() const
{
    return LayoutUnit(computedCSSPaddingBefore().floor() + intrinsicPaddingBefore());
}

LayoutUnit LayoutTableCell::paddingAfter() const
{
    return LayoutUnit(computedCSSPaddingAfter().floor() + intrinsicPaddingAfter());
}

void LayoutTableCell::setOverrideLogicalContentHeightFromRowHeight(LayoutUnit rowHeight)
{
    clearIntrinsicPadding();
    setOverrideLogicalContentHeight((rowHeight - borderAndPaddingLogicalHeight()).clampNegativeToZero());
}

LayoutSize LayoutTableCell::offsetFromContainer(const LayoutObject* o) const
{
    ASSERT(o == container());

    LayoutSize offset = LayoutBlockFlow::offsetFromContainer(o);
    if (parent())
        offset -= parentBox()->locationOffset();

    return offset;
}

LayoutRect LayoutTableCell::localOverflowRectForPaintInvalidation() const
{
    // If the table grid is dirty, we cannot get reliable information about adjoining cells,
    // so we ignore outside borders. This should not be a problem because it means that
    // the table is going to recalculate the grid, relayout and issue a paint invalidation of its current rect, which
    // includes any outside borders of this cell.
    if (!table()->collapseBorders() || table()->needsSectionRecalc())
        return LayoutBlockFlow::localOverflowRectForPaintInvalidation();

    bool rtl = !styleForCellFlow().isLeftToRightDirection();
    int outlineOutset = style()->outlineOutsetExtent();
    int left = std::max(borderHalfLeft(true), outlineOutset);
    int right = std::max(borderHalfRight(true), outlineOutset);
    int top = std::max(borderHalfTop(true), outlineOutset);
    int bottom = std::max(borderHalfBottom(true), outlineOutset);
    if ((left && !rtl) || (right && rtl)) {
        if (LayoutTableCell* before = table()->cellBefore(this)) {
            top = std::max(top, before->borderHalfTop(true));
            bottom = std::max(bottom, before->borderHalfBottom(true));
        }
    }
    if ((left && rtl) || (right && !rtl)) {
        if (LayoutTableCell* after = table()->cellAfter(this)) {
            top = std::max(top, after->borderHalfTop(true));
            bottom = std::max(bottom, after->borderHalfBottom(true));
        }
    }
    if (top) {
        if (LayoutTableCell* above = table()->cellAbove(this)) {
            left = std::max(left, above->borderHalfLeft(true));
            right = std::max(right, above->borderHalfRight(true));
        }
    }
    if (bottom) {
        if (LayoutTableCell* below = table()->cellBelow(this)) {
            left = std::max(left, below->borderHalfLeft(true));
            right = std::max(right, below->borderHalfRight(true));
        }
    }

    LayoutRect selfVisualOverflowRect = this->selfVisualOverflowRect();
    LayoutPoint location(std::max(LayoutUnit(left), -selfVisualOverflowRect.x()), std::max(LayoutUnit(top), -selfVisualOverflowRect.y()));
    return LayoutRect(-location.x(), -location.y(), location.x() + std::max(size().width() + right, selfVisualOverflowRect.maxX()), location.y() + std::max(size().height() + bottom, selfVisualOverflowRect.maxY()));
}

bool LayoutTableCell::mapToVisualRectInAncestorSpace(const LayoutBoxModelObject* ancestor, LayoutRect& r, VisualRectFlags visualRectFlags) const
{
    if (ancestor == this)
        return true;
    if (parent())
        r.moveBy(-parentBox()->location()); // Rows are in the same coordinate space, so don't add their offset in.
    return LayoutBlockFlow::mapToVisualRectInAncestorSpace(ancestor, r, visualRectFlags);
}

int LayoutTableCell::cellBaselinePosition() const
{
    // <http://www.w3.org/TR/2007/CR-CSS21-20070719/tables.html#height-layout>: The baseline of a cell is the baseline of
    // the first in-flow line box in the cell, or the first in-flow table-row in the cell, whichever comes first. If there
    // is no such line box or table-row, the baseline is the bottom of content edge of the cell box.
    int firstLineBaseline = firstLineBoxBaseline();
    if (firstLineBaseline != -1)
        return firstLineBaseline;
    return borderBefore() + paddingBefore() + contentLogicalHeight();
}

void LayoutTableCell::styleDidChange(StyleDifference diff, const ComputedStyle* oldStyle)
{
    ASSERT(style()->display() == TABLE_CELL);

    LayoutBlockFlow::styleDidChange(diff, oldStyle);
    setHasBoxDecorationBackground(true);

    if (parent() && section() && oldStyle && style()->height() != oldStyle->height())
        section()->rowLogicalHeightChanged(row());

    // Our intrinsic padding pushes us down to align with the baseline of other cells on the row. If our vertical-align
    // has changed then so will the padding needed to align with other cells - clear it so we can recalculate it from scratch.
    if (oldStyle && style()->verticalAlign() != oldStyle->verticalAlign())
        clearIntrinsicPadding();

    // If border was changed, notify table.
    if (parent()) {
        LayoutTable* table = this->table();
        if (table && !table->selfNeedsLayout() && !table->normalChildNeedsLayout()&& oldStyle && oldStyle->border() != style()->border())
            table->invalidateCollapsedBorders();
    }
}

// The following rules apply for resolving conflicts and figuring out which border
// to use.
// (1) Borders with the 'border-style' of 'hidden' take precedence over all other conflicting
// borders. Any border with this value suppresses all borders at this location.
// (2) Borders with a style of 'none' have the lowest priority. Only if the border properties of all
// the elements meeting at this edge are 'none' will the border be omitted (but note that 'none' is
// the default value for the border style.)
// (3) If none of the styles are 'hidden' and at least one of them is not 'none', then narrow borders
// are discarded in favor of wider ones. If several have the same 'border-width' then styles are preferred
// in this order: 'double', 'solid', 'dashed', 'dotted', 'ridge', 'outset', 'groove', and the lowest: 'inset'.
// (4) If border styles differ only in color, then a style set on a cell wins over one on a row,
// which wins over a row group, column, column group and, lastly, table. It is undefined which color
// is used when two elements of the same type disagree.
static bool compareBorders(const CollapsedBorderValue& border1, const CollapsedBorderValue& border2)
{
    // Sanity check the values passed in. The null border have lowest priority.
    if (!border2.exists())
        return false;
    if (!border1.exists())
        return true;

    // Rule #1 above.
    if (border1.style() == BorderStyleHidden)
        return false;
    if (border2.style() == BorderStyleHidden)
        return true;

    // Rule #2 above.  A style of 'none' has lowest priority and always loses to any other border.
    if (border2.style() == BorderStyleNone)
        return false;
    if (border1.style() == BorderStyleNone)
        return true;

    // The first part of rule #3 above. Wider borders win.
    if (border1.width() != border2.width())
        return border1.width() < border2.width();

    // The borders have equal width.  Sort by border style.
    if (border1.style() != border2.style())
        return border1.style() < border2.style();

    // The border have the same width and style.  Rely on precedence (cell over row over row group, etc.)
    return border1.precedence() < border2.precedence();
}

static CollapsedBorderValue chooseBorder(const CollapsedBorderValue& border1, const CollapsedBorderValue& border2)
{
    return compareBorders(border1, border2) ? border2 : border1;
}

bool LayoutTableCell::hasStartBorderAdjoiningTable() const
{
    bool isStartColumn = !absoluteColumnIndex();
    bool isEndColumn = table()->absoluteColumnToEffectiveColumn(absoluteColumnIndex() + colSpan() - 1) == table()->numEffectiveColumns() - 1;
    bool hasSameDirectionAsTable = hasSameDirectionAs(table());

    // The table direction determines the row direction. In mixed directionality, we cannot guarantee that
    // we have a common border with the table (think a ltr table with rtl start cell).
    return (isStartColumn && hasSameDirectionAsTable) || (isEndColumn && !hasSameDirectionAsTable);
}

bool LayoutTableCell::hasEndBorderAdjoiningTable() const
{
    bool isStartColumn = !absoluteColumnIndex();
    bool isEndColumn = table()->absoluteColumnToEffectiveColumn(absoluteColumnIndex() + colSpan() - 1) == table()->numEffectiveColumns() - 1;
    bool hasSameDirectionAsTable = hasSameDirectionAs(table());

    // The table direction determines the row direction. In mixed directionality, we cannot guarantee that
    // we have a common border with the table (think a ltr table with ltr end cell).
    return (isStartColumn && !hasSameDirectionAsTable) || (isEndColumn && hasSameDirectionAsTable);
}

CollapsedBorderValue LayoutTableCell::computeCollapsedStartBorder(IncludeBorderColorOrNot includeColor) const
{
    LayoutTable* table = this->table();

    // For the start border, we need to check, in order of precedence:
    // (1) Our start border.
    int startColorProperty = includeColor ? CSSProperty::resolveDirectionAwareProperty(CSSPropertyWebkitBorderStartColor, styleForCellFlow().direction(), styleForCellFlow().getWritingMode()) : 0;
    int endColorProperty = includeColor ? CSSProperty::resolveDirectionAwareProperty(CSSPropertyWebkitBorderEndColor, styleForCellFlow().direction(), styleForCellFlow().getWritingMode()) : 0;
    CollapsedBorderValue result(style()->borderStart(), includeColor ? resolveColor(startColorProperty) : Color(), BorderPrecedenceCell);

    // (2) The end border of the preceding cell.
    LayoutTableCell* cellBefore = table->cellBefore(this);
    if (cellBefore) {
        CollapsedBorderValue cellBeforeAdjoiningBorder = CollapsedBorderValue(cellBefore->borderAdjoiningCellAfter(this), includeColor ? cellBefore->resolveColor(endColorProperty) : Color(), BorderPrecedenceCell);
        // |result| should be the 2nd argument as |cellBefore| should win in case of equality per CSS 2.1 (Border conflict resolution, point 4).
        result = chooseBorder(cellBeforeAdjoiningBorder, result);
        if (!result.exists())
            return result;
    }

    bool startBorderAdjoinsTable = hasStartBorderAdjoiningTable();
    if (startBorderAdjoinsTable) {
        // (3) Our row's start border.
        result = chooseBorder(result, CollapsedBorderValue(row()->borderAdjoiningStartCell(this), includeColor ? parent()->resolveColor(startColorProperty) : Color(), BorderPrecedenceRow));
        if (!result.exists())
            return result;

        // (4) Our row group's start border.
        result = chooseBorder(result, CollapsedBorderValue(section()->borderAdjoiningStartCell(this), includeColor ? section()->resolveColor(startColorProperty) : Color(), BorderPrecedenceRowGroup));
        if (!result.exists())
            return result;
    }

    // (5) Our column and column group's start borders.
    LayoutTable::ColAndColGroup colAndColGroup = table->colElementAtAbsoluteColumn(absoluteColumnIndex());
    if (colAndColGroup.colgroup && colAndColGroup.adjoinsStartBorderOfColGroup) {
        // Only apply the colgroup's border if this cell touches the colgroup edge.
        result = chooseBorder(result, CollapsedBorderValue(colAndColGroup.colgroup->borderAdjoiningCellStartBorder(this), includeColor ? colAndColGroup.colgroup->resolveColor(startColorProperty) : Color(), BorderPrecedenceColumnGroup));
        if (!result.exists())
            return result;
    }
    if (colAndColGroup.col) {
        // Always apply the col's border irrespective of whether this cell touches it. This is per HTML5:
        // "For the purposes of the CSS table model, the col element is expected to be treated as if it
        // "was present as many times as its span attribute specifies".
        result = chooseBorder(result, CollapsedBorderValue(colAndColGroup.col->borderAdjoiningCellStartBorder(this), includeColor ? colAndColGroup.col->resolveColor(startColorProperty) : Color(), BorderPrecedenceColumn));
        if (!result.exists())
            return result;
    }

    // (6) The end border of the preceding column.
    if (cellBefore) {
        LayoutTable::ColAndColGroup colAndColGroup = table->colElementAtAbsoluteColumn(absoluteColumnIndex() - 1);
        // Only apply the colgroup's border if this cell touches the colgroup edge.
        if (colAndColGroup.colgroup && colAndColGroup.adjoinsEndBorderOfColGroup) {
            result = chooseBorder(CollapsedBorderValue(colAndColGroup.colgroup->borderAdjoiningCellEndBorder(this), includeColor ? colAndColGroup.colgroup->resolveColor(endColorProperty) : Color(), BorderPrecedenceColumnGroup), result);
            if (!result.exists())
                return result;
        }
        // Always apply the col's border irrespective of whether this cell touches it. This is per HTML5:
        // "For the purposes of the CSS table model, the col element is expected to be treated as if it
        // "was present as many times as its span attribute specifies".
        if (colAndColGroup.col) {
            result = chooseBorder(CollapsedBorderValue(colAndColGroup.col->borderAdjoiningCellAfter(this), includeColor ? colAndColGroup.col->resolveColor(endColorProperty) : Color(), BorderPrecedenceColumn), result);
            if (!result.exists())
                return result;
        }
    }

    if (startBorderAdjoinsTable) {
        // (7) The table's start border.
        result = chooseBorder(result, CollapsedBorderValue(table->tableStartBorderAdjoiningCell(this), includeColor ? table->resolveColor(startColorProperty) : Color(), BorderPrecedenceTable));
        if (!result.exists())
            return result;
    }

    return result;
}

CollapsedBorderValue LayoutTableCell::computeCollapsedEndBorder(IncludeBorderColorOrNot includeColor) const
{
    LayoutTable* table = this->table();
    // Note: We have to use the effective column information instead of whether we have a cell after as a table doesn't
    // have to be regular (any row can have less cells than the total cell count).
    bool isEndColumn = table->absoluteColumnToEffectiveColumn(absoluteColumnIndex() + colSpan() - 1) == table->numEffectiveColumns() - 1;

    // For end border, we need to check, in order of precedence:
    // (1) Our end border.
    int startColorProperty = includeColor ? CSSProperty::resolveDirectionAwareProperty(CSSPropertyWebkitBorderStartColor, styleForCellFlow().direction(), styleForCellFlow().getWritingMode()) : 0;
    int endColorProperty = includeColor ? CSSProperty::resolveDirectionAwareProperty(CSSPropertyWebkitBorderEndColor, styleForCellFlow().direction(), styleForCellFlow().getWritingMode()) : 0;
    CollapsedBorderValue result = CollapsedBorderValue(style()->borderEnd(), includeColor ? resolveColor(endColorProperty) : Color(), BorderPrecedenceCell);

    // (2) The start border of the following cell.
    if (!isEndColumn) {
        if (LayoutTableCell* cellAfter = table->cellAfter(this)) {
            CollapsedBorderValue cellAfterAdjoiningBorder = CollapsedBorderValue(cellAfter->borderAdjoiningCellBefore(this), includeColor ? cellAfter->resolveColor(startColorProperty) : Color(), BorderPrecedenceCell);
            result = chooseBorder(result, cellAfterAdjoiningBorder);
            if (!result.exists())
                return result;
        }
    }

    bool endBorderAdjoinsTable = hasEndBorderAdjoiningTable();
    if (endBorderAdjoinsTable) {
        // (3) Our row's end border.
        result = chooseBorder(result, CollapsedBorderValue(row()->borderAdjoiningEndCell(this), includeColor ? parent()->resolveColor(endColorProperty) : Color(), BorderPrecedenceRow));
        if (!result.exists())
            return result;

        // (4) Our row group's end border.
        result = chooseBorder(result, CollapsedBorderValue(section()->borderAdjoiningEndCell(this), includeColor ? section()->resolveColor(endColorProperty) : Color(), BorderPrecedenceRowGroup));
        if (!result.exists())
            return result;
    }

    // (5) Our column and column group's end borders.
    LayoutTable::ColAndColGroup colAndColGroup = table->colElementAtAbsoluteColumn(absoluteColumnIndex() + colSpan() - 1);
    if (colAndColGroup.colgroup && colAndColGroup.adjoinsEndBorderOfColGroup) {
        // Only apply the colgroup's border if this cell touches the colgroup edge.
        result = chooseBorder(result, CollapsedBorderValue(colAndColGroup.colgroup->borderAdjoiningCellEndBorder(this), includeColor ? colAndColGroup.colgroup->resolveColor(endColorProperty) : Color(), BorderPrecedenceColumnGroup));
        if (!result.exists())
            return result;
    }
    if (colAndColGroup.col) {
        // Always apply the col's border irrespective of whether this cell touches it. This is per HTML5:
        // "For the purposes of the CSS table model, the col element is expected to be treated as if it
        // "was present as many times as its span attribute specifies".
        result = chooseBorder(result, CollapsedBorderValue(colAndColGroup.col->borderAdjoiningCellEndBorder(this), includeColor ? colAndColGroup.col->resolveColor(endColorProperty) : Color(), BorderPrecedenceColumn));
        if (!result.exists())
            return result;
    }

    // (6) The start border of the next column.
    if (!isEndColumn) {
        LayoutTable::ColAndColGroup colAndColGroup = table->colElementAtAbsoluteColumn(absoluteColumnIndex() + colSpan());
        if (colAndColGroup.colgroup && colAndColGroup.adjoinsStartBorderOfColGroup) {
            // Only apply the colgroup's border if this cell touches the colgroup edge.
            result = chooseBorder(result, CollapsedBorderValue(colAndColGroup.colgroup->borderAdjoiningCellStartBorder(this), includeColor ? colAndColGroup.colgroup->resolveColor(startColorProperty) : Color(), BorderPrecedenceColumnGroup));
            if (!result.exists())
                return result;
        }
        if (colAndColGroup.col) {
            // Always apply the col's border irrespective of whether this cell touches it. This is per HTML5:
            // "For the purposes of the CSS table model, the col element is expected to be treated as if it
            // "was present as many times as its span attribute specifies".
            result = chooseBorder(result, CollapsedBorderValue(colAndColGroup.col->borderAdjoiningCellBefore(this), includeColor ? colAndColGroup.col->resolveColor(startColorProperty) : Color(), BorderPrecedenceColumn));
            if (!result.exists())
                return result;
        }
    }

    if (endBorderAdjoinsTable) {
        // (7) The table's end border.
        result = chooseBorder(result, CollapsedBorderValue(table->tableEndBorderAdjoiningCell(this), includeColor ? table->resolveColor(endColorProperty) : Color(), BorderPrecedenceTable));
        if (!result.exists())
            return result;
    }

    return result;
}

CollapsedBorderValue LayoutTableCell::computeCollapsedBeforeBorder(IncludeBorderColorOrNot includeColor) const
{
    LayoutTable* table = this->table();

    // For before border, we need to check, in order of precedence:
    // (1) Our before border.
    int beforeColorProperty = includeColor ? CSSProperty::resolveDirectionAwareProperty(CSSPropertyWebkitBorderBeforeColor, styleForCellFlow().direction(), styleForCellFlow().getWritingMode()) : 0;
    int afterColorProperty = includeColor ? CSSProperty::resolveDirectionAwareProperty(CSSPropertyWebkitBorderAfterColor, styleForCellFlow().direction(), styleForCellFlow().getWritingMode()) : 0;
    CollapsedBorderValue result = CollapsedBorderValue(style()->borderBefore(), includeColor ? resolveColor(beforeColorProperty) : Color(), BorderPrecedenceCell);

    LayoutTableCell* prevCell = table->cellAbove(this);
    if (prevCell) {
        // (2) A before cell's after border.
        result = chooseBorder(CollapsedBorderValue(prevCell->style()->borderAfter(), includeColor ? prevCell->resolveColor(afterColorProperty) : Color(), BorderPrecedenceCell), result);
        if (!result.exists())
            return result;
    }

    // (3) Our row's before border.
    result = chooseBorder(result, CollapsedBorderValue(parent()->style()->borderBefore(), includeColor ? parent()->resolveColor(beforeColorProperty) : Color(), BorderPrecedenceRow));
    if (!result.exists())
        return result;

    // (4) The previous row's after border.
    if (prevCell) {
        LayoutObject* prevRow = nullptr;
        if (prevCell->section() == section())
            prevRow = parent()->previousSibling();
        else
            prevRow = prevCell->section()->lastRow();

        if (prevRow) {
            result = chooseBorder(CollapsedBorderValue(prevRow->style()->borderAfter(), includeColor ? prevRow->resolveColor(afterColorProperty) : Color(), BorderPrecedenceRow), result);
            if (!result.exists())
                return result;
        }
    }

    // Now check row groups.
    LayoutTableSection* currSection = section();
    if (!rowIndex()) {
        // (5) Our row group's before border.
        result = chooseBorder(result, CollapsedBorderValue(currSection->style()->borderBefore(), includeColor ? currSection->resolveColor(beforeColorProperty) : Color(), BorderPrecedenceRowGroup));
        if (!result.exists())
            return result;

        // (6) Previous row group's after border.
        currSection = table->sectionAbove(currSection, SkipEmptySections);
        if (currSection) {
            result = chooseBorder(CollapsedBorderValue(currSection->style()->borderAfter(), includeColor ? currSection->resolveColor(afterColorProperty) : Color(), BorderPrecedenceRowGroup), result);
            if (!result.exists())
                return result;
        }
    }

    if (!currSection) {
        // (8) Our column and column group's before borders.
        LayoutTableCol* colElt = table->colElementAtAbsoluteColumn(absoluteColumnIndex()).innermostColOrColGroup();
        if (colElt) {
            result = chooseBorder(result, CollapsedBorderValue(colElt->style()->borderBefore(), includeColor ? colElt->resolveColor(beforeColorProperty) : Color(), BorderPrecedenceColumn));
            if (!result.exists())
                return result;
            if (LayoutTableCol* enclosingColumnGroup = colElt->enclosingColumnGroup()) {
                result = chooseBorder(result, CollapsedBorderValue(enclosingColumnGroup->style()->borderBefore(), includeColor ? enclosingColumnGroup->resolveColor(beforeColorProperty) : Color(), BorderPrecedenceColumnGroup));
                if (!result.exists())
                    return result;
            }
        }

        // (9) The table's before border.
        result = chooseBorder(result, CollapsedBorderValue(table->style()->borderBefore(), includeColor ? table->resolveColor(beforeColorProperty) : Color(), BorderPrecedenceTable));
        if (!result.exists())
            return result;
    }

    return result;
}

CollapsedBorderValue LayoutTableCell::computeCollapsedAfterBorder(IncludeBorderColorOrNot includeColor) const
{
    LayoutTable* table = this->table();

    // For after border, we need to check, in order of precedence:
    // (1) Our after border.
    int beforeColorProperty = includeColor ? CSSProperty::resolveDirectionAwareProperty(CSSPropertyWebkitBorderBeforeColor, styleForCellFlow().direction(), styleForCellFlow().getWritingMode()) : 0;
    int afterColorProperty = includeColor ? CSSProperty::resolveDirectionAwareProperty(CSSPropertyWebkitBorderAfterColor, styleForCellFlow().direction(), styleForCellFlow().getWritingMode()) : 0;
    CollapsedBorderValue result = CollapsedBorderValue(style()->borderAfter(), includeColor ? resolveColor(afterColorProperty) : Color(), BorderPrecedenceCell);

    LayoutTableCell* nextCell = table->cellBelow(this);
    if (nextCell) {
        // (2) An after cell's before border.
        result = chooseBorder(result, CollapsedBorderValue(nextCell->style()->borderBefore(), includeColor ? nextCell->resolveColor(beforeColorProperty) : Color(), BorderPrecedenceCell));
        if (!result.exists())
            return result;
    }

    // (3) Our row's after border. (FIXME: Deal with rowspan!)
    result = chooseBorder(result, CollapsedBorderValue(parent()->style()->borderAfter(), includeColor ? parent()->resolveColor(afterColorProperty) : Color(), BorderPrecedenceRow));
    if (!result.exists())
        return result;

    // (4) The next row's before border.
    if (nextCell) {
        result = chooseBorder(result, CollapsedBorderValue(nextCell->parent()->style()->borderBefore(), includeColor ? nextCell->parent()->resolveColor(beforeColorProperty) : Color(), BorderPrecedenceRow));
        if (!result.exists())
            return result;
    }

    // Now check row groups.
    LayoutTableSection* currSection = section();
    if (rowIndex() + rowSpan() >= currSection->numRows()) {
        // (5) Our row group's after border.
        result = chooseBorder(result, CollapsedBorderValue(currSection->style()->borderAfter(), includeColor ? currSection->resolveColor(afterColorProperty) : Color(), BorderPrecedenceRowGroup));
        if (!result.exists())
            return result;

        // (6) Following row group's before border.
        currSection = table->sectionBelow(currSection, SkipEmptySections);
        if (currSection) {
            result = chooseBorder(result, CollapsedBorderValue(currSection->style()->borderBefore(), includeColor ? currSection->resolveColor(beforeColorProperty) : Color(), BorderPrecedenceRowGroup));
            if (!result.exists())
                return result;
        }
    }

    if (!currSection) {
        // (8) Our column and column group's after borders.
        LayoutTableCol* colElt = table->colElementAtAbsoluteColumn(absoluteColumnIndex()).innermostColOrColGroup();
        if (colElt) {
            result = chooseBorder(result, CollapsedBorderValue(colElt->style()->borderAfter(), includeColor ? colElt->resolveColor(afterColorProperty) : Color(), BorderPrecedenceColumn));
            if (!result.exists())
                return result;
            if (LayoutTableCol* enclosingColumnGroup = colElt->enclosingColumnGroup()) {
                result = chooseBorder(result, CollapsedBorderValue(enclosingColumnGroup->style()->borderAfter(), includeColor ? enclosingColumnGroup->resolveColor(afterColorProperty) : Color(), BorderPrecedenceColumnGroup));
                if (!result.exists())
                    return result;
            }
        }

        // (9) The table's after border.
        result = chooseBorder(result, CollapsedBorderValue(table->style()->borderAfter(), includeColor ? table->resolveColor(afterColorProperty) : Color(), BorderPrecedenceTable));
        if (!result.exists())
            return result;
    }

    return result;
}

int LayoutTableCell::borderLeft() const
{
    return table()->collapseBorders() ? borderHalfLeft(false) : LayoutBlockFlow::borderLeft();
}

int LayoutTableCell::borderRight() const
{
    return table()->collapseBorders() ? borderHalfRight(false) : LayoutBlockFlow::borderRight();
}

int LayoutTableCell::borderTop() const
{
    return table()->collapseBorders() ? borderHalfTop(false) : LayoutBlockFlow::borderTop();
}

int LayoutTableCell::borderBottom() const
{
    return table()->collapseBorders() ? borderHalfBottom(false) : LayoutBlockFlow::borderBottom();
}

// FIXME: https://bugs.webkit.org/show_bug.cgi?id=46191, make the collapsed border drawing
// work with different block flow values instead of being hard-coded to top-to-bottom.
int LayoutTableCell::borderStart() const
{
    return table()->collapseBorders() ? borderHalfStart(false) : LayoutBlockFlow::borderStart();
}

int LayoutTableCell::borderEnd() const
{
    return table()->collapseBorders() ? borderHalfEnd(false) : LayoutBlockFlow::borderEnd();
}

int LayoutTableCell::borderBefore() const
{
    return table()->collapseBorders() ? borderHalfBefore(false) : LayoutBlockFlow::borderBefore();
}

int LayoutTableCell::borderAfter() const
{
    return table()->collapseBorders() ? borderHalfAfter(false) : LayoutBlockFlow::borderAfter();
}

int LayoutTableCell::borderHalfLeft(bool outer) const
{
    const ComputedStyle& styleForCellFlow = this->styleForCellFlow();
    if (styleForCellFlow.isHorizontalWritingMode())
        return styleForCellFlow.isLeftToRightDirection() ? borderHalfStart(outer) : borderHalfEnd(outer);
    return styleForCellFlow.isFlippedBlocksWritingMode() ? borderHalfAfter(outer) : borderHalfBefore(outer);
}

int LayoutTableCell::borderHalfRight(bool outer) const
{
    const ComputedStyle& styleForCellFlow = this->styleForCellFlow();
    if (styleForCellFlow.isHorizontalWritingMode())
        return styleForCellFlow.isLeftToRightDirection() ? borderHalfEnd(outer) : borderHalfStart(outer);
    return styleForCellFlow.isFlippedBlocksWritingMode() ? borderHalfBefore(outer) : borderHalfAfter(outer);
}

int LayoutTableCell::borderHalfTop(bool outer) const
{
    const ComputedStyle& styleForCellFlow = this->styleForCellFlow();
    if (styleForCellFlow.isHorizontalWritingMode())
        return styleForCellFlow.isFlippedBlocksWritingMode() ? borderHalfAfter(outer) : borderHalfBefore(outer);
    return styleForCellFlow.isLeftToRightDirection() ? borderHalfStart(outer) : borderHalfEnd(outer);
}

int LayoutTableCell::borderHalfBottom(bool outer) const
{
    const ComputedStyle& styleForCellFlow = this->styleForCellFlow();
    if (styleForCellFlow.isHorizontalWritingMode())
        return styleForCellFlow.isFlippedBlocksWritingMode() ? borderHalfBefore(outer) : borderHalfAfter(outer);
    return styleForCellFlow.isLeftToRightDirection() ? borderHalfEnd(outer) : borderHalfStart(outer);
}

int LayoutTableCell::borderHalfStart(bool outer) const
{
    CollapsedBorderValue border = computeCollapsedStartBorder(DoNotIncludeBorderColor);
    if (border.exists())
        return (border.width() + ((styleForCellFlow().isLeftToRightDirection() ^ outer) ? 1 : 0)) / 2; // Give the extra pixel to top and left.
    return 0;
}

int LayoutTableCell::borderHalfEnd(bool outer) const
{
    CollapsedBorderValue border = computeCollapsedEndBorder(DoNotIncludeBorderColor);
    if (border.exists())
        return (border.width() + ((styleForCellFlow().isLeftToRightDirection() ^ outer) ? 0 : 1)) / 2;
    return 0;
}

int LayoutTableCell::borderHalfBefore(bool outer) const
{
    CollapsedBorderValue border = computeCollapsedBeforeBorder(DoNotIncludeBorderColor);
    if (border.exists())
        return (border.width() + ((styleForCellFlow().isFlippedBlocksWritingMode() ^ outer) ? 0 : 1)) / 2; // Give the extra pixel to top and left.
    return 0;
}

int LayoutTableCell::borderHalfAfter(bool outer) const
{
    CollapsedBorderValue border = computeCollapsedAfterBorder(DoNotIncludeBorderColor);
    if (border.exists())
        return (border.width() + ((styleForCellFlow().isFlippedBlocksWritingMode() ^ outer) ? 1 : 0)) / 2;
    return 0;
}

void LayoutTableCell::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset) const
{
    TableCellPainter(*this).paint(paintInfo, paintOffset);
}

static void addBorderStyle(LayoutTable::CollapsedBorderValues& borderValues,
    CollapsedBorderValue borderValue)
{
    if (!borderValue.isVisible())
        return;
    size_t count = borderValues.size();
    for (size_t i = 0; i < count; ++i) {
        if (borderValues[i].isSameIgnoringColor(borderValue))
            return;
    }
    borderValues.append(borderValue);
}

void LayoutTableCell::collectBorderValues(LayoutTable::CollapsedBorderValues& borderValues)
{
    CollapsedBorderValues newValues = {
        computeCollapsedStartBorder(),
        computeCollapsedEndBorder(),
        computeCollapsedBeforeBorder(),
        computeCollapsedAfterBorder()
    };

    bool changed = false;
    if (!newValues.startBorder.isVisible() && !newValues.endBorder.isVisible() && !newValues.beforeBorder.isVisible() && !newValues.afterBorder.isVisible()) {
        changed = !!m_collapsedBorderValues;
        m_collapsedBorderValues = nullptr;
    } else if (!m_collapsedBorderValues) {
        changed = true;
        m_collapsedBorderValues = wrapUnique(new CollapsedBorderValues(newValues));
    } else {
        // We check visuallyEquals so that the table cell is invalidated only if a changed
        // collapsed border is visible in the first place.
        changed = !m_collapsedBorderValues->startBorder.visuallyEquals(newValues.startBorder)
            || !m_collapsedBorderValues->endBorder.visuallyEquals(newValues.endBorder)
            || !m_collapsedBorderValues->beforeBorder.visuallyEquals(newValues.beforeBorder)
            || !m_collapsedBorderValues->afterBorder.visuallyEquals(newValues.afterBorder);
        if (changed)
            *m_collapsedBorderValues = newValues;
    }

    // If collapsed borders changed, invalidate the cell's display item client on the table's backing.
    // TODO(crbug.com/451090#c5): Need a way to invalidate/repaint the borders only.
    if (changed)
        table()->slowSetPaintingLayerNeedsRepaintAndInvalidateDisplayItemClient(*this, PaintInvalidationStyleChange);

    addBorderStyle(borderValues, newValues.startBorder);
    addBorderStyle(borderValues, newValues.endBorder);
    addBorderStyle(borderValues, newValues.beforeBorder);
    addBorderStyle(borderValues, newValues.afterBorder);
}

void LayoutTableCell::sortBorderValues(LayoutTable::CollapsedBorderValues& borderValues)
{
    std::sort(borderValues.begin(), borderValues.end(), compareBorders);
}

void LayoutTableCell::paintBoxDecorationBackground(const PaintInfo& paintInfo, const LayoutPoint& paintOffset) const
{
    TableCellPainter(*this).paintBoxDecorationBackground(paintInfo, paintOffset);
}

void LayoutTableCell::paintMask(const PaintInfo& paintInfo, const LayoutPoint& paintOffset) const
{
    TableCellPainter(*this).paintMask(paintInfo, paintOffset);
}

bool LayoutTableCell::boxShadowShouldBeAppliedToBackground(BackgroundBleedAvoidance, const InlineFlowBox*) const
{
    return false;
}

void LayoutTableCell::scrollbarsChanged(bool horizontalScrollbarChanged, bool verticalScrollbarChanged)
{
    LayoutBlock::scrollbarsChanged(horizontalScrollbarChanged, verticalScrollbarChanged);
    int scrollbarHeight = scrollbarLogicalHeight();
    if (!scrollbarHeight)
        return; // Not sure if we should be doing something when a scrollbar goes away or not.

    // We only care if the scrollbar that affects our intrinsic padding has been added.
    if ((isHorizontalWritingMode() && !horizontalScrollbarChanged)
        || (!isHorizontalWritingMode() && !verticalScrollbarChanged))
        return;

    // Shrink our intrinsic padding as much as possible to accommodate the scrollbar.
    if (style()->verticalAlign() == VerticalAlignMiddle) {
        LayoutUnit totalHeight = logicalHeight();
        LayoutUnit heightWithoutIntrinsicPadding = totalHeight - intrinsicPaddingBefore() - intrinsicPaddingAfter();
        totalHeight -= scrollbarHeight;
        LayoutUnit newBeforePadding = (totalHeight - heightWithoutIntrinsicPadding) / 2;
        LayoutUnit newAfterPadding = totalHeight - heightWithoutIntrinsicPadding - newBeforePadding;
        setIntrinsicPaddingBefore(newBeforePadding);
        setIntrinsicPaddingAfter(newAfterPadding);
    } else {
        setIntrinsicPaddingAfter(intrinsicPaddingAfter() - scrollbarHeight);
    }
}

LayoutTableCell* LayoutTableCell::createAnonymous(Document* document)
{
    LayoutTableCell* layoutObject = new LayoutTableCell(nullptr);
    layoutObject->setDocumentForAnonymous(document);
    return layoutObject;
}

LayoutTableCell* LayoutTableCell::createAnonymousWithParent(const LayoutObject* parent)
{
    LayoutTableCell* newCell = LayoutTableCell::createAnonymous(&parent->document());
    RefPtr<ComputedStyle> newStyle = ComputedStyle::createAnonymousStyleWithDisplay(parent->styleRef(), TABLE_CELL);
    newCell->setStyle(newStyle.release());
    return newCell;
}

bool LayoutTableCell::backgroundIsKnownToBeOpaqueInRect(const LayoutRect& localRect) const
{
    // If this object has layer, the area of collapsed borders should be transparent
    // to expose the collapsed borders painted on the underlying layer.
    if (hasLayer() && table()->collapseBorders())
        return false;
    return LayoutBlockFlow::backgroundIsKnownToBeOpaqueInRect(localRect);
}

} // namespace blink
