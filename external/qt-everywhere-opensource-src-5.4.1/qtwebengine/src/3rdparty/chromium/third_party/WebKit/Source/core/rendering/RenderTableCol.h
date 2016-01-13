/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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

#ifndef RenderTableCol_h
#define RenderTableCol_h

#include "core/rendering/RenderBox.h"

namespace WebCore {

class RenderTable;
class RenderTableCell;

class RenderTableCol FINAL : public RenderBox {
public:
    explicit RenderTableCol(Element*);

    RenderObject* firstChild() const { ASSERT(children() == virtualChildren()); return children()->firstChild(); }
    RenderObject* lastChild() const { ASSERT(children() == virtualChildren()); return children()->lastChild(); }

    // If you have a RenderTableCol, use firstChild or lastChild instead.
    void slowFirstChild() const WTF_DELETED_FUNCTION;
    void slowLastChild() const WTF_DELETED_FUNCTION;

    const RenderObjectChildList* children() const { return &m_children; }
    RenderObjectChildList* children() { return &m_children; }

    void clearPreferredLogicalWidthsDirtyBits();

    unsigned span() const { return m_span; }
    void setSpan(unsigned span) { m_span = span; }

    bool isTableColumnGroupWithColumnChildren() { return firstChild(); }
    bool isTableColumn() const { return style()->display() == TABLE_COLUMN; }
    bool isTableColumnGroup() const { return style()->display() == TABLE_COLUMN_GROUP; }

    RenderTableCol* enclosingColumnGroup() const;
    RenderTableCol* enclosingColumnGroupIfAdjacentBefore() const
    {
        if (previousSibling())
            return 0;
        return enclosingColumnGroup();
    }

    RenderTableCol* enclosingColumnGroupIfAdjacentAfter() const
    {
        if (nextSibling())
            return 0;
        return enclosingColumnGroup();
    }


    // Returns the next column or column-group.
    RenderTableCol* nextColumn() const;

    const BorderValue& borderAdjoiningCellStartBorder(const RenderTableCell*) const;
    const BorderValue& borderAdjoiningCellEndBorder(const RenderTableCell*) const;
    const BorderValue& borderAdjoiningCellBefore(const RenderTableCell*) const;
    const BorderValue& borderAdjoiningCellAfter(const RenderTableCell*) const;

private:
    virtual RenderObjectChildList* virtualChildren() OVERRIDE { return children(); }
    virtual const RenderObjectChildList* virtualChildren() const OVERRIDE { return children(); }

    virtual const char* renderName() const OVERRIDE { return "RenderTableCol"; }
    virtual bool isRenderTableCol() const OVERRIDE { return true; }
    virtual void updateFromElement() OVERRIDE;
    virtual void computePreferredLogicalWidths() OVERRIDE { ASSERT_NOT_REACHED(); }

    virtual void insertedIntoTree() OVERRIDE;
    virtual void willBeRemovedFromTree() OVERRIDE;

    virtual bool isChildAllowed(RenderObject*, RenderStyle*) const OVERRIDE;
    virtual bool canHaveChildren() const OVERRIDE;
    virtual LayerType layerTypeRequired() const OVERRIDE { return NoLayer; }

    virtual LayoutRect clippedOverflowRectForPaintInvalidation(const RenderLayerModelObject* paintInvalidationContainer) const OVERRIDE;
    virtual void imageChanged(WrappedImagePtr, const IntRect* = 0) OVERRIDE;

    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) OVERRIDE;

    RenderTable* table() const;

    RenderObjectChildList m_children;
    unsigned m_span;
};

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderTableCol, isRenderTableCol());

}

#endif
