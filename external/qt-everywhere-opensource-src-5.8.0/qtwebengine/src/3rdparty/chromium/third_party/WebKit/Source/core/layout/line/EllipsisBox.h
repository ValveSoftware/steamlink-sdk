/**
 * Copyright (C) 2003, 2006 Apple Computer, Inc.
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

#ifndef EllipsisBox_h
#define EllipsisBox_h

#include "core/layout/api/SelectionState.h"
#include "core/layout/line/InlineBox.h"

namespace blink {

class HitTestRequest;
class HitTestResult;

class EllipsisBox final : public InlineBox {
public:
    EllipsisBox(LineLayoutItem item, const AtomicString& ellipsisStr, InlineFlowBox* parent,
        LayoutUnit width, int height, int x, int y, bool firstLine, bool isVertical)
        : InlineBox(item, LayoutPoint(x, y), width, firstLine, true, false, false, isVertical, 0, 0, parent)
        , m_height(height)
        , m_str(ellipsisStr)
        , m_selectionState(SelectionNone)
    {
        setHasVirtualLogicalHeight();
    }

    void paint(const PaintInfo&, const LayoutPoint&, LayoutUnit lineTop, LayoutUnit lineBottom) const override;
    bool nodeAtPoint(HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, LayoutUnit lineTop, LayoutUnit lineBottom) override;
    void setSelectionState(SelectionState s) { m_selectionState = s; }
    IntRect selectionRect() const;

    LayoutUnit virtualLogicalHeight() const override { return LayoutUnit(m_height); }
    SelectionState getSelectionState() const override { return m_selectionState; }
    const AtomicString& ellipsisStr() const { return m_str; }

    const char* boxName() const override;

private:

    int m_height;
    AtomicString m_str;
    SelectionState m_selectionState;
};

} // namespace blink

#endif // EllipsisBox_h
