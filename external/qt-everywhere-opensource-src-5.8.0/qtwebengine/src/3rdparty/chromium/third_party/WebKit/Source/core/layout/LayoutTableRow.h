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

#ifndef LayoutTableRow_h
#define LayoutTableRow_h

#include "core/CoreExport.h"
#include "core/layout/LayoutTableSection.h"

namespace blink {

// There is a window of opportunity to read |m_rowIndex| before it is set when
// inserting the LayoutTableRow or during LayoutTableSection::recalcCells.
// This value is used to detect that case.
static const unsigned unsetRowIndex = 0x7FFFFFFF;
static const unsigned maxRowIndex = 0x7FFFFFFE; // 2,147,483,646

// LayoutTableRow is used to represent a table row (display: table-row).
//
// LayoutTableRow is a simple object. The reason is that most operations
// have to be coordinated at the LayoutTableSection level and thus are
// handled in LayoutTableSection (see e.g. layoutRows).
//
// The table model prevents any layout overflow on rows (but allow visual
// overflow). For row height, it is defined as "the maximum of the row's
// computed 'height', the computed 'height' of each cell in the row, and
// the minimum height (MIN) required by  the cells" (CSS 2.1 - section 17.5.3).
// Note that this means that rows and cells differ from other LayoutObject as
// they will ignore 'height' in some situation (when other LayoutObject just
// allow some layout overflow to occur).
//
// LayoutTableRow doesn't establish a containing block for the underlying
// LayoutTableCells. That's why it inherits from LayoutTableBoxComponent and not LayoutBlock.
// One oddity is that LayoutTableRow doesn't establish a new coordinate system
// for its children. LayoutTableCells are positioned with respect to the
// enclosing LayoutTableSection (this object's parent()). This particularity is
// why functions accumulating offset while walking the tree have to special case
// LayoutTableRow (see e.g. PaintInvalidationState or
// LayoutBox::positionFromPoint()).
//
// LayoutTableRow is also positioned with respect to the enclosing
// LayoutTableSection. See LayoutTableSection::layoutRows() for the placement
// logic.
class CORE_EXPORT LayoutTableRow final : public LayoutTableBoxComponent {
public:
    explicit LayoutTableRow(Element*);

    LayoutTableCell* firstCell() const;
    LayoutTableCell* lastCell() const;

    LayoutTableRow* previousRow() const;
    LayoutTableRow* nextRow() const;

    LayoutTableSection* section() const { return toLayoutTableSection(parent()); }
    LayoutTable* table() const { return toLayoutTable(parent()->parent()); }

    static LayoutTableRow* createAnonymous(Document*);
    static LayoutTableRow* createAnonymousWithParent(const LayoutObject*);
    LayoutBox* createAnonymousBoxWithSameTypeAs(const LayoutObject* parent) const override
    {
        return createAnonymousWithParent(parent);
    }

    void setRowIndex(unsigned rowIndex)
    {
        if (UNLIKELY(rowIndex > maxRowIndex))
            CRASH();

        m_rowIndex = rowIndex;
    }

    bool rowIndexWasSet() const { return m_rowIndex != unsetRowIndex; }
    unsigned rowIndex() const
    {
        ASSERT(rowIndexWasSet());
        ASSERT(!section() || !section()->needsCellRecalc()); // index may be bogus if cells need recalc.
        return m_rowIndex;
    }

    const BorderValue& borderAdjoiningTableStart() const
    {
        if (section()->hasSameDirectionAs(table()))
            return style()->borderStart();

        return style()->borderEnd();
    }

    const BorderValue& borderAdjoiningTableEnd() const
    {
        if (section()->hasSameDirectionAs(table()))
            return style()->borderEnd();

        return style()->borderStart();
    }

    const BorderValue& borderAdjoiningStartCell(const LayoutTableCell*) const;
    const BorderValue& borderAdjoiningEndCell(const LayoutTableCell*) const;

    bool nodeAtPoint(HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;

    void computeOverflow();

    const char* name() const override { return "LayoutTableRow"; }

    // Whether a row has opaque background depends on many factors, e.g. border spacing,
    // border collapsing, missing cells, etc.
    // For simplicity, just conservatively assume all table rows are not opaque.
    bool foregroundIsKnownToBeOpaqueInRect(const LayoutRect&, unsigned) const override { return false; }
    bool backgroundIsKnownToBeOpaqueInRect(const LayoutRect&) const override { return false; }

private:
    void addOverflowFromCell(const LayoutTableCell*);

    bool isOfType(LayoutObjectType type) const override { return type == LayoutObjectTableRow || LayoutTableBoxComponent::isOfType(type); }

    void willBeRemovedFromTree() override;

    void addChild(LayoutObject* child, LayoutObject* beforeChild = nullptr) override;
    void layout() override;

    PaintLayerType layerTypeRequired() const override
    {
        if (hasTransformRelatedProperty() || hasHiddenBackface() || hasClipPath() || createsGroup() || style()->shouldCompositeForCurrentAnimations() || isStickyPositioned() || style()->hasCompositorProxy())
            return NormalPaintLayer;

        if (hasOverflowClip())
            return OverflowClipPaintLayer;

        return NoPaintLayer;
    }

    void paint(const PaintInfo&, const LayoutPoint&) const override;

    void styleDidChange(StyleDifference, const ComputedStyle* oldStyle) override;

    void nextSibling() const = delete;
    void previousSibling() const = delete;

    // This field should never be read directly. It should be read through
    // rowIndex() above instead. This is to ensure that we never read this
    // value before it is set.
    unsigned m_rowIndex : 31;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutTableRow, isTableRow());

inline LayoutTableRow* LayoutTableRow::previousRow() const
{
    return toLayoutTableRow(LayoutObject::previousSibling());
}

inline LayoutTableRow* LayoutTableRow::nextRow() const
{
    return toLayoutTableRow(LayoutObject::nextSibling());
}

inline LayoutTableRow* LayoutTableSection::firstRow() const
{
    return toLayoutTableRow(firstChild());
}

inline LayoutTableRow* LayoutTableSection::lastRow() const
{
    return toLayoutTableRow(lastChild());
}

} // namespace blink

#endif // LayoutTableRow_h
