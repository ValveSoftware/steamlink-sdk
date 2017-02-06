// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TableRowPainter_h
#define TableRowPainter_h

#include "core/style/ShadowData.h"
#include "wtf/Allocator.h"

namespace blink {

class LayoutPoint;
class LayoutTableCell;
class LayoutTableRow;
struct PaintInfo;

class TableRowPainter {
    STACK_ALLOCATED();
public:
    TableRowPainter(const LayoutTableRow& layoutTableRow) : m_layoutTableRow(layoutTableRow) { }

    void paint(const PaintInfo&, const LayoutPoint&);
    void paintOutline(const PaintInfo&, const LayoutPoint&);
    void paintBoxShadow(const PaintInfo&, const LayoutPoint&, ShadowStyle);
    void paintBackgroundBehindCell(const LayoutTableCell&, const PaintInfo&, const LayoutPoint&);

private:
    const LayoutTableRow& m_layoutTableRow;
};

} // namespace blink

#endif // TableRowPainter_h
